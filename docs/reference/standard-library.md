# Standard Library

This page documents the standard-library source that exists and is tested on
the current `v0-1-exp` branch. It is an implementation reference, not a list of
planned APIs.

Runes' standard library is still a bootstrap library. It currently provides
`Option<T>`, `Result<T, E>`, portable foundational error types,
allocation-free byte-slice operations, and an initial raw Linux x86-64 syscall
boundary. It does not yet provide portable safe terminal input, files, owning
strings, growable collections, formatting, networking, or concurrency.

## Library layout

| Module or file | Current status |
|---|---|
| `std.core` | Implemented: `Option<T>`, `Result<T, E>`, their methods, and foundational errors |
| `std.bytes` | Implemented: byte fill, copy, equality, search, and prefix checking |
| `std.os.linux.syscall` | Raw Linux x86-64 syscall calls with zero through six arguments |
| `std.os.linux.result` | Errno-preserving `LinuxResult<T>` and `LinuxStatus` |
| `std.os.linux.fd` | Raw descriptor read, write, close, seek, duplication, and `openat` operations |
| `std.os.linux.memory` | Raw `mmap`, `munmap`, and `mprotect` operations |
| `std.os.linux.constants` | Private staged Linux ABI constants; ready to migrate to implemented `pub const` |
| `std.io` | Empty source placeholder; not currently exported by `std` |
| `std/prelude.runes` | Compiler/runtime ABI declarations, not an application library |

The root [`src/std/mod.runes`](../../src/std/mod.runes) currently exports
`core`, `bytes`, and `os`.

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
    | Unknown

pub type ParseError =
    | InvalidSyntax(usize)
    | UnexpectedEnd(usize)
    | OutOfRange(usize)

pub type AllocationError =
    | OutOfMemory
    | CapacityOverflow
```

They are portable identities for higher-level library APIs. Platform backends
must translate native values such as Linux errno rather than expose those
values as the public error type. These types are foundations; modules may add
more specific errors when their contracts require them.

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
- `write(fd, buffer: []u8) -> LinuxResult<usize>`;
- `write_text(fd, text: str) -> LinuxResult<usize>`;
- `write_all_text(fd, text: str) -> LinuxStatus`;
- `close(fd) -> LinuxStatus`;
- `seek(fd, offset, origin) -> LinuxResult<usize>`;
- `duplicate(fd) -> LinuxResult<i32>`;
- `duplicate_to(fd, destination) -> LinuxResult<i32>`;
- `open_at_raw(directory, path, flags, mode) -> LinuxResult<i32>`.

`write_all_text` handles partial writes, retries `EINTR`, and reports a
zero-byte write as `EIO`. `open_at_raw` requires a valid NUL-terminated pointer.

Descriptors remain plain `i32` values. Runes does not yet have move-only
resources or deterministic destruction, so this module does not claim that a
descriptor is uniquely owned or automatically closed.

The mutable-slice requirement on `write` is now a library migration item:
read-only slices expose `*const T` for FFI, but this wrapper has not yet changed
its public signature. `write_text` is available for borrowed strings.

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

- portable `std.io`, safe stdin/stdout, and general I/O interfaces;
- typed allocators and allocator capabilities;
- `RawBox<T>`, `Vec<T>`, arena containers, and GC containers;
- owning `String` and `StringBuilder`;
- formatting, filesystem, path, process, networking, async, and threading
  libraries.

The [`print` builtin](modules-ffi-tooling.md) and contracts from
`std/prelude.runes` are compiler/runtime facilities. Their presence does not
mean that a general `std.io` module already exists.

## Executable coverage

Current behavior is exercised by:

- [`core_codegen_std_option.runes`](../../src/tests/samples/core_codegen_std_option.runes);
- [`core_codegen_std_result.runes`](../../src/tests/samples/core_codegen_std_result.runes);
- [`core_codegen_std_bytes.runes`](../../src/tests/samples/core_codegen_std_bytes.runes);
- [`core_codegen_linux_syscalls.runes`](../../src/tests/samples/core_codegen_linux_syscalls.runes);
- expected failures for invalid Result constructor payloads, invalid variant
  method bodies, and method/arm name collisions.

Run the complete compiler, runtime, stdlib, and documentation checks with:

```bash
make test
```
