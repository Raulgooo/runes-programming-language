# Formatting

`std.format` provides locale-independent typed formatting into caller-owned
byte storage, an owning `String`, or any statically dispatched
`std.io.Writer`.

```runes
options := IntegerFormat.hex_upper()
options.prefix = true
options.width = 10
options.padding = 0x30

match write_u64(output, 42, options) {
    Ok(written) -> {
        -- output[0..written] is "0X0000002A"
    }
    Err(_) -> {}
}
```

`IntegerFormat.decimal()`, `.hex_lower()`, and `.hex_upper()` produce standard
defaults. Its fields select the base, alignment, sign policy, hexadecimal
prefix, minimum byte width, and ASCII padding byte.

Width never truncates. With right alignment and `0` padding, zeroes follow the
sign and prefix: `+0X00002A`, not `000+0X2A`. Signed hexadecimal uses a sign
plus magnitude, not a two's-complement reinterpretation.

## API and errors

Fixed-buffer functions are `write_u8`, `write_u16`, `write_u32`, `write_u64`,
`write_usize`, `write_i8`, `write_i16`, `write_i32`, and `write_i64`. The
corresponding `append_*` functions target `*String`.

`integer_u64` and `integer_i64` produce an owning `IntegerCursor`.
`write_integer` and `append_integer` drain it. This is the common conversion
seam for every destination.

Textual values use `TextFormat.standard()`. Its fields select left/right
alignment, minimum byte width, and an ASCII padding byte. The typed functions
are:

```runes
write_bool(output, value, options)
write_char(output, value, options)
write_str(output, value, options)
write_debug_str(output, value, options)

append_bool(string, value, options)
append_char(string, value, options)
append_str(string, value, options)
append_debug_str(string, value, options)
```

Generic writer functions use the same names with a `_to` suffix:

```runes
write_u64_to<MyWriter>(&writer, value, options)
write_i64_to<MyWriter>(&writer, value, options)
write_bool_to<MyWriter>(&writer, value, options)
write_char_to<MyWriter>(&writer, value, options)
write_str_to<MyWriter>(&writer, value, options)
write_debug_str_to<MyWriter>(&writer, value, options)
```

`write_integer_to` and `write_text_to` drain existing cursors. `W: Writer` is
monomorphized at compile time; there is no writer vtable, erased data pointer,
runtime realm tag, or allocation introduced by dispatch.

`text`, `debug_text`, `boolean`, and `scalar` create a `TextCursor`.
`write_text_cursor` and `append_text_cursor` drain it. The cursor traverses
Unicode scalar values, not raw source bytes, so fixed buffers and `String`
produce identical UTF-8 without exposing the String's `Vec<u8>`.

Debug strings are enclosed in double quotes. NUL, tab, newline, carriage
return, quote, and backslash use `\0`, `\t`, `\n`, `\r`, `\"`, and `\\`.
Other ASCII control bytes use lowercase `\xNN`. Other Unicode scalars remain
unchanged.

Every operation returns `Result<usize, FormatError>`:

- `InsufficientSpace(required)` leaves a fixed buffer unchanged;
- `InvalidPadding(byte)` rejects non-ASCII padding;
- `InvalidUtf8` rejects a malformed `str` produced through unsafe FFI;
- `Allocation(failure)` preserves a String growth failure;
- `Writer(failure)` preserves the portable `WriteError` reported by a writer.

String append validates the persistent owner and reserves complete capacity
before mutation. Allocation and nested-owner failure therefore preserve the
original value. All mutation goes through public String operations rather than
its visible-for-now representation. Success returns the new total byte length.

Writer output may already contain a prefix when a later write fails. This is
intentional stream behavior and differs from direct fixed-buffer formatting,
which is all-or-error. `write_all_to` handles partial progress, retries
`Io(Interrupted)`, rejects impossible counts, and reports `NoProgress` instead
of spinning on `Ok(0)`.

`FixedBufferWriter` and `StringWriter` are the initial adapters.
`StringWriter` preserves its target's inferred dynamic, regional, or GC owner
realm. It accepts only complete valid UTF-8 chunks; splitting a multi-byte
scalar across separate calls returns `InvalidUtf8`.

## Current boundary

Floating-point formatting remains a separately approved later sub-gate.
Pointer formatting is intentionally excluded from the initial API. There is
deliberately no runtime format-string mini-language.
