# Semantics Reference

This chapter defines typing, conversion, evaluation, aggregate, abstraction,
and error behavior. Memory provenance and unsafe operations are specified in
[memory-and-unsafe.md](memory-and-unsafe.md).

## Type system

Runes is statically typed. Every expression is assigned a type before C is
generated. Unresolved names and types are errors; there is no dynamic fallback.

### Primitive types

| Type | Meaning |
|---|---|
| `i8`, `i16`, `i32`, `i64` | Signed integers of the stated width |
| `u8`, `u16`, `u32`, `u64` | Unsigned integers of the stated width |
| `usize` | Target-sized unsigned integer; `u64` on the v0.1 target |
| `f32`, `f64` | IEEE-style C backend floating types |
| `bool` | Exactly `true` or `false`; integers are not truth values |
| `char` | Unicode scalar value represented in 32 bits |
| `str` | Borrowed UTF-8 byte pointer plus byte length |
| `void` | Absence of a value, used by FFI and `!void` |

`char` rejects surrogate values and values above `U+10FFFF`. `str` is not an
owning string and is not necessarily NUL-terminated.

### Constructed types

- `*T`: non-null pointer to `T`;
- `*const T`: non-null read-only pointer to `T`;
- `?*T`: nullable pointer to `T`;
- `?*const T`: nullable read-only pointer to `T`;
- `[N]T`: fixed inline array with positive literal length `N`;
- `[]T`: mutable borrowed slice;
- `[]const T`: read-only borrowed slice;
- `(T, U, ...)`: tuple;
- `[realm] f(T, ...) -> R`: function or closure value;
- `!T`: fallible success value;
- nominal struct, variant, interface, and error-set types;
- concrete generic instantiations such as `Box<i32>`.

Structs, variants, interfaces, and error sets are nominal: independently
declared types remain incompatible even when their visible shapes match.
Different concrete instantiations of a generic are distinct types.

### Copy behavior

| Value category | Assignment/pass behavior |
|---|---|
| Primitive | Copies the value |
| Fixed array | Copies every inline element |
| Tuple | Copies every component |
| Struct/variant | Copies inline representation and contained values |
| Pointer | Copies the address |
| `str` | Copies pointer and byte length |
| Slice | Copies pointer and element length |
| Interface | Copies data reference and method-table handle |
| Closure | Copies function/environment handle |

Copying a reference-bearing value does not copy its backing storage and does
not extend that storage's lifetime.

## Inference and assignment

`name := expression` infers one concrete type from the initializer. Inference
does not make the binding dynamically typed; later assignments must remain
compatible.

Integer literals receive a type from context when possible and must fit that
type. Otherwise the compiler chooses its default integer interpretation.
Floating literals similarly use their expected floating type. Runes does not
perform broad implicit numeric promotion between already typed values.

`const` prevents assignment to the binding. It does not recursively freeze
storage reached through a pointer or reference-bearing field. `volatile`
preserves externally observable loads and stores in generated C; it does not
provide atomicity or synchronization.

Inner scopes may shadow outer bindings. Duplicate declarations in one scope
are errors. Blocks, functions, loop captures, match arms, and modules introduce
relevant scopes.

## Conversions and coercions

### Implicit coercions

The implemented safe coercions include:

| Source | Destination | Rule |
|---|---|---|
| fitting integer literal | integer type | Compile-time range checked |
| `[N]T` storage/view | `[]T` | Mutable view when source is mutable |
| `[N]T` storage/view | `[]const T` | Read-only view |
| `[]T` | `[]const T` | Drops mutation permission |
| `*T` | `?*T` | Adds nullability |
| `*T` | `*const T` | Drops pointee mutation permission |
| concrete value | satisfied interface | Builds data/vtable interface value |
| success `T` or matching error | `!T` | Builds fallible result |

No coercion may increase mutability, invent non-nullness, or extend storage
lifetime. A read-only slice cannot become mutable. A nullable pointer cannot
become non-null without `unwrap`.

The `.ptr` property of `[]T` has type `*T`; the `.ptr` property of
`[]const T` has type `*const T`. A string's `.ptr` has type `*const u8`.
Mutation through a read-only pointer is rejected even inside `unsafe`.

### Explicit `as` conversions

Numeric width or signedness changes use `as`. Integer-to-`char` checks at
runtime that the result is a Unicode scalar. `char` can convert to a compatible
integer representation.

Pointer construction from integers and unrelated pointer reinterpretation are
unsafe. Zero cannot construct `*T`; represent absence as `null` in `?*T`.
`*void` is the untyped FFI/allocation pointer and requires an explicit cast to
or from typed pointers.

An unsafe cast from `*u8` or a pointer to a fixed `u8` array to `str` scans for
NUL, validates the supported contract, and creates a borrowed view. It neither
copies nor extends the source lifetime.

## Evaluation

Boolean `and` and `or` short-circuit. Assignment is right-associative. A
closure callee expression is evaluated once before invocation.

The v0.1 C backend does not establish a portable left-to-right guarantee for
all independent operand and function-argument evaluations. Do not write code
whose result depends on the relative order of side effects in separate call
arguments or arithmetic operands. Sequence those effects in statements first.

Expression blocks used by `if`, `match`, and `catch` take their value from the
final expression on the selected path. Every reachable value path must produce
a compatible type.

## Operators

Arithmetic operands must be compatible numeric types. `%` and all bitwise
operators require integers. `and`, `or`, and `!` require `bool`.

Equality applies to supported compatible values. Ordering applies to supported
numeric values, characters, and strings. String equality and ordering use the
stored byte length and lexicographic UTF-8 byte order.

`str + str` concatenates and allocates the result in the active allocation
realm. Other `+` uses are numeric or pointer arithmetic as separately allowed.

## Runtime checks

These checks remain active in every build mode, including inside `unsafe`:

- signed and unsigned integer addition, subtraction, and multiplication;
- integer division and remainder, including zero and signed-minimum/`-1`;
- signed negation;
- shift counts and checked left-shift overflow;
- dynamic array, slice, and string indexes;
- slice range order and bounds;
- UTF-8 scalar boundaries for string ranges;
- null `unwrap`;
- integer-to-`char` scalar validity;
- allocation and runtime contract failures.

Compile-time-known invalid literal, index, range, or conversion cases should be
diagnosed before code generation. Dynamic violations trap with source location
where the runtime contract carries one.

Standard-library storage operations with an ordinary name use this terminating
failure boundary. Their `t`-prefixed counterparts return `Result` instead; see
the [standard-library allocation convention](standard-library.md#stdallocation).

There are no wrapping/saturating arithmetic operators or unchecked indexing
forms in v0.1.

## Strings and characters

`str.len` is a byte count. Indexing a string returns a byte, not a `char`.
String ranges return borrowed views and verify that boundaries do not split a
UTF-8 scalar. A string can contain embedded NUL because its length is explicit.

Concatenation creates new storage in the active realm. Comparing strings does
not require NUL termination. Converting to a C string requires an explicit
runtime helper or unsafe assertion described in the FFI reference.

The prelude exposes UTF-8 validation, decoding, encoding, hashing, and checked
view construction as runtime contracts. Higher-level grapheme iteration,
normalization, case mapping, and owning strings are not language builtins.

## Aggregate values

### Tuples

Tuple arity and element types are part of the type. Tuples copy by value.
Typed destructuring requires the same number of target bindings and compatible
element types:

```runes
(i32, bool) state = (42, true)
i32 number, bool ready = state
```

### Arrays and slices

An array initializer must contain exactly its declared number of homogeneous,
compatible elements. `[]` zero-initializes an array when a fixed-array type is
available from context.

Arrays own inline storage. Slices never own their elements. Both expose `.len`,
indexing, ranges, and iteration. Mutable slices additionally expose mutable
element access and `.ptr`; read-only raw-pointer extraction requires an
explicit unsafe FFI policy.

A slice keeps the provenance and lifetime of its source. Storing a slice in an
aggregate or interface does not hide or extend that lifetime. Temporary or
shorter-lived slice backing is rejected when it would escape.

### Structs

Struct construction validates field names, duplicates, required/defaulted
fields, and types. Declared defaults fill omitted fields. Struct values copy by
value, including reference-bearing fields as handles.

Direct by-value recursive layout is rejected. Recursive structures must use a
pointer or another indirection. Field access through supported pointer receivers
is automatically lowered, but pointer provenance and unsafe dereference rules
still apply.

### Variants

A variant stores one tag and the selected arm's ordered payload. Construction
requires the exact payload arity and compatible types. Variants copy by value;
reference-bearing payloads retain their provenance.

## Functions and returns

A value-returning function declares a named result. Reaching the end returns
the current result after the compiler's definite-result checks. A bare `return`
returns that current result. `return expression` validates the expression
against the named result and returns it directly through the same cleanup path.

The current C backend does not lower a value-producing `if` or `match` directly
inside `return expression`. Assign that control-flow value to the named result
first. Parsing and type checking accept the nested form, but C emission rejects
it; this is a backend gap rather than intended final semantics.

Void functions omit the result declaration. Root `main` must be void and have
no parameters or generic parameters.

Functions are collected for their containing scope before bodies are checked,
so forward references are allowed. Overloading is not; a name resolves to one
declaration in a scope.

Function types include parameter, result, fallibility, and realm information.
Calls must respect all of those parts.

## Methods and interfaces

An inherent method is selected by receiver type and method name. A receiver
written as `self` uses the owning type; `self: *Type` explicitly requests a
pointer receiver. Method calls use `receiver.method(arguments)`.

An interface is a nominal method set. An implementation must match receiver,
parameter types, result type, fallibility, and realm exactly. Missing methods,
extra implementation methods, or signature mismatches are errors.

An interface method may use either value-shaped `self` or an explicit
non-null pointer receiver such as `self: *Writer`. A concrete implementation
of the pointer form must use `self: *Concrete` with the same pointer
mutability/nullability. This enables statically dispatched mutation without
turning the interface value into a runtime object.

Concrete-to-interface conversion creates a runtime data/vtable pair. The data
reference retains the concrete value's provenance, so an interface cannot be
used to smuggle stack- or arena-backed data into a longer-lived location.

## Generics

Functions, structs, variants, and method owners may declare type parameters.
Methods may declare their own additional parameters. A parameter constrained as
`T: Interface` permits only operations provided by that interface.

Calls infer type arguments only when the mapping is unique. Explicit arguments
use `<...>`:

```runes
Box<i32> box = Box<i32>(value: 42)
i32 value = identity<i32>(42)
bool old = box.replace<bool>(true)
domain.Box<i32> other = domain.Box<i32>(value: 7)
```

Each used concrete instantiation is monomorphized before resolution and type
checking. Arity mismatches, conflicting inference, ambiguous inference,
constraint failures, type arguments on nongeneric declarations, and missing
required arguments are compile-time errors.

An imported generic may be instantiated with a public concrete type declared
by the consuming module. The compiler generates the required qualified type
binding; `std.text.split_once` exercises this with `Option<TextSplit>`.
Using a private nested-module type as an argument to a generic imported from a
different module is not implemented yet. Root-module private concrete
arguments continue to work.

There are no const generics, higher-kinded types, specialization, variance, or
runtime generic type erasure.

## Nested functions and closures

A nested function can capture lexical bindings by reference. It may mutate a
mutable capture, but the resulting borrowing closure cannot escape any captured
binding's lifetime.

`move f` is valid only for nested functions. It copies captures into an
allocated environment governed by the active realm. The environment can escape
only when its realm and every captured value permit it.

Closure values are first class: they can be variables, parameters, results,
array elements, tuple/struct fields, and variant payloads. Arena closure
environments participate in deep promotion; GC closure environments are traced.

## Errors and fallible values

Error sets are nominal. `!T` carries either a success value of type `T` or an
error value. `!void` represents success without a payload or failure.

`try expression` requires a fallible enclosing function. On success it yields
the success payload. On failure it assigns/returns the error through the current
function's fallible result and performs required realm cleanup.

`expression catch fallback` converts failure into a compatible success value.
`catch |error_name| { ... }` binds the error inside its handler. Fallible
success types and participating error sets must be compatible; unrelated
nominal error sets are not interchangeable merely because member names match.

Matching a fallible value uses `Ok(payload)` and `Err(error)` patterns. The
ordinary match typing, binding, and exhaustiveness rules apply.
