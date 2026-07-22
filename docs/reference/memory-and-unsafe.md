# Memory, Lifetimes, Pointers, and Unsafe Operations

Runes attaches an allocation realm to each function and tracks the provenance
of reference-bearing values. The goal is to make allocation policy visible at
call boundaries while rejecting references that outlive their storage.

## Memory realms

| Function form | Realm | `alloc` behavior | Cleanup model |
|---|---|---|---|
| `f`, `stack f` | stack | owning allocation unavailable | stack lifetime |
| `dynamic f` | raw heap | raw allocation | explicit `raw_free` |
| `regional f` | arena | current call's arena | outer regional tree teardown |
| `gc f` | managed | precise scoped GC heap | tracing at safepoints |
| `flex f` | inherited | caller arena/GC, otherwise raw | inherited |
| root `f main()` | orchestration | raw fallback | may call every realm |

The qualifier controls owning allocation and legal calls. It does not force all
ordinary locals into that storage: primitives and inline aggregates can remain
ordinary automatic values.

### Call matrix

| Caller | Stack | Flex | Dynamic | Regional | GC |
|---|---:|---:|---:|---:|---:|
| stack `f` | yes | yes | no | no | no |
| `regional f` | yes | yes | no | yes | no |
| `gc f` | yes | yes | no | no | yes |
| `dynamic f` | yes | yes | yes | yes | yes |
| root `main` | yes | yes | yes | yes | yes |

Calls violating the matrix are compile-time errors. A flex call is checked
using the caller's effective allocation context.

## Provenance and escape checking

The compiler distinguishes stack, arena, raw, GC, borrowed, external,
inherited, and unknown provenance. Provenance follows pointers, strings,
slices, interfaces, closures, and reference-bearing aggregate fields.

A reference may not escape into a location that can outlive its source. Checks
apply to:

- function results and direct return expressions;
- assignments to outer scopes or globals;
- arrays, tuples, structs, and variant payloads;
- interface conversions;
- slices and subslices;
- borrowing and move closure environments.

Copying a handle does not erase provenance. Wrapping a stack pointer in a
struct, variant, interface, slice, or closure does not make it safe to return.
Inline values that are fully copied do not require a realm conversion.

## Stack storage

Stack functions are appropriate for calculations and borrowed views that do
not allocate owning dynamic storage. `alloc` is rejected in a stack function.

A pointer, slice, interface, string view, or borrowing closure referring to a
local cannot escape the local's lifetime. Taking an address is safe by itself;
using or storing the address remains subject to type and lifetime rules.

## Raw and dynamic allocation

In a dynamic function or root context without an active arena/GC scope,
`alloc(size)` returns raw-owned memory as `*void`. Convert it explicitly before
typed access:

```runes
dynamic f make_value() = result: *i32 {
    unsafe {
        result = alloc(sizeof(i32)) as *i32
        *result = 42
    }
}
```

`raw_alloc(size)` always allocates raw memory, independent of the current
realm. `raw_free(pointer)` releases raw allocation. The owner must arrange
exactly one valid release after the final use; Runes v0.1 has no destructor or
ownership type that performs it automatically.

The backend recognizes `raw_alloc_aligned`, but the default prelude currently
does not declare it. Code using it must provide the matching extern declaration
and ABI contract explicitly. This inconsistency is tracked in
[implementation status](implementation-status.md#prelude-and-runtime-surface).

## Regional arenas

Calling a regional function from outside regional execution creates a root
arena. Calling one regional function from another creates a child arena. A
child remains attached after its call returns, allowing arena-backed values to
flow to the regional parent.

The complete tree is destroyed when the outermost regional invocation exits,
including bare returns, return expressions, and fallible propagation. Arena
cleanup releases memory only; it does not close files, sockets, handles, locks,
or other external resources.

Arena-backed references cannot escape the root regional invocation unless they
are deep-promoted.

## Deep promotion

```runes
*Node raw_copy = promote(&node) as dynamic
*Node gc_copy = promote(&node) as gc
```

Promotion is valid only in a regional ownership context, requires an explicit
`dynamic` or `gc` target, and requires a pointer to a sized arena-backed root.
It is a graph clone, not a cast.

Compiler-generated descriptors traverse owned edges in pointers, arrays,
slices, strings, structs, variants, tuples, interfaces, and closure
environments. A clone map preserves cycles and aliases. Borrowed, raw,
external, MMIO, and already-GC edges remain pointer values and are not claimed
as arena ownership.

Promotion is rejected for nonregional sources, nonpointer/void roots, and
graphs whose borrowed edges would be made invalid by the escape.

## Scoped garbage collection

GC functions use a precise, non-moving mark/sweep heap owned by one OS thread.
GC invocations on that thread share the heap. Collection occurs at allocation
slow paths or an explicit `runes_gc_collect()` call; there is no asynchronous
collector pause in arbitrary non-GC code.

The compiler emits exact descriptors, shadow-stack frames, roots for live
locals/parameters/globals, and transient protection for expressions and return
values. Arena code cannot enter GC execution and therefore cannot hold GC edges
across a GC safepoint.

Current restrictions:

- one owning OS thread;
- no cross-thread GC references;
- no concurrent, incremental, generational, or compacting collection;
- no weak references or finalizers;
- no public `gc_free`;
- no public foreign registered-root API.

GC manages memory, not external resources.

## Flex functions

A flex function inherits its caller's active allocation policy:

- arena allocation when called from regional execution;
- GC allocation and a GC frame when called from GC execution;
- raw allocation when called from root or dynamic execution.

Flex functions are useful for allocation-polymorphic helpers. Their API must
still state the provenance and lifetime of returned reference-bearing values.

## Pointers

`*T` excludes null. `?*T` admits `null`. A non-null pointer widens safely to a
nullable pointer. `unwrap(nullable)` checks for null and returns `*T`; null
traps.

Pointer copying copies the address. It does not copy, own, or free the pointee.
Nullable pointers cannot be dereferenced or used in arithmetic.

Taking `&storage` is safe for assignable storage. Dereferencing requires an
unsafe block:

```runes
i32 value = 42
*i32 pointer = &value
unsafe { *pointer = 43 }
```

`*void` represents an untyped foreign/allocation pointer. Nested pointer types
such as `**Node` are supported.

## Pointer arithmetic and casts

Inside `unsafe`, the supported arithmetic forms are pointer plus integer,
integer plus pointer, and pointer minus integer. The integer is an element
count, not a byte count. Pointer-pointer subtraction and other arithmetic forms
are rejected.

The programmer must ensure the computed pointer remains within a valid object
or allowed one-past position before any use. Bounds are not recovered merely
because a pointer originated from an array.

Integer-to-pointer construction and unrelated pointer reinterpretation require
`unsafe`. Zero cannot construct a non-null pointer. Casts do not extend
lifetime, establish alignment, initialize storage, or prove that the pointee
has the destination type.

## Slices from raw pointers

```runes
unsafe {
    []u8 writable = slice(pointer, length)
    []const u8 readonly = const_slice(pointer, length)
}
```

Both constructors require a non-null pointer and integer length. They establish
a bounds-checked view but cannot prove that the foreign allocation is alive,
aligned, initialized, or at least that long. The slice retains its pointer's
provenance and lifetime.

## Unsafe operations

The following require a lexical `unsafe { ... }` block:

- pointer dereference;
- pointer arithmetic;
- volatile/MMIO access through pointers;
- integer-to-pointer construction and unsafe pointer reinterpretation;
- ordinary extern calls not marked `#[safe]` and not trusted runtime contracts;
- `slice` and `const_slice` raw view construction;
- direct string `.ptr` access;
- byte-pointer/array-pointer to `str` conversion;
- inline assembly.

`unsafe` grants access to these operations. It does not disable type checking,
null checks, arithmetic checks, slice bounds, string-boundary checks, realm
rules, or escape checking.

Keep unsafe blocks small and document the facts the compiler cannot verify:
allocation size, alignment, initialization, aliasing, lifetime, foreign ABI,
device semantics, or assembly effects.

## Volatile and MMIO

`volatile` can qualify variables and struct fields and can form a volatile
access expression. The backend preserves volatile loads/stores. Pointer-based
volatile/MMIO access remains unsafe because the compiler cannot prove the
address or device contract.

Volatile is neither atomic nor a memory-ordering primitive. It does not make
shared-memory concurrency safe.
