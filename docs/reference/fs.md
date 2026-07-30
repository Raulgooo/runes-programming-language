# Filesystem

`std.fs` is the safe owning filesystem layer for hosted applications. Its
public contracts are platform-oriented rather than Linux-oriented, while the
only implemented backend is currently hosted Linux x86-64.

The module is unavailable for freestanding targets. Kernel and freestanding
programs must provide their own filesystem or device backend; importing
`std.fs` does not silently introduce Linux into those builds.

## Basic use

```runes
use std.fs
use std.fs.File
use std.fs.OpenOptions
use std.io.write_all_to
use std.path.PathView

dynamic f main() {
    match fs.open(
        PathView.from_str("notes.txt"),
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

Paths are accepted as `PathView`, preserve arbitrary non-NUL bytes, and are not
normalized implicitly. The filesystem boundary uses `PlatformPath`
internally. Application code does not need `unsafe`, a raw descriptor, a
NUL-terminated pointer, Linux flags, or errno.

## File ownership

`File` owns exactly one descriptor:

```runes
pub type File = {
    descriptor: i32
    is_open: bool
}
```

These fields are library-private by contract until field visibility is
enforced by the compiler. Do not construct `File` directly.

Runes does not yet enforce move-only types. Treat binding a newly returned
`File` to its working local as an ownership move, do not intentionally
duplicate that live value, and close the one ownership lineage exactly once.
`opened()` reports whether the value still owns a descriptor.

`close()` follows the Linux-safe ownership rule:

- ownership is cleared before the backend close call;
- a successful first close returns `Ok(true)`;
- a repeated close returns `Ok(false)` and issues no syscall;
- close is never retried after an error, because Linux may already have freed
  and reused the descriptor;
- even when the first close returns `Err(...)`, the `File` remains cleared.

This policy prevents a close retry from accidentally closing an unrelated,
reused descriptor.

## Open options

`OpenOptions` has common associated constructors:

| Constructor | Behavior |
|---|---|
| `read_only()` | Open an existing file for reading |
| `write_only()` | Open an existing file for writing without truncation |
| `read_write()` | Open an existing file for reading and writing |
| `append()` | Open for append and create if absent |
| `create()` | Create or overwrite for writing |
| `create_new()` | Exclusively create; fail if the path exists |

The value-returning modifiers are:

- `with_create(enabled)`;
- `with_create_new(enabled)`;
- `with_truncate(enabled)`;
- `with_append(enabled)`;
- `with_permissions(mode)`.

Modifiers return a changed copy. Assign the result:

```runes
options := OpenOptions.read_write()
options = options.with_create(true)
options = options.with_truncate(true)
```

Creation defaults to permission mode `0666` before the process umask is
applied. Runes source does not yet accept octal literals, so an explicit mode
is currently written as its decimal or hexadecimal value.

Invalid combinations, such as requesting creation without write access,
return `InvalidInput`; they do not terminate the process.

## Stream capabilities

`File` implements the M5 static interfaces:

- `Reader.read(self: *File, bytes: []u8)`;
- `Writer.write(self: *File, bytes: []const u8)`;
- `Seeker.seek(self: *File, from: SeekFrom)`;
- `Flusher.flush(self: *File)`;
- `Closer.close(self: *File)`.

Calls are statically specialized. There is no runtime interface object,
allocation, vtable, or realm branch. Generic helpers such as
`read_exact_from<File>` and `write_all_to<File>` therefore work directly with
files and handle interruption or partial progress according to the I/O
contracts.

`read` and `write` are partial operations. Empty buffers succeed with zero.
A backend zero-byte read is reported as `Io(EndOfInput)`.

`File` has no user-space staging buffer, so `flush()` is an allocation-free
no-op that verifies the file is open. Persistence is explicit:

- `sync_data()` asks the backend to persist data;
- `sync_all()` asks it to persist data and associated metadata.

## Seeking, length, and metadata

`seek(SeekFrom.Start(n))`, `seek(SeekFrom.Current(delta))`, and
`seek(SeekFrom.End(delta))` return the resulting absolute byte offset.
An unsigned start that cannot fit the backend signed offset is rejected as
`InvalidInput`.

`set_length(length)` truncates or extends an open file. Extending may create a
sparse file according to the host filesystem.

Metadata is available through both `file.metadata()` and
`fs.metadata(path)`. The initial portable record is:

```runes
pub type Metadata = {
    kind: FileType
    length: usize
    permissions: u32
    modified_seconds: i64
}
```

`FileType` is `Regular`, `Directory`, `SymbolicLink`, or `Other`. This initial
surface intentionally does not expose device/inode identity, ownership IDs,
nanoseconds, or raw backend fields.

## Bounded whole-file reads

`file.read_to_end(limit)` reads from the current position into a realm-aware
`Vec<u8>`. It never returns more than `limit` bytes. If exactly the limit is
reached, it performs one extra one-byte probe:

- EOF after exactly `limit` bytes is success;
- another byte produces `LimitExceeded(limit)`.

`file.read_to_string(limit)` applies the same bound and then requires valid
UTF-8. It returns `InvalidUtf8` instead of replacing malformed bytes.

Both operations are `flex`: their temporary and returned owning storage is
specialized for dynamic, regional, or GC execution without a realm argument
at the call site. Allocation failure is returned as data.

## Path operations

The initial safe path operations are:

```runes
fs.open(path, options)
fs.metadata(path)
fs.create_directory(path, permissions)
fs.remove_file(path)
fs.remove_directory(path)
fs.rename(source, destination)
```

`create_directory` creates one level only. `remove_directory` requires an
empty directory. `rename` uses the host's rename semantics. None of these
operations normalizes paths or resolves symbolic links first.

Directory iteration and canonicalization are deliberately deferred; they are
not hidden behind incomplete APIs.

## Errors

Filesystem operations return:

```runes
pub type FsError =
    | Io(FsOperation, IoError)
    | Allocation(FsOperation, AllocationError)
    | InvalidPath(FsOperation, usize)
```

`FsOperation` identifies `Open`, `SyncData`, `SyncAll`, `Metadata`,
`SetLength`, `CreateDirectory`, `RemoveFile`, `RemoveDirectory`, or `Rename`.
The static stream methods already carry their operation in the specific
`ReadError`, `WriteError`, or `IoError` return contract.

The portable `IoError` reasons include `NotFound`, `PermissionDenied`,
`AlreadyExists`, `InvalidInput`, `Interrupted`, `WouldBlock`,
`BrokenPipe`, `EndOfInput`, `Unsupported`, and `Unknown`.

Errors intentionally do not retain the path or raw errno:

- retaining a path would require hidden allocation or an unsafe borrowed
  lifetime;
- portable application logic should not depend on Linux errno numbers;
- the operation plus portable reason is enough for ordinary recovery;
- the private raw backend remains the place for platform diagnostics.

`InvalidPath(operation, index)` reports the byte offset of an embedded NUL.

Whole-file reads use the more specific `FileReadError`:

```runes
pub type FileReadError =
    | Read(ReadError)
    | Allocation(AllocationError)
    | LimitExceeded(usize)
    | InvalidUtf8
```

## Realm and platform behavior

The public path-taking functions and bounded reads are realm-polymorphic.
Calling the same source from dynamic, regional, or GC code specializes all
reachable storage operations for that effective realm. Callers do not pass an
allocator or spell a realm generic argument.

The current backend is `std.os.linux.filesystem` plus the raw descriptor
module. That backend is an internal implementation boundary. Future hosted
platforms can implement the same safe surface without changing application
imports.

## Verification

The executable M7 suite covers:

- create, overwrite, append, exclusive create, seek, truncate, sparse
  extension, metadata, rename, directory creation, and removal;
- empty, missing, embedded-NUL, permission-denied, and already-existing paths;
- short and interrupted reads/writes through an injected syscall backend;
- EOF at the exact bound, limit overflow, invalid UTF-8, and allocation
  failure;
- close failure, repeated close, and proof that close is not retried;
- repeated open/close descriptor counts through `/proc/self/fd`;
- dynamic, regional, and GC specialization;
- strict C11 generation with warnings as errors;
- AddressSanitizer and UndefinedBehaviorSanitizer runs in temporary
  directories with explicit cleanup.

[Back to the complete reference](README.md)
