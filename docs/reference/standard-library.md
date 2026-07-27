# Standard Library

This page documents the standard-library source that exists and is tested on
the current `v0-1-exp` branch. It is an implementation reference, not a list of
planned APIs.

Runes' standard library is still a bootstrap library. It currently provides
`Option<T>` and allocation-free byte-slice operations. It does not yet provide
safe terminal input, files, owning strings, growable collections, formatting,
networking, or concurrency.

## Library layout

| Module or file | Current status |
|---|---|
| `std.core` | Implemented: `Option<T>`, `UnwrapError`, and option methods |
| `std.bytes` | Implemented: byte fill, copy, equality, search, and prefix checking |
| `std.os` | Declared module with no public operations yet |
| `std.io` | Empty source placeholder; not currently exported by `std` |
| `std/prelude.runes` | Compiler/runtime ABI declarations, not an application library |

The root [`src/std/mod.runes`](../../src/std/mod.runes) currently exports
`core`, `bytes`, and the empty `os` module.

## Imports

Ordinary public byte functions can be imported individually:

```runes
use std.bytes.copy
use std.bytes.find
```

Imported generic types currently have a bootstrap limitation: generic
specialization runs before ordinary imported-member resolution. Import the
containing module and qualify `Option` until that compiler limitation is fixed:

```runes
use std.core

core.Option<i32> value = core.Option.Some(42)
```

The intended ergonomic form, `use std.core.Option` followed by
`Option<i32>`, is not reliable yet. This is an implementation limitation, not
the desired long-term API.

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
core.Option<i32> present = core.Option.Some(21)
core.Option<i32> absent = core.Option.None<i32>()
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
core.Option<i32> doubled = present.map<i32>(double)
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
find(buffer: []const u8, byte: u8) -> core.Option<usize>
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

## What is not implemented

The following are planned but are not current stdlib APIs:

- safe stdin/stdout and general I/O interfaces;
- Linux raw I/O bindings under `std.os`;
- `Result<T, E>` as a separate generic type;
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
- [`core_codegen_std_bytes.runes`](../../src/tests/samples/core_codegen_std_bytes.runes);
- expected failures for invalid variant method bodies and method/arm name
  collisions.

Run the complete compiler, runtime, stdlib, and documentation checks with:

```bash
make test
```
