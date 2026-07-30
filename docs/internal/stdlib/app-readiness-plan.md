# Standard Library Application-Readiness Plan

Status: active implementation plan after the first Linux x86-64 boundary.

This document answers one practical question:

> What must exist before Runes can comfortably build normal applications?

It covers the missing compiler support, core library types, I/O, text,
containers, formatting, files, processes, networking, testing, and portability
work. The broader [ecosystem roadmap](roadmap.md) remains the long-term catalog;
this document defines the order in which to build the usable foundation.

The CLI portion is expanded into concrete contracts, failure matrices,
maintainer decision gates, and per-milestone exit criteria in the
[application foundation execution plan](application-foundation-execution-plan.md).
That execution plan controls implementation details; this document remains the
broader finish-line map through networking and graphical applications.

## Outcomes

Application readiness is split into three finish lines.

### Finish line A: command-line applications

A program can:

- read arguments and environment variables;
- print formatted text and diagnostics;
- read stdin and files;
- allocate growable collections and strings;
- parse user input;
- report errors without exposing Linux errno;
- close files and release dynamic memory explicitly;
- run without application-level `unsafe`.

### Finish line B: networked applications

A program can additionally:

- resolve addresses;
- create TCP and UDP sockets;
- connect, listen, accept, send, and receive;
- apply timeouts;
- process several connections using blocking or readiness-based I/O;
- use clocks and secure operating-system randomness.

TLS, HTTP, database clients, and async executors can then be ecosystem
libraries built over the standard socket and I/O contracts.

### Finish line C: graphical applications

A program can additionally:

- open a platform window or connect to a display server;
- receive keyboard, pointer, resize, and close events;
- create a pixel surface or graphics context;
- load assets from files;
- run a stable event loop with time and input;
- communicate with the future Runes OS graphical services through a different
  backend.

Window-system protocols, GPU APIs, font shaping, widgets, and full UI
frameworks do not all belong in `std`. The standard library must provide the
portable process, I/O, time, memory, file, synchronization, and dynamic-library
foundations those ecosystem packages require.

## Current foundation

The repository already has:

- `Option<T>` and initial methods in `std.core`;
- `Result<T, E>`, all milestone-1 methods, and portable foundational errors in
  `std.core`;
- allocation-free borrowed byte operations in `std.bytes`;
- `std.os.linux.syscall.call0` through `call6`;
- an x86-64 assembly syscall register bridge;
- all installed Linux x86-64 syscall numbers;
- errno-preserving `LinuxResult<T>` and `LinuxStatus`;
- Linux descriptor read, write, complete text write, close, seek, duplicate,
  and raw `openat`;
- Linux `mmap`, `munmap`, and `mprotect`;
- integration tests for output, errno, file input, mapping, and unmapping;
- a UAPI comparison test for the generated syscall table.

This is enough to begin higher-level Linux library work. It is not yet a
portable standard library.

## Dependency map

```text
target triples + cfg ---------> portable backend selection
pub const --------------------> shared OS constants

Result + portable errors -----> std.io ------------------+
borrowed text ----------------> parsing                  |
typed allocation ------------> Vec<T> -> String --------+-> formatting
                                                       |
Linux fd + memory --------------------------------------+-> files/process
                                                          -> networking

files + process + time + input -> real CLI application
network + readiness + time ----> server application
files + time + input + window backend -> graphical application
```

Compiler work and library work can proceed in parallel. Linux-only `std.io`
does not need to wait for a second OS backend, but its public signatures must
avoid Linux-specific types so portability does not require an API rewrite.

## Design rules

Every milestone follows these rules:

- raw pointers, syscalls, and foreign ABIs remain under `std.os`;
- portable modules do not expose Linux syscall numbers or errno as their error
  identity;
- borrowed input uses `str`, `[]const T`, or `[]T`;
- owning results state their allocation and cleanup policy;
- basic operations work without hidden allocation where possible;
- partial I/O is distinct from complete I/O;
- all capacity and byte-count arithmetic is checked;
- UTF-8 validity is an invariant of owning text;
- external resources are explicitly closed until move-only values and
  deterministic destruction exist;
- every accepted API has executable success, edge-case, and failure tests;
- freestanding support never acquires libc through a convenience wrapper.

## Parallel compiler track

Implementation status (2026-07-27): C1 through C6 are implemented and tested.
The standard namespace loads top-level children from actual references,
while explicit project `mod` declarations intentionally retain eager checking.

These compiler milestones unblock clean library APIs. They do not all need to
finish before Linux-first library development begins.

### Compiler C1: public constants

Status: implemented.

Implement:

```runes
pub const i32 STDOUT_FILENO = 1
```

Required behavior:

- public constants cross module boundaries;
- imported constants preserve their declared type;
- mutation remains rejected;
- duplicate names and private access produce normal diagnostics;
- constant values do not require runtime storage where the backend can embed
  them;
- ABI tables stop duplicating values beside every wrapper.

After this lands, promote the staged values in
`std.os.linux.constants` and the x86-64 syscall-number table.

### Compiler C2: explicit targets

Status: implemented for the three x86-64 bootstrap triples.

Add an explicit target model to the compiler, driver, and manifest:

```text
runec build --target x86_64-unknown-linux-gnu
```

The resolved target contains architecture, OS, environment, pointer width,
endianness, and hosted/freestanding profile. Host inference is only the default
when no target was requested. Cross-compilation must never silently use the
host ABI.

### Compiler C3: conditional compilation

Status: implemented before external-module loading and semantic analysis.

Implement `#[cfg(...)]` pruning before resolution and monomorphization:

```runes
#[cfg(target_os = "linux")]
use std.os.linux.fd as platform_fd
```

Initial predicates:

- `target_os`;
- `target_arch`;
- `target_env`;
- `hosted`;
- `freestanding`;
- `all`, `any`, and `not`.

This connects portable `std.io` to exactly one backend.

### Compiler C4: import-driven loading and emission

Status: implemented. Standard top-level children are materialized from actual
references, canonical source identity prevents duplicate loads, and
reachability eliminates unused functions and platform symbols. Explicit
project modules remain eager by language design.

The compiler previously loaded and emitted the complete exported
standard-library tree, forcing every program to link the Linux syscall bridge.
It now omits unreachable functions and foreign declarations, and a program
that does not use Linux I/O links without that bridge.

Implement:

- import-driven module loading;
- canonical module identity;
- reachability-based function/global emission;
- platform link inputs only when their module is reachable.

### Compiler C5: read-only slice FFI conversion

Status: implemented with `*const T`, `?*const T`, and const-safe `.ptr`.

`[]const u8` intentionally cannot expose a mutable pointer, but the Linux
`write` ABI needs a read-only address and length.

Add a narrow unsafe conversion or const-pointer type. Do not make `.ptr`
silently mutable. Then change Linux write operations from temporary mutable
slices to read-only slices.

### Compiler C6: resource and cleanup direction

Status: the interim `defer expression` direction is implemented. Cleanup is
lexical and LIFO across normal exits, return, error propagation, break, and
continue. Handles remain copyable; this is not RAII or a move-only type system.

Files, sockets, dynamic vectors, and strings need deterministic cleanup.
Before claiming RAII-like safety, choose and implement one of:

- move-only owning values plus destructors;
- explicit ownership with compiler-checked non-copyability;
- scoped cleanup/defer as an interim mechanism.

Until then, library resources use explicit `close` or `deinit`, remain copyable
handles, and document double-close/double-free hazards.

## Library milestone 1: core results and error vocabulary

Status: implemented and tested (2026-07-27).

Add a general value result:

```runes
pub type Result<T, E> =
    | Ok(T)
    | Err(E)
```

Initial methods:

- `is_ok`;
- `is_err`;
- `unwrap_or`;
- `map`;
- `map_error`;
- `and_then`.

Keep `!T` for nominal language errors. Use `Result<T, E>` when the error must
carry data such as an OS code, parse position, or nested cause.

Also define small reusable concepts rather than one universal error enum:

- `IoError`;
- `ParseError`;
- `AllocationError`;
- path and process errors as their modules require.

Acceptance:

- generic success and error payloads work for primitive and user-defined types;
- mapping does not allocate;
- matching is exhaustive;
- large payload copying is documented and measured;
- invalid constructor payloads fail at compile time.

The executable Result test covers all six methods, primitive payloads,
user-defined payloads, every foundational error family, and a measured 64-byte
aggregate passed through `unwrap_or`. The generated implementation performs no
allocation. A negative compiler test verifies constructor payload typing.

## Library milestone 2: minimal Linux-backed `std.io`

Status: implemented and tested for hosted Linux x86-64 (2026-07-28).

Export `std.io` with Linux-backed internals and portable signatures.

Initial surface:

```text
write(text)
write_line(text)
write_error(text)
write_error_line(text)
read(buffer)
```

Required behavior:

- stdout and stderr use complete writes;
- partial writes are completed;
- `EINTR` is retried where correct;
- zero-byte writes do not spin forever;
- stdin reads accept a caller-owned mutable slice;
- zero-byte input is represented as end of input, not an arbitrary failure;
- Linux errno maps into `IoError`;
- no public signature contains `LinuxResult` or a file descriptor;
- these primitives allocate nothing.

The first implementation may be explicitly documented as Linux x86-64 only.
Compiler milestones C2 and C3 later select the backend without changing this
surface.

Acceptance:

- a program prints and reads without application `unsafe`;
- invalid/closed descriptors are tested through backend tests;
- broken-pipe behavior is tested in a child process;
- deterministic fake-backend tests cover partial writes and interruption;
- output compiles with `-Werror`.

The implementation returns `Result<usize, IoError>` from all five operations.
Integration tests cover stdout, stderr, piped stdin, EOF, an invalid and a
closed descriptor, and a real closed pipe handled by the hosted runtime's
process-wide `SIGPIPE` policy. The pipe fixture does not suppress the signal
itself. A syscall fake deterministically forces interruption,
partial writes, zero-byte writes, newline-stage failure, every current errno
mapping, and `EPIPE`. Adversarial tests force impossible oversized backend
counts and verify that both read and write reject them. Empty buffers, empty
text, embedded NUL bytes, multibyte UTF-8, and an 8 KiB kernel read are also
covered. Freestanding targets reject unavailable calls during compilation.

## Library milestone 3: borrowed text

Status: implemented and tested (2026-07-29). The public contract is in the
[borrowed text reference](../../reference/text.md).

Extend borrowed `str` operations before building ownership.

Initial operations:

- `is_empty`;
- byte length;
- UTF-8 validation;
- scalar count;
- `starts_with` and `ends_with`;
- `contains` and `find`;
- trim views;
- split views;
- scalar iteration;
- boundary-checked slicing.

Rules:

- distinguish byte indexes from Unicode scalar indexes;
- never claim grapheme-cluster behavior without Unicode grapheme data;
- slicing must reject non-scalar boundaries;
- borrowed substrings allocate nothing and cannot outlive their source;
- byte-oriented algorithms stay under `std.bytes`.

Acceptance:

- [x] ASCII, multibyte UTF-8, empty strings, invalid external bytes, and
  boundary failures are tested;
- [x] operations do not copy the complete source;
- [x] iteration cannot walk beyond the declared string length.

## Library milestone 4: typed allocation

Status: implemented and tested (2026-07-29), including paired ordinary and
recoverable `t` operations, initialized `allocate<T>(value)`, GC
initialized-prefix publication, pointer-bearing in-place growth and shrink,
owner validation, and release semantics.

Provide allocation operations that derive size and alignment from `T`:

```text
allocate<T>(value)
tallocate<T>(value)
allocate_array<T>(count)
tallocate_array<T>(count)
publish_initialized<T>(pointer, expected_old, new_initialized, capacity)
tpublish_initialized<T>(pointer, expected_old, new_initialized, capacity)
resize_array<T>(pointer, initialized, old_capacity, new_capacity)
tresize_array<T>(pointer, initialized, old_capacity, new_capacity)
release_array<T>(pointer)
```

Required behavior:

- checked multiplication and capacity growth;
- correct alignment;
- failure is represented, not converted into an unexplained null pointer;
- ordinary names terminate through the portable storage-failure policy, while
  the `t` form returns the represented failure;
- pointer-bearing GC values receive correct metadata;
- regional allocations are never individually freed;
- dynamic allocations have an explicit release path;
- allocation failure injection is available to tests.

Do not spread `alloc(sizeof(T)) as *T` throughout every container.

The implementation includes deterministic failure injection and backend
counters, preserves dynamic storage on failed growth, rejects an unavailable
regional owner, and passes explicit initialized length to GC sequence
metadata. Publication validates the GC allocation base, descriptor, capacity,
and previous length. The internal proof buffer exercises pointer-bearing
in-place push, pop, truncate, clear, resize, release, forced collection, and
owner preservation through ordinary functions. The compiler/runtime gate for
milestone 5 is complete.

## Library milestone 5: `Vec<T>`

Status: implemented and tested for dynamic, regional, and GC owners
(2026-07-29).

The implementation uses one shared algorithm and an inferred hidden owner
realm. The GC type-family member has the same public layout as the fallback and
exists to preserve persistent owner identity through receiver dispatch.
Callers never pass an allocator or write a realm argument.

Minimum API:

- `new`;
- `tnew`;
- `with_capacity`;
- `twith_capacity`;
- `len`;
- `capacity`;
- `is_empty`;
- `reserve`;
- `treserve`;
- `push`;
- `tpush`;
- `pop`;
- `tpop`;
- `get`;
- checked mutation;
- `as_slice`;
- `as_mut_slice`;
- `clear`;
- `tclear`;
- `truncate`;
- `ttruncate`;
- `deinit`.

Required behavior:

- capacity growth is checked;
- failed growth leaves the old vector valid;
- element moves/copies follow the language value model;
- zero-sized element behavior is explicitly decided;
- no operation frees arena or GC memory through the dynamic free path;
- storing references obeys realm escape checks.

Acceptance:

- [x] empty, one-element, repeated-growth, maximum-capacity, and
  allocation-failure cases;
- [x] pointer-bearing elements under sanitizers and forced GC tests;
- [x] nested regional-owner rejection with state preservation;
- [x] no leaks or double frees in normal explicit-deinit use;
- [x] clear documentation that copying an owning vector is unsafe until C6.

## Library milestone 6: owning UTF-8 `String`

Build `String` over owned growable bytes rather than a second independent
capacity implementation.

Minimum API:

- `new`;
- `tnew`;
- `with_capacity`;
- `twith_capacity`;
- validated `from_str`;
- validated/lossy construction from bytes;
- `push` Unicode scalar;
- `tpush` Unicode scalar;
- `push_str`;
- `tpush_str`;
- `reserve`;
- `treserve`;
- `truncate` at a scalar boundary;
- `clear`;
- `as_str`;
- byte length and capacity;
- `deinit`.

Invariant:

> Every live `String` contains valid UTF-8.

Mutable raw bytes stay private or unsafe. Appending and truncating must preserve
the invariant.

Acceptance:

- [x] ASCII and multibyte growth;
- [x] invalid UTF-8 rejection;
- [x] embedded NUL handling;
- [x] boundary-safe truncation;
- [x] allocation failure leaves the original string valid;
- [x] dynamic cleanup passes sanitizers.

## Library milestone 7: formatting and parsing

Runes has no variadics or macros, so begin with explicit builders and writer
interfaces rather than pretending format strings are compile-time checked.

Formatting foundation:

- signed/unsigned integers;
- decimal and hexadecimal bases;
- booleans and characters;
- strings and escaped debug strings;
- width, padding, alignment, and sign;
- formatting into a fixed buffer, `String`, or writer;
- deterministic floating-point formatting after integer formatting is stable.

Parsing foundation:

- signed/unsigned integers with explicit base;
- overflow diagnostics;
- booleans;
- Unicode scalars;
- decimal floats;
- consumed length and error position.

Later compiler work may add checked format strings. The initial API must remain
usable without macros.

Acceptance:

- minimum/maximum integers;
- invalid digits and overflow;
- insufficient fixed-buffer capacity;
- formatting performs no allocation when given a fixed output buffer;
- formatted output can target stdout, files, and strings through one writer
  contract.

## Library milestone 8: buffered I/O

Define small common interfaces or concrete adapters:

- reader;
- writer;
- seeker;
- flusher;
- closer.

Add:

- buffered reader and writer;
- `read_exact`;
- `write_all`;
- byte and line reads with explicit maximum lengths;
- in-memory slice reader;
- fixed-buffer writer;
- EOF distinct from failure;
- flush behavior.

Do not add `input(prompt)` until output flushing, bounded line input, UTF-8
validation, and owning `String` are working.

Acceptance:

- short reads/writes;
- interruption;
- EOF before and after data;
- line longer than the configured maximum;
- invalid UTF-8;
- flush failure;
- no one-syscall-per-byte line reading.

## Library milestone 9: paths and files

Add lexical `Path`/`PathView` operations separately from filesystem calls.

Path foundation:

- component iteration;
- join;
- parent and filename;
- absolute/relative checks;
- normalization that does not silently resolve symlinks;
- explicit conversion to NUL-terminated platform paths.

File foundation:

- open and create options;
- read, write, seek, flush, and close;
- metadata;
- directory iteration;
- remove, rename, and create-directory operations;
- bounded `read_to_end` and `read_to_string`.

Until compiler milestone C6, `File` uses explicit close and documents that
copying the handle does not duplicate kernel ownership.

Acceptance:

- nonexistent and permission-denied paths;
- embedded NUL rejection;
- partial reads and writes;
- large file offsets;
- directory entry errors;
- close failure policy;
- no path traversal assumptions hidden in lexical normalization.

## Library milestone 10: process, environment, time, and randomness

Applications need:

- command-line arguments as borrowed startup data;
- environment lookup and iteration;
- exit status;
- process spawning and waiting;
- pipes and standard-stream redirection;
- monotonic and wall clocks;
- sleeping with interruption policy;
- durations and instants;
- secure bytes from the OS randomness facility.

Do not use wall time for deadlines. Do not implement secure randomness with a
user-space pseudo-random generator seeded from timestamps.

Acceptance:

- empty/non-UTF-8 argument and environment policy is explicit;
- child success, failure, signal termination, and missing executable;
- pipe cleanup on every failure path;
- monotonic deadlines;
- random byte requests handle partial/interrupted kernel results.

Completing milestones 1 through 10 establishes finish line A.

## Library milestone 11: networking

Start with low-level, blocking sockets:

- socket address types;
- IPv4 and IPv6 parsing/formatting;
- TCP connect/listen/accept;
- UDP send/receive;
- shutdown and close;
- socket options;
- timeouts;
- DNS/address resolution through an explicit hosted backend.

Then add Linux readiness:

- nonblocking mode;
- `poll` or `epoll`;
- event registration and removal;
- wakeup mechanism;
- cancellation contract.

Async/await is not required for the first networked applications. A
readiness-based event loop can be a library. Language-level async work can
later improve syntax and state-machine generation.

Acceptance:

- loopback TCP/UDP integration;
- connection refusal and timeout;
- IPv4/IPv6;
- partial send/receive;
- peer shutdown;
- descriptor reuse hazards;
- readiness edge cases and cancellation.

Completing milestone 11 establishes finish line B.

## Library milestone 12: synchronization and concurrency foundation

Before general multithreaded applications:

- define OS thread creation/join;
- mutex, condition variable, semaphore, and once;
- atomics and memory-order vocabulary;
- thread-local storage;
- channels or queues as library types;
- document the GC restriction that references cannot currently cross OS
  threads.

Compiler/runtime work is required before GC-backed values can safely move
between threads. Dynamic/raw data can be explored first with explicit
ownership.

## Library milestone 13: graphical-application foundations

Keep the standard library boundary small:

- dynamic library loading where hosted platforms require it;
- file, time, input, threading, and networking foundations;
- a portable event-loop contract only if multiple backends prove it useful.

Linux ecosystem packages can then provide:

- Wayland client protocol;
- X11 fallback if desired;
- DRM/KMS for direct display access;
- evdev/libinput input;
- OpenGL/Vulkan bindings;
- font loading, shaping, and rasterization;
- image decoding;
- window and UI abstractions.

The future Runes OS supplies its own window/compositor and input service
backend. `std.io`, files, clocks, and synchronization remain portable; Linux
display protocols do not.

Completing a window, event input, a drawable surface, and an event loop
establishes finish line C.

## Cross-cutting testing strategy

Every module receives:

1. Positive compile/run tests.
2. Expected compiler failures for unsafe or invalid use.
3. Boundary tests: empty, minimum, maximum, overflow, and invalid encoding.
4. Failure injection for allocation and I/O.
5. Linux integration tests where a real kernel contract matters.
6. Fake-backend tests for partial, interrupted, and deterministic behavior.
7. Generated C compilation under `-Werror`.
8. Address/undefined-behavior sanitizer coverage for owning memory.
9. UAPI/ABI comparison tests for generated platform tables.
10. At least one end-to-end application at each finish line.

Tests must not rely solely on the same implementation they are checking. For
example, syscall constants are compared to Linux UAPI headers, and formatting
vectors should include independently calculated expected strings.

## Ordered implementation count

The recommended working order is:

1. `Result<T, E>` and portable error vocabulary.
2. Minimal Linux-backed `std.io`. (done)
3. Borrowed text utilities.
4. Typed allocation.
5. Realm-aware `Vec<T>`. (done)
6. Owning UTF-8 `String`. (done)
7. Formatting and parsing.
8. Buffered I/O.
9. Paths and files.
10. Arguments, environment, processes, time, and randomness.
11. Networking and readiness.
12. Synchronization and concurrency.
13. Graphical-application foundations.

The compiler foundation now includes public constants, explicit targets,
conditional compilation, import-driven standard-module loading,
reachability-based emission, read-only FFI pointers, and scoped deterministic
cleanup. Move-only resource ownership remains later compiler work rather than
a blocker for the next library sprint.

## Immediate next sprint

Milestones 1 through 6 are complete. The next focused sprint is milestone 7:
formatting into fixed buffers, owning `String`, and later writer abstractions.
The design questions, failure matrix, and exit gate are in the
[application foundation execution plan](application-foundation-execution-plan.md#11-m3-formatting).

## Definition of “functional for apps”

The stdlib is functionally ready for ordinary CLI applications when:

- an application imports only portable modules;
- no application-level unsafe is needed for normal I/O, files, or allocation;
- errors preserve useful context without exposing platform-only identities;
- vectors and strings have tested cleanup and allocation-failure behavior;
- formatting and parsing cover normal command-line data;
- files, arguments, environment, time, and randomness work;
- the same portable I/O tests run against Linux and a fake backend;
- the complete suite passes with sanitizers and `-Werror`;
- one nontrivial CLI is maintained as a regression application.

It is ready for portable applications only after explicit targets and
conditional backend selection are implemented. Linux-first usefulness and
cross-platform correctness are separate milestones and should be described
honestly.
