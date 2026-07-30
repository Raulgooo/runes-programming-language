# GC Initialized Storage and High-Level Allocation Plan

Status: compiler/runtime prerequisite implemented and tested on 2026-07-29.
The exit gate at the end of this document passes, and public `std.vec.Vec<T>`
is now implemented on the contract. The later public naming decision exposes
ordinary terminating operations and recoverable `t` forms; both use the same
fallible storage implementation.

## Why this work is required

The current typed storage foundation can:

- allocate typed capacity in dynamic, regional, and GC realms;
- resize while preserving an explicit initialized prefix;
- release according to the selected owner realm;
- preserve the original allocation on failure.

Fresh GC array allocation records an initialized length of zero. Resize records
the initialized length supplied by the container. Before this milestone, the
missing operation was publishing changes when a container initialized or
removed elements without resizing. That operation is now implemented as
`std.allocation.tpublish_initialized<T>` (with
`publish_initialized<T>` as its terminating counterpart).

That makes the following future `Vec<T>` sequence incomplete for
pointer-bearing `T`:

```runes
vector.data[vector.len] = value
vector.len = vector.len + 1
```

The collector traces only the runtime object's recorded initialized prefix.
Changing the Runes `len` field alone does not update that runtime metadata.
A collection after an in-place `push` could therefore miss the new element's
pointers.

## Required invariant

Every typed GC sequence must carry:

```text
element descriptor
capacity
initialized prefix length
```

The invariant is:

```text
0 <= initialized <= capacity
```

Only elements in `[0, initialized)` may be traced. Elements in
`[initialized, capacity)` are storage, not values.

Container `len` and the runtime initialized prefix must agree at every GC
safepoint. The runtime must reject:

- a non-base pointer;
- a pointer owned by another backend;
- a mismatched element descriptor;
- a claimed old initialized length that is stale;
- a new initialized length beyond capacity.

## High-level `allocate()`

A high-level safe allocation function is possible, but it should initialize a
value:

```runes
pub flex f allocate<T>(
    value: T
) = outcome: *T

pub flex f tallocate<T>(
    value: T
) = outcome: Result<*T, AllocationError>
```

Semantics:

1. allocate capacity for one `T`;
2. write `value` into slot zero;
3. publish initialized length one;
4. return the pointer only after all three operations succeed.

This form works for arbitrary `T` without requiring a `Default` interface.
It also prevents safe code from receiving an uninitialized `*T`.

The following should not be called the high-level safe API:

```runes
allocate<T>() -> *T
tallocate<T>() -> Result<*T, AllocationError>
```

For arbitrary `T`, that would return uninitialized storage. If the standard
library needs such an operation internally, name it explicitly
`allocate_uninitialized<T>` and keep it private or unsafe.

`allocate_array<T>(capacity)` and recoverable
`tallocate_array<T>(capacity)` remain container-authoring primitives. Their
initial initialized length is zero.

## Implemented storage publication primitive

The compiler-recognized internal operation is:

```text
tstorage_set_initialized<T>(
    pointer,
    expected_old,
    new_initialized,
    capacity
)
```

This is not an application API. It is a trusted standard-library/compiler
contract.

Lowering supplies the concrete `T` descriptor and owner realm. The runtime
performs the validations above and updates GC metadata without allocating or
triggering collection.

Dynamic and regional specializations validate the supplied bounds but have no
GC trace length to update. Keeping one shared operation allows container
algorithms to remain realm-polymorphic.

The public container-authoring wrapper is:

```runes
tpublish_initialized<T>(
    pointer,
    expected_old,
    new_initialized,
    capacity
) -> Result<usize, AllocationError>
```

`publish_initialized<T>(...) -> usize` delegates to that operation and invokes
the storage-failure policy on `Err`.

The runtime stores GC sequence capacity explicitly. GC publication validates
the exact base pointer, descriptor identity, capacity, and expected old length.
Regional publication validates direct arena ownership. Publication counters,
shrink counters, and rejected-transition counters are available to runtime
tests.

### Publishing an inserted element

The shared implementation behind `Vec.push` and `Vec.tpush` follows this exact
critical sequence:

```text
evaluate value into a rooted local
write data[len] = value
tstorage_set_initialized(data, len, len + 1, capacity)
set len = len + 1
```

There must be no GC safepoint between the slot write and metadata publication.
The publication operation itself must be non-allocating.

The compiler must document and test whether ordinary assignment of `T` can
introduce a safepoint. If it can, replace the two-step write/publication
sequence with a compiler-lowered `storage_initialize_slot<T>` operation that
accepts the value through a temporary address, writes it, and publishes the
new length atomically under the GC no-collect guard.

The implemented compiler uses the two-step form. Generated assignment lowering
is a direct C scalar or aggregate assignment followed only by
`runes_gc_commit_allocations()`, which changes transient bookkeeping but never
allocates a GC object or collects. The subsequent publication wrapper roots its
pointer and calls the non-allocating runtime update. The same assignment path is
used by primitive, pointer-bearing struct, variant, string, interface, and
closure values; existing generated-C tests cover those aggregate categories,
and the storage proof specifically covers a pointer-bearing element.

### Removing elements

For `pop`, `truncate`, and `clear`:

1. move or copy any returned value into a rooted local;
2. perform required element/resource cleanup;
3. reduce the runtime initialized prefix;
4. update the container's `len`;
5. optionally zero removed bytes for diagnostics.

Shrinking the prefix allows removed pointer-bearing elements to become
collectible even while the backing allocation remains reachable.

GC `release_array` must set the initialized prefix to zero after required
element cleanup. A container `deinit` must also clear its pointer, length, and
capacity so repeated use cannot retain or reuse a released handle.

## Compiler work

### Type checking and provenance

- recognize the internal initialized-prefix operation;
- require a concrete non-stack realm;
- require a non-null `*T`;
- require `usize` lengths and capacity;
- propagate the receiver's owner realm through method specialization;
- reject direct application use if the intrinsic is marked stdlib-internal;
- retain the existing stack-realm rejection.

### Monomorphization

- combine `T` and hidden owner-realm specialization;
- preserve owner identity through ordinary function parameters and returns;
- support imported and aliased container implementations;
- emit no runtime realm branch;
- ensure generic methods on realm-specific generic owners keep their owner
  bindings.

### Code generation

- emit the exact dynamic, regional, or GC publication backend;
- pass the element descriptor and source location;
- make publication non-allocating;
- ensure the value being inserted is rooted before any possible GC safepoint;
- inspect generated code for all pointer-bearing aggregate categories;
- keep inactive realm backends unreachable.

## Runtime work

- derive sequence capacity from allocation size and descriptor size, or store
  it explicitly if that produces clearer validation;
- add an exact-base-object lookup for initialized-prefix updates;
- validate descriptor identity and expected old length;
- update initialized length without allocation;
- make GC release publish length zero;
- add counters for publication, shrink, and rejected transitions;
- preserve single-thread ownership rules used by the current collector;
- leave a clear synchronization boundary for a future concurrent collector.

## Test methodology

### Runtime unit tests

1. Allocate pointer-bearing GC capacity with initialized length zero.
2. Publish one initialized element and verify its referent survives collection.
3. Push a second element without resizing and collect again.
4. Shrink from two to one and verify only the removed referent is collected.
5. Shrink to zero while the backing array stays rooted and verify all
   referents become collectible.
6. Release while the backing object remains rooted and verify it traces no
   former elements.
7. Reject stale old length, length beyond capacity, interior pointer, wrong
   descriptor, and non-GC pointer.
8. Verify zero-capacity and zero-sized-type policy explicitly.

### Compiler and executable tests

- `allocate<T>(value)` and `tallocate<T>(value)` for primitive and
  pointer-bearing user types;
- dynamic, regional, and GC allocation with unchanged source syntax;
- repeated `push` operations both with and without resize;
- forced collection after every successful push;
- resize followed by additional in-place pushes;
- `pop`, `truncate`, `clear`, and release followed by collection;
- owner preservation through an ordinary helper function;
- nested regional owner rejection;
- allocation failure and capacity overflow preserving the old vector;
- stack specialization rejection;
- imported and aliased container code;
- generated-C assertions showing one exact backend per specialization.

### Memory tooling

- ASan and UBSan for all proof programs;
- leak detection for dynamic storage;
- GC object-count assertions for retained and removed referents;
- failure injection at allocation zero and after multiple successful growths;
- frontend fuzz seeds for the new intrinsic and high-level API syntax.

## Ordered milestones

1. Complete: freeze initialized-prefix semantics and safepoint rules.
2. Complete: implement runtime validation and metadata publication.
3. Complete: implement compiler typing, monomorphization, and exact lowering.
4. Complete: implement and test ordinary `allocate<T>(value)` and recoverable
   `tallocate<T>(value)`.
5. Complete: extend the internal `Buffer<T>` proof with pointer-bearing
   in-place push, pop, truncate, clear, and release.
6. Complete: run the compiler, runtime, sanitizer, dynamic leak, and fuzz
   suites.
7. Complete: implement the public `std.vec.Vec<T>`.

## Exit gate before `Vec<T>`

Public `Vec<T>` work may begin only when all of these are true:

- [x] a pointer-bearing element pushed without resizing survives forced GC;
- [x] removed elements stop being traced while backing storage remains alive;
- [x] stale GC initialized lengths are rejected instead of silently diverging;
- [x] failed growth leaves pointer, length, capacity, and elements unchanged;
- [x] dynamic, regional, and GC generated code selects only its owner backend;
- [x] high-level `allocate<T>(value)` and `tallocate<T>(value)` never expose
  uninitialized safe storage;
- [x] stack and unavailable regional-owner cases fail deterministically;
- [x] the compiler, runtime, sanitizer, dynamic leak, and fuzz suites pass.
