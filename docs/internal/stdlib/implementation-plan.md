# Standard Library Implementation Plan

This is the concrete starting plan for the Runes standard library. It turns the
broader [standard-library roadmap](roadmap.md) into an ordered sequence
of implementable milestones.

## First objective

The first meaningful finish line is:

> A normal Runes application imports `std.io`, safely reads stdin into a fixed
> buffer, uses `Option` and typed errors, and contains no `unsafe` code.

This goal is intentionally small. It establishes module layout, generic core
types, byte operations, nominal errors, an audited operating-system boundary,
and executable stdlib tests without first requiring containers or owning
strings.

## Why this direction

The order is based on dependencies, not on which APIs sound most exciting.
Every later module should be built from smaller pieces that are already tested.

Think of it like building a kitchen:

- tests are the measuring tools;
- `Option` is a basic container for “there may be a value”;
- byte slices are the raw ingredients used by parsers, strings, and files;
- the OS layer is the dangerous gas/electric connection;
- `std.io` is the safe switch applications are allowed to touch;
- typed allocation is the rule for acquiring storage safely;
- vectors and strings are the reusable work surfaces built on that storage.

Starting with `HashMap`, networking, or a large `String` API would hide several
unfinished problems under one complicated implementation. A failure could come
from generics, allocation, UTF-8, modules, pointer lifetimes, I/O, or the
container itself. Building in dependency order keeps each failure attributable
to one small layer.

### Why tests come first

The stdlib will become code that every Runes program trusts. A broken compiler
feature can make a broken library appear to work, and a broken library can make
the language appear unreliable.

The test harness gives every API a concrete definition:

- what compiles;
- what output it produces;
- what misuse is rejected;
- which memory realms it works in;
- whether raw ownership survives sanitizer checks.

Without this harness, the stdlib roadmap would be a collection of intentions
rather than an implementation plan.

### Why `Option<T>` comes first

Many APIs need to say “a value might not exist”: searching a byte, reading a
configuration entry, popping a collection, or finding a map key.

`Option<T>` solves that without allocation or operating-system code. It also
stress-tests generic variants, methods, matching, imports, and visibility—the
same compiler features that later containers will require.

Runes already has `!T` for operations that can fail, so a separate `Result<T,
E>` type is not the first priority:

- use `Option<T>` when absence is normal;
- use `!T` when an operation failed;
- use a normal value when a result is guaranteed.

### Why bytes come before strings

Files, terminals, sockets, encodings, parsers, and C APIs ultimately exchange
bytes. A string adds the extra rule that those bytes must be valid UTF-8.

If byte copying, searching, slicing, and bounds handling are not correct, a
string implementation cannot be correct either. Byte-slice APIs also work
without owning allocation, so they let the library become useful before the
typed-allocation design is settled.

### Why the raw OS layer is separate from `std.io`

The Linux `read` system call accepts a pointer and an unrelated length. The
compiler cannot prove that the pointer references that many writable bytes.
Calling it therefore belongs inside a small audited `unsafe` boundary.

Applications should receive this API instead:

```runes
read_into(fd, writable_slice)
```

The slice already keeps its pointer and length together. `std.io` can perform
the unsafe call once, validate the result, and give every application a safe
interface.

This separation does not pretend the OS is safe. It concentrates the dangerous
assumptions in one reviewable place.

### Why Linux comes before a portable I/O abstraction

The current compiler target is hosted Linux x86-64. Implementing and testing
one real backend contract is more useful than designing a supposedly portable
interface with no second platform implementation.

The public `std.io` contract should avoid unnecessary Linux details, while
`std.os.linux` contains the platform-specific binding. A future platform can
implement the same high-level behavior behind its own OS module.

### Why fixed buffers come before `Vec` and `String`

A fixed `[4096]u8` buffer already supports useful input programs. It has clear
ownership, no resizing, and no deallocation requirement.

That lets Runes prove the safe I/O boundary before mixing in:

- dynamic allocation;
- capacity growth;
- reallocation and copying;
- ownership transfer;
- UTF-8 validation;
- realm-specific cleanup.

The fixed buffer is not the final user experience. It is the smallest reliable
step toward it.

### Why typed allocation must precede owning containers

The current bootstrap allocation form is:

```runes
alloc(sizeof(T)) as *T
```

It works, but the programmer manually repeats the type in the size calculation
and cast. A mismatch can request the wrong size or give the GC the wrong idea
about pointer-bearing fields.

Containers allocate frequently and are responsible for many elements. Baking
the byte-oriented form into every container would spread unsafe assumptions
throughout the stdlib. A typed operation such as `alloc<T>()` or
`alloc_array<T>(count)` gives the compiler one place to derive size, alignment,
pointer type, realm provenance, checked arithmetic, and GC metadata.

### Why ownership-specific containers are separate

The same word “vector” can hide very different cleanup obligations:

- raw memory must be explicitly released;
- arena memory must not be individually released;
- GC memory must remain traceable and must not be passed to `raw_free`.

One container whose cleanup silently changes with its caller would be easy to
misuse. Separate `Vec<T>`, `ArenaVec<T>`, and `GcVec<T>` families make the
ownership policy visible in the type and documentation.

They may share algorithms internally, but they should not lie to users about
who owns the memory.

### Why strings come after vectors

An owning UTF-8 string is primarily:

1. owned growable bytes;
2. a guarantee that those bytes are valid UTF-8;
3. boundary-aware text operations.

Implementing growable storage once in a tested vector foundation avoids
building a second allocator and capacity system inside `String`.

### Why a small CLI is the final proof

Individual module tests prove pieces in isolation. A small compiler-oriented
CLI proves that modules, errors, I/O, bytes, allocation, vectors, and strings
work together in an ordinary program.

If the stdlib cannot comfortably support one real command-line tool, it is not
ready for larger goals such as self-hosting, servers, package management, or
language tooling.

## Current state

The standard-library namespace exists, but very little library code has been
written:

- `src/std/mod.runes` declares the `core` and `os` modules;
- `src/std/core.runes` is empty;
- `src/std/os.runes` is empty;
- `src/std/prelude.runes` contains compiler/runtime ABI declarations, not
  application-level library APIs.

The prelude must remain limited to compiler-required runtime contracts.
Ordinary library APIs should be imported explicitly from `std`.

## Target source layout

Start with:

```text
src/std/
├── mod.runes
├── prelude.runes
├── core.runes
├── core/
│   └── option.runes
├── bytes.runes
├── io.runes
├── os.runes
└── os/
    └── linux.runes
```

Responsibilities:

- `prelude.runes`: compiler/runtime contracts only;
- `core.option`: generic optional values;
- `bytes`: safe operations over borrowed byte slices;
- `os.linux`: raw unsafe POSIX declarations;
- `io`: safe wrappers around raw operating-system operations.

## Milestone 1: add a stdlib test harness

Create:

```text
src/tests/stdlib/
├── positive/
├── negative/
└── test.bash
```

Add a Make target:

```make
test-stdlib:
	bash src/tests/stdlib/test.bash
```

The harness must:

- compile and run positive examples;
- compare output with declared expectations;
- compile negative examples and check diagnostic substrings;
- use the repository standard library and normal prelude;
- make it easy to add sanitizer coverage for owning containers later.

This comes first so no stdlib API is accepted without executable behavior.

## Milestone 2: implement `std.core.option`

Start with:

```runes
pub type Option<T> =
    | None
    | Some(T)
```

Implement only:

```text
is_some
is_none
unwrap_or
map
```

Example target:

```runes
use std.core.option.Option

f port_from_config(found: bool) = result: Option<u32> {
    if found {
        result = Option.Some(8080)
    } else {
        result = Option.None
    }
}
```

Do not begin with `take`, iterators, pointer conversions, or a large collection
of combinators. First prove:

- `Option<i32>` and `Option<str>` work;
- `Some` and `None` match across module boundaries;
- generic methods work after importing the type;
- `map` works with an ordinary function value;
- invalid payload types produce useful diagnostics.

`Option` is the correct first type because it requires no allocation while
exercising generic variants, methods, visibility, modules, and pattern
matching.

## Milestone 3: implement `std.bytes`

Build allocation-free operations over `[]u8` and `[]const u8`.

Initial API:

```text
fill(buffer: []u8, value: u8)
copy(destination: []u8, source: []const u8) -> !usize
equal(left: []const u8, right: []const u8) -> bool
find(buffer: []const u8, byte: u8) -> Option<usize>
starts_with(buffer: []const u8, prefix: []const u8) -> bool
```

Use checked loops and indexing before optimizing with runtime memory
primitives. Test:

- fixed arrays coercing to slices;
- mutable and read-only inputs;
- empty slices;
- destination-too-small errors;
- byte search at the start, middle, end, and absent case;
- rejection of mutation through read-only slices.

These functions become the base for parsers, strings, file I/O, protocols, and
compiler source handling.

## Milestone 4: establish the raw Linux I/O boundary

Put raw bindings in `std.os.linux`:

```runes
extern f read(
    fd: i32,
    buffer: *u8,
    count: usize
) = result: i64
```

This binding must remain unsafe. The pointer and count are independent, so its
type cannot prove that the destination is valid and writable for `count`
bytes.

The current compiler exposes extern declarations across their containing
module. Until private externs exist, place raw bindings in an explicitly
low-level module and document them as unstable. Do not pretend that the
visibility gap makes the binding safe.

## Milestone 5: expose safe `std.io.read_into`

Define nominal errors and an explicit EOF result:

```runes
error IoError = {
    | ReadFailed
    | InvalidForeignResult
}

pub type ReadResult =
    | Eof
    | Read(usize)
```

The intended API is:

```runes
pub f read_into(
    fd: i32,
    buffer: []u8
) = result: !ReadResult {
    unsafe {
        i64 count = read(fd, buffer.ptr, buffer.len)

        if count < 0 {
            result = error.IoError.ReadFailed
        } else if count == 0 {
            result = ReadResult.Eof
        } else if count as usize > buffer.len {
            result = error.IoError.InvalidForeignResult
        } else {
            result = ReadResult.Read(count as usize)
        }
    }
}
```

Adjust syntax as required by the compiler, but preserve the contract:

- callers supply one writable slice;
- pointer and length come from that same slice;
- application code does not need `unsafe`;
- EOF is distinct from failure;
- negative results become nominal errors;
- an impossible foreign result larger than the buffer is rejected;
- partial reads are returned rather than hidden.

Initially, `ReadFailed` may not preserve the native error code. Before treating
the API as stable, add a deliberate errno/native-error representation.

## Milestone 6: ship the first real stdlib program

Write an executable stdin byte counter:

```runes
use std.io.read_into

f main() {
    [4096]u8 buffer = []

    match read_into(0, buffer) {
        Ok(Read(count)) -> print("read ", count, " bytes"),
        Ok(Eof) -> print("end of input"),
        Err(problem) -> print("read failed: ", problem),
    }
}
```

Acceptance criteria:

- the application contains no `unsafe`;
- raw pointer use exists only inside the stdlib boundary;
- input, EOF, and error paths are tested;
- the program builds through an ordinary `runec` project;
- an executable test pipes known stdin and checks exact output.

At this point Runes has the beginning of a real standard library rather than
only a namespace and design document.

## Milestone 7: settle typed allocation

Do not implement `Vec<T>` before deciding the language-facing typed allocation
surface.

The bootstrap form:

```runes
alloc(sizeof(T)) as *T
```

works, but repeating unsafe size/cast logic throughout containers would make
the stdlib fragile. Prefer a compiler-supported surface such as:

```runes
alloc<T>()
alloc_array<T>(count)
```

The compiler can then derive:

- size and alignment;
- result pointer type;
- realm provenance;
- GC tracing descriptor;
- checked sequence-size arithmetic.

Keep `raw_alloc(bytes)` as an explicit low-level escape hatch rather than the
normal container interface.

## Milestone 8: implement owning containers

Do not make one container whose cleanup contract changes invisibly by realm.
Use distinct ownership families:

```text
Vec<T>       raw/dynamic ownership with explicit deinit
ArenaVec<T>  regional ownership with arena cleanup
GcVec<T>     traced GC ownership
```

Implementation order:

1. `RawBox<T>`;
2. `Vec<T>`;
3. `ArenaVec<T>`;
4. `GcVec<T>`;
5. owning UTF-8 `String`.

Minimum `Vec<T>` API:

```text
new
with_capacity
len
capacity
as_slice
as_mut_slice
push
pop
reserve
clear
deinit
```

Do not initially add insertion, sorting, deduplication, retain, iterators, or
dozens of convenience methods. First prove:

- capacity growth is checked;
- elements survive reallocation;
- slices expose the correct lifetime;
- `deinit` releases raw ownership exactly once by contract;
- arena storage is never passed to `raw_free`;
- GC element references remain traced;
- realm escape errors are diagnosed;
- sanitizer tests pass.

## Milestone 9: build owning text

Build `String` and `StringBuilder` on stable byte storage:

```text
from_utf8
from_utf8_unchecked
as_str
as_bytes
push_char
push_str
reserve
clear
deinit
```

Reuse the UTF-8 runtime contracts already declared in `prelude.runes`.
Do not initially implement normalization, grapheme segmentation, locale-aware
case conversion, or formatting macros.

## Required test inventory

Grow the test tree toward:

```text
src/tests/stdlib/
├── positive/
│   ├── option.runes
│   ├── bytes.runes
│   ├── read_into.runes
│   ├── vec_dynamic.runes
│   ├── vec_regional.runes
│   └── vec_gc.runes
├── negative/
│   ├── readonly_bytes.runes
│   ├── escaped_arena_vec.runes
│   └── invalid_raw_free.runes
└── test.bash
```

Every stdlib module should have:

- executable expected-output coverage;
- expected diagnostics for important misuse;
- tests in every supported memory realm when it allocates;
- sanitizer coverage for raw ownership;
- small, documented unsafe boundaries.

## Work that should wait

Do not start with:

- hash maps;
- files and path normalization beyond the minimal read boundary;
- networking or HTTP;
- async or threads;
- formatting macros;
- Unicode grapheme support;
- logging frameworks;
- package management.

Those features depend on stable bytes, allocation, strings, errors, and I/O.
Starting with them would repeatedly force redesign of their foundations.

## The 11 implementation milestones

### Milestone 1 of 11: build the stdlib test harness

**What gets built:** `src/tests/stdlib`, positive and negative test runners,
expected-output checking, and a `make test-stdlib` target.

**ELI5:** Before making library pieces, build a machine that tells us whether a
piece works or is broken.

**Why it matters:** Every later API gets an executable contract instead of
being accepted because one example happened to compile.

**What it unlocks:** Safe, repeatable implementation of every remaining
milestone.

**Done when:** one positive and one expected-failure sample run through
`make test-stdlib`, and failures make the target fail.

### Milestone 2 of 11: implement `Option<T>`

**What gets built:** `Some(T)`, `None`, `is_some`, `is_none`, `unwrap_or`, and
`map`.

**ELI5:** Give the language a standard box that can either contain one value or
be empty.

**Why it matters:** Search, parsing, collections, configuration, and lookup APIs
need to represent ordinary absence without inventing sentinel values.

**What it unlocks:** `bytes.find`, collection `pop`, map lookup, optional
configuration, and many iterator operations.

**Done when:** `Option<i32>` and `Option<str>` work across modules, matching and
methods work, and invalid payload use has a tested diagnostic.

### Milestone 3 of 11: implement byte-slice utilities

**What gets built:** `fill`, `copy`, `equal`, `find`, and `starts_with` for
borrowed byte slices.

**ELI5:** Teach programs how to safely look at, compare, search, and copy raw
bytes.

**Why it matters:** Bytes are the common input format for files, terminals,
networking, parsers, encodings, and strings.

**What it unlocks:** Safe I/O processing, integer parsing, UTF-8 construction,
protocol decoding, and later `String` operations.

**Done when:** arrays and slices work, empty inputs are covered, destination
capacity is checked, and read-only mutation is rejected.

### Milestone 4 of 11: add the raw Linux OS bridge

**What gets built:** a minimal `std.os.linux.read` declaration and its audited
unsafe calling boundary.

**ELI5:** Connect Runes to the operating system through one clearly marked
dangerous wire.

**Why it matters:** Useful programs need external input, but raw OS pointers and
lengths cannot be treated as safe.

**What it unlocks:** implementation of a safe `std.io` wrapper.

**Done when:** the raw binding can read known stdin data in a test, remains
unsafe to call, and is isolated in the platform module.

### Milestone 5 of 11: implement safe `std.io.read_into`

**What gets built:** `IoError`, `ReadResult`, and a safe wrapper accepting one
writable slice.

**ELI5:** Put a safe switch over the dangerous OS wire so normal applications
do not have to touch it.

**Why it matters:** Pointer and length come from one checked slice, EOF is
separate from failure, and impossible OS results are rejected.

**What it unlocks:** safe terminal input, file readers, buffered readers, and
parsers that contain no application-level unsafe code.

**Done when:** successful, partial, EOF, negative-result, and oversized-result
paths are tested.

### Milestone 6 of 11: ship the stdin byte counter

**What gets built:** a real example program that reads stdin with `std.io` and
reports the number of bytes or a typed failure.

**ELI5:** Use the pieces together to build the first actually useful toy.

**Why it matters:** It proves imports, `Option`, errors, bytes, OS bindings, and
safe I/O work together outside isolated unit samples.

**What it unlocks:** confidence to design the owning-memory layer without also
debugging the basic module and I/O stack.

**Done when:** the application has no `unsafe`, handles input/EOF/error, and an
automated test pipes input and checks exact output.

### Milestone 7 of 11: implement typed allocation

**What gets built:** the final compiler-supported equivalent of `alloc<T>()`
and `alloc_array<T>(count)`, with documented behavior in every memory realm.

**ELI5:** Instead of asking for “some number of mystery bytes,” ask for “one
Node” or “ten Nodes,” and let the compiler calculate the dangerous details.

**Why it matters:** It prevents size/cast mismatches and gives arena and GC code
correct type, alignment, provenance, and tracing information.

**What it unlocks:** containers that can allocate without duplicating unsafe
byte calculations throughout the stdlib.

**Done when:** object and sequence allocation work in dynamic, regional, GC,
and flex contexts; stack behavior is explicit; overflow and realm misuse are
tested.

### Milestone 8 of 11: implement `RawBox<T>` and `Vec<T>`

**What gets built:** one raw-owned value and a raw-owned growable sequence with
explicit `deinit`.

**ELI5:** Build a resizable row of values whose owner knows it must clean up
afterward.

**Why it matters:** Most useful programs need collections larger than a fixed
compile-time array.

**What it unlocks:** growable buffers, compiler token/AST lists, builders,
stacks, queues, and the storage foundation for owning strings.

**Done when:** growth preserves elements, capacity arithmetic is checked,
slices borrow correctly, `deinit` releases ownership, misuse tests exist, and
sanitizers pass.

### Milestone 9 of 11: implement `ArenaVec<T>` and `GcVec<T>`

**What gets built:** growable sequences whose cleanup follows arena and GC
ownership instead of `raw_free`.

**ELI5:** Make versions of the resizable row for “throw the whole workspace
away together” and “let the collector find unused objects.”

**Why it matters:** Realm-aware allocation is a central language feature. The
stdlib should not force every collection into raw manual ownership.

**What it unlocks:** efficient parser/compiler arenas and convenient managed
graphs or application data without pretending all memory has one owner.

**Done when:** all three vector families share expected behavior, but escape,
freeing, and tracing rules remain distinct and tested.

### Milestone 10 of 11: implement owning `String`

**What gets built:** validated UTF-8 ownership, `StringBuilder`, borrowed
`str`/byte views, append operations, capacity management, and explicit cleanup
where required.

**ELI5:** Turn the tested growable byte row into text that promises its bytes
form valid UTF-8.

**Why it matters:** Command-line tools, diagnostics, paths, parsers, and user
input need owned text rather than only borrowed string literals.

**What it unlocks:** line reading, parsing, formatting builders, diagnostics,
file content, process arguments, and most user-facing APIs.

**Done when:** valid/invalid UTF-8 construction, scalar appending, growth,
borrowing, C conversion, realm behavior, and cleanup are tested.

### Milestone 11 of 11: build a compiler-oriented CLI

**What gets built:** a small real tool that reads input, stores bytes/text,
parses something useful, and renders typed diagnostics using only public stdlib
APIs.

**ELI5:** Build one small house with all the bricks to prove they actually fit
together.

**Why it matters:** Library pieces can pass isolated tests while still forming
an awkward or incomplete application experience.

**What it unlocks:** a credible path toward self-hosting tools, richer CLI
applications, filesystem modules, formatting, package tooling, and eventually
the compiler itself.

**Done when:** the program contains no unnecessary unsafe/FFI code, needs no
private runtime contracts, has deterministic integration tests, and exposes
specific ergonomic gaps to guide the next roadmap revision.

Do not advance an ownership-heavy milestone merely because its happy path
works. Its positive behavior, important misuse cases, realm semantics, and
cleanup obligations must be executable and documented first.
