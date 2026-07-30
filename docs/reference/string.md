# Owning `String` Reference

`std.string.String` is Runes' realm-aware owning, growable UTF-8 text type. It
is a distinct wrapper over `Vec<u8>` and uses the same source API under
dynamic, regional, and GC ownership. Stack code cannot construct it.

Use `str` and [`std.text`](text.md) for borrowed text. Use `String` when text
must be copied, retained, or grown.

## Import and basic use

```runes
use std.string.String

dynamic f example() {
    message := String.from_str("hello")
    message.push(',')
    message.push_str(" world")
    print(message.as_str())
    message.deinit()
}
```

`String` is logically move-only, although the compiler does not enforce moves
yet. Copying it copies an ownership handle rather than duplicating its bytes.

## Representation and permanent invariant

The current representation is:

```runes
pub type String = {
    bytes: Vec<u8>
}
```

Field privacy is not implemented. The `bytes` field is nevertheless
library-private by contract: application code must not assign to it or mutate
it through `Vec<u8>`.

Every live `String` satisfies:

- `bytes.length <= bytes.reserved`;
- exactly `bytes.length` bytes are initialized;
- the initialized prefix is valid UTF-8;
- spare capacity is not text and is not exposed as initialized data;
- the backing vector retains the hidden owner realm selected at construction.

There is deliberately no safe mutable-byte view. Such a view could create
invalid UTF-8 without giving the library a point at which to restore the
invariant.

## Construction

| Ordinary, terminating | Recoverable | Meaning |
|---|---|---|
| `String.new() -> String` | `String.tnew() -> Result<String, AllocationError>` | Empty string with capacity 1 |
| `String.with_capacity(n) -> String` | `String.twith_capacity(n) -> Result<String, AllocationError>` | Empty string with byte capacity `max(n, 1)` |
| `String.from_str(value) -> String` | `String.tfrom_str(value) -> Result<String, AllocationError>` | Validated copy of borrowed UTF-8 |

Capacity is measured in bytes, not Unicode scalars. All constructors allocate
at least one byte of backing capacity. Ordinary forms terminate through
`AllocationError.fail()` if allocation fails; `t` forms return the error.

Arbitrary bytes use a validation result:

```runes
String.from_bytes(value: []const u8)
    -> Result<String, StringError>
```

`from_bytes` copies valid UTF-8. It returns `InvalidUtf8` without constructing
a live `String` for malformed input. Embedded NUL bytes are accepted. There is
no lossy constructor in the current API; invalid data is never silently
replaced.

## Observation and borrowed views

| Method | Complexity | Result |
|---|---:|---|
| `byte_len() -> usize` | O(1) | Initialized UTF-8 byte length |
| `capacity() -> usize` | O(1) | Backing byte capacity |
| `is_empty() -> bool` | O(1) | Whether byte length is zero |
| `as_str() -> str` | O(1) | Borrowed valid UTF-8 view |
| `as_bytes() -> []const u8` | O(1) | Borrowed read-only byte view |

`byte_len` is not a scalar or grapheme count. Pass `as_str()` to
`std.text.scalar_count` when a scalar count is required.

Views point into current backing storage. They must end before `reserve`,
`push`, `push_str`, `truncate`, `clear`, or `deinit`. Runes does not yet enforce
that lifetime. In particular, do not pass a view of a string back into a
mutation of the same string:

```runes
-- Invalid aliasing discipline until enforced borrowing exists:
-- value.push_str(value.as_str())
```

Growth can change the backing address, and every mutation ends all earlier
views even when that particular call did not reallocate.

## Capacity and append

| Ordinary, terminating | Recoverable | Success result |
|---|---|---|
| `reserve(additional) -> usize` | `treserve(additional) -> Result<usize, AllocationError>` | Actual capacity |
| `push(scalar: char) -> usize` | `tpush(scalar) -> Result<usize, AllocationError>` | New byte length |
| `push_str(value: str) -> usize` | `tpush_str(value) -> Result<usize, AllocationError>` | New byte length |

`reserve` ensures room for `byte_len() + additional` bytes. It does not promise
an exact capacity increase. Growth is geometric and checked for arithmetic and
byte-layout overflow. `treserve` also validates the String's persistent owner
when current capacity is already sufficient; a nested regional caller cannot
silently operate on storage owned by another active region.

`char` is a Unicode scalar value, so `push` appends its one-to-four-byte UTF-8
encoding. `push_str` accepts valid borrowed UTF-8 and preserves embedded NUL.
Appending an empty `str` is a no-op.

Append is transactional with respect to logical text:

1. capacity is reserved;
2. every source byte is copied into spare capacity;
3. the complete new initialized prefix is published once;
4. only then is logical byte length changed.

If allocation or publication fails, the old text and byte length remain valid.
Capacity and backing address may already have changed if growth succeeded
before publication failed. Spare bytes written by a failed append are not
observable as text.

## Truncation and clearing

```runes
value.truncate(new_length: usize) -> usize
value.ttruncate(new_length: usize) -> Result<usize, StringError>

value.clear() -> usize
value.tclear() -> Result<usize, AllocationError>
```

All lengths are byte offsets.

- `truncate(n)` only shrinks. If `n >= byte_len()`, it is a no-op.
- A shrinking index must be zero, the current byte length, or the start of a
  UTF-8 scalar.
- `ttruncate` returns `InvalidBoundary(n)` without mutation if `n` splits a
  multibyte scalar.
- Ordinary `truncate` terminates through the checked string-boundary
  diagnostic for the same invalid index.
- Neither operation reduces capacity.
- `clear` publishes length zero and preserves capacity.

## Errors and failure policy

```runes
pub type StringError =
    | InvalidUtf8
    | InvalidBoundary(usize)
    | Allocation(AllocationError)
```

`InvalidUtf8` belongs to arbitrary-byte validation.
`InvalidBoundary(index)` belongs to recoverable truncation.
`Allocation(error)` wraps a storage failure where a string-specific operation
must also represent validation or boundary errors.

Pure allocation operations use `AllocationError` directly:

- `OutOfMemory`;
- `CapacityOverflow`;
- `OwnerUnavailable`.

Ordinary allocation operations delegate to their `t` form and terminate
instead of returning an allocation error. Domain validation remains explicit:
`from_bytes` always returns `Result`.

## Realm behavior

The caller does not pass or write a realm argument:

```runes
dynamic f a()  { value := String.new() }
regional f b() { value := String.new() }
gc f c()       { value := String.new() }
```

The compiler specializes constructors and methods from the inferred execution
or receiver-owner realm:

| Owner realm | Behavior |
|---|---|
| dynamic | Growth replaces and frees dynamic backing storage; `deinit` frees current storage |
| regional | Growth stays in the directly active owning arena; replaced bytes remain until arena teardown |
| GC | Growth uses traced byte storage; clear/deinit make old storage collectible |
| stack | Owning construction is rejected at compile time |

A string created in one regional arena cannot grow or publish a changed length
while a nested child arena is active. Recoverable operations return
`OwnerUnavailable`; the old logical text remains valid. Ownership is never
silently transferred.

No runtime realm tag or realm branch is part of `String`.

## Deinitialization and current ownership limits

```runes
value.deinit()
```

`deinit` delegates to its `Vec<u8>` owner policy and clears the handle. Calling
it twice on the same value is harmless. No normal operation is valid after
deinitialization.

Until move-only values are enforced:

- do not create two live copies of one `String`;
- pass `*String` to helpers that mutate the same value;
- call `deinit` exactly once for each ownership lineage;
- do not use or deinitialize a stale copied handle;
- do not retain views across mutation or cleanup;
- do not share a string concurrently without external synchronization.

## Deliberate omissions

The initial API does not provide:

- lossy UTF-8 conversion;
- mutable raw bytes;
- scalar or grapheme indexing;
- insertion/removal in the middle;
- Unicode normalization or case conversion;
- formatting builders;
- automatic destruction or compiler-enforced moves.

Formatting is the next application-foundation milestone and will target this
type through its supported methods rather than its visible field.

## Verification

Implementation: [`src/std/string.runes`](../../src/std/string.runes).

Primary coverage:

- [`core_codegen_std_string.runes`](../../src/tests/samples/core_codegen_std_string.runes);
- [`core_codegen_std_string_realms.runes`](../../src/tests/samples/core_codegen_std_string_realms.runes);
- [`core_codegen_std_string_failures.runes`](../../src/tests/samples/core_codegen_std_string_failures.runes);
- stack, ordinary-allocation, and invalid-boundary expected-failure samples;
- generated-C dynamic/regional/GC specialization checks;
- ASan/UBSan/leak checks and a frontend fuzz seed.

The tests cover every UTF-8 width, embedded NUL, malformed encodings,
transactional allocation/publication failure, nested regional ownership, GC
collection, and explicit dynamic cleanup.
