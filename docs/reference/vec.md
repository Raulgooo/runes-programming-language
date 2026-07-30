# `Vec<T>` Reference

`std.vec.Vec<T>` is Runes' first public owning collection and the reference
design for later contiguous collections. It is implemented and tested for
dynamic, regional, and GC ownership. Stack code cannot construct owning
vectors.

This page defines the complete current API and its safety contract. The
[standard-library reference](standard-library.md#stdvec) gives the shorter
module overview.

## Import and construction

```runes
use std.vec.Vec

dynamic f example() {
    values := Vec<i32>.new()
    values.push(10)
    values.deinit()
}
```

Associated constructors are the canonical spelling:

| Ordinary, terminating | Recoverable | Meaning |
|---|---|---|
| `Vec<T>.new() -> Vec<T>` | `Vec<T>.tnew() -> Result<Vec<T>, AllocationError>` | Empty vector with capacity 1 |
| `Vec<T>.with_capacity(n) -> Vec<T>` | `Vec<T>.twith_capacity(n) -> Result<Vec<T>, AllocationError>` | Empty vector with capacity `max(n, 1)` |

Construction allocates backing storage even when the requested capacity is
zero. Ordinary forms call `AllocationError.fail()` and do not return if
allocation fails. Recoverable forms return the error.

The module-level `new<T>`, `tnew<T>`, `with_capacity<T>`, and
`twith_capacity<T>` functions remain compatibility wrappers. New code should
use associated constructors.

## Representation and invariants

The current visible fields are:

```runes
pub type Vec<T> = {
    data: ?*T
    length: usize
    reserved: usize
}
```

Field privacy is not implemented yet. These fields are nevertheless an
internal representation; application code must not assign to them.

Every live vector satisfies:

- `length <= reserved`;
- `data != null`;
- `reserved >= 1`;
- elements in `0 .. length` are initialized;
- runtime initialized-prefix metadata equals `length`;
- the hidden storage owner remains the owner chosen when the backing storage
  was allocated.

After `deinit`, all three fields are cleared to zero/null and the value is no
longer live.

## Complete method contract

Complexity below counts vector elements and excludes the fixed-size cost of
copying one `T`.

### Observation

| Method | Complexity | Exact result |
|---|---:|---|
| `len(self) -> usize` | O(1) | Current initialized element count |
| `capacity(self) -> usize` | O(1) | Number of elements the allocation can hold |
| `is_empty(self) -> bool` | O(1) | `len() == 0` |

### Capacity and append

| Ordinary, terminating | Recoverable | Complexity | Success result |
|---|---|---:|---|
| `reserve(additional) -> usize` | `treserve(additional) -> Result<usize, AllocationError>` | O(1) without growth; O(len) with growth | Actual capacity after ensuring room for `len() + additional` |
| `push(value) -> usize` | `tpush(value) -> Result<usize, AllocationError>` | Amortized O(1); O(len) when it grows | New length after copying `value` to the end |

`reserve` does not mean “increase capacity by exactly this amount.” It ensures
space relative to the current length. Growth doubles geometrically until it
meets the requirement, except near `usize` overflow where it uses the exact
required capacity. Arithmetic overflow or a `T` byte-layout overflow produces
`CapacityOverflow`.

`push` may invoke growth. It writes the new value and then publishes the new
initialized prefix to the storage runtime. This ordering keeps
pointer-bearing GC elements alive without exposing an uninitialized slot to a
collector.

The recoverable methods provide the following basic failure guarantee:

- existing logical elements remain valid and in the same order;
- logical length does not increase on failed `tpush`;
- failed allocation inside `treserve` leaves its allocation and fields
  unchanged;
- `tpush` may have successfully increased capacity before a later publication
  failure, so capacity and backing address are not promised to remain
  unchanged.

Code must not depend on spare-capacity contents.

### Removal and shrinking

| Ordinary, terminating | Recoverable | Complexity | Success result |
|---|---|---:|---|
| `pop() -> Option<T>` | `tpop() -> Result<Option<T>, AllocationError>` | O(1) | A copy of the last value, or `None` when empty |
| `truncate(n) -> usize` | `ttruncate(n) -> Result<usize, AllocationError>` | O(1) | Resulting length |
| `clear() -> usize` | `tclear() -> Result<usize, AllocationError>` | O(1) | Zero |

`truncate(n)` only shrinks. If `n >= len()`, it is a no-op and returns the
current length. Shrinking does not reduce capacity. `clear()` is
`truncate(0)`.

`pop`, `truncate`, and `clear` publish the shorter initialized prefix. This is
necessary for GC vectors so removed pointers stop being traced. On recoverable
publication failure, the old logical length and sequence remain valid.

These methods do not invoke user-defined destruction or cleanup. Before
discarding elements that own files, sockets, locks, or other external
resources, application code must explicitly clean those resources.

### Indexed access

| Method | Complexity | Exact behavior |
|---|---:|---|
| `get(index) -> Option<T>` | O(1) | Copies the element, or returns `None` when out of bounds |
| `set(index, value) -> bool` | O(1) | Copies `value` into an existing slot and returns `true`; returns `false` without writing when out of bounds |

`get` returns a copy because Runes does not yet have a safe reference-return
model. Neither operation changes length or capacity.

### Slice views

```runes
values.as_slice() -> []const T
values.as_mut_slice() -> []T
```

Both methods return a non-owning view of exactly `len()` elements and perform
no allocation. They are O(1).

Runes does not yet enforce borrowing or exclusive mutable access. The caller
must therefore follow these rules:

- do not retain either view across a vector mutation or `deinit`;
- while a mutable view exists, do not read or mutate the vector through
  another alias;
- do not deinitialize or copy ownership of the vector while a view exists;
- never use a view after an operation that can grow the vector, because its
  backing address may have changed.

Treat a vector mutation as ending all prior views, even if that particular
operation happened not to reallocate.

### Deinitialization

```runes
values.deinit()
```

`deinit` relinquishes current backing storage according to its persistent owner
realm, then clears `data`, `length`, and `reserved`. Calling it twice on the
same value is harmless. No other operation is valid after `deinit`.

| Owner realm | `deinit` effect |
|---|---|
| dynamic | Frees the backing allocation |
| regional | Relinquishes the storage handle; arena teardown owns the bytes |
| GC | Clears the initialized prefix and relinquishes the handle so unreachable storage and elements can be collected |

`deinit` does not recursively clean up external resources held by elements.

## Realm behavior

The caller never writes a realm argument. `Vec<T>` preserves a hidden owner
identity and the compiler specializes its `flex` methods:

```runes
dynamic f a()  { values := Vec<i32>.new() }
regional f b() { values := Vec<i32>.new() }
gc f c()       { values := Vec<i32>.new() }
```

- dynamic growth allocates replacement storage, copies initialized elements,
  and frees the old allocation;
- regional growth allocates in the vector's directly active owning arena and
  leaves replaced arena bytes for arena teardown;
- GC growth allocates traced replacement storage and publishes its initialized
  prefix;
- stack construction is a compile-time error.

Calling a regional allocating mutation from a nested child arena returns
`OwnerUnavailable` from its recoverable form. Ownership is never silently
transferred to the child realm.

## Errors and ordinary failure policy

Recoverable operations can report:

| Error | Meaning |
|---|---|
| `AllocationError.OutOfMemory` | The selected storage backend could not allocate |
| `AllocationError.CapacityOverflow` | Element count or byte-layout arithmetic overflowed |
| `AllocationError.OwnerUnavailable` | The persistent owner cannot safely service this operation |

The matching ordinary operation delegates to the recoverable implementation.
On `Err`, it invokes the portable non-returning storage failure policy. Use the
ordinary API for applications that cannot recover meaningfully; use the
`t`-prefixed API at recovery boundaries and inside reusable library code.

## Ownership and current safety limitations

`Vec<T>` is logically move-only, but the compiler does not enforce move-only
values yet. Copying a vector copies its ownership handle, not its elements.
Two copied handles can therefore cause stale access or dynamic double-free.

Until move enforcement exists:

- do not assign, return, or pass a vector by value when that creates two live
  owning copies;
- pass `*Vec<T>` when a helper needs to mutate the same vector;
- call `deinit` exactly once for each ownership lineage;
- do not directly mutate the visible representation fields;
- do not share a vector concurrently; it provides no synchronization;
- do not use zero-sized element types, which typed allocation currently
  rejects.

## Blueprint for later collections

`Vec<T>` is the starting point of the owning collection library, not yet a
complete collection framework. Future owning containers should follow these
proven rules:

1. use typed `std.allocation` operations rather than duplicating raw allocation
   and casts;
2. preserve a hidden owner realm and never ask callers to pass a realm;
3. pair an ordinary terminating operation with a directly prefixed
   recoverable form when failure is meaningful (`push`/`tpush`);
4. check element-count and byte-size arithmetic before allocation;
5. publish initialized storage before any possible GC safepoint;
6. specify view invalidation, ownership transfer, element cleanup, complexity,
   and the precise failure guarantee;
7. test the same source API under dynamic, regional, and GC owners, including
   forced allocation failure and pointer-bearing GC elements.

The internal [stdlib building guide](../internal/stdlib/building-guide.md) and
[app-readiness plan](../internal/stdlib/app-readiness-plan.md) explain how this
contract leads into `String`, iterators, maps, and later collections.

## Verification

The implementation is in
[`src/std/vec.runes`](../../src/std/vec.runes). Its executable coverage includes:

- ordinary and recoverable constructors and mutations;
- empty, indexed, slice, pop, clear, truncate, and repeated growth behavior;
- overflow and deterministic allocation failure;
- dynamic release and double-`deinit`;
- regional owner preservation and nested-owner rejection;
- pointer-bearing GC elements, initialized-prefix updates, and forced
  collection;
- stack-realm rejection and ordinary non-returning failure.

The primary executable sample is
[`core_codegen_std_vec.runes`](../../src/tests/samples/core_codegen_std_vec.runes).
Sanitizer and leak checks are part of the compiler test targets.
