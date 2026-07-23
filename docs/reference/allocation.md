# Allocation with `alloc()`

`alloc()` is the standard realm-sensitive allocation operation in Runes v0.1.
It uses the active function realm to select ownership and cleanup behavior.

The default prelude declares:

```runes
extern f alloc(size: usize) = result: *void
```

Although it has function-call syntax, the compiler recognizes `alloc`
specially for realm checking, provenance tracking, code generation, GC
descriptors, and runtime selection. It is not a lexical keyword.

## Realm behavior

| Calling context | Allocation selected | Owner and cleanup |
|---|---|---|
| `stack f` or plain non-root `f` | Rejected | Stack functions cannot call `alloc()` directly |
| `dynamic f` | Raw heap | Program must eventually call `raw_free()` |
| `regional f` | Current call's arena | Outermost regional tree frees it automatically |
| `gc f` | Traced GC heap | Collector reclaims it after it becomes unreachable |
| `flex f` | Caller's active arena/GC; otherwise raw | Determined by effective caller |
| root `f main()` | Raw fallback | Program must eventually call `raw_free()` |

This selection is automatic. There is no separate `arena_alloc()` or
`gc_alloc()` in ordinary language code.

## Basic typed allocation

`alloc()` accepts a byte count and returns `*void`. Convert the result to a
typed pointer before using it:

```runes
type Box = { value: i32 }

dynamic f make_box(value: i32) = result: *Box {
    unsafe {
        result = alloc(sizeof(Box)) as *Box
        result.value = value
    }
}
```

The cast and dereference require `unsafe`. The compiler knows the resulting
pointer's realm provenance, but the programmer remains responsible for:

- requesting enough bytes;
- using suitable alignment for the destination type;
- initializing values before reading them;
- avoiding invalid aliasing and pointer arithmetic;
- obeying the cleanup policy selected by the realm.

`alloc(0)` and allocation-failure behavior follow the checked runtime contract;
they are not a portable way to manufacture a usable zero-length object.

## Dynamic allocation

In a dynamic function, `alloc()` creates raw-owned storage:

```runes
dynamic f calculate() = result: i32 {
    unsafe {
        *i32 value = alloc(sizeof(i32)) as *i32
        *value = 42
        result = *value
        raw_free(value as *void)
    }
}
```

Pointers do not have destructors. Every raw allocation needs one valid
`raw_free()` after its final use. Returning the pointer transfers that
ownership obligation by convention; the type itself does not encode the
transfer.

See the executable
[`memory-dynamic.runes`](../examples/positive/memory-dynamic.runes) example.

## Regional allocation

In a regional function, `alloc()` uses that invocation's arena:

```runes
regional f calculate() = result: i32 {
    unsafe {
        *i32 value = alloc(sizeof(i32)) as *i32
        *value = 42
        result = *value
    }
}
```

Do not call `raw_free()` on an arena allocation. Nested regional calls create
child arenas that remain attached to their parent. The complete tree is
destroyed automatically when the outermost regional call exits.

An arena-backed pointer cannot escape that root call. To preserve an owned
graph beyond the arena lifetime, use:

```runes
promote(pointer) as dynamic
promote(pointer) as gc
```

Promotion deep-clones eligible arena-owned edges; it is not an allocation cast.

See the executable
[`memory-regional.runes`](../examples/positive/memory-regional.runes) example.

## GC allocation

In a GC function, `alloc()` creates a traced object:

```runes
gc f make_node() = result: *Node {
    unsafe {
        result = alloc(sizeof(Node)) as *Node
        result.value = 42
        result.next = null
    }
}
```

Do not call `raw_free()` on GC storage. The compiler emits type descriptors and
roots for typed pointers, aggregates, interfaces, closures, temporary values,
and returns. Collection occurs on allocation slow paths or an explicit
`runes_gc_collect()` call.

The allocation must be converted to its correct typed pointer so the compiler
and runtime can trace its pointer-bearing fields accurately. GC memory still
does not manage files, sockets, locks, or other external resources.

See the executable
[`memory-gc.runes`](../examples/positive/memory-gc.runes) example.

## Stack functions

A stack function cannot call `alloc()` directly:

```runes
stack f invalid() = result: *i32 {
    unsafe { result = alloc(sizeof(i32)) as *i32 }
}
```

The compiler reports:

```text
alloc is not available in a stack function
```

Stack functions use inline local values and borrowed views. This keeps direct
owning allocation out of their implementation.

The rejection is covered by
[`stack-alloc.runes`](../examples/negative/stack-alloc.runes), while
[`memory-stack.runes`](../examples/positive/memory-stack.runes) demonstrates
valid inline stack storage.

## Flex allocation

A flex function inherits the effective caller's allocator:

```runes
flex f make_cell(value: i32) = result: *Cell {
    unsafe {
        result = alloc(sizeof(Cell)) as *Cell
        result.value = value
    }
}
```

- a regional caller receives an arena-backed pointer;
- a GC caller receives a traced pointer;
- a dynamic or root caller receives a raw-owned pointer.

Stack functions may currently call flex functions. With no arena or GC active,
an allocating flex function falls back to raw ownership, so stack callers must
eventually use `raw_free()`. This is implemented behavior, but it weakens the
simple intuition that stack functions never cause owning allocation and remains
a language-design question.

See [`memory-flex.runes`](../examples/positive/memory-flex.runes) for all four
caller cases.

## `alloc()` versus raw allocation

| Operation | Selection | Intended use |
|---|---|---|
| `alloc(size)` | Active realm | Normal realm-aware allocation |
| `raw_alloc(size)` | Always raw | Explicit low-level ownership escape hatch |
| `raw_alloc_aligned(size, align)` | Always raw | Explicit aligned raw allocation; not in default prelude |
| `raw_free(pointer)` | Raw only | Release a raw allocation |

Application code should prefer `alloc()` when allocation should follow the
function's declared memory policy. `raw_alloc()` is appropriate when raw
ownership is deliberately required regardless of the active realm.

Never pass arena or GC storage to `raw_free()`. Never assume `raw_alloc()` will
be cleaned up by an arena or collector.

## Allocation and API design

A function returning raw-owned memory should document who calls `raw_free()`.
A regional API should avoid returning arena references beyond the regional
root unless it promotes them. A GC API should not expose GC references across
OS threads. A flex API must document that the returned ownership changes with
its caller.

The realm qualifier is therefore part of the allocation contract, not merely
an optimization hint.
