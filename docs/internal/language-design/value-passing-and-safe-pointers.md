# Value Passing, Zero-Copy Matching, and Safe Pointers

Status: open design questions; no new syntax or semantics are approved here.

## Why this note exists

The first `Option<T>` implementation uses value receivers and value patterns:

```runes
method Option<T> {
    f is_some(self) = result: bool {
        result = match self {
            Some(_) -> true,
            None -> false,
        }
    }

    f unwrap(self) = result: !T {
        result = match self {
            Some(value) -> value,
            None -> error.UnwrapError.NoValueError,
        }
    }
}
```

This raised two related-looking but actually separate questions:

1. How can read-only operations avoid copying a potentially large `T`?
2. Can Runes have safe pointers without adopting Rust's borrowing model?

They must be answered separately. Eliminating an unnecessary compiler-generated
copy does not require a public borrowing API or safe pointer system.

## What the compiler does today

Runes variants copy by value. A method receiver written as `self` uses the
owning type, so the receiver is also semantically passed by value.

The current C backend additionally materializes the subject of every `match`
into a temporary in `emit_match`:

```c
Option_T temporary = subject;
```

For a large payload, an `is_some` call can therefore copy the option at the
call boundary and again before inspecting its tag. A binding such as
`Some(value)` may copy the payload as well. An optimizing C compiler may remove
some copies, but the Runes compiler should not depend on that for basic
aggregate performance.

## Question 1: should read-only value receivers use hidden indirection?

Candidate direction:

- keep ordinary Runes source written with `self`;
- preserve value semantics at the language level;
- allow the ABI to pass a large, read-only value through a hidden pointer;
- prevent the callee from mutating the caller's value through that hidden
  implementation detail;
- pass small values in registers when appropriate.

Under that design:

```runes
present.is_some()
```

could lower approximately to:

```c
bool Option_is_some(const Option_T *self) {
    return self->tag == OPTION_SOME;
}
```

There is no pointer in the Runes API. This is an ABI optimization, not a
reference feature.

The optimization is valid only while it preserves observable value semantics.
Aliasing, callbacks, concurrency, interior mutation, address identity, and
future destructor behavior must be considered before treating hidden
indirection as universally equivalent to a copy.

## Question 2: should `match` inspect an existing place directly?

The backend should distinguish an addressable value from a temporary
expression.

For an addressable, read-only subject:

```runes
match self {
    Some(_) -> true,
    None -> false,
}
```

the compiler can inspect `self` in place. `Some(_)` needs only the variant tag
and should never copy the payload.

For a temporary subject, the compiler may materialize it once so every arm sees
one stable value.

Pattern semantics still matter:

- `Some(_)` does not request the payload;
- `Some(value)` currently requests an ordinary value binding and may therefore
  copy `T`;
- extracting ownership without copying requires a separate move/consumption
  decision;
- referring directly to payload storage would be a reference feature and must
  not be smuggled in as an optimization.

The first implementation target should be zero-copy tag inspection. It can
improve `is_some` and `is_none` without deciding the entire ownership model.

## Question 3: what should `unwrap` mean?

`unwrap` and `is_some` have different requirements.

`is_some` only observes the tag. It can be implemented without copying the
payload.

`unwrap` returns a `T`:

```runes
f unwrap(self) = result: !T
```

Returning a pointer would change its meaning from owned extraction to
reference access. The standard library should not make that change merely to
avoid a copy.

The open question is whether owned extraction:

- copies the payload;
- consumes the option and moves the payload;
- uses copy for copyable types and move for non-copyable types.

That decision belongs to Runes value and move semantics. A method named
`borrow`, or a borrowed `unwrap`, is not currently desired.

## Question 4: can pointers be safe without Rust-style borrowing?

The phrase “safe pointer” is incomplete. At minimum, pointer safety may include:

- non-nullness;
- temporal validity: the target is still alive;
- spatial validity: access remains within the target allocation;
- alignment and type validity;
- mutation permission;
- thread validity;
- compatibility with regional cleanup and garbage collection.

Rust answers many of these questions with statically checked ownership,
lifetimes, and aliasing. Renaming the same rules “provenance checking” would
still amount to importing the same underlying model. Runes should not do that
accidentally.

Alternatives that do not expose Rust-style borrowing include:

| Model | Basic rule | Main cost |
|---|---|---|
| Raw pointers only | `*T` remains low-level and dereference stays `unsafe` | Safe code cannot directly use arbitrary pointers |
| Runtime-checked handles | Pointer-like values carry bounds and liveness information | Larger values and a check on access |
| GC references | Managed objects remain alive while reachable | Collector/runtime cost and restricted low-level control |
| Region-bound references | Every reference remains valid until its whole region ends | Coarse lifetime and retained memory |
| Stable indices/handles | APIs expose IDs into managed storage rather than addresses | Lookup cost and container-specific access |

These approaches may coexist. For example, normal application code could use
values, regional objects, GC-managed objects, slices, and stable handles while
FFI, allocators, drivers, and kernels use raw pointers inside `unsafe`.

No option in this table has been selected as the universal Runes pointer model.

## Current working direction

The narrow direction worth implementing first is:

1. Do not add `borrow()` to `Option<T>`.
2. Do not change `unwrap()` into a pointer-returning operation.
3. Keep the current value-oriented Runes API.
4. Make `match` avoid copying an addressable subject when only observation is
   required.
5. Allow the ABI to pass large read-only values indirectly when doing so
   preserves value semantics.
6. Design consuming extraction and moves separately.
7. Keep raw pointer safety as an explicit open language decision.

This solves the concrete performance problem without committing Runes to a
Rust-shaped public type system.

## Questions that must be answered before implementation

### Value and ABI questions

- What size or type classification triggers indirect argument passing?
- Is the choice a documented ABI guarantee or a private backend decision?
- Can a method callback observe mutation of an aliased original value?
- Will Runes support interior mutability or destructors?
- Does taking the address of a value give it observable identity?

### Match questions

- Which expressions count as stable addressable places?
- How are guards handled without changing evaluation order?
- Does `Some(value)` copy, move, or depend on `T`?
- How are GC roots registered when a match reads an existing value in place?
- Can nested matches share the same storage safely?

### Pointer questions

- Is `*T` always raw, or can it represent both checked and unchecked addresses?
- If checked pointers exist, are checks static, runtime, realm-wide, or
  collector-backed?
- How are external and memory-mapped addresses introduced?
- How are pointer mutation and thread sharing controlled?
- Which pointer operations remain permanently inside `unsafe`?

## Minimum validation for the zero-copy work

The implementation should include generated-C or IR assertions proving that:

- `Option<Large>.is_some()` does not copy `Large`;
- `Some(_)` reads only the tag;
- a temporary match subject is evaluated exactly once;
- `Some(value)` retains its documented copy or move behavior;
- guards preserve source evaluation order;
- stack, regional, dynamic, and GC values remain correctly rooted and cleaned;
- debug and optimized builds have identical language behavior.

Only after those tests pass should the stdlib describe `Option` inspection as
zero-copy.
