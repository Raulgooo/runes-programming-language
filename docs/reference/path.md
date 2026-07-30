# Paths

`std.path` provides portable, purely lexical path manipulation. It does not
open files, query metadata, resolve symbolic links, or depend on a hosted
operating system.

## Representation

A path is an arbitrary sequence of bytes:

- UTF-8 is accepted, but not required;
- byte `0x2f` (`/`) is the lexical separator;
- embedded NUL is valid in `PathView` and `Path`, but cannot be converted to a
  syscall path;
- no operation silently changes or validates text encoding.

This contract preserves Linux filenames that are not valid UTF-8. Applications
that require Unicode paths must impose that policy separately.

```runes
use std.path.PathView

view := PathView.from_str("assets/icons")
print(view.is_relative())
```

## Borrowed paths

`PathView` contains one borrowed `[]const u8`:

```runes
pub type PathView = {
    bytes: []const u8
}
```

Construct it with:

- `PathView.from_bytes(bytes)`;
- `PathView.from_str(value)`.

The view does not own or copy its source. Its lifetime cannot exceed the
lifetime of those bytes.

Queries are allocation-free:

- `len()` and `is_empty()`;
- `is_absolute()` and `is_relative()`;
- `contains_nul()`;
- `file_name()`;
- `parent()`;
- `components()`.

`parent()` removes the final raw lexical component and adjacent trailing
separators. It returns a borrowed sub-view and does not normalize the path.

## Components

`components()` returns a `PathComponents` cursor. Repeated separators are
skipped, and `next()` returns:

```runes
pub type PathComponent =
    | Root
    | Current
    | Parent
    | Normal([]const u8)
```

`Current` represents `.`, `Parent` represents `..`, and `Normal` borrows the
component bytes from the original path. Iteration allocates nothing.

## Owning paths

`Path` owns a realm-aware `Vec<u8>`:

```runes
pub type Path = {
    bytes: Vec<u8>
}
```

Its constructors follow the standard two-level naming convention:

| Ordinary | Fallible | Meaning |
|---|---|---|
| `Path.new()` | `Path.tnew()` | Empty owned path |
| `Path.with_capacity(n)` | `Path.twith_capacity(n)` | Reserve byte capacity |
| `Path.from_view(view)` | `Path.tfrom_view(view)` | Copy borrowed bytes |
| `Path.from_str(text)` | `Path.tfrom_str(text)` | Copy UTF-8 string bytes |
| `Path.normalize(view)` | `Path.tnormalize(view)` | Lexically normalize |
| `Path.join(base, child)` | `Path.tjoin(base, child)` | Join and normalize |

Fallible forms return `Result<Path, AllocationError>`. Ordinary forms terminate
through the standard allocation-failure boundary if allocation fails.

`as_view()` borrows the owned bytes. `deinit()` releases or relinquishes
backing storage according to the `Path` owner realm. `Path` construction and
growth work in dynamic, regional, and GC execution; stack execution rejects
owning storage.

## Lexical normalization

Normalization is explicit. It:

- collapses repeated `/`;
- removes `.`;
- cancels a preceding normal component when processing `..`;
- preserves unmatched `..` in relative paths;
- discards attempts to walk above an absolute root;
- returns `.` for an empty normalized relative path;
- preserves `/` for the absolute root.

Examples:

| Input | Normalized |
|---|---|
| `""` | `.` |
| `a//b/../c` | `a/c` |
| `../../a` | `../../a` |
| `/../../a` | `/a` |
| `/a/./b/` | `/a/b` |

`Path.join(base, child)` returns the normalized child unchanged when the child
is absolute. Otherwise it concatenates the two lexical paths with one
separator and normalizes the result.

Do not normalize automatically before a filesystem syscall. Lexical removal
of `..` can change meaning in the presence of symbolic links. The caller must
choose normalization when that transformation is appropriate.

## Syscall representation

`PlatformPath` owns the exact source bytes followed by one NUL terminator:

```runes
match PlatformPath.tfrom_view(view) {
    Ok(value) -> {
        platform := value
        unsafe {
            raw_api(platform.as_pointer())
        }
        platform.deinit()
    }
    Err(failure) -> {
        -- Allocation(...) or ContainsNul(index)
    }
}
```

Conversion does not normalize. `tfrom_view()` returns
`Result<PlatformPath, PathError>`, where:

```runes
pub type PathError =
    | Allocation(AllocationError)
    | ContainsNul(usize)
```

`as_view()` excludes the terminator. `as_bytes_with_nul()` includes it.
`as_pointer()` exposes a pointer for FFI and syscall boundaries, so dereference
and external use remain unsafe. There is intentionally no ordinary
`PlatformPath.from_view`: an embedded NUL is recoverable input failure, not an
allocation failure that should terminate the process.

## What belongs elsewhere

`std.path` is platform-neutral lexical infrastructure. Opening files,
directories, metadata, and resource ownership belong in `std.fs`;
canonicalization is not implemented yet. OS-specific syscall wrappers accept
`PlatformPath` rather than teaching `Path` about Linux handles.

## Verification

The executable path suite covers empty/root/relative paths, repeated
separators, `.` and `..`, above-root behavior, trailing separators, component
iteration, parent and filename views, absolute-child replacement, normalization
idempotence, arbitrary non-UTF-8 bytes, embedded NUL rejection, allocation
failure, and dynamic/regional/GC specialization. Generated programs are also
run under AddressSanitizer and UndefinedBehaviorSanitizer.
