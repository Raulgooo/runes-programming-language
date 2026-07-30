# Understanding Runes by Building Its Standard Library

This guide is for the point where you can recognize Runes syntax but do not yet
feel confident deciding what belongs in a type, how `match` should be used, when
a generic is appropriate, or how a standard library grows without becoming a
pile of unrelated functions.

You do not need to already be a standard-library expert. The goal is to build
the mental model needed to make one small, defensible decision at a time.

This is an explanatory companion to the ordered
[application-readiness plan](app-readiness-plan.md). The plan says **what to
build next**. This guide explains **how to think about it and why the pieces
have that shape**.

Runes is experimental. Examples in this guide describe current v0.1 behavior
unless a section is explicitly labeled as a proposed library design.

## 1. The two systems you are building

It helps to separate two things that are easy to mix together.

### The language

The language is the set of rules enforced by the compiler:

- which source text parses;
- which values have which types;
- which operations are legal on those types;
- which `match` expressions are exhaustive;
- how generic code is specialized;
- which functions may call which memory realms;
- whether a pointer or slice may outlive its storage;
- what code is emitted and what the runtime must do.

Changing any of those usually means changing the parser, resolver, type
checker, code generator, runtime, specification, and tests.

For example, adding `alloc_array<T>(count)` as a compiler-supported operation is
language/runtime work. A normal Runes module cannot by itself invent the
compiler metadata needed to trace arbitrary `T` correctly in the GC.

### The standard library

The standard library is ordinary, privileged-by-convention Runes code built on
top of those language rules:

- `Option<T>` gives a standard name and API to “a value may be absent”;
- byte utilities put safe bounds and error behavior around loops;
- `Vec<T>` maintains growable-storage invariants;
- `String` maintains both storage invariants and valid UTF-8;
- `std.io` turns unsafe operating-system calls into safe slice-based APIs.

The library cannot overrule the language. It can combine language features into
reusable policies.

One useful test is:

> If this feature needs new syntax, new type-checking knowledge, new provenance
> rules, or new runtime metadata, it is probably language work. If it can be
> expressed and tested as a normal `.runes` module, it is probably library
> work.

Some designs need both. `Vec<T>` is library code, but its safe typed allocation
layer required compiler and runtime support so the implementation would not
repeat fragile size, alignment, cast, and GC-tracing logic.

## 2. What happens to a Runes program

A simplified compiler pipeline is:

```text
source text
    |
    v
tokens and syntax tree
    |
    v
name/module resolution
    |
    v
types, constraints, realms, and lifetime checks
    |
    v
concrete generic instantiations
    |
    v
C11 code plus runtime metadata
    |
    v
native executable
```

This matters to library design because an API is not only a function body. Its
signature participates in resolution and type checking before its body runs.

Consider:

```runes
f first_or(values: []const i32, fallback: i32) = result: i32 {
    if values.len == 0 {
        result = fallback
    } else {
        result = values[0]
    }
}
```

The signature already promises a great deal:

- the function does not own the elements;
- it cannot mutate them;
- the slice carries its length;
- it does not allocate because it is a stack `f`;
- absence is converted into the caller-provided fallback;
- it always returns an `i32`.

Good library design places as much truth as practical in the signature. The
body should fulfill that contract, not secretly redefine it.

## 3. Types are descriptions of possible states

A type tells the compiler which values may exist and which operations make
sense. For library design, the more important question is:

> Which states does this type permit, and are all of them meaningful?

### Primitive values

Use primitives for values that really are numbers, booleans, characters, or
borrowed text:

```runes
u32 port = 8080
bool connected = false
char separator = '/'
str label = "server"
```

Do not use a primitive sentinel when the state has a name. A lookup returning
`usize` with `usize` maximum meaning “not found” forces every caller to remember
a hidden convention. `Option<usize>` makes absence part of the type.

### Arrays own inline elements

```runes
[4096]u8 buffer = []
```

The array contains exactly 4096 bytes inside its own value. Its length is known
as part of its type, and copying the array copies all elements.

Arrays are useful early in the stdlib because they require no allocator and no
cleanup policy.

### Slices borrow elements

```runes
[]u8 writable = buffer[..]
[]const u8 readable = buffer[..]
```

A slice is a pointer-and-length view. It does not own the bytes. This is why a
safe I/O API should accept:

```runes
f read_into(fd: i32, destination: []u8) = result: !ReadResult
```

instead of:

```runes
f dangerous_read(fd: i32, destination: *u8, count: usize) = result: i64
```

The slice keeps the pointer and length together and communicates that the
function borrows caller-owned storage.

### Structs express “all of these fields”

```runes
type SourceLocation = {
    line: u32
    column: u32
}
```

A valid `SourceLocation` has both a line and a column. Structs model product
types: `A and B and C`.

Use a struct when the fields belong together and their names explain the
meaning better than tuple positions.

### Variants express “exactly one of these cases”

```runes
type ReadResult =
    | Eof
    | Read(usize)
```

A `ReadResult` is either `Eof` or `Read(count)`, never both. Variants model sum
types: `A or B or C`.

Use a variant when the program has a closed set of meaningfully different
states. Do not encode those states as a boolean plus fields that are valid only
when the boolean has a particular value.

Compare:

```runes
type WeakReadResult = {
    eof: bool
    count: usize
}
```

This representation permits confusing states such as `eof: true` with a
nonzero count. The variant removes those states:

```runes
type ReadResult =
    | Eof
    | Read(usize)
```

That is a core library-design technique: **make invalid states difficult or
impossible to construct**.

### Errors express failure, not ordinary alternatives

```runes
error IoError = {
    | ReadFailed
    | InvalidForeignResult
}
```

`ReadResult.Eof` is not an error. Reaching the end of a file is an ordinary
outcome of reading. A failed operating-system operation is an error.

Therefore:

```runes
!ReadResult
```

means:

```text
success: Eof or Read(count)
failure: IoError
```

This separation makes caller behavior clearer:

- match the success variant when every case is normal;
- use `try` when the current function cannot handle a failure;
- use `catch` when a meaningful fallback exists;
- match `Ok(...)` and `Err(...)` when both paths need local handling.

### Pointers and owning containers need an ownership story

`*T` says there is a non-null address of a `T`. It does not, by itself, answer:

- who allocated it;
- who frees it;
- how long it remains valid;
- whether other aliases exist;
- whether it points into raw, arena, GC, stack, or external storage.

Runes tracks provenance, hidden owner-realm type identity, and realm-sensitive
methods. The implemented vector therefore keeps one public name:

```text
Vec<T> in dynamic execution  raw storage; deinit frees backing memory
Vec<T> in regional execution arena storage; deinit relinquishes the handle
Vec<T> in GC execution       traced storage; deinit clears traced elements
```

The caller never writes a realm argument or allocator. The compiler preserves
the vector's inferred owner realm and selects growth and release from that
owner. Documentation must still explain that `deinit` has realm-specific
memory effects and does not clean up external resources stored in elements.

## 4. `match` is structured case analysis

`match` is best understood as answering:

> Which shape does this value have, and what names should I give the pieces in
> that shape?

Given:

```runes
type Option<T> =
    | None
    | Some(T)
```

you can inspect it:

```runes
f unwrap_or<T>(value: Option<T>, fallback: T) = result: T {
    result = match value {
        Some(inner) -> inner,
        None -> fallback,
    }
}
```

`Some(inner)` does two things:

1. it checks that the variant is `Some`;
2. it binds the payload to the local name `inner`.

`None` has no payload to bind.

### Matching as a statement

Use a statement match when each arm performs an effect:

```runes
f describe(result: ReadResult) {
    match result {
        Eof -> print("end of input"),
        Read(count) -> print("read ", count, " bytes"),
    }
}
```

### Matching as an expression

Use an expression match when each arm computes the same result type:

```runes
f count_or_zero(result: ReadResult) = answer: usize {
    answer = match result {
        Eof -> 0,
        Read(count) -> count,
    }
}
```

Every reachable arm must produce a compatible value. In the current C backend,
assign a value-producing `match` to the named result instead of placing it
directly inside `return`.

### Exhaustiveness is future-proofing

For variants, the compiler checks that every possible arm is handled:

```runes
type ParseState =
    | Empty
    | Valid(i32)
    | Invalid
```

If code handles only `Empty` and `Valid`, compilation fails. This is useful:
when a library author later adds or changes a state, affected internal code is
found by the compiler instead of silently doing the wrong thing.

Be deliberate with `_`. A wildcard is useful when all remaining cases truly
have identical meaning. It can also hide the fact that a newly added variant
deserves new behavior.

### Guards refine a pattern

```runes
str category = match result {
    Read(count) if count == 0 -> "impossible zero read",
    Read(count) if count < 1024 -> "small read",
    Read(_) -> "large read",
    Eof -> "end",
}
```

A guard does not cover the whole variant. `Read(count) if count < 1024` still
needs another `Read(...)` arm.

### Matching is not a replacement for every `if`

Use `if` for a boolean choice:

```runes
if buffer.len == 0 {
    return
}
```

Use `match` when the shape of data matters:

```runes
match option {
    Some(value) -> use(value),
    None -> use_default(),
}
```

The distinction is not a law, but it keeps code legible.

## 5. Generics are reusable blueprints

A generic declaration describes a family of declarations:

```runes
type Pair<A, B> = {
    first: A
    second: B
}
```

`Pair<i32, bool>` and `Pair<str, usize>` are different concrete types built
from the same blueprint.

### Current Runes uses monomorphization

For every concrete generic use, the compiler creates a specialized
instantiation before final resolution and type checking:

```runes
f identity<T>(value: T) = result: T {
    result = value
}

i32 number = identity<i32>(42)
bool ready = identity<bool>(true)
```

Conceptually, the compiler produces an integer identity and a boolean identity.
There is no runtime “what type is `T`?” lookup and no runtime generic erasure.

This gives efficient concrete code, but it also means:

- each used instantiation must type-check;
- many instantiations can increase generated code size;
- generic implementation cannot assume operations that were never promised;
- Runes currently has no specialization, const generics, variance,
  higher-kinded types, or generic union types such as `u32 or u64`.

### An unconstrained `T` promises almost nothing

This is valid because copying and returning `T` requires no special behavior:

```runes
f identity<T>(value: T) = result: T {
    result = value
}
```

This is not valid merely because some types happen to have `hash()`:

```runes
f hash_anything<T>(value: T) = result: u64 {
    result = value.hash()
}
```

The function signature never required `T` to be hashable.

### Interfaces state required behavior

```runes
interface Hash {
    f hash(self) = result: u64
}

f hash_value<T: Hash>(value: T) = result: u64 {
    result = value.hash()
}
```

The constraint is a promise in both directions:

- the caller promises that its concrete `T` implements `Hash`;
- the generic function promises to rely only on behavior provided by `Hash`
  and ordinary operations valid for `T`.

### Generic constraint or interface value?

These solve related but different problems.

Use a constrained generic when the concrete type can remain known:

```runes
f print_hash<T: Hash>(value: T) {
    print(value.hash())
}
```

The compiler specializes this for each concrete `T`. This is static
polymorphism.

Accept an interface value when different concrete types need one runtime
representation and dynamic dispatch:

```runes
f print_unknown_hash(value: Hash) {
    print(value.hash())
}
```

An interface value carries a data reference and a method-table handle. That has
runtime representation and lifetime consequences.

Use the least powerful mechanism that expresses the requirement:

1. concrete type when only one type is intended;
2. generic type when an algorithm is identical across types;
3. constrained generic when the algorithm needs a small behavior set;
4. interface value when runtime heterogeneity or dynamic replacement is
   actually required.

Do not create an interface merely to make code look architecturally advanced.
An interface earns its place when at least two credible implementations or a
real runtime substitution boundary exist.

## 6. Methods turn representation into an abstraction

A public type is not only its fields. It is the set of operations and
invariants users can rely on.

```runes
type Counter = {
    current: i32
}

method Counter {
    f value(self) = result: i32 {
        result = self.current
    }

    f increment(self: *Counter) {
        unsafe {
            self.current = self.current + 1
        }
    }
}
```

A value receiver reads a copied value. A pointer receiver can mutate the
original storage and therefore requires the relevant pointer operation inside
`unsafe` under current rules.

For a container, methods should protect invariants such as:

```text
0 <= len <= capacity
data is null exactly when the representation permits it
initialized elements occupy data[0..len]
uninitialized capacity is never exposed as initialized T values
the buffer belongs to the container's documented realm
raw memory is released at most once
```

The public API should make the valid operations easy and direct field
corruption unnecessary. Current v0.1 field visibility is incomplete, so the
compiler cannot yet enforce the ideal encapsulation boundary. Treat direct
field access as an implementation-status gap, not as the desired final API.

## 7. A complete small example: designing `Option<T>`

`Option<T>` is an excellent first stdlib type because it needs no allocation
while exercising variants, generics, methods, functions, modules, visibility,
and matching.

### Step 1: write the meaning in plain language

> `Option<T>` contains either one `T` or no value. `None` represents ordinary
> absence, not failure.

If that sentence is unclear, implementation should not begin.

### Step 2: define the states

Current module `src/std/core.runes`:

```runes
pub type Option<T> =
    | None
    | Some(T)
```

There is no invalid third state and no sentinel value.

### Step 3: choose a deliberately small API

```text
is_some
is_none
unwrap_or
unwrap
map
```

Each operation has one clear purpose:

- `is_some`: query presence without extracting;
- `is_none`: query absence;
- `unwrap_or`: turn optionality into one definite value;
- `unwrap`: extract a value or return `UnwrapError.NoValueError`;
- `map`: transform the payload while preserving absence.

### Step 4: implement operations with `match`

The intended shape is:

```runes
method Option<T> {
    f is_some(self) = result: bool {
        result = match self {
            Some(_) -> true,
            None -> false,
        }
    }

    f is_none(self) = result: bool {
        result = match self {
            Some(_) -> false,
            None -> true,
        }
    }

    f unwrap_or(self, fallback: T) = result: T {
        result = match self {
            Some(value) -> value,
            None -> fallback,
        }
    }

    f unwrap(self) = result: !T {
        match self {
            Some(value) -> {
                result = value
            }
            None -> {
                result = error.UnwrapError.NoValueError
            }
        }
    }

    f map<U>(
        self,
        transform: f(T) -> U
    ) = result: Option<U> {
        result = match self {
            Some(value) -> Option.Some<U>(transform(value)),
            None -> Option.None<U>(),
        }
    }
}
```

The implemented code uses explicit constructor type arguments inside generic
`map` (`Option.Some<U>(...)` and `Option.None<U>()`). `unwrap` returns `!T` and
produces `error.UnwrapError.NoValueError` for `None`. Every method handles both
states and allocation remains unnecessary.

### Step 5: test behavior, types, and misuse

Positive behavior:

```runes
use std.core.Option

Option<i32> present = Option.Some(42)
Option<i32> absent = Option.None<i32>()

print(present.is_some())
print(absent.unwrap_or(7))
```

Compile-time behavior:

- `Option<i32>` is distinct from `Option<str>`;
- `Some("text")` cannot initialize `Option<i32>`;
- a match missing `None` is rejected;
- `map` from `i32` to `bool` produces `Option<bool>`;
- the type and its intended operations work after a cross-module import.

Edge behavior:

- mapping `None` does not call the transform;
- `unwrap_or` returns the stored value for `Some`;
- copying `Option<str>` copies the borrowed string view, not owned text.

That test list is part of the design. It reveals the contract more precisely
than a happy-path example alone.

## 8. The library-builder's five-question method

Before implementing any public stdlib API, answer these questions in order.

### 1. What states exist?

Examples:

- lookup: found index or absent;
- read: bytes read, EOF, or failure;
- parser: success with value or typed failure;
- vector: initialized length plus spare capacity.

Choose a type that represents those states directly.

### 2. Who owns every piece of memory?

For every pointer, slice, string view, interface, or closure, write:

- owner;
- allowed lifetime;
- whether mutation is permitted;
- cleanup rule;
- whether returning or storing it can escape.

If the answer is “it depends but the type does not say how,” the API is not
ready.

### 3. What can fail, and what is ordinary absence?

Use:

- `Option<T>` for ordinary absence;
- a variant for multiple normal outcomes;
- `!T` plus a nominal error set for failure;
- a trap only for a violated language/runtime invariant that callers cannot
  reasonably recover from.

Do not use one mechanism for all four.

### 4. What behavior must be generic?

Ask whether the implementation truly repeats unchanged across types.

- `Option<T>` genuinely stores any `T`;
- byte search initially needs only `u8`, so generic search can wait;
- sorting needs an ordering behavior and therefore an interface constraint;
- OS `read` is about bytes and file descriptors, not arbitrary `T`.

Start concrete. Generalize after the common operation is visible.

### 5. What is public, and what remains an implementation detail?

Public surface includes:

- module and declaration names;
- types and signatures;
- errors and normal variants;
- realm and cleanup requirements;
- complexity and allocation behavior;
- documented edge cases.

Private implementation includes:

- growth factor;
- helper loops;
- raw FFI declarations;
- internal capacity rounding;
- runtime bridge details.

An implementation can change. A public semantic promise is much harder to
change once users depend on it.

## 9. Memory realms are part of the API

Runes does not have one universal heap. A function realm answers how allocation
inside that call behaves.

| Realm | `alloc` destination | Library consequence |
|---|---|---|
| stack `f` | unavailable | ideal for borrowed and allocation-free algorithms |
| `dynamic f` | raw heap | returned ownership needs explicit cleanup |
| `regional f` | new/child arena | values die with the regional tree |
| `gc f` | traced GC heap | references must remain traceable |
| `flex f` | compile-time caller specialization | stack rejects owning allocation; other ownership follows the effective caller |

### Prefer allocation-free foundations

These functions can be ordinary stack functions:

```runes
f equal(left: []const u8, right: []const u8) = result: bool
f find(bytes: []const u8, needle: u8) = result: Option<usize>
f starts_with(bytes: []const u8, prefix: []const u8) = result: bool
```

They are easy to call from every realm, easy to test, and make no cleanup
promise.

### Make ownership visible when allocation is essential

For the implemented realm-aware `Vec<T>`:

```runes
use std.vec.Vec

dynamic f dynamic_work() { Vec<i32>.new() }
regional f regional_work() { Vec<i32>.new() }
gc f managed_work() { Vec<i32>.new() }
```

The same `flex` implementation grows all three. A regional vector rejects
growth from a nested child arena rather than changing owner. GC replacement
becomes collectible, while dynamic replacement frees old backing storage.

Normal application code uses the concise operation:

```runes
values := Vec<i32>.new()
values.push(10)
```

When a caller intends to recover from storage failure, prefix that operation
with `t`:

```runes
match Vec<i32>.tnew() {
    Ok(values) -> {
        match values.tpush(10) {
            Ok(_) -> use(values),
            Err(failure) -> recover(failure),
        }
    }
    Err(failure) -> recover(failure),
}
```

The ordinary operation delegates to the `t` implementation and invokes the
portable storage-failure policy on `Err`; the two names must never contain
independent allocation algorithms.

### Memory cleanup is not resource cleanup

Arenas and GC can reclaim memory. They do not automatically:

- close a file;
- unlock a mutex;
- flush a stream;
- close a socket;
- destroy a window or graphics context.

Resource APIs need an explicit lifecycle even in GC code. Runes has lexical
`defer expression`, so callers can schedule `close` or `deinit` at acquisition
time. It still has no move-only values or deterministic destructors:
resource-owning handles remain copyable, and libraries must document
double-close/double-free hazards.

## 10. Building safe boundaries over unsafe primitives

The most valuable stdlib code often does not eliminate danger. It confines and
audits it.

Raw operating-system read:

```runes
extern f read(
    fd: i32,
    destination: *u8,
    count: usize
) = result: i64
```

The OS receives a pointer and count separately. Nothing in that signature
proves the pointer names `count` writable bytes.

Safe public wrapper:

```runes
pub f read_into(
    fd: i32,
    destination: []u8
) = result: !ReadResult
```

Inside the wrapper:

1. obtain pointer and length from the same slice;
2. call the raw function inside `unsafe`;
3. turn negative results into a nominal error;
4. turn zero into `Eof`;
5. reject a foreign result larger than the supplied slice;
6. return the checked count.

The wrapper is safe because its implementation establishes an invariant that
the raw signature cannot express.

Use this pattern throughout the stdlib:

```text
small unsafe primitive
    |
    v
one audited validation boundary
    |
    v
safe types used by ordinary application code
```

Do not sprinkle `unsafe` across convenience APIs. Concentrate it at the lowest
layer that can state and check the complete invariant.

## 11. Modules should follow responsibilities

The first planned layout is:

```text
src/std/
|-- mod.runes
|-- prelude.runes
|-- core.runes
|-- core/
|   `-- option.runes
|-- bytes.runes
|-- io.runes
|-- os.runes
`-- os/
    `-- linux.runes
```

Each layer has a reason to exist:

- `prelude.runes`: compiler/runtime ABI contracts required implicitly;
- `core.option`: universally useful, allocation-free semantic type;
- `bytes`: safe algorithms over borrowed byte views;
- `os.linux`: explicit platform-specific unsafe bindings;
- `io`: portable-looking safe behavior built over the platform bridge.

Dependency direction should remain simple:

```text
application
    |
    v
std.io  --->  std.bytes / std.core
    |
    v
std.os.linux
    |
    v
runtime / operating system
```

Higher layers may depend on lower layers. The raw OS layer should not import
the friendly I/O layer. `Option` should not depend on `Vec`. Cycles at the
foundation usually indicate that responsibilities are mixed.

### Import policy today

Runes can import one public final member:

```runes
use std.bytes.find
```

Explicit aliases are implemented. Grouped imports, wildcard imports, and
public re-exports are not. Methods remain receiver-based; import the type and
call `value.method()`.

Generic declarations can be imported directly or given an explicit local
alias:

```runes
use std.core.Option as Maybe

Maybe<i32> value = Maybe.Some(42)
```

The alias does not create a distinct type or generic specialization.

Design module paths with the actual current syntax, not with conveniences that
the compiler does not yet provide.

## 12. How to decide what belongs in the first release

The first version of a module should solve one real use case with the smallest
coherent API.

For `Option<T>`:

```text
Some / None
is_some / is_none
unwrap_or / unwrap / map
```

For bytes:

```text
fill / copy / equal / find / starts_with
```

For `Vec<T>`:

```text
new / tnew / with_capacity / twith_capacity
len / capacity
as_slice / as_mut_slice
push / tpush / pop / tpop / reserve / treserve
clear / tclear / truncate / ttruncate / deinit
```

Every added operation creates permanent work:

- semantics to define;
- names to keep consistent;
- behavior and misuse to test;
- realm behavior to verify;
- documentation to maintain;
- compatibility decisions later.

“Maybe useful” is not enough for a foundation API. Add an operation when a
real program needs it and its contract can be stated precisely.

## 13. Testing is executable semantics

A stdlib test suite should not ask only “does it compile?”

### Positive behavior

Verify outputs and state changes:

- `Some(42).unwrap_or(7)` returns 42;
- `None.unwrap_or(7)` returns 7;
- pushing beyond capacity preserves previous vector elements;
- `read_into` distinguishes input, partial read, and EOF.

### Boundary behavior

Test:

- empty slices;
- zero capacity;
- first and last valid indexes;
- capacity growth near arithmetic limits;
- invalid UTF-8;
- empty input and repeated EOF;
- cleanup after early failure.

### Negative compile tests

The compiler should reject:

- mutation through `[]const T`;
- mismatched generic payloads;
- non-exhaustive variant matches;
- arena-backed data escaping its region;
- illegal cross-realm calls;
- freeing arena or GC memory as raw storage.

These tests verify that the API and language jointly prevent misuse.

### Ownership tests

For raw containers:

- run under address/leak sanitizers;
- test empty and populated `deinit`;
- test every failure path after partial initialization;
- define whether repeated `deinit` is supported or forbidden;
- verify that elements survive every reallocation.

For arena containers:

- verify no path calls `raw_free`;
- test parent/child regional use and escape rejection.

For GC containers:

- force collection while the container is live;
- verify pointer-bearing elements remain reachable;
- remove references and verify objects become collectible where diagnostics
  permit it.

### Integration tests

A module can be correct alone but awkward in a program. Build one small
application at the end of a group of milestones. That reveals missing
conversions, imports, errors, or ownership operations.

The first planned integration programs are:

1. a stdin byte counter;
2. a small compiler-oriented CLI using bytes, owned text, errors, and vectors.

## 14. The implementation loop

Use this loop for each public feature.

### Step 1: write one user story

Example:

> A caller searches a borrowed byte slice and receives either the found index
> or ordinary absence without allocation.

### Step 2: write the signature before the body

```runes
pub f find(
    bytes: []const u8,
    needle: u8
) = result: Option<usize>
```

Check what the signature says:

- borrowed read-only input;
- concrete byte search;
- no allocation;
- ordinary absence, not failure;
- no unnecessary interface.

### Step 3: list invariants and edge cases

```text
empty input -> None
first match -> Some(0)
last match -> Some(len - 1)
duplicate matches -> first index
absent -> None
input remains unchanged
```

### Step 4: write tests first or alongside the implementation

At minimum, add:

- one ordinary positive test;
- every meaningful boundary;
- one important expected compiler rejection if the API relies on static safety.

### Step 5: implement the simplest correct version

Use checked loops and indexing. Do not begin with `memcpy`, vectorization,
specialized search, or raw pointer arithmetic.

### Step 6: compile it as a real imported module

Testing only inside the declaration file can miss module visibility, public
path, generic instantiation, or name-resolution failures.

### Step 7: document the public contract

State:

- meaning;
- parameters and result;
- normal alternatives and errors;
- allocation and cleanup;
- realm restrictions;
- mutation;
- complexity when it is not obviously constant;
- one example showing why the API is useful.

### Step 8: optimize only with evidence

Keep the behavior tests unchanged. Add benchmarks separately. Optimization is
an implementation change only if it preserves every public semantic promise.

## 15. A practical learning path

Do these exercises in order. Each produces useful stdlib work while teaching
one language idea.

### Exercise 1: implement `Option<T>`

Learn:

- variants;
- generic types and methods;
- exhaustive matching;
- module visibility;
- cross-module generic instantiation.

Finish when `Option<i32>` and `Option<str>` work through a public import and
incorrect payloads are rejected.

### Exercise 2: implement byte algorithms

Learn:

- arrays versus slices;
- mutable versus read-only borrowing;
- checked indexing and loops;
- `Option<usize>` as ordinary absence;
- nominal errors for failed copy.

Finish when empty, boundary, and read-only misuse cases are tested.

### Exercise 3: wrap Linux `read`

Learn:

- FFI;
- `unsafe` as a proof obligation;
- pointer/length validation;
- variants versus errors;
- safe public boundary design.

Finish when application code can read stdin without containing `unsafe`.

### Exercise 4: design typed allocation

Learn:

- why size, alignment, type, provenance, and GC metadata belong together;
- which parts require compiler support;
- overflow and zero-count policy;
- how one source operation changes behavior by realm.

Finish when the syntax and semantics are documented and positive/negative
tests cover dynamic, regional, GC, flex, and stack behavior.

### Exercise 5: understand the implemented `Vec<T>`

Learn:

- representation invariants;
- generic owning storage;
- growth and checked arithmetic;
- pointer operations hidden behind safe methods;
- explicit cleanup and sanitizer testing.

This milestone is complete. Reproduce the executable proof and inspect how one
source API selects dynamic, regional, and GC storage.

### Exercise 6: extend realm-aware containers

Learn:

- shared algorithm versus different ownership policy;
- regional lifetime constraints;
- GC tracing;
- why identical method names can retain predictable realm-specific cleanup.

Finish when a second container reuses the implicit-owner design without
requiring allocator or realm arguments at call sites.

### Exercise 7: build `String`

Learn:

- representation invariants layered over `Vec<u8>`;
- owned versus borrowed text;
- UTF-8 validation;
- byte indexes versus Unicode scalars;
- foreign NUL-terminated text boundaries.

Finish when invalid UTF-8 cannot enter through safe construction and borrowed
`str` views cannot outlive the owner.

## 16. How to review an API proposal

Before accepting an API, read actual call sites rather than only its
declaration.

Ask:

### Meaning

- Can a beginner explain the result states in one sentence?
- Does a familiar technical term keep its familiar meaning?
- Is absence different from failure?

### Type safety

- Can invalid combinations be removed with a variant or dedicated type?
- Does a slice replace a loose pointer/count pair?
- Is a generic constraint the smallest behavior required?

### Ownership

- Is the owner visible?
- Is cleanup explicit where necessary?
- Can a borrowed value escape?
- Does the same name change destruction behavior unexpectedly?

### Calls

- Does an ordinary use read naturally?
- Is common use short without hiding important cost or danger?
- Are rare controls available without complicating the common path?

### Failure

- Which failures are recoverable?
- Which conditions trap?
- Are partial results possible?
- Does cleanup occur on every error path?

### Evolution

- Could an internal representation change without breaking callers?
- Does a wildcard match hide future cases?
- Is the public surface larger than the proven use case?

### Verification

- Is there an executable example?
- Are edge cases tested?
- Is important misuse rejected and tested?
- Are allocation and runtime costs stated?

If these answers are not known, the proposal needs another design pass before
implementation.

## 17. Common traps

### Copying another language's API exactly

Rust, Go, Swift, Zig, C++, and Python have different ownership, dispatch,
error, module, and runtime models. Study their reasoning, then redesign the API
for Runes.

### Starting with the largest container

A hash map combines allocation, generic keys and values, equality, hashing,
collision policy, resizing, iteration, and error behavior. It is a poor first
test of unfinished foundations.

### Using interfaces everywhere

Interfaces add runtime representation and dispatch. Many algorithms need only
a concrete type or a constrained generic.

### Generalizing before two examples exist

One concrete implementation teaches you the problem. A second reveals what is
actually shared. Generalize the shared part after that.

### Treating `unsafe` as “checks disabled”

Runes still keeps runtime arithmetic, indexing, range, null-unwrapping, and
other contract checks inside `unsafe`. The keyword permits specific pointer,
cast, FFI, assembly, and mutation operations; it does not prove them correct.

### Hiding ownership without preserving it

One `Vec<T>` spelling is ergonomic only because the compiler preserves its
hidden owner realm and methods dispatch from that owner. A single spelling
without persistent owner identity would be unsafe: a nested regional or GC
call could otherwise route existing storage through the wrong backend.

### Documenting only the happy path

An API is defined just as much by empty input, invalid input, errors, overflow,
cleanup, and lifetime rejection as by its main example.

## 18. Recommended external reading

These readings are for transferable design reasoning, not for copying another
language's syntax.

### Start here

1. [Rust: Enums and Pattern Matching](https://doc.rust-lang.org/book/ch06-00-enums.html)
   explains variants, payloads, `Option`, and exhaustive case analysis in a
   beginner-friendly sequence. Translate the concepts to Runes `type` variants
   and `match`.
2. [Rust: Generic Types, Traits, and Lifetimes](https://doc.rust-lang.org/book/ch10-00-generics.html)
   introduces the difference between a generic placeholder and a behavioral
   constraint. Runes interfaces play the constraint role, though Runes syntax
   and lifetime model differ.
3. [Crafting Interpreters: A Map of the Territory](https://craftinginterpreters.com/a-map-of-the-territory.html)
   gives a readable overview of the stages between source text and execution.
   Use it to understand which changes belong in parsing, static analysis, code
   generation, or the runtime.

### API and package design

4. [Swift API Design Guidelines](https://www.swift.org/documentation/api-design-guidelines/)
   emphasizes clarity at the call site, precise terminology, and documenting
   every declaration. Those principles transfer well even though Runes naming
   conventions are not yet Swift's.
5. [Rust API Guidelines](https://rust-lang.github.io/api-guidelines/)
   is a review checklist for predictable types, conversions, documentation,
   validation, and interoperability. Treat it as questions to ask, not as
   mandatory Runes conventions.
6. [Go: Package Names](https://go.dev/blog/package-names)
   explains how a module/package name provides context and how to avoid names
   that repeat that context. This is directly useful when shaping `std.bytes`,
   `std.io`, and later collection modules.
7. [Effective Go: Interfaces and Generality](https://go.dev/doc/effective_go#generality)
   shows the value of small behavior-focused interfaces. Runes interfaces are
   nominal rather than Go's implicit structural interfaces, so copy the
   small-interface reasoning, not the satisfaction rules.

### Allocation and testing

8. [Zig Language Reference: Choosing an Allocator](https://ziglang.org/documentation/0.14.1/#Choosing-an-Allocator)
   is a useful comparison for allocator-explicit library design. Runes chooses
   realm-qualified functions instead, but the same central question remains:
   who selects the allocator and who owns the result?
9. [Zig Language Reference: testing and leak detection](https://ziglang.org/documentation/0.14.1/#Report-Memory-Leaks)
   demonstrates why allocation-heavy container tests should include leak
   detection instead of checking output alone.
10. [Rust API Guidelines: Dependability](https://rust-lang.github.io/api-guidelines/dependability.html)
    recommends encoding valid inputs in types where practical and validating
    arguments otherwise. That is the reasoning behind variants, slices, and
    checked stdlib boundaries in this guide.

Read one source to answer a current design question. Do not postpone building
until every resource is finished.

## 19. The immediate direction for Runes

The next work should remain intentionally small:

1. build the stdlib test harness;
2. implement and test `Option<T>`;
3. implement allocation-free byte-slice utilities;
4. isolate the raw Linux input boundary;
5. expose safe `std.io.read_into`;
6. build the stdin byte-counter integration program;
7. settle typed allocation in the language/runtime;
8. implement realm-aware `Vec<T>`; (done)
9. implement raw-owned `RawBox<T>`;
10. build owning UTF-8 `String`; (done)
11. prove the whole surface with a small compiler-oriented CLI.

Do not treat this order as a measure of sophistication. Finishing a small
`Option<T>` with correct imports, matches, generics, documentation, and misuse
tests is real language and library engineering. Each completed layer removes
uncertainty from the next one.

The practical goal is not to know everything before starting. It is to make
the next decision small enough that its meaning, ownership, failure behavior,
generic behavior, and tests can all be understood at once.
