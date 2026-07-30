# Parsing

`std.parse` converts borrowed text into typed scalar values without allocation.
All functions are locale-independent, consume the complete input, and return
`Result<T, ParseError>`.

```runes
use std.core.Result
use std.parse.IntegerParse
use std.parse.parse_i32
use std.parse.parse_u64_with

match parse_i32("-42") {
    Ok(value) -> { print(value) }
    Err(failure) -> { print("invalid number") }
}

match parse_u64_with("0xff", IntegerParse.auto()) {
    Ok(value) -> { print(value) } -- 255
    Err(_) -> {}
}
```

## Integer functions

Decimal whole-input parsing is available through:

```runes
parse_i8(value)    parse_u8(value)
parse_i16(value)   parse_u16(value)
parse_i32(value)   parse_u32(value)
parse_i64(value)   parse_u64(value)
                   parse_usize(value)
```

Every integer function also has an explicit-options form:

```runes
parse_i8_with(value, options)    parse_u8_with(value, options)
parse_i16_with(value, options)   parse_u16_with(value, options)
parse_i32_with(value, options)   parse_u32_with(value, options)
parse_i64_with(value, options)   parse_u64_with(value, options)
                                 parse_usize_with(value, options)
```

`IntegerParse` constructors select the grammar:

| Constructor | Digits |
|---|---|
| `decimal()` | `0` through `9` |
| `binary()` | `0` and `1` |
| `octal()` | `0` through `7` |
| `hexadecimal()` | `0` through `9`, `a` through `f`, or `A` through `F` |
| `auto()` | Decimal unless `0b`, `0o`, or `0x` selects another base |

Prefix letters are case-insensitive in automatic mode. Explicit-base modes
accept digits only and therefore reject a prefix. Leading zeroes have no
special meaning.

Signed functions accept an optional `+` or `-`. Unsigned functions accept an
optional `+` and reject `-`. A sign must be followed by at least one digit.

Whitespace is never removed implicitly. Use `std.text.trim_ascii` explicitly
when an input contract permits surrounding ASCII whitespace. Numeric `_`
separators, trailing text, and partial consumption are not accepted.

Overflow is checked before multiplication or addition. Every integer width
accepts its exact minimum and maximum, including `-9223372036854775808` and
`18446744073709551615`. `usize` follows the target width; every current
bootstrap target is x86-64, so its current range matches `u64`.

## Boolean and character functions

```runes
parse_bool(value) -> Result<bool, ParseError>
parse_char(value) -> Result<char, ParseError>
```

`parse_bool` accepts exactly lowercase `true` or `false`. It does not accept
case variants, numbers, or surrounding whitespace.

`parse_char` accepts exactly one valid Unicode scalar value:

```runes
parse_char("界")  -- Ok('界')
parse_char("ab")  -- Err(InvalidSyntax(1))
parse_char("")    -- Err(UnexpectedEnd(0))
```

The returned character is a Unicode scalar, not a byte or grapheme cluster.

## Errors and positions

```runes
pub type ParseError =
    | InvalidSyntax(usize)
    | UnexpectedEnd(usize)
    | OutOfRange(usize)
```

All positions are zero-based UTF-8 byte offsets:

- `InvalidSyntax(index)` identifies the first byte not admitted by the
  selected grammar, including trailing input;
- `UnexpectedEnd(index)` identifies the byte offset where required input was
  missing;
- `OutOfRange(index)` identifies the digit that first made the value
  unrepresentable in the requested type.

The parser does not skip valid prefixes after an error, replace invalid UTF-8,
or return a partial value.

## Complexity and memory

Parsing is linear in the input byte length and uses constant stack storage. It
performs no owning allocation in stack, dynamic, regional, or GC callers.

Floating-point parsing and prefix/stream parsing are not part of the current
API. Float spelling, rounding, exponent, infinity, and NaN rules require a
separate approved contract.
