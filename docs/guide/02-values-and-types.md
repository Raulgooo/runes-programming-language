# Values, Types, and Variables

## The basic idea

A **value** is data such as `42`, `true`, or `"hello"`. A **type** describes
what kind of data it is and which operations are valid. A **variable** gives a
value a name.

```runes
i32 age = 30
bool active = true
str name = "Raul"
```

The type comes first, followed by the variable name and its initial value.
Runes checks that the value matches the declared type before generating C.

## Primitive types

| Type | Use |
|---|---|
| `i8`, `i16`, `i32`, `i64` | Signed whole numbers |
| `u8`, `u16`, `u32`, `u64` | Nonnegative whole numbers and raw bytes |
| `usize` | Sizes and indexes; currently the same width as `u64` |
| `f32`, `f64` | Floating-point numbers |
| `bool` | `true` or `false` |
| `char` | One Unicode scalar value |
| `str` | Immutable, borrowed UTF-8 text view |
| `void` | No value, mainly for FFI and `!void` |

Choose a signed integer unless negative values are impossible by definition.
Use `usize` for lengths and indexes. Use `u8` for bytes. Fixed-width integer
types are important in file formats, protocols, hardware, and FFI.

## Literals

```runes
i32 decimal = 42
u8 byte = 0x2A
u32 mask = 0b101010
f64 ratio = 3.5
bool ready = true
char letter = 'R'
str greeting = "hello"
```

Text and character literals may contain UTF-8 and escapes. A `char` must be one
valid Unicode scalar, not an arbitrary byte or multi-character string.

`str` is a pointer plus a byte length. It is not necessarily NUL-terminated,
may contain embedded NUL bytes, and does not own the memory it views.

## Type inference

Use `:=` when the initializer makes the intended type clear:

```runes
name := "Runes"  -- str
ready := true    -- bool
```

Explicit types are usually clearer at public APIs, storage boundaries, FFI,
and places where integer width matters.

## Constants and mutation

Variables can be assigned again:

```runes
i32 count = 0
count = count + 1
```

`const` prevents reassignment:

```runes
const usize BUFFER_SIZE = 4096
```

This is a binding rule, not a compile-time metaprogramming system. v0.1 has no
general compile-time evaluation or const generics.

Variables belong to the nearest surrounding block. An inner block may shadow
an outer name; the outer value becomes visible again when the inner block ends.
Avoid unnecessary shadowing when it makes code difficult to follow.

## Composite types at a glance

Later chapters explain these in detail:

```runes
*i32                       -- non-null pointer
?*i32                      -- nullable pointer
[4]i32                     -- four inline i32 values
[]i32                      -- mutable borrowed slice
[]const i32                -- read-only borrowed slice
(i32, bool)                -- tuple
f(i32) -> i32              -- function/closure value
[regional] f(i32) -> i32   -- realm-qualified function value
!i32                       -- either i32 or an error
```

Arrays, structs, variants, and tuples copy their inline values. Pointers copy
an address. Slices copy a pointer-and-length view, not the backing elements.

## Numeric conversions

Runes avoids silent narrowing. Convert explicitly with `as`:

```runes
i64 wide = 42
i32 narrow = wide as i32
usize index = narrow as usize
```

An integer literal can initialize a narrower type only when the compiler can
prove that it fits. Dynamic casts use the target C representation; design code
so the value is known to be in range before narrowing.

Pointer-related casts have stricter rules and usually require `unsafe`; see
chapter 7.

## Operators

Arithmetic:

```runes
a + b
a - b
a * b
a / b
a % b
```

Comparison:

```runes
a == b
a != b
a < b
a <= b
a > b
a >= b
```

Boolean logic:

```runes
ready and connected
failed or cancelled
!ready
```

Bitwise integer operations:

```runes
flags & mask
flags | option
flags ^ changed
value << amount
value >> amount
```

The operands must have compatible types. `%` is integer-only. `and` and `or`
require booleans; they are not integer bitwise operators.

## Runtime checks

Ordinary integer addition, subtraction, multiplication, division, remainder,
negation, and left shift are checked. The program traps on overflow, division
by zero, invalid shift counts, and signed minimum divided by `-1`.

Array, slice, and string indexing also checks bounds. These checks remain active
inside `unsafe`; `unsafe` grants access to specific operations, not permission
to ignore all language checks.

v0.1 does not yet have explicit wrapping/saturating arithmetic or unchecked
indexing.

## Strings and characters

Strings compare by their UTF-8 bytes and stored length:

```runes
bool same = "runes" == "runes"
bool before = "abc" < "abd"
str combined = "hello, " + "world"
```

Concatenation allocates in the active memory realm. A string index returns a
byte, not a Unicode character. Runtime helpers provide UTF-8 boundary checks,
decoding, validation, encoding, and hashing; higher-level text iteration belongs
in the standard library.

Use `.len` for the byte length:

```runes
usize bytes = "hé".len -- 3 UTF-8 bytes
```

## `volatile`

`volatile` tells the C backend that storage may change outside normal program
flow, as with memory-mapped hardware:

```runes
f write_uart() {
    unsafe {
        volatile *u32 uart = 0x10000000 as *u32
        *uart = 65
    }
}
```

It does not make access atomic, synchronized, or memory-safe. Pointer casts and
dereferences still require `unsafe`.

## Common mistakes

- Treating `str.len` as a Unicode-character count. It is a byte count.
- Expecting numeric types to convert implicitly.
- Using `u8` for every positive number instead of only bounded byte-sized data.
- Assuming `const` recursively freezes memory behind a pointer.
- Assuming `unsafe` disables overflow or bounds checks.

[Next: Functions, control flow, and errors](03-functions-and-control-flow.md)
