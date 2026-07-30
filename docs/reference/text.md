# Borrowed Text Reference

`std.text` provides allocation-free operations over borrowed UTF-8 `str`
values. It does not own, copy, grow, or deinitialize text.

Every position in this API is a byte offset. Operations that create a `str`
view validate that their endpoints are UTF-8 scalar boundaries.

## Import

Import only the operations and types a module uses:

```runes
use std.text.ScalarCursor
use std.text.find
use std.text.substring
use std.text.trim_ascii
```

`std.text` is distinct from `std.bytes`. Use text operations when UTF-8
boundaries matter and byte operations when the data is not necessarily text.

## Text validity

Safe language-created `str` values contain valid UTF-8. Unsafe foreign code
can still supply a length-bearing `str`, so the module exposes:

```runes
is_valid_utf8(value: str) -> bool
```

Operations other than `is_valid_utf8` require valid UTF-8. They do not repair
or replace invalid input.

Embedded NUL is ordinary text data because `str` carries an explicit byte
length.

## Observation

```runes
is_empty(value: str) -> bool
byte_len(value: str) -> usize
scalar_count(value: str) -> usize
is_scalar_boundary(value: str, byte_index: usize) -> bool
```

| Operation | Complexity | Behavior |
|---|---:|---|
| `is_empty` | O(1) | Tests whether byte length is zero |
| `byte_len` | O(1) | Returns byte length, not scalar or grapheme count |
| `scalar_count` | O(bytes) | Counts Unicode scalar values |
| `is_scalar_boundary` | O(1) | Accepts zero, length, and the start of each encoded scalar |

A Unicode scalar is not a user-perceived grapheme. For example, a base letter
and a following combining mark count as two scalars.

## Prefix, suffix, and search

```runes
starts_with(value: str, prefix: str) -> bool
ends_with(value: str, suffix: str) -> bool
contains(value: str, needle: str) -> bool
find(value: str, needle: str) -> Option<usize>
```

Search is exact, case-sensitive, and byte-preserving. `find` returns the byte
offset of the first match. An empty needle matches at byte offset zero.

| Operation | Complexity |
|---|---:|
| `starts_with` | O(prefix bytes) |
| `ends_with` | O(suffix bytes) |
| `find` | O(value bytes × needle bytes) in the current straightforward implementation |
| `contains` | Same as `find` |

No locale, normalization, case folding, or grapheme behavior is implied.

## Checked substring views

```runes
substring(
    value: str,
    start: usize,
    end: usize
) -> Option<str>
```

The range is half-open: `[start, end)`. Both values are byte offsets.

`substring` returns `None` when:

- `start > end`;
- `end > value.len`;
- either endpoint splits a UTF-8 scalar.

Otherwise it returns a borrowed view without allocation. Empty ranges at any
valid boundary, including `value.len`, succeed.

The returned view points into the original storage and must not outlive or be
used after mutation/deinitialization of that storage. Runes does not yet
enforce a general borrow system, so callers must preserve this rule.

The function is named `substring` because `slice` is already a language
builtin for constructing raw slices.

## Explicit ASCII trimming

```runes
trim_ascii_start(value: str) -> str
trim_ascii_end(value: str) -> str
trim_ascii(value: str) -> str
```

These functions remove only:

- horizontal tab, byte `0x09`;
- line feed through carriage return, bytes `0x0a` through `0x0d`;
- space, byte `0x20`.

They return borrowed views and allocate nothing. Non-ASCII whitespace,
including non-breaking space, is preserved.

The explicit `ascii` name is intentional. Unqualified `trim` remains reserved
for a future approved Unicode `White_Space` contract backed by versioned
Unicode data.

## Splitting once

```runes
pub type TextSplit = {
    before: str
    after: str
}

split_once(value: str, delimiter: str) -> Option<TextSplit>
```

`split_once` uses the first exact delimiter match. `before` excludes the
delimiter, and `after` begins immediately after it. Both fields are borrowed
views into `value`.

No match returns `None`. An empty delimiter matches at byte offset zero and
returns an empty `before` plus the complete source as `after`.

The current implementation has the same worst-case search complexity as
`find`. It allocates nothing.

## Unicode scalar cursor

```runes
pub type ScalarCursor = {
    source: str
    byte_index: usize
}

ScalarCursor.new(source: str) -> ScalarCursor
scalars(source: str) -> ScalarCursor

cursor.position() -> usize
cursor.is_done() -> bool
cursor.next() -> Option<char>
```

`ScalarCursor` is a forward, non-owning, allocation-free cursor:

- `position` is the byte offset of the next scalar;
- `next` returns and advances over one Unicode scalar;
- `next` returns `None` at the end;
- `is_done` is true when the position reaches byte length.

Each successful `next` examines at most four bytes. Iterating the complete
source is O(bytes).

The cursor borrows `source`. The source storage must remain alive and unchanged
until cursor use ends. Copying a cursor creates an independent traversal
position over the same borrowed text; it does not copy or own the text.

Example:

```runes
use std.text.scalars

cursor := scalars("hé世界")
while !cursor.is_done() {
    match cursor.next() {
        Some(scalar) -> print(scalar),
        None -> {},
    }
}
```

## Allocation and realms

Every `std.text` operation is allocation-free and can run in stack, dynamic,
regional, or GC functions. Returned strings and cursors retain the provenance
of their source; the module never changes ownership or promotes storage.

## Deliberate omissions

The current module does not itself provide:

- ownership or growth; use [`std.string.String`](string.md);
- Unicode grapheme iteration;
- Unicode normalization or case folding;
- Unicode whitespace trimming;
- reverse scalar traversal;
- a general split iterator;
- regular expressions;
- locale-sensitive comparison.

These omissions are not silently approximated by ASCII behavior.

## Verification

Primary executable coverage:

- [`core_codegen_std_text.runes`](../../src/tests/samples/core_codegen_std_text.runes);
- [`core_codegen_std_text_edges.runes`](../../src/tests/samples/core_codegen_std_text_edges.runes);
- [`core_codegen_std_text_external.runes`](../../src/tests/samples/core_codegen_std_text_external.runes);
- UTF-8 validation/round-trip property tests in
  [`runtime_test.c`](../../src/tests/runtime_test.c).

The generated-C tests enforce `-Werror` and verify that the module emits no
allocation calls. Sanitizer and frontend fuzz-smoke coverage are part of the
repository test targets.
