# Pointers, Unsafe Code, FFI, and Input

Read this chapter before calling C, operating-system, device, or allocator APIs.
`unsafe` is not an optimization switch. It marks code where the programmer must
uphold facts that the compiler cannot prove.

## Pointers

A pointer stores a memory address:

```runes
i32 value = 42
*i32 pointer = &value
```

`&value` takes the address of assignable storage. `*i32` is a non-null pointer
to an `i32`. Reading or writing through it requires `unsafe`:

```runes
unsafe {
    print(*pointer)
    *pointer = 43
}
```

A copied pointer still refers to the same storage. It does not copy the pointed
value and does not automatically own or free it.

## Nullable pointers

Normal `*T` pointers cannot contain `null`. Use `?*T` when absence is possible:

```runes
i32 value = 42
*i32 present = &value
?*i32 maybe = present
?*i32 missing = null
```

Compare a nullable pointer with `null`. Before dereferencing, unwrap it:

```runes
*i32 definite = unwrap(maybe)
unsafe { print(*definite) }
```

`unwrap` traps if the pointer is null. Nullable pointers cannot be dereferenced
or used in pointer arithmetic directly.

## Pointer arithmetic and casts

Inside `unsafe`, pointers support:

```runes
*i32 next = pointer + 1
*i32 previous = pointer - 1
```

The integer counts elements, not bytes. Pointer-plus-integer,
integer-plus-pointer, and pointer-minus-integer are the only pointer arithmetic
forms. The programmer must keep the result within valid storage.

Integer-to-pointer and most pointer reinterpretation casts require `unsafe`:

```runes
unsafe {
    *u32 device = 0x10000000 as *u32
}
```

`*void` is the untyped FFI/allocation pointer. Explicit casts convert it to or
from typed pointers. A zero integer cannot create a non-null pointer; use
`null` with `?*T`.

## Raw slices

When foreign code supplies a pointer and a verified length, construct a slice:

```runes
unsafe {
    []u8 writable = slice(pointer, length)
    []const u8 readonly = const_slice(pointer, length)
}
```

The constructor cannot validate that the memory is actually alive and large
enough. The resulting slice gains bounds checks but retains the source
pointer's lifetime and provenance.

## Operations requiring `unsafe`

The current compiler requires a lexical unsafe block for:

- pointer dereference;
- pointer arithmetic;
- integer-to-pointer and unrelated pointer casts;
- raw `slice` and `const_slice` construction;
- access to volatile/MMIO storage;
- arbitrary extern function calls;
- inline assembly;
- direct string pointer access;
- explicit byte-pointer/array-pointer to C-string-style `str` conversion.

`unsafe` does not disable type checking, integer overflow traps, or bounds
checks. Keep blocks small enough that their required assumptions are visible.

## Foreign declarations

Declare a C ABI function with `extern f`:

```runes
extern f read(fd: i32, buffer: *u8, count: usize) = result: i64
extern u64 external_counter
```

An ordinary extern call is unsafe:

```runes
unsafe {
    i64 count = read(0, &buffer, buffer.len)
}
```

The type checker verifies the declared argument and result types. It cannot
verify that the foreign declaration matches the actual linked symbol, that a
pointer is valid for the supplied length, or that the foreign implementation
obeys its contract.

Variadic C functions are not supported. Bind a fixed-signature C wrapper
instead of exposing functions such as variadic `scanf` directly.

### `#[safe]` externs

The binding author may assert that an extern is safe for every well-typed call:

```runes
#[safe]
#[link_name("abs")]
extern f c_abs(value: i32) = result: i32
```

This removes the call-site unsafe requirement. It does not inspect or sandbox
the foreign function. Do not mark raw `read(fd, *u8, count)` safe: a caller can
still provide a dangling pointer or a length larger than its allocation.

**Current visibility gap:** extern functions and variables are always exposed
across their containing module. Private extern bindings are still needed for a
clean standard-library implementation boundary.

## Safe wrappers around unsafe FFI

A normal Runes function may contain an unsafe block. Calling that normal
function remains safe because the wrapper is responsible for checking its
contract.

The planned standard-library byte API follows this shape:

```runes
extern f read(fd: i32, buffer: *u8, count: usize) = result: i64

error IoError = {
    | ReadFailed
    | InvalidCount
}

pub f read_into(fd: i32, buffer: []u8) = result: !usize {
    unsafe {
        i64 count = read(fd, buffer.ptr, buffer.len)
        if count < 0 {
            result = error.IoError.ReadFailed
        } else if count as usize > buffer.len {
            result = error.IoError.InvalidCount
        } else {
            result = count as usize
        }
    }
}
```

The raw pointer and length come from one slice, so safe callers cannot disagree
about them. A production wrapper must also translate platform error state,
retry interrupted reads where appropriate, preserve partial reads, and define
EOF behavior.

This API is roadmap code, not yet implemented in `std.io`.

## Current raw terminal input

Until the standard library wrapper exists, Linux programs can call `read`
directly:

```runes
extern f read(fd: i32, buffer: *u8, count: usize) = result: i64

f main() {
    [64]u8 input = []
    print("What's your name?: ")

    i64 count = 0
    unsafe {
        count = read(0, &input, 63)
    }

    if count > 0 {
        usize length = count as usize
        if input[length - 1] == '\n' {
            length = length - 1
        }
        input[length] = 0
        unsafe {
            print("hello, ", (&input) as str)
        }
    }
}
```

Important details:

- file descriptor `0` is standard input on POSIX systems;
- terminal input normally includes the newline typed by the user;
- one byte is reserved for a NUL terminator;
- `read` can fail, return EOF, return fewer bytes than requested, or be
  interrupted;
- `print` adds its own final newline;
- the prompt above therefore appears with a newline; interactive `input(prompt)`
  needs standard-library write-without-newline and flush operations.

The cast to `str` treats the pointer as a NUL-terminated UTF-8 assertion. It
scans for NUL, creates a borrowed view, and does not copy or extend the buffer's
lifetime. Only `*u8` and pointers to fixed `u8` arrays may use this cast, and it
requires `unsafe`.

The future `read_line`/`input` APIs need an owning realm-aware `String`, bounded
growth, buffering, UTF-8 validation, explicit EOF, and typed I/O errors. The
complete work list is in the standard-library roadmap.

## Runes strings versus C strings

Runes `str` has this ABI shape:

```c
typedef struct {
    const uint8_t *ptr;
    size_t len;
} RunesStr;
```

It is length-bearing, may contain embedded NUL, and is not automatically a C
`char *`. Runtime helpers explicitly validate byte views, validate C strings,
or allocate a NUL-terminated C copy. A C copy returned by `runes_str_to_c` uses
raw allocated storage and must be released with `raw_free`.

## Layout and attributes

Use compile-time layout queries:

```runes
usize bytes = sizeof(Packet)
usize alignment = alignof(Packet)
```

Implemented systems attributes include:

```runes
#[repr(C)]
#[packed]
#[align(16)]
type Packet = { tag: u8, value: u32 }

#[section(".data.device")]
#[align(32)]
volatile i32 device_state = 0

#[link_name("foreign_symbol")]
#[callconv("sysv64")]
extern f foreign(value: i32) = result: i32
```

`#[callconv("win64")]` is also recognized where applicable. Unknown,
duplicated, malformed, or inapplicable attributes are rejected. Attribute
strings cannot contain embedded NUL.

## Inline assembly

Inline assembly is target-specific GNU-style backend assembly:

```runes
f halt() {
    unsafe {
        asm { "cli; hlt" }
    }
}
```

An output can bind to an existing mutable integer or pointer variable:

```runes
u64 value = 0
unsafe {
    asm { "mov %cr3, %rax" } -> value
}
```

Assembly cannot contain NUL bytes. Invalid, missing, const, or unsupported
output bindings are rejected. `#[interrupt]` signatures are recognized, but the
v0.1 C backend rejects their emission; use an external assembly entry stub.

## Common mistakes

- Marking an extern `#[safe]` because writing wrappers feels inconvenient.
- Passing a buffer capacity larger than the actual allocation.
- Returning a `str` or slice into a local input array.
- Assuming an FFI string uses the Runes `str` ABI when C expects `char *`.
- Forgetting partial reads, EOF, EINTR, or output flushing.
- Treating `volatile` as atomic synchronization.
- Making one large unsafe block whose invariants cannot be audited.

[Next: Language and tooling reference](08-reference.md)
