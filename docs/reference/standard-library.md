# Standard Library

This page documents the standard-library source that exists and is tested on
the current `v0-1-exp` branch. It is an implementation reference, not a list of
planned APIs.

Runes' standard library is still a bootstrap library. It currently provides
`Option<T>`, `Result<T, E>`, portable foundational error types,
allocation-free byte-slice operations, and an initial raw Linux x86-64 syscall
boundary. It also provides allocation-free safe terminal I/O with portable
signatures on hosted Linux x86-64 plus typed realm-sensitive backing-storage
operations, allocation-free borrowed UTF-8 text utilities, a realm-aware
growable `Vec<T>`, owning realm-aware UTF-8 `String`, typed formatting, and
allocation-free typed integer, boolean, and Unicode-scalar parsing, plus
composable static readers/writers, explicit buffering, exact I/O, and bounded
line input, plus byte-preserving lexical path manipulation and explicit
NUL-terminated syscall conversion, plus safe owning files and initial
filesystem operations. It does not yet provide networking or concurrency.

## Library layout

| Module or file | Current status |
|---|---|
| `std.core` | Implemented: `Option<T>`, `Result<T, E>`, their methods, and foundational errors |
| `std.bytes` | Implemented: byte fill, copy, equality, search, and prefix checking |
| `std.text` | Implemented: allocation-free borrowed UTF-8 observation, search, checked views, explicit ASCII trimming, split-once, and scalar traversal |
| `std.allocation` | Implemented: paired ordinary/recoverable `t` typed allocation, initialized-prefix publication, owner-sensitive resize, and realm-correct release |
| `std.vec` | Implemented: realm-aware owning `Vec<T>` with concise ordinary and recoverable `t` operations, checked growth, slices, mutation, shrink, and explicit deinitialization |
| `std.string` | Implemented: realm-aware owning valid UTF-8 `String` over `Vec<u8>`, validated construction, transactional append, checked truncation, views, and explicit deinitialization |
| `std.format` | Implemented initial gate: typed integer, bool, char, str, and escaped/debug formatting into fixed buffers, owning `String`, or static `Writer` implementations |
| `std.parse` | Implemented initial gate: strict allocation-free parsing for every integer width, bool, and exactly one Unicode scalar |
| `std.os.linux.syscall` | Raw Linux x86-64 syscall calls with zero through six arguments |
| `std.os.linux.result` | Errno-preserving `LinuxResult<T>` and `LinuxStatus` |
| `std.os.linux.fd` | Raw descriptor read, write, close, seek, duplication, and `openat` operations |
| `std.os.linux.memory` | Raw `mmap`, `munmap`, and `mprotect` operations |
| `std.os.linux.constants` | Private staged Linux ABI constants; ready to migrate to implemented `pub const` |
| `std.io` | Implemented M5 gate: static `Reader`/`Writer` capabilities, exact operations, memory/String/standard-stream adapters, borrowed buffering, bounded byte/text lines, flush and seek contracts; hosted Linux x86-64 backend |
| `std.path` | Implemented M6 gate: borrowed and realm-owning arbitrary-byte paths, components, filename/parent views, explicit lexical normalization/join, and checked NUL-terminated syscall conversion |
| `std.fs` | Implemented M7 gate on hosted Linux x86-64: owning `File`, open policy, stream interfaces, seek/sync/truncate/metadata, bounded whole-file reads, and basic path mutation |
| `std.os.linux.filesystem` | Raw Linux x86-64 stat, sync, truncate, mkdir, unlink, and rename backend used by `std.fs` |
| `std/prelude.runes` | Compiler/runtime ABI declarations, not an application library |

The root [`src/std/mod.runes`](../../src/std/mod.runes) currently exports
`core`, `bytes`, `text`, `allocation`, `vec`, `string`, `format`, `parse`,
`io`, `path`, hosted-target `fs`, and `os`.

## Imports

Ordinary public byte functions can be imported individually:

```runes
use std.bytes.copy
use std.bytes.find
```

Generic types can also be imported directly or assigned a local alias:

```runes
use std.core.Option
use std.core.Option as Maybe

Option<i32> value = Option.Some(42)
Maybe<i32> empty = Maybe.None<i32>()
```

The alias is only a local name. `Option<i32>` and `Maybe<i32>` have the same
type and share one generic specialization.

## `std.core`

### `Option<T>`

```runes
pub type Option<T> =
    | None
    | Some(T)
```

`None` represents ordinary absence. `Some(T)` stores one value of `T`.
`Option<T>` performs no allocation.

Constructors may need explicit type arguments when context cannot determine
the payload type:

```runes
use std.core.Option

Option<i32> present = Option.Some(21)
Option<i32> absent = Option.None<i32>()
```

### Option methods

| Method | Result | Behavior |
|---|---|---|
| `is_some(self)` | `bool` | `true` for `Some`, otherwise `false` |
| `is_none(self)` | `bool` | `true` for `None`, otherwise `false` |
| `unwrap_or(self, fallback: T)` | `T` | Stored value or the supplied fallback |
| `unwrap(self)` | `!T` | Stored value or `error.UnwrapError.NoValueError` |
| `map<U>(self, transform: f(T) -> U)` | `Option<U>` | Transforms `Some`; preserves `None` |

Generic method type arguments are currently written explicitly:

```runes
Option<i32> doubled = present.map<i32>(double)
```

The current methods use value receivers and value patterns. For large payloads,
that can copy `T`. A safe reference-returning option API has not been designed
or implemented.

### `UnwrapError`

```runes
pub error UnwrapError = {
    | NoValueError
}
```

Handle `unwrap` with normal fallible control flow:

```runes
i32 value = absent.unwrap() catch -1
```

Use `unwrap_or` when absence is expected and a fallback is already available.

### `Result<T, E>`

```runes
pub type Result<T, E> =
    | Ok(T)
    | Err(E)
```

`Ok(T)` stores a successful value and `Err(E)` stores a failure value.
`Result<T, E>` is appropriate when failure needs data, such as a parse
position. Continue using the language's `!T` nominal-error form when an error
identity without a payload is sufficient.

```runes
use std.core.Result

Result<i32, ParseError> parsed =
    Result.Ok<i32, ParseError>(42)
```

### Result methods

| Method | Result | Behavior |
|---|---|---|
| `is_ok(self)` | `bool` | `true` for `Ok`, otherwise `false` |
| `is_err(self)` | `bool` | `true` for `Err`, otherwise `false` |
| `unwrap_or(self, fallback: T)` | `T` | Successful value or the supplied fallback |
| `map<U>(self, transform: f(T) -> U)` | `Result<U, E>` | Transforms `Ok`; preserves `Err` |
| `map_error<F>(self, transform: f(E) -> F)` | `Result<T, F>` | Transforms `Err`; preserves `Ok` |
| `and_then<U>(self, transform: f(T) -> Result<U, E>)` | `Result<U, E>` | Chains a fallible transformation |

Generic method type arguments are currently explicit:

```runes
Result<i64, ParseError> doubled = parsed.map<i64>(double)
```

Constructing, matching, and mapping a `Result` do not allocate. The current
language value model passes Result receivers and variant payloads by value, so
large `T` or `E` values can be copied. Executable coverage measures a
64-byte user-defined payload and verifies that it survives `unwrap_or`.
Prefer a small handle or pointer payload when copying a large value is not
acceptable; this is not yet a borrow or reference-returning API.

### Portable foundational errors

`std.core` currently exports three small variant types:

```runes
pub type IoError =
    | EndOfInput
    | Interrupted
    | PermissionDenied
    | NotFound
    | BrokenPipe
    | InvalidInput
    | WouldBlock
    | Unsupported
    | Unknown

pub type ParseError =
    | InvalidSyntax(usize)
    | UnexpectedEnd(usize)
    | OutOfRange(usize)

pub type AllocationError =
    | OutOfMemory
    | CapacityOverflow
    | OwnerUnavailable
```

They are portable identities for higher-level library APIs. Platform backends
must translate native values such as Linux errno rather than expose those
values as the public error type. These types are foundations; modules may add
more specific errors when their contracts require them.

`AllocationError.code()` maps the portable variants to the runtime storage
failure codes. `AllocationError.fail()` invokes the non-returning portable
storage-failure policy and is used by ordinary allocation/container wrappers.

## `std.allocation`

`std.allocation` exports paired typed, realm-sensitive backing-storage
operations. The ordinary spelling terminates through the runtime storage
failure policy; prefixing the operation with `t` requests an explicit
`Result`:

```runes
allocate<T>(value: T) -> *T
tallocate<T>(value: T) -> Result<*T, AllocationError>

allocate_array<T>(count) -> *T
tallocate_array<T>(count) -> Result<*T, AllocationError>

publish_initialized<T>(
    pointer: *T,
    expected_old: usize,
    new_initialized: usize,
    capacity: usize
) -> usize
tpublish_initialized<T>(...) -> Result<usize, AllocationError>

resize_array<T>(
    pointer: *T,
    initialized: usize,
    old_capacity: usize,
    new_capacity: usize
) -> *T
tresize_array<T>(...) -> Result<*T, AllocationError>

release_array<T>(pointer: *T)
```

The `t` prefix means “recoverable form of this otherwise terminating
operation”; it is not a separate algorithm. For example, `allocate` delegates
to `tallocate`, and `Vec.push` delegates to `Vec.tpush`. `AllocationError.fail`
routes an error into the portable runtime failure policy.

`allocate<T>(value)` returns one initialized value. `allocate_array` and
`publish_initialized` are for container implementations: fresh capacity begins
with length zero, and every in-place length change must be published before a
GC safepoint.

The compiler supplies size, alignment, and GC tracing descriptors from `T`.
Resize preserves the original allocation on failure. Dynamic release frees
storage; regional release relinquishes its arena handle; GC release clears the
traceable initialized prefix. A regional owner that is not the directly active
arena produces `OwnerUnavailable`.

These functions are the container-authoring layer. Raw `alloc()` and casts
remain available for systems code, but ordinary vectors and strings should not
repeat that unsafe machinery.

## `std.vec`

`Vec<T>` is the first public owning collection. It uses the same source-level
API in dynamic, regional, and GC functions; the compiler infers its hidden
owner realm and emits no runtime realm branch.

This section is the module overview. The dedicated
[`Vec<T>` reference](vec.md) is authoritative for every method, representation
invariant, complexity, ownership rule, view-invalidation rule, realm behavior,
and failure guarantee.

```runes
use std.vec.Vec

dynamic f example() {
    values := Vec<i32>.new()
    values.push(10)
    values.push(20)

    match values.get(1) {
        Some(value) -> print(value),
        None -> print("missing"),
    }

    values.deinit()
}
```

Constructors have ordinary and explicitly recoverable associated forms:

```runes
Vec<T>.new() -> Vec<T>
Vec<T>.tnew() -> Result<Vec<T>, AllocationError>

Vec<T>.with_capacity(capacity: usize) -> Vec<T>
Vec<T>.twith_capacity(capacity: usize)
    -> Result<Vec<T>, AllocationError>
```

Both constructors allocate at least one element of backing capacity, including
`with_capacity(0)`. The ordinary forms terminate through the portable storage
failure policy if construction fails. The `t` forms return the error. Every
successfully constructed vector has a valid pointer for empty slices.

The original module functions `std.vec.new<T>()` and
`std.vec.with_capacity<T>()` remain compatibility wrappers during the
constructor migration and now follow ordinary terminating semantics.
`std.vec.tnew<T>()` and `std.vec.twith_capacity<T>()` provide the recoverable
module-level counterparts. New code should use the associated spelling.

The current methods are:

```runes
values.len() -> usize
values.capacity() -> usize
values.is_empty() -> bool

values.reserve(additional) -> usize
values.treserve(additional) -> Result<usize, AllocationError>
values.push(value) -> usize
values.tpush(value) -> Result<usize, AllocationError>
values.pop() -> Option<T>
values.tpop() -> Result<Option<T>, AllocationError>

values.get(index) -> Option<T>
values.set(index, value) -> bool
values.as_slice() -> []const T
values.as_mut_slice() -> []T

values.truncate(new_length) -> usize
values.ttruncate(new_length) -> Result<usize, AllocationError>
values.clear() -> usize
values.tclear() -> Result<usize, AllocationError>
values.deinit()
```

`reserve` takes additional capacity relative to the current length. Growth is
geometric and checks both `usize` arithmetic and the final `T` byte layout.
The `t` operations return `AllocationError`. Failed allocation preserves the
old vector; more generally, existing elements and logical length remain valid.
`tpush` can commit successful capacity growth before a later initialized-prefix
publication failure, so it does not promise that capacity and backing address
are unchanged. Ordinary counterparts use the same implementation but terminate
instead of returning the error. `get` and `pop` currently copy their values
because Runes has no safe borrowed-reference return model.

`truncate` treats a requested length at or above the current length as a
no-op. `set` returns `false` for an out-of-bounds index. Slices borrow the
vector's current backing storage and must not be retained across growth,
any mutation or `deinit`. Runes does not enforce this borrowing rule yet.

Realm cleanup is automatic to select but still explicit to invoke:

| Owner realm | Growth and `deinit` behavior |
|---|---|
| dynamic | Replacement frees the old allocation; `deinit` frees current backing storage |
| regional | Replacement stays in the same directly active arena; `deinit` relinquishes the handle |
| GC | Replacement is traced; shrink and `deinit` stop tracing removed elements |
| stack | Construction and owning growth are compile-time errors |

Calling a regional growth operation from a nested child arena is rejected with
`OwnerUnavailable`; ownership is never silently transferred to the child.
Pointer-bearing GC vectors publish every initialized-length change, so pushed
referents survive collection and cleared or popped referents become
collectible.

Current ownership limitations are important:

- `Vec<T>` is logically move-only, but the compiler does not enforce moves
  yet. Copying it duplicates an ownership handle and can cause double release
  in dynamic code.
- Call `deinit` exactly once per ownership lineage. Repeating it on the same
  value is harmless because the handle is cleared, but a copied value remains
  a separate stale handle.
- `clear`, `truncate`, and `deinit` do not run user-defined cleanup for files,
  sockets, locks, or other external resources stored in elements.
- Zero-sized element types are not currently supported by typed allocation.
- Do not use a vector after `deinit`, except to call `deinit` again.
- Fields are visible because Runes does not yet support private fields; treat
  `data`, `length`, and `reserved` as implementation details.

Values introduced by constructor matches retain their inferred owner realm.
Consequently one generic helper can accept `*Vec<T>` and is specialized
separately when called with dynamic, regional, and GC vectors; no realm
argument or concrete-realm wrapper is required.

## `std.string`

`String` is distinct owning, growable UTF-8 text built over `Vec<u8>`. Its
permanent invariant is that every initialized byte belongs to valid UTF-8.
The compiler infers dynamic, regional, or GC ownership; stack construction is
rejected.

```runes
use std.string.String

dynamic f example() {
    value := String.from_str("hello")
    value.push(' ')
    value.push_str("世界")
    print(value.as_str())
    value.deinit()
}
```

Construction and pure storage operations use ordinary/recoverable pairs:

```runes
String.new()
String.tnew()
String.with_capacity(capacity)
String.twith_capacity(capacity)
String.from_str(value)
String.tfrom_str(value)

value.reserve(additional)
value.treserve(additional)
value.push(scalar)
value.tpush(scalar)
value.push_str(text)
value.tpush_str(text)
value.clear()
value.tclear()
```

`String.from_bytes([]const u8) -> Result<String, StringError>` validates
arbitrary bytes and rejects malformed UTF-8. The initial API has no lossy
conversion.

Observation uses `byte_len`, `capacity`, `is_empty`, `as_str`, and
`as_bytes`. Both views borrow current storage and must end before mutation or
`deinit`. `ttruncate` returns `InvalidBoundary(index)` if a byte offset splits
a scalar; ordinary `truncate` terminates through the checked boundary
diagnostic. Append copies bytes and publishes the complete new initialized
prefix once, so a failed append cannot expose a partial scalar.

Fields remain visible only because field privacy is incomplete. Treat the
underlying vector as private, avoid copying ownership handles, and call
`deinit` exactly once per ownership lineage.

The complete [`String` reference](string.md) is authoritative for signatures,
UTF-8 invariants, failure guarantees, views, realm behavior, and current
ownership limits.

## `std.format`

`std.format` implements locale-independent formatting for integers, booleans,
Unicode scalars, borrowed text, and escaped/debug text. `IntegerCursor` and
`TextCursor` feed fixed byte buffers, owning `String`, and generic
`Writer` implementations. Writer dispatch is monomorphized and introduces no
runtime vtable. See the [formatting reference](format.md) for exact options,
escaping, errors, and mutation guarantees.

## `std.text`

`std.text` is the allocation-free borrowed UTF-8 layer. It provides:

```runes
is_empty(value) -> bool
byte_len(value) -> usize
is_valid_utf8(value) -> bool
is_scalar_boundary(value, byte_index) -> bool
scalar_count(value) -> usize

starts_with(value, prefix) -> bool
ends_with(value, suffix) -> bool
contains(value, needle) -> bool
find(value, needle) -> Option<usize>

substring(value, start, end) -> Option<str>
trim_ascii_start(value) -> str
trim_ascii_end(value) -> str
trim_ascii(value) -> str
split_once(value, delimiter) -> Option<TextSplit>

ScalarCursor.new(value) -> ScalarCursor
scalars(value) -> ScalarCursor
```

Every position is a byte offset. `substring` validates UTF-8 scalar boundaries,
and all returned strings borrow their source. ASCII trimming is explicitly
named and does not remove non-ASCII whitespace. `ScalarCursor` traverses
Unicode scalar values without allocation; it does not claim grapheme
segmentation.

See the complete [borrowed text reference](text.md) for exact empty-input
behavior, complexity, view lifetime, embedded-NUL handling, and cursor rules.

## `std.parse`

`std.parse` strictly consumes borrowed text into every signed/unsigned integer
width, `usize`, `bool`, or one Unicode scalar. Decimal is the concise default;
`IntegerParse` selects binary, octal, hexadecimal, or automatic `0b`/`0o`/`0x`
prefix detection. Parsing allocates nothing and reports invalid syntax,
unexpected end, or overflow at a UTF-8 byte offset.

See the complete [parsing reference](parse.md) for signatures and grammar.

## `std.bytes`

All current byte operations are ordinary stack functions. They allocate
nothing and accept borrowed slices.

### `fill`

```runes
fill(buffer: []u8, value: u8)
```

Writes `value` into every element of `buffer`. An empty buffer is accepted.

### `copy`

```runes
copy(destination: []u8, source: []const u8) -> !void
```

Copies every source byte into the beginning of `destination`. If the
destination is shorter than the source, it returns
`error.CopyError.DestinationTooSmall` before modifying the destination.

The current implementation is a forward element-by-element copy. It does not
promise `memmove` behavior for partially overlapping slices.

### `equal`

```runes
equal(left: []const u8, right: []const u8) -> bool
```

Returns `true` only when lengths and every corresponding byte are equal. Two
empty slices are equal.

### `find`

```runes
find(buffer: []const u8, byte: u8) -> Option<usize>
```

Returns the index of the first matching byte. It returns `None` when the byte
is absent or the buffer is empty.

### `starts_with`

```runes
starts_with(buffer: []const u8, prefix: []const u8) -> bool
```

Returns `true` when every prefix byte matches the beginning of `buffer`.
An empty prefix always matches. A prefix longer than the buffer returns
`false`.

### `CopyError`

```runes
pub error CopyError = {
    | DestinationTooSmall
}
```

`copy` is all-or-error: it never performs a partial copy solely because the
destination is too small.

## `std.io`

`std.io` is the safe, portable-signature layer over the selected platform
backend. The first backend is available for
`x86_64-unknown-linux-gnu`. The module is exported on every target, but its
operations are absent on targets without an implementation, producing a
compile-time “module has no member” diagnostic instead of a runtime failure.

```runes
use std.io
use std.core.IoError
use std.core.Result

Result<usize, IoError> output = io.write_line("hello")

[256]u8 buffer = []
Result<usize, IoError> input = io.read(buffer)
```

Every current operation is allocation-free and returns
`Result<usize, IoError>`:

| Function | Behavior |
|---|---|
| `write(text)` | Writes every byte of `text` to standard output |
| `write_line(text)` | Writes `text`, then one newline byte, to standard output |
| `write_error(text)` | Writes every byte of `text` to standard error |
| `write_error_line(text)` | Writes `text`, then one newline byte, to standard error |
| `read(buffer)` | Reads once from standard input into caller-owned mutable storage |

The module also defines the statically dispatched interfaces `Reader`,
`Writer`, `Flusher`, `Seeker`, and `Closer`. `SliceReader`,
`Stdin`/`Stdout`/`Stderr`, `BufferedReader<R>`, and `BufferedWriter<W>` provide
the initial composable adapters. `read_exact_from` reports partial EOF with an
exact completed-byte count; bounded byte-line input distinguishes EOF before
data from EOF after a partial line; text-line input additionally validates
UTF-8. Caller-provided staging capacity is explicit and allocation-free.

The output side retains
`Writer.write(self: *Writer, bytes) -> Result<usize, WriteError>`.
`FixedBufferWriter` permits partial writes into caller storage, while
`StringWriter` appends complete valid UTF-8 chunks and retains the String's
hidden owner realm. `write_all_to<W: Writer>` completes partial writes,
retries `Io(Interrupted)`, returns `NoProgress` for `Ok(0)`, and rejects a
count larger than the remaining slice as `InvalidCount(count)`.

A successful result is the byte count. Writes retry interruption, complete
partial writes, and convert a zero-byte write into an error so they cannot spin
forever. A backend-reported count larger than the outstanding request is
rejected as `Unknown` instead of being trusted. Line writes are complete but
not atomic: the text may have been written if the separate newline write
fails.

`read` retries interruption. A nonempty buffer receiving a zero-byte platform
read returns `Err(IoError.EndOfInput)`. An empty buffer returns `Ok(0)` without
calling the backend, because a zero-capacity request cannot determine whether
the input has ended.

The public signatures expose no descriptor, Linux errno, raw pointer, or
`unsafe` requirement. Known Linux failures map to semantic `IoError` variants;
unrecognized errno values become `Unknown`. `WouldBlock` may be returned for a
nonblocking standard descriptor, although this initial layer does not itself
configure nonblocking mode.

See the dedicated [I/O reference](io.md) for partial-progress rules, buffering,
line framing, error behavior, realm semantics, and the current file/socket
boundary.

Hosted Linux programs establish an ignore policy for `SIGPIPE` at runtime
startup. Writing to a pipe whose readers are gone therefore returns
`Err(IoError.BrokenPipe)` instead of unexpectedly terminating the process.
Applications do not need to install signal handlers around `std.io` calls.
Command-line applications may normally treat `BrokenPipe` as a clean early
exit when a downstream pipeline command, such as `head`, has finished reading.
Freestanding targets do not install this policy because they have no Linux
process signals.

## `std.path`

`std.path` is portable lexical infrastructure and performs no syscalls.
`PathView` borrows arbitrary bytes, `Path` owns a realm-aware `Vec<u8>`, and
`PathComponents` distinguishes root, `.`, `..`, and ordinary components.
`Path.normalize` and `Path.join` explicitly apply the documented lexical rules;
they do not resolve symbolic links or filesystem identity.

`PlatformPath.tfrom_view` preserves exact source bytes, rejects an embedded NUL
with its byte index, and appends one trailing NUL for FFI/syscall use. The
complete [path reference](path.md) defines normalization, ownership, failure,
and safety behavior.

## `std.fs`

`std.fs` is the safe application filesystem layer. It currently uses hosted
Linux x86-64, but exposes portable errors and `PathView` rather than raw
descriptors, C strings, flags, or errno.

It provides owning `File`, explicit `OpenOptions`, static stream interfaces,
seek/sync/truncate/metadata operations, bounded realm-aware whole-file reads,
and basic directory/file create, remove, and rename operations. Close clears
ownership before the backend call, is never retried, and returns `Ok(false)`
when repeated.

Until move-only ownership is compiler-enforced, applications must not
intentionally duplicate a live `File`. The complete API, error model, realm
behavior, and cleanup contract are documented in [Filesystem](fs.md).

## `std.os.linux`

This is a target-specific kernel boundary, not the portable I/O API. It is
currently implemented and tested only for hosted Linux x86-64.

### Raw syscall calls

`std.os.linux.syscall` supplies `call0` through `call6`. Every call accepts a
`usize` syscall number, followed by zero through six `u64` register arguments,
and returns the exact kernel `i64` value. A value from `-1` through `-4095`
encodes a positive Linux errno.

The boundary is implemented by
`src/platform/linux/x86_64/syscall.S`. `runec build` and `runec run` select
platform link behavior from the resolved target triple. Unreachable Linux
wrappers and their foreign symbols are omitted from generated C.

### Results

```runes
pub type LinuxResult<T> =
    | Value(T)
    | Failure(i32)

pub type LinuxStatus =
    | Success
    | Failure(i32)
```

The `i32` payload is the positive errno number. This explicit result is used
because built-in nominal error sets cannot carry an errno payload.

### File descriptors

`std.os.linux.fd` currently provides:

- `read(fd, buffer: []u8) -> LinuxResult<usize>`;
- `write(fd, buffer: []const u8) -> LinuxResult<usize>`;
- `write_text(fd, text: str) -> LinuxResult<usize>`;
- `write_all_text(fd, text: str) -> LinuxStatus`;
- `close(fd) -> LinuxStatus`;
- `seek(fd, offset, origin) -> LinuxResult<usize>`;
- `duplicate(fd) -> LinuxResult<i32>`;
- `duplicate_to(fd, destination) -> LinuxResult<i32>`;
- `open_at_raw(directory, path, flags, mode) -> LinuxResult<i32>`.

`write_all_text` handles partial writes, retries `EINTR`, and reports a
zero-byte write as `EIO`. `open_at_raw` requires a valid NUL-terminated pointer.

Descriptors in this raw module remain plain `i32` values and are not
automatically closed. Applications should use the owning `std.fs.File`
surface; the raw boundary remains for backend implementation.

Descriptor writes accept read-only slices and use `*const u8` at the syscall
boundary. `write_text` is available for borrowed strings.

### Virtual memory

`std.os.linux.memory` provides raw `map`, `unmap`, and `protect` wrappers over
`mmap`, `munmap`, and `mprotect`. These are kernel mapping primitives, not an
allocator. Callers remain responsible for range validity, alignment,
protection, mapping ownership, and use after unmapping.

### Constants

The x86-64 syscall table contains every definition found in the installed
Linux UAPI `asm/unistd_64.h`, preserving numeric gaps. General descriptor,
open, `*at`, seek, mapping, remapping, and synchronization constants are staged
in `std.os.linux.constants`.

The compiler supports public module constants. This source module has not yet
been migrated, so wrappers still repeat the private constants they consume.
Promoting the staged declarations to `pub const` is now library work rather
than a compiler blocker.

## What is not implemented

The following are planned but are not current stdlib APIs:

- `std.io` backends beyond hosted Linux x86-64 and owning socket streams;
- `std.fs` backends beyond hosted Linux x86-64, directory iteration, and
  canonicalization;
- typed allocators and allocator capabilities;
- `RawBox<T>` and specialized collections beyond realm-aware `Vec<T>`;
- `StringBuilder` and owning-string operations beyond the documented initial
  `String` API;
- floating-point formatting/parsing, process, networking, async, and threading
  libraries.

The [`print` builtin](modules-ffi-tooling.md) remains a compiler/runtime
facility. `std.io` is the typed library boundary applications should use when
they need explicit success or failure.

## Executable coverage

Current behavior is exercised by:

- [`core_codegen_std_option.runes`](../../src/tests/samples/core_codegen_std_option.runes);
- [`core_codegen_std_result.runes`](../../src/tests/samples/core_codegen_std_result.runes);
- [`core_codegen_std_vec.runes`](../../src/tests/samples/core_codegen_std_vec.runes);
- [`core_codegen_std_string.runes`](../../src/tests/samples/core_codegen_std_string.runes);
- [`core_codegen_std_text.runes`](../../src/tests/samples/core_codegen_std_text.runes);
- [`core_codegen_std_text_edges.runes`](../../src/tests/samples/core_codegen_std_text_edges.runes);
- [`core_codegen_std_text_external.runes`](../../src/tests/samples/core_codegen_std_text_external.runes);
- [`core_codegen_std_format_integer.runes`](../../src/tests/samples/core_codegen_std_format_integer.runes);
- [`core_codegen_std_format_text.runes`](../../src/tests/samples/core_codegen_std_format_text.runes);
- [`core_codegen_std_format_realms.runes`](../../src/tests/samples/core_codegen_std_format_realms.runes);
- [`core_codegen_std_format_writer.runes`](../../src/tests/samples/core_codegen_std_format_writer.runes);
- [`core_codegen_std_parse.runes`](../../src/tests/samples/core_codegen_std_parse.runes);
- [`core_codegen_std_parse_failures.runes`](../../src/tests/samples/core_codegen_std_parse_failures.runes);
- [`core_codegen_std_parse_realms.runes`](../../src/tests/samples/core_codegen_std_parse_realms.runes);
- [`core_codegen_std_path.runes`](../../src/tests/samples/core_codegen_std_path.runes);
- [`core_codegen_std_path_realms.runes`](../../src/tests/samples/core_codegen_std_path_realms.runes);
- [`core_codegen_std_path_failures.runes`](../../src/tests/samples/core_codegen_std_path_failures.runes);
- [`core_codegen_std_fs.runes`](../../src/tests/samples/core_codegen_std_fs.runes);
- [`core_codegen_std_fs_failures.runes`](../../src/tests/samples/core_codegen_std_fs_failures.runes);
- [`core_codegen_std_fs_realms.runes`](../../src/tests/samples/core_codegen_std_fs_realms.runes);
- [`core_codegen_std_fs_fake.runes`](../../src/tests/samples/core_codegen_std_fs_fake.runes);
- [`core_codegen_std_fs_descriptors.runes`](../../src/tests/samples/core_codegen_std_fs_descriptors.runes);
- [`core_codegen_std_io_output.runes`](../../src/tests/samples/core_codegen_std_io_output.runes);
- [`core_codegen_std_io_read.runes`](../../src/tests/samples/core_codegen_std_io_read.runes);
- [`core_codegen_std_io_large_read.runes`](../../src/tests/samples/core_codegen_std_io_large_read.runes);
- [`core_codegen_std_io_exact_bytes.runes`](../../src/tests/samples/core_codegen_std_io_exact_bytes.runes);
- [`core_codegen_std_io_fake.runes`](../../src/tests/samples/core_codegen_std_io_fake.runes);
- [`core_codegen_std_io_invalid_count.runes`](../../src/tests/samples/core_codegen_std_io_invalid_count.runes);
- [`core_codegen_std_io_broken_pipe.runes`](../../src/tests/samples/core_codegen_std_io_broken_pipe.runes);
- [`core_codegen_std_bytes.runes`](../../src/tests/samples/core_codegen_std_bytes.runes);
- [`core_codegen_linux_syscalls.runes`](../../src/tests/samples/core_codegen_linux_syscalls.runes);
- expected failures for invalid Result constructor payloads, invalid variant
  method bodies, and method/arm name collisions.

Run the complete compiler, runtime, stdlib, and documentation checks with:

```bash
make test
```
