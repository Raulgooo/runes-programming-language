# Data Modeling

This chapter covers the types used to group values: tuples, arrays, slices,
structs, variants, methods, interfaces, generics, and closures.

## Tuples

A tuple groups a fixed number of values without naming fields:

```runes
(i32, bool) state = (42, true)
i32 number, bool ready = state
```

Tuple element count and types are part of the tuple type. Tuples copy by value.
Typed tuple destructuring assigns each element to a separate local binding.
Use them for small local groupings; use a struct when field names communicate
meaning or the shape belongs in a public API.

## Arrays and slices

An array owns a fixed number of inline elements:

```runes
[4]i32 values = [10, 20, 30, 40]
[4]i32 zeros = []
```

`[4]i32` means exactly four `i32` values. An explicit initializer must contain
exactly the declared count and compatible element types. `[]` zero-initializes
a fixed array whose type is already declared.

Arrays copy their elements when assigned or passed by value.

A slice is a non-owning view containing a pointer and a length:

```runes
[]i32 all = values[..]
[]const i32 tail = values[1..]
[]const i32 first_two = values[..2]
[]const i32 inclusive = values[1..=2]
```

The slice does not copy or own the elements. A mutable slice `[]T` permits
element assignment. A read-only slice `[]const T` does not. Mutable slices can
become read-only; read-only slices cannot become mutable.

Arrays coerce to compatible slices when passed to a function:

```runes
f sum(values: []const i32) = result: i32 {
    result = 0
    for (values) |value| {
        result = result + value
    }
}

f main() {
    [3]i32 values = [10, 20, 12]
    print(sum(values))
}
```

Arrays and slices expose `.len`, indexing, ranges, and iteration. Dynamic
indexes and ranges are checked at runtime. A slice cannot outlive the storage
it views; the compiler tracks stack, arena, raw, GC, and external provenance.

Borrowed UTF-8 search, checked substring views, explicit ASCII trimming, and
Unicode scalar traversal are available through
[`std.text`](../reference/text.md). Its indexes are byte offsets; text views
reject offsets that split a UTF-8 scalar.

Use arrays for fixed inline storage and slices for borrowed function
parameters. The implemented `std.vec.Vec<T>` provides realm-aware growable
storage, and `std.string.String` provides owning valid UTF-8 over `Vec<u8>`.
Both are logically move-only, require explicit `deinit`, and have additional
view-safety rules, so read the complete
[`Vec<T>` reference](../reference/vec.md) and
[`String` reference](../reference/string.md) before using them.

## Structs

A struct gives names to a fixed set of fields:

```runes
type User = {
    id: i32
    name: str
}

f main() {
    User user = User(id: 1, name: "Raul")
    print(user.name)
}
```

A compact declaration is also accepted:

```runes
type Point = x: i32, y: i32
```

Fields can declare defaults. A constructor may omit those fields:

```runes
type Position = {
    x: i32 = 0
    y: i32 = 0
}

Position origin = Position()
Position right = Position(x: 10)
```

Constructors use field names, so argument order does not silently change the
meaning. The compiler rejects unknown, duplicate, missing required, and
wrongly typed fields. A field may have a declared default.

Structs copy by value. A field containing a pointer or slice still copies only
that reference/view. Recursive data structures must use pointers; a struct
cannot contain itself directly by value because that would have infinite size.

Fields do not currently have independent visibility. If the containing type is
public, its fields are accessible to callers.

## Methods

A method is a function attached to a type:

```runes
type Counter = { current: i32 }

method Counter {
    f value(self) = result: i32 {
        result = self.current
    }
}

f main() {
    Counter counter = Counter(current: 42)
    print(counter.value())
}
```

`self` is the receiver. Method calls use `value.method(arguments)`. Methods may
be generic and may use any function realm allowed by their signature.

**Current limitation:** method visibility is not enforced consistently. Treat
methods on a public type as public until the compiler gains complete method
visibility rules.

## Interfaces

An interface describes behavior without fixing one concrete representation:

```runes
interface Value {
    f value(self) = result: i32
}

method Value for Counter {
    f value(self) = result: i32 {
        result = self.current
    }
}
```

The implementation signature must match exactly: receiver, parameters, result,
fallibility, and memory realm. Converting a concrete value to an interface
creates a data pointer plus a method table. Because that pair references the
concrete value, interface values participate in lifetime and escape checking.

Use an interface when several types should support the same operation and
runtime dispatch is useful. Use a generic constraint when the concrete type can
remain known at compile time.

## Variants

A variant represents exactly one of several named alternatives:

```runes
type Message =
    | Quit
    | Move(i32, i32)
    | Text(str)
```

`Quit` carries no payload, `Move` carries two integers, and `Text` carries one
string. Constructors require the exact payload count and types.

Variants are useful for protocol messages, state machines, syntax trees, and
values that would otherwise need a tag plus manually synchronized fields.

## Pattern matching

`match` inspects a variant and can bind its payload:

```runes
f describe(message: Message) {
    match message {
        Quit -> print("quit"),
        Move(x, y) if x == y -> print("diagonal ", x),
        Move(x, y) -> print(x, ",", y),
        Text(value) -> print(value),
    }
}
```

Patterns include variant arms, literals, bindings, `_` wildcards, tuples, and
struct shapes. A guard after `if` adds a condition to an arm.

Struct patterns can name fields and either test or bind their values:

```runes
str location = match point {
    Point(x: 0, y: 0) -> "origin",
    Point(x: 0, y) -> "y-axis",
    Point(x, y: 0) -> "x-axis",
    Point(x, y) -> "other",
}
```

A match can produce a value when every reachable arm produces a compatible
value:

```runes
str label = match message {
    Quit -> "quit",
    Move(_, _) -> "move",
    Text(_) -> "text",
}
```

The compiler checks variant payload arity and exhaustiveness. Guarded arms do
not by themselves prove that an alternative is fully covered.

## Generics

Generics let one declaration work with several concrete types:

```runes
type Pair<T, U> = {
    first: T
    second: U
}

type Maybe<T> = | None | Some(T)

f identity<T>(value: T) = result: T {
    result = value
}
```

The compiler creates a specialized copy for each concrete use. This is called
**monomorphization**; it has no runtime type-erasure cost.

Function calls infer type arguments when the mapping is unambiguous. Explicit
type arguments are required when inference has insufficient information.

```runes
Pair<i32, bool> pair = Pair<i32, bool>(first: 42, second: true)
i32 value = identity<i32>(pair.first)
bool changed = identity<bool>(false)
```

The same `<...>` syntax works on qualified calls such as
`algorithms.identity<i32>(42)`. A constructor can often infer its arguments
from the declared destination, as in `Pair<i32, bool> pair = Pair(...)`.

Constrain a type parameter with an interface:

```runes
interface Value {
    f value(self) = result: i32
}

f read<T: Value>(item: T) = result: i32 {
    result = item.value()
}
```

Constraints use exact interface satisfaction. v0.1 has no const generics,
higher-kinded types, specialization, variance, or runtime generic erasure.

## Nested functions and closures

A nested function may use names from its surrounding function:

```runes
f apply(callback: f(i32) -> i32, value: i32) = result: i32 {
    result = callback(value)
}

f example(base: i32) = result: i32 {
    f add(value: i32) = answer: i32 {
        answer = base + value
    }
    result = apply(add, 2)
}
```

Here `add` captures `base` by reference. A borrowing closure may mutate mutable
captures, but it cannot outlive the captured storage.

`move f` copies captures into an allocated closure environment:

```runes
dynamic f make_adder(base: i32) = result: f(i32) -> i32 {
    move f add(value: i32) = answer: i32 {
        answer = base + value
    }
    result = add
}
```

`move f` is valid only for nested functions. The environment follows the
active realm: raw/dynamic environments need explicit ownership, arena closures
cannot escape without deep promotion, and GC closure environments are traced.

Function values can be passed, returned, invoked, and stored in arrays, structs,
tuples, and variant payloads.

## Common mistakes

- Returning a slice into a local array.
- Expecting a slice assignment to copy its elements.
- Treating an interface as compile-time-only; it contains runtime dispatch.
- Omitting a variant arm or payload in `match`.
- Expecting unconstrained generic code to call arbitrary methods.
- Returning a borrowing closure after its captured stack variables die.

[Next: Projects, modules, and visibility](05-projects-and-modules.md)
