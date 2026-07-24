# Freestanding Runtime and Allocation Decoupling Plan

This is the internal implementation plan for separating the Runes runtime and
fundamental library facilities from hosted Linux, producing valid freestanding
C, and proving both hosted and freestanding behavior continuously.

It does not design the operating system's physical-page allocator, scheduler,
network stack, or graphical environment. It defines the boundary those systems
must implement so ordinary Runes code can run on them.

## 1. Desired outcome

The same compiler and shared runtime logic should support two environments:

```text
Hosted build
Runes -> C -> hosted runtime core -> hosted platform adapter -> libc/OS

Freestanding build
Runes -> C -> freestanding runtime core -> OS platform adapter -> kernel
```

Realm semantics must not change between targets:

- `dynamic f` creates raw-owned storage;
- `regional f` creates arena-owned storage;
- `gc f` creates traced storage only when the selected runtime supports GC;
- `flex f` inherits the effective allocation context;
- stack code cannot directly allocate;
- provenance and escape rules remain compiler-enforced.

The source of backing memory changes. Ownership rules do not.

The first credible finish line is:

> A Runes program using checked arithmetic, slices, raw allocation, and a
> regional function compiles as freestanding C, links without libc, boots in a
> test harness, prints deterministic serial results, and passes the same
> semantic cases as its hosted build.

GC is not required for that first finish line.

## 2. Why this work is necessary

The current architecture has two separate hosted dependencies.

### Runtime coupling

`src/runtime.c` directly uses:

- `malloc`, `aligned_alloc`, `realloc`, and `free`;
- `fprintf`, `fwrite`, `stdout`, and `stderr`;
- `abort`;
- hosted `memcpy`, `memset`, `memcmp`, and `strlen`;
- `_Thread_local`;
- C atomics for GC ownership.

`src/utils/arena.c` directly uses:

- `malloc` and `free` for arena blocks and child objects;
- hosted memory and string functions.

Raw ownership goes through `runes_raw_alloc`, but arena blocks, GC metadata,
GC objects, frozen roots, promotion maps, and worklists bypass that function
and call libc directly. Replacing only `runes_raw_alloc` would therefore leave
most runtime allocation hosted.

### Generated-C coupling

`src/codegen.c` currently emits these headers unconditionally:

```c
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

Generated bounds, arithmetic, and null traps call `fprintf` and `abort`
directly. Primitive `print` emits `printf`, `fputs`, and `fputc`. Array copies
emit `memcpy`. `sqrt` lowers to `sqrtf`.

Root `main` is always emitted as hosted `int main(...)` and currently enters a
GC frame. Global runtime initializers are also injected into hosted `main`.

Consequently, `--no-prelude` is useful for name resolution, but it does not by
itself make emitted C freestanding.

## 3. Design principles

### Preserve semantics, replace mechanisms

Realm selection, provenance, promotion rules, and tracing descriptors belong
to the compiler/shared runtime. `malloc` and kernel heap calls are platform
mechanisms and must live below a replaceable boundary.

### Keep the platform ABI smaller than the runtime ABI

The platform should not know about Runes variants, type descriptors, arenas,
or GC roots. It should supply raw memory, fatal diagnostics, basic output, and
the minimum execution context required by the selected profile.

### Do not require platform `realloc`

Many kernel allocators cannot resize an allocation in place. Shared runtime
code should grow internal arrays using:

```text
allocate new
copy old bytes
release old
```

The runtime already knows the old and new capacities, so this policy is
portable and testable.

### Make unsupported facilities explicit

The first freestanding profile should support:

- stack functions;
- checked arithmetic, null, index, and range traps;
- raw/dynamic allocation after platform memory is initialized;
- regional allocation;
- allocation-free strings and slices;
- explicit platform output.

It should initially reject or clearly leave unresolved:

- `gc f`;
- promotion to GC;
- move closures requiring GC;
- floating-point printing;
- `sqrt` unless a math provider is selected;
- thread-dependent runtime behavior.

An intentional compile-time capability diagnostic is better than a kernel
that links and fails unpredictably.

### Keep the standard library above this boundary

`Option`, slice algorithms, collections, and text policy remain Runes modules.
They consume language/runtime contracts but do not implement platform memory.
Linux file descriptors, POSIX calls, and future kernel handles belong in
separate platform modules.

## 4. Target architecture

Refactor toward:

```text
src/runtime/
|-- abi.h
|-- platform.h
|-- memory.c
|-- arena.c
|-- promotion.c
|-- gc.c
|-- strings.c
|-- traps.c
|-- print.c
`-- platform/
    |-- hosted.c
    `-- freestanding_test.c
```

The existing files do not need to move in one large commit. First create the
boundary, migrate callers, and split files only when tests remain green.

Future OS code supplies another adapter:

```text
os/runtime/platform.c
```

The shared runtime must compile without including:

```text
stdio.h
stdlib.h
unistd.h
sys/*
```

The hosted adapter is allowed to include them.

## 5. Platform ABI

Start with an internal C ABI. Do not expose it as an application stdlib API.

```c
#ifndef RUNES_PLATFORM_H
#define RUNES_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

void *runes_platform_acquire(size_t size, size_t alignment);

void runes_platform_release(
    void *pointer,
    size_t size,
    size_t alignment
);

void runes_platform_write(
    const uint8_t *bytes,
    size_t length
);

_Noreturn void runes_platform_abort(void);

#endif
```

The contract is:

- `size` is nonzero by the time it reaches the provider;
- `alignment` is a nonzero power of two;
- successful acquisition returns suitably aligned, non-overlapping storage;
- acquisition returns null on exhaustion;
- release receives the same logical size/alignment associated with acquisition;
- releasing null is either forbidden before the call or explicitly a no-op;
- `write` is best-effort diagnostic output and must not allocate;
- `abort` never returns and must not allocate.

Do not pass source locations or formatted strings to the allocator. The shared
runtime handles validation and constructs diagnostics before calling the
minimal platform operations.

### Recovering size and alignment for `raw_free(pointer)`

The public low-level source operation currently frees only by pointer:

```runes
raw_free(pointer)
```

If the platform release operation requires size and alignment, raw allocation
must store an internal header:

```text
platform allocation base
| runtime header: base, total size, platform alignment
| alignment padding
` user-visible payload
```

`runes_raw_free` recovers the header and passes the original platform
allocation to `runes_platform_release`.

Arena blocks and GC objects already know their backing allocation sizes and
can release directly. Add exact size/alignment fields where current metadata
is insufficient.

The header design must:

- preserve requested payload alignment;
- check header/padding arithmetic;
- define a recognizable debug magic in test builds;
- poison or invalidate the header before release in debug builds;
- never expose header bytes as the typed allocation;
- support zero-size source requests through the existing one-byte policy.

### Internal runtime growth helper

Replace every direct `realloc` with one helper:

```c
bool runes_runtime_grow_array(
    void **items,
    size_t old_count,
    size_t new_count,
    size_t element_size,
    size_t element_alignment
);
```

It must:

1. check all multiplication and padding arithmetic;
2. acquire new storage;
3. copy the initialized prefix;
4. release the previous storage using its known size/alignment;
5. leave the original pointer unchanged on failure.

Use it for:

- GC mark/protection worklists;
- promotion clone maps;
- any future runtime-owned growable table.

## 6. Hosted platform adapter

The first adapter preserves current behavior:

```text
runes_platform_acquire -> aligned hosted allocation
runes_platform_release -> free
runes_platform_write   -> fwrite to stderr or selected runtime stream
runes_platform_abort   -> abort
```

This migration must be behavior-neutral. Existing generated programs and
diagnostic substrings should remain unchanged.

Use the hosted adapter to prove that all shared runtime allocation passes
through the new boundary before adding freestanding code.

Add a repository check that rejects these identifiers outside hosted/test
platform files:

```text
malloc
calloc
realloc
aligned_alloc
free
fprintf
fwrite
stdout
stderr
abort
```

The check should search shared runtime sources, not compiler implementation
sources; the compiler itself is a hosted development tool and may continue
using libc.

## 7. Memory primitives

Freestanding linking cannot assume libc supplies:

```text
memcpy
memmove
memset
memcmp
strlen
```

Compilers may also synthesize calls to memory primitives even when the source
does not explicitly call them. GCC documents that `-nostdlib` builds must
supply functions such as `memcpy`, `memset`, `memmove`, and `memcmp` when
generated code requires them.

Provide two levels:

### Shared internal operations

```c
void *runes_memory_copy(void *dst, const void *src, size_t count);
void *runes_memory_move(void *dst, const void *src, size_t count);
void *runes_memory_set(void *dst, uint8_t value, size_t count);
int runes_memory_compare(const void *left, const void *right, size_t count);
size_t runes_c_string_length(const char *value);
```

Shared runtime code uses only these names.

### Freestanding compiler-support symbols

The freestanding runtime additionally exports standard signatures:

```c
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
```

These may delegate to the internal operations. Keep them simple enough that
the compiler does not optimize their implementations recursively back into
calls to themselves. Test at both `-O0` and `-O2`.

`strlen` is required only if emitted/shared code still uses it. Prefer removing
implicit NUL-string dependencies from the freestanding path.

## 8. Traps and primitive output

Generated C must stop formatting traps directly with stdio.

Add stable runtime functions:

```c
_Noreturn void runes_trap_bounds(
    size_t index,
    size_t length,
    unsigned line,
    unsigned column
);

_Noreturn void runes_trap_arithmetic(
    uint32_t operation,
    unsigned line,
    unsigned column
);

_Noreturn void runes_trap_null(
    unsigned line,
    unsigned column
);

_Noreturn void runes_trap_runtime(
    uint32_t category,
    unsigned line,
    unsigned column
);
```

Use numeric categories internally rather than requiring `printf` formatting.
The runtime can render decimal/hex integers using small allocation-free
formatters and call `runes_platform_write`.

Hosted output must preserve current diagnostic text where tests depend on it.
The freestanding adapter may send the same bytes to serial.

Primitive `print` should lower to runtime operations rather than stdio:

```c
void runes_print_i64(int64_t);
void runes_print_u64(uint64_t);
void runes_print_bool(bool);
void runes_print_char(uint32_t, unsigned, unsigned);
void runes_print_str(RunesStr);
void runes_print_newline(void);
```

Float printing can remain hosted-only initially. The compiler should report a
target-capability error for freestanding float printing until a deliberate
formatter exists.

## 9. Freestanding generated C

Introduce an explicit compiler target/profile. `--no-prelude` is not a target.

Initial spelling can be:

```text
--target hosted
--target x86_64-freestanding
```

The option must reach lexer-to-codegen orchestration and be recorded in the
codegen context. `runec` must pass it through to the lower-level compiler.

### Freestanding preamble

Freestanding generated C may use headers guaranteed by the selected C
toolchain/profile:

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
```

It must not unconditionally include:

```c
<math.h>
<stdio.h>
<stdlib.h>
<string.h>
```

Move the duplicated Runes runtime ABI declarations into a generated or shared
freestanding-safe ABI header, or emit only the declarations actually required
by the translation unit.

### Entry points

Do not force freestanding code through hosted `int main`.

The first profile should:

- permit an exported function such as `kernel_main`;
- respect `#[link_name]` and calling convention;
- emit no C runtime startup assumptions;
- avoid automatically entering a GC frame;
- forbid nonconstant global runtime initializers until there is an explicit
  target initialization function.

Later, the compiler may emit:

```text
runes_module_initialize()
```

for the assembly/boot runtime to call before the exported entry. Do not hide
that call in platform-specific magic.

### Capability validation

Before C emission, reject used features unavailable in the selected runtime:

```text
gc realm requires target capability: gc
GC promotion requires target capability: gc
float print requires target capability: float-format
sqrt requires target capability: math-sqrt
move closure allocation requires target capability: allocation
```

Capabilities should describe behavior, not OS names. A future freestanding
runtime may support GC without becoming hosted.

### Toolchain flags

The reference x86-64 freestanding compile should begin with:

```text
-std=c11
-ffreestanding
-fno-stack-protector
-fno-pic
-mno-red-zone
-Wall
-Wextra
-Werror
```

Link separately with:

```text
-nostdlib
-Wl,-T,path/to/linker.ld
-Wl,-Map,build/kernel.map
```

Whether `-lgcc` or compiler-rt is required must be decided from the undefined
symbol audit, not assumed. GCC notes that `-nostdlib` also omits `libgcc`, even
though generated code may still require compiler support routines.

Use a target triple/cross compiler deliberately. Clang supports target
selection through `-target`; GCC commonly uses a target-specific cross-driver.

## 10. Driver changes

Replace the current hardcoded hosted recipe in `runec` with target
descriptions.

Conceptually:

```text
hosted:
    prelude = src/std/prelude.runes
    runtime = shared + platform/hosted.c
    cflags = hosted C11 flags
    libraries = -lm
    output = process executable

x86_64-freestanding:
    prelude = selected freestanding prelude
    runtime = shared subset + OS/test adapter
    cflags = freestanding x86-64 flags
    startup = supplied boot/start object
    linker_script = supplied script
    libraries = explicit allowlist
    output = ELF kernel/object
```

Do not bake one particular OS repository or bootloader into the Runes
compiler. The target establishes C/runtime semantics; project configuration
supplies startup objects and linker script.

An initial command may be:

```bash
runec build \
    --target x86_64-freestanding \
    --runtime path/to/runtime \
    kernel.runes \
    -o build/kernel.elf \
    -- startup.o -T linker.ld
```

Improve manifest integration only after this explicit command works.

## 11. Thread-local and atomic policy

The current arena and GC runtime uses `_Thread_local`; the GC also uses C
atomics to claim one owner thread.

The first freestanding profile is single-thread/single-core and should not
silently depend on a TLS runtime.

Implement one explicit policy:

- compile the raw/regional runtime with a single execution-context state
  object; or
- require the platform to return the current runtime context.

Prefer an explicit context abstraction:

```c
RunesExecutionContext *runes_platform_execution_context(void);
```

Hosted implementation may use `_Thread_local`. A single-core kernel initially
returns one static context. A multicore kernel later returns per-CPU state.

Keep GC excluded from the first freestanding capability set. Port it only after
per-thread/per-CPU roots, atomic guarantees, safepoints, and interrupt policy
are designed.

## 12. Migration milestones

Each milestone must leave hosted tests green.

### Milestone 1: freeze current runtime behavior

Before refactoring:

- record allocation alignment, zero-size, OOM, and diagnostic behavior;
- add missing direct tests for raw allocation and release;
- record existing arena and GC debug-counter expectations;
- keep current generated-C output tests available as behavioral baselines.

Done when the existing suite and the new boundary cases pass under GCC and
Clang.

### Milestone 2: introduce the platform ABI and hosted adapter

Add `runtime/platform.h` and the hosted implementation. Route only
`runes_raw_alloc`, aligned raw allocation, raw release, runtime failure, and
diagnostic writes through it.

Done when no observable hosted behavior changes and sanitizer tests pass.

### Milestone 3: remove direct libc allocation from arena and promotion

Route:

- arena scope objects;
- root/child arena structures;
- arena backing blocks;
- promotion clone maps;
- dynamic promotion allocations

through the platform ABI and shared growth helper.

Done when an automated source audit finds no allocator calls in these shared
modules.

### Milestone 4: remove direct libc allocation from GC

Migrate:

- GC objects and backing allocations;
- frame and root records;
- frozen roots;
- mark/protection worklists.

This makes GC allocator-independent even though the first freestanding target
does not enable it.

Done when hosted GC semantics, forced collection, promotion, and sanitizers
remain green with failure injection.

### Milestone 5: centralize memory primitives

Replace shared runtime use of libc memory/string calls with internal
operations. Provide freestanding compiler-support symbols.

Done when the shared runtime compiles with forbidden hosted headers removed and
memory primitive property tests pass.

### Milestone 6: move traps and print behind runtime operations

Generated C no longer emits `fprintf`, `printf`, `fputs`, `fputc`, `fwrite`,
or `abort`.

Done when hosted diagnostics remain byte-for-byte compatible and a fake
platform captures the same messages without stdio.

### Milestone 7: implement target-aware C emission

Add compiler target selection, freestanding preamble, entry-point policy, and
capability diagnostics.

Done when a minimal non-hosted Runes translation unit compiles with
`-ffreestanding -Werror` and contains no hosted includes.

### Milestone 8: add freestanding compile and link fixtures

Create:

```text
src/tests/freestanding/
|-- platform.c
|-- start.S
|-- linker.ld
|-- positive/
|-- negative/
`-- test.bash
```

The test platform initially uses a fixed static byte arena; it does not need a
real page allocator.

Done when the fixture links with `-nostdlib` and undefined symbols match an
empty or documented allowlist.

### Milestone 9: execute in QEMU

Boot a minimal image and report results through serial plus a deterministic
exit mechanism.

Test:

- checked arithmetic success;
- bounds/null/arithmetic trap routing;
- aligned raw allocation;
- OOM behavior;
- arena allocation and root cleanup;
- early and fallible regional exits;
- string/slice operations supported by the profile.

Done when tests have timeouts, deterministic exit status, and captured serial
expectations.

### Milestone 10: publish the OS adapter contract

Document exactly what an external OS repository supplies:

- memory acquire/release;
- execution context;
- diagnostic write;
- fatal abort;
- startup/linker integration;
- enabled runtime capabilities.

Done when the QEMU test adapter and one external-style sample adapter both
implement the same headers without modifying shared runtime sources.

### Milestone 11: enable higher runtime capabilities deliberately

Consider, separately:

- regional execution across multiple CPUs;
- GC;
- float formatting;
- math functions;
- move closures in each realm;
- module runtime initialization.

Each capability needs its own implementation, negative tests while disabled,
and QEMU/runtime tests when enabled.

## 13. Testing methodology

### Hosted regression suite

Every milestone runs:

```bash
make test
make test-codegen
make test-runtime-sanitize
make test-sanitize
```

Hosted behavior is not allowed to regress merely because freestanding support
is being added.

### Platform-contract unit tests

Use a fake provider with:

- configurable capacity;
- forced failure after allocation N;
- alignment verification;
- allocation/release counters;
- active-allocation table;
- canaries before and after payloads;
- fill patterns on acquire/release.

Test every runtime subsystem against this provider.

Required properties:

- returned ranges never overlap while live;
- alignment holds for every supported power of two;
- size arithmetic traps before provider invocation;
- every successful internal acquisition is eventually released;
- failed growth keeps the original table intact;
- arena root teardown releases all child blocks;
- promotion failure does not leak partially cloned graphs;
- GC failure/collection paths preserve runtime invariants.

### Freestanding compile-only tests

For representative generated files:

```bash
gcc -ffreestanding -fno-builtin -Werror -c generated.c
clang -ffreestanding -fno-builtin -Werror -c generated.c
```

Check:

- no hosted headers;
- no implicit function declarations;
- no warnings;
- expected sections and exported symbols;
- no `main` requirement;
- disabled features produce Runes diagnostics before C compilation.

### Undefined-symbol audit

After freestanding linking or partial linking:

```bash
nm -u object-or-elf
```

Compare the result to a checked-in allowlist. The goal for the final kernel ELF
is no unresolved symbols.

Pay special attention to:

```text
memcpy
memmove
memset
memcmp
__stack_chk_fail
compiler-rt/libgcc division helpers
TLS helpers
libatomic helpers
```

### Hosted/freestanding differential tests

Run the same allocation-free and raw/regional semantic program:

1. as an ordinary hosted executable;
2. through the freestanding test/QEMU adapter.

Compare normalized result records rather than platform-specific decoration.
This catches semantic drift in arithmetic, matches, slices, errors, allocation,
and cleanup.

### QEMU integration tests

Every QEMU test must have:

- a fixed timeout;
- serial capture;
- a deterministic success/failure exit;
- no need for visual inspection;
- one purpose per image where practical.

Keep a tiny smoke image in the default CI path. Larger stress images can run in
an extended job.

### Sanitizers and host tools

ASan/UBSan remain hosted tests for the shared runtime. Freestanding QEMU does
not replace them.

Add:

- leak checks for the hosted platform adapter;
- failure-injection cleanup tests;
- allocator canaries;
- randomized allocation/alignment sequences;
- randomized arena tree creation/destruction;
- randomized promotion graphs where existing descriptors permit it.

### Optimization matrix

At minimum, test:

```text
GCC   -O0 and -O2
Clang -O0 and -O2
```

Memory primitives and ABI code are particularly vulnerable to optimizer-only
failures.

## 14. Standard-library dependency policy

Classify stdlib modules by allowed dependencies:

```text
portable core:
    no platform calls, preferably no allocation

portable owning:
    may use Runes allocation contracts
    no Linux/OS calls

portable behavior:
    Reader/Writer/Clock-style contracts

platform modules:
    Linux bindings or OS bindings
```

Enforce these rules structurally:

```text
std.core            -> nothing platform-specific
std.bytes           -> std.core
std.collections     -> core + allocation contracts
std.text            -> core + collections/runtime UTF-8
std.io              -> portable types/interfaces
std.os.linux        -> Linux FFI
```

Do not make `std.io` call Linux `read` directly forever. Linux file-descriptor
implementations belong in `std.os.linux` or a hosted adapter. A future Runes OS
can implement the same portable I/O behavior using native handles.

## 15. Work that should not be mixed into this refactor

Do not simultaneously:

- implement the kernel page allocator;
- implement the graphical stack;
- add networking;
- redesign every realm;
- add Linux binary compatibility;
- self-host the compiler;
- stabilize a large stdlib API;
- port GC to multicore kernel execution.

The runtime boundary must be testable using a fixed fake memory provider before
real kernel memory exists.

## 16. External implementation references

These are toolchain contracts used by this plan:

- [GCC freestanding and hosted environments](https://gcc.gnu.org/onlinedocs/gcc/Standards.html)
  explains that kernels are freestanding, startup is implementation-defined,
  and `-ffreestanding` changes compiler assumptions.
- [GCC link options](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html)
  documents `-nostdlib` and warns that memory functions and compiler support
  routines may still need explicit implementations/libraries.
- [Clang cross-compilation](https://clang.llvm.org/docs/CrossCompilation.html)
  documents target triples and the `-target` mechanism.
- [GNU linker scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)
  documents using `-T` scripts to control output memory layout.

They define C/toolchain mechanics, not Runes language semantics.

## 17. Definition of done

Allocation is decoupled from Linux when:

- shared runtime sources contain no direct hosted allocator calls;
- every raw, arena, promotion, GC, and runtime-bookkeeping allocation passes
  through the platform ABI;
- the hosted adapter preserves all current tests;
- a fake provider can force and observe every allocation path;
- platform release receives enough metadata to release correctly;
- the source audit preventing regression runs in CI.

Freestanding C is supported when:

- the compiler has an explicit freestanding target;
- generated C contains no unconditional hosted headers or stdio/abort calls;
- entry and initialization do not assume hosted `main`;
- unsupported runtime features fail with target-capability diagnostics;
- representative code compiles with both GCC and Clang `-ffreestanding`;
- a fixture links with `-nostdlib`;
- undefined symbols are audited;
- raw and regional Runes programs execute successfully in QEMU;
- hosted/freestanding differential tests agree;
- all existing hosted gates remain green.

Only after these conditions hold should the OS kernel replace the fixed test
provider with its real heap.
