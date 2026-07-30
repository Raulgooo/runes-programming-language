# Ordinary and Recoverable `t` Operations

Status: implemented standard-library convention on `v0-1-exp`.

## Decision

When one logical operation needs both an ergonomic form and an explicitly
recoverable form, Runes prefixes the recoverable operation with `t`:

```runes
values := Vec<i32>.new()
fallible := Vec<i32>.tnew()

length := values.push(42)
fallible_length := values.tpush(42)
```

The prefix is concatenated directly with the operation name. Use `tnew`,
`tpush`, and `tallocate`, not `try_new`, `tryPush`, or another spelling.

## Contract

- The ordinary operation returns the success value directly.
- The `t` operation returns `Result<Success, Error>`.
- The ordinary operation delegates to the `t` implementation.
- On `Err`, the ordinary operation invokes the relevant portable runtime
  failure policy.
- The pair must not contain two independent algorithms.
- A `t` name exists only when callers can meaningfully recover from the same
  operation. Do not mechanically prefix every method.

For storage operations, `AllocationError.fail()` invokes the runtime storage
failure policy. Hosted execution currently prints a source-bearing diagnostic
and terminates. Freestanding runtimes must provide the same semantic boundary
using their platform panic/halt policy; the standard library does not call
Linux directly.

## Implemented pairs

### Typed allocation

| Ordinary | Recoverable |
|---|---|
| `allocate` | `tallocate` |
| `allocate_array` | `tallocate_array` |
| `publish_initialized` | `tpublish_initialized` |
| `resize_array` | `tresize_array` |

### `Vec<T>`

| Ordinary | Recoverable |
|---|---|
| `Vec<T>.new` | `Vec<T>.tnew` |
| `Vec<T>.with_capacity` | `Vec<T>.twith_capacity` |
| `reserve` | `treserve` |
| `push` | `tpush` |
| `pop` | `tpop` |
| `truncate` | `ttruncate` |
| `clear` | `tclear` |

Module-level Vec constructor compatibility wrappers follow the same pairing.

### `String`

| Ordinary | Recoverable |
|---|---|
| `String.new` | `String.tnew` |
| `String.with_capacity` | `String.twith_capacity` |
| `String.from_str` | `String.tfrom_str` |
| `reserve` | `treserve` |
| `push` | `tpush` |
| `push_str` | `tpush_str` |
| `truncate` | `ttruncate` |
| `clear` | `tclear` |

`from_bytes` always returns `Result<String, StringError>` because malformed
UTF-8 is a domain result, not merely a storage failure. It therefore has no
terminating `tfrom_bytes` pair.

## Guidance for future data structures

Apply the same policy to owning strings, maps, sets, queues, boxes, and other
structures:

```runes
text := String.new()
recoverable_text := String.tnew()

table := Map<Key, Value>.with_capacity(64)
recoverable_table := Map<Key, Value>.twith_capacity(64)
```

Operations that naturally report absence or bounds without storage failure
keep their existing contracts. For example, `get -> Option<T>` and
`set -> bool` do not acquire `tget` or `tset` merely because they can report a
negative result.

## Testing requirement

Every new pair needs:

- ordinary success coverage;
- recoverable success and injected-failure coverage;
- proof that failed mutation preserves the original value;
- ordinary failure-policy coverage;
- dynamic, regional, and GC specialization where the operation owns storage;
- stack rejection where owning allocation is unavailable;
- generated-C and sanitizer coverage.
