# Memory Realms

## Why Runes puts a realm on a function

Programs create values in memory. Different programs need different lifetime
and performance tradeoffs: short-lived stack locals, individually freed heap
objects, arena batches, or garbage-collected graphs.

Runes records that choice on the function:

```runes
f local_work() {}
dynamic f owned_heap_work() {}
regional f temporary_batch() {}
gc f managed_graph() {}
flex f reusable_helper() {}
```

This makes allocation behavior visible at call boundaries. It does **not** mean
every value inside a function is dynamically allocated. Integers, arrays,
structs, tuples, and other ordinary locals can still live inline on the stack.

`alloc(size)` is the standard realm-sensitive allocation operation. It returns
`*void`, normally cast to the required typed pointer inside `unsafe`. The same
call selects raw, arena, or GC storage from the active realm. See the
[complete `alloc()` reference](../reference/allocation.md) and the
[executable realm examples](../examples/README.md#memory-realm-examples).

## The realms

| Function form | `alloc` destination | Typical lifetime/cleanup |
|---|---|---|
| `f` or `stack f` | unavailable | local stack values live until return |
| `dynamic f` | raw heap | owner calls `raw_free` |
| `regional f` | new or child arena | entire regional tree freed together |
| `gc f` | shared scoped GC heap | unreachable objects reclaimed at safepoints |
| `flex f` | caller's arena/GC, otherwise raw allocation | inherited from caller |
| root `main` | orchestration/raw fallback | may call every realm |

`f` and `stack f` are equivalent in v0.1. Writing `stack f` can emphasize an
API boundary, but it does not create a different runtime mechanism.

## Stack functions

Use `f` for calculations and borrowed transformations that do not create
owning dynamic storage:

```runes
f sum(values: []const i32) = result: i32 {
    result = 0
    for (values) |value| {
        result = result + value
    }
}
```

Stack functions may call stack and flex functions. `alloc` is rejected inside
a stack function. A pointer, slice, interface, or borrowing closure cannot be
returned when it references the function's local stack storage.

## Dynamic functions

Use `dynamic f` for manually owned heap storage:

```runes
dynamic f make_value() = result: *i32 {
    unsafe {
        result = alloc(sizeof(i32)) as *i32
        *result = 42
    }
}
```

With no active arena or GC scope, `alloc` uses raw heap behavior. The program
must eventually release owned memory through the corresponding raw-free policy.
Pointers do not carry an automatic destructor, so ownership must be explicit in
the API and documentation.

`raw_alloc` always uses raw memory regardless of the current realm, and
`raw_free` releases it. The backend also recognizes `raw_alloc_aligned`, but
the default prelude does not currently declare it; programs using it must
provide the matching extern declaration explicitly.

## Regional functions and arenas

An arena allocates many objects cheaply and releases them as a group. Calling a
root `regional f` creates an arena:

```runes
regional f build_request() {
    -- alloc(...) uses this call's arena
}
```

A regional function called by another regional function creates a child arena.
When the child returns, that child stays attached to the parent regional tree.
This permits the child to return arena-backed data to the regional parent.

The whole tree is destroyed when the outermost regional call exits, including
early returns and fallible exits. Returning arena-backed data from the root
regional call into `main`, a dynamic function, or GC code is rejected unless it
is explicitly promoted.

Arena cleanup releases memory only. It does not close files, sockets, locks,
windows, or other operating-system resources.

## Deep promotion

Promotion copies an arena-owned graph into a longer-lived realm:

```runes
promote(&value) as dynamic
promote(&value) as gc
```

Promotion is a deep clone, not a pointer cast. Compiler-generated type
descriptors traverse structs, variants, arrays, slices, strings, interfaces,
and closure environments. A clone map preserves shared references and cycles.

Only arena-owned data needs or permits promotion. Inline values already return
by value. Borrowed, external, raw, and GC edges are not recursively claimed as
arena ownership.

The source must be a pointer to a sized value so the runtime knows what graph
root to clone. Promotion is valid only from within a regional function.

## Garbage-collected functions

`gc f` uses one precise, non-moving mark/sweep heap per process. Any number of
GC function calls on its owning OS thread share that heap.

```runes
gc f build_graph() {
    -- typed alloc(...) objects are traced
}
```

Collection is cooperative and synchronous. It happens only on a GC allocation
slow path or an explicit `runes_gc_collect()` call. Code outside GC execution
does not receive hidden periodic polling.

The compiler emits type descriptors and shadow-stack roots for live locals,
parameters, globals, aggregates, interfaces, closures, and protected temporary
or return values.

Current GC limitations:

- one owning OS thread;
- GC references cannot cross threads;
- no concurrent marking or compaction;
- no generations, weak references, or finalizers;
- no public `gc_free`;
- regional code cannot call GC code, so arenas are not GC root containers.

Runtime diagnostics include `runes_gc_collect`,
`runes_gc_debug_object_count`, and `runes_gc_debug_collection_count` through
the prelude.

## Flex functions

A `flex f` follows its caller's active allocation context:

- called from regional code, allocation uses the caller's arena tree;
- called from GC code, allocation uses the GC heap and enters a GC frame;
- otherwise allocation falls back to raw behavior.

This is useful for reusable algorithms and standard-library helpers that should
work in several ownership contexts. It also means their returned ownership and
lifetime must be documented carefully.

## Legal calls

| Caller | May call |
|---|---|
| stack `f` | stack, flex |
| `regional f` | stack, flex, regional |
| `gc f` | stack, flex, GC |
| `dynamic f` | all implemented realms |
| root `main` | all implemented realms |

The compiler rejects illegal calls. In particular, regional code cannot enter
GC execution, and stack code cannot silently enter an owning realm.

## Choosing a realm

Use this order:

1. Choose ordinary `f` when no owned allocation is required.
2. Choose `regional f` when many related values can die together.
3. Choose `dynamic f` when ownership and individual cleanup are explicit.
4. Choose `gc f` for complex shared/cyclic graphs where tracing is worthwhile.
5. Choose `flex f` for a helper intentionally designed to inherit its caller's
   allocation policy.

Do not choose GC merely to avoid designing resource cleanup. GC manages memory,
not file descriptors or other external resources.

## Common mistakes

- Assuming every `f` value is heap allocated or every `gc f` value is on the
  GC heap. Ordinary locals can remain inline.
- Returning a root-arena pointer without promotion.
- Using `raw_alloc` in an arena and expecting arena cleanup to free it.
- Treating promotion as a cheap cast; it may copy an entire graph.
- Sending GC references to another OS thread.
- Expecting GC or arenas to close external resources.

[Next: Pointers, unsafe code, FFI, and input](07-unsafe-ffi-and-io.md)
