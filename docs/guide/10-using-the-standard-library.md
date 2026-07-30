# Using the Standard Library

Runes now has enough foundational library code to build command-line data
flows without dropping into C for ordinary text, collections, formatting, or
terminal and file I/O. Lexical paths remain portable and separate from hosted
filesystem access.

The current application-level path is:

```text
borrowed str
    -> std.text observation/trimming
    -> std.parse typed values
    -> Vec<T> / String owned data
    -> std.format text output
    -> std.io terminal or Writer destination
    -> std.path lexical path preparation
    -> std.fs owning files and path operations
```

## Import only what you use

Public declarations can be imported directly:

```runes
use std.core.Result
use std.parse.parse_u32
use std.string.String
use std.vec.Vec
```

Generic imports may also be aliased with `use ... as ...`.

## Borrowed and owned text

Use `str` and `std.text` while the source storage already exists. Text indexes
are UTF-8 byte offsets:

```runes
use std.text.trim_ascii
use std.text.scalar_count

str cleaned = trim_ascii("  hello  ")
print(cleaned, " has ", scalar_count(cleaned), " scalars")
```

Use `String` when text must grow or own its storage. `String` is backed by
`Vec<u8>`, preserves valid UTF-8, and follows the active dynamic, regional, or
GC owner realm.

```runes
use std.string.String

dynamic f build() {
    text := String.from_str("answer: ")
    text.push_str("42")
    print(text.as_str())
    text.deinit()
}
```

Until move-only ownership exists, do not copy `String` or `Vec<T>` ownership
handles. End borrowed views before mutation and call `deinit` once per dynamic
ownership lineage.

## Parse external text

Parsing is strict and allocation-free:

```runes
use std.core.Result
use std.parse.parse_i32
use std.text.trim_ascii

match parse_i32(trim_ascii("  -42  ")) {
    Ok(value) -> { print(value) }
    Err(failure) -> { print("bad integer") }
}
```

Parsers do not trim implicitly or ignore trailing bytes. Errors distinguish
invalid syntax, missing input, and overflow, with UTF-8 byte positions.

## Format typed values

`std.format` writes typed values to fixed storage, `String`, or a static
`Writer` implementation. It does not use a runtime format-string language:

```runes
use std.core.Result
use std.format.IntegerFormat
use std.format.write_i64

[32]u8 storage = []
[]u8 output = storage
match write_i64(output, -42, IntegerFormat.decimal()) {
    Ok(length) -> { print("wrote ", length, " bytes") }
    Err(_) -> {}
}
```

Fixed-buffer formatting never allocates. `String` formatting grows according
to the String's owner realm. Generic Writer adapters are monomorphized and do
not introduce a vtable or runtime realm branch.

## Use `Vec<T>`

`Vec<T>` is the starting point for owning collections:

```runes
use std.vec.Vec

dynamic f collect() {
    values := Vec<i32>.new()
    values.push(10)
    values.push(20)
    print(values.len())
    values.deinit()
}
```

Ordinary operations terminate through the standard allocation-failure policy.
The corresponding `t` operations—such as `tnew`, `tpush`, and `treserve`—
return `Result` when recovery is required.

## Terminal I/O and writers

Hosted Linux x86-64 currently supplies safe standard-stream functions:

```runes
use std.io

match io.write_line("hello") {
    Ok(_) -> {}
    Err(_) -> {}
}
```

`std.io.Reader`, `Writer`, fixed/slice/String/standard-stream adapters,
`BufferedReader`, `BufferedWriter`, exact operations, and bounded byte or UTF-8
line reads provide portable static I/O contracts. Buffer storage is supplied
explicitly by the caller, and buffered writers must be flushed explicitly.
`std.fs.File` implements these contracts. Networking and additional OS
backends remain later milestones.

## Work with paths

`std.path` keeps path manipulation separate from filesystem calls. Paths are
arbitrary bytes rather than mandatory UTF-8, so valid Linux names are not
discarded:

```runes
use std.path.Path
use std.path.PathView

dynamic f prepare_path() {
    path := Path.join(
        PathView.from_str("/srv/app"),
        PathView.from_str("../config")
    )
    print(path.as_view().bytes.len)
    path.deinit()
}
```

Normalization is explicit and purely lexical. `PlatformPath.tfrom_view`
preserves exact bytes, rejects embedded NUL, and appends the terminator needed
by syscall boundaries. It does not silently normalize across symlinks.

## Open and use files

Hosted Linux x86-64 applications use `std.fs`; ordinary code does not import
the raw Linux backend:

```runes
use std.fs
use std.fs.File
use std.fs.OpenOptions
use std.io.write_all_to
use std.path.PathView

dynamic f save() {
    match fs.open(
        PathView.from_str("message.txt"),
        OpenOptions.create()
    ) {
        Ok(value) -> {
            file := value
            [5]u8 storage = [0x68, 0x65, 0x6c, 0x6c, 0x6f]
            []const u8 bytes = storage
            match write_all_to<File>(&file, bytes) {
                Ok(_) -> {}
                Err(_) -> {}
            }
            match file.close() {
                Ok(_) -> {}
                Err(_) -> {}
            }
        }
        Err(_) -> {}
    }
}
```

`File` is an owning handle and is not yet protected by compiler-enforced move
semantics. Do not intentionally copy a live file. Close its single ownership
lineage; repeated close is harmless, and even a reported close failure clears
ownership so Linux close is never retried.

Files support partial/exact reads and writes, seek, explicit data/full sync,
truncate or sparse extension, metadata, bounded read-to-end, and strict UTF-8
read-to-string. `std.fs` also creates/removes directories, removes files, and
renames paths. Calls preserve exact path bytes and never normalize implicitly.

The safe surface is portable in shape but currently has only a hosted Linux
x86-64 backend. It is intentionally absent in freestanding builds.

## Where to look next

- [Current standard library](../reference/standard-library.md)
- [Borrowed text](../reference/text.md)
- [`String`](../reference/string.md)
- [`Vec<T>`](../reference/vec.md)
- [Formatting](../reference/format.md)
- [Parsing](../reference/parse.md)
- [I/O](../reference/io.md)
- [Paths](../reference/path.md)
- [Filesystem](../reference/fs.md)
- [Allocation and realms](../reference/allocation.md)

[Back to the handbook index](README.md)
