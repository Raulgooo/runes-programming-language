# Runes Language Specification v0.1

Status: experimental specification for the implemented hosted C bootstrap.

This document states normative v0.1 behavior. The practical syntax reference
and examples are in the [language handbook](guide/README.md). Runtime-only ABI
requirements are in
[v0.1-runtime-requirements.md](v0.1-runtime-requirements.md).

## 1. Execution model

1. A program consists of one or more root source files plus recursively loaded
   external modules.
2. The compiler lexes, parses, monomorphizes, resolves, type-checks, and emits
   C11.
3. The supported target is hosted Linux x86-64 through GCC or Clang.
4. Only a root declaration exactly matching `f main()` is the process entry.
   It has no parameters, type parameters, or return value.
5. `main` may call every memory realm. A non-root function named `main` is
   ordinary and receives normal name mangling and realm rules.
6. The compiler runtime is not a standard library.

## 2. Lexical rules

Identifiers begin with an ASCII letter or `_` and continue with ASCII letters,
digits, or `_`. Keywords cannot be identifiers.

Line comments begin with `--`. Block comments are delimited by `---`.
Statements end at semicolon or at an unambiguous newline. Newlines inside
delimiters or after incomplete operators are whitespace.

String and character literals contain UTF-8 and the implemented escapes.
String literals may contain embedded NUL. Character literals denote exactly one
Unicode scalar value.

Integer literals are decimal or hexadecimal (`0x`/`0X`). Binary and octal
prefixes and digit separators are not part of v0.1. Floating literals contain
a decimal point and may have an exponent after the fractional part. The escape
set is `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\0`, and `\u{HEX}`.

## 3. Types

Primitive types are signed `i8`, `i16`, `i32`, `i64`; unsigned `u8`, `u16`,
`u32`, `u64`, `usize`; floating `f32`, `f64`; `bool`; Unicode scalar `char`;
length-bearing UTF-8 view `str`; and `void`.

`usize` is target-sized and is `u64` on the v0.1 target. `char` has a `u32`
representation but excludes surrogate values and values above `U+10FFFF`.
`str` has pointer and byte-length representation and is not a C string.

Constructed types are:

- non-null pointer `*T` and nullable pointer `?*T`;
- fixed array `[N]T`, where `N` is a positive integer literal;
- mutable slice `[]T` and read-only slice `[]const T`;
- tuple `(T, U, ...)`;
- function/closure `f(T, ...) -> R`, optionally realm-qualified;
- nominal struct, variant, interface, and error-set types;
- fallible `!T`;
- compile-time generic instantiations.

Unrelated nominal types are incompatible even when their layouts match.
Unresolved types are errors.

## 4. Values and storage

Variables have explicit types or use `name := expression` inference. `const`
storage is not assignable. `volatile` storage preserves volatile access in
generated C and requires an unsafe context when accessed through raw pointers.
Variables may be local or module-global; module globals are currently private.

Primitive, fixed-array, tuple, struct, and variant values copy by value.
Pointers, strings, slices, interfaces, and closures contain references and copy
their view/handle; copying does not extend backing-storage lifetime.

Array literal elements are homogeneous. A declared `[N]T` initializer must
contain exactly `N` values, except `[]`, which zero-initializes the array.

Slices are non-owning pointer/length views. Arrays coerce to compatible slices;
mutable slices coerce to read-only slices. No conversion may increase
mutability or storage lifetime.

## 5. Expressions and checks

Arithmetic operators require compatible numeric operands. `%` and bitwise
operators require integers. `and` and `or` require booleans. `str + str`
concatenates, and string comparison is length-aware byte lexicographic order.

Ordinary integer add, subtract, multiply, divide, remainder, negation, and left
shift are checked in every build mode. Invalid operations trap. Right shift is
defined through the emitted checked helpers for its operand width/sign.

Array, slice, and string indexes are checked. Static out-of-range cases are
diagnosed; dynamic cases trap with source location. Sub-slicing validates range
order, bounds, and UTF-8 scalar boundaries where a string result requires it.

`as` is an explicit conversion. Integer narrowing, pointer construction from an
integer, and unsafe pointer conversions are never implicit.

`sizeof(T)` and `alignof(T)` use target C layout. `print(arguments...)` accepts
the implemented primitive/pointer values, writes each consecutively, and then
one newline. It inserts no separator.

## 6. Functions

A value-returning function declares a named result:

```runes
f add(a: i32, b: i32) = result: i32 { result = a + b }
```

A void function omits the return declaration. An explicit `return` performs
all compiler-managed arena and GC cleanup. Reaching the end returns the current
named result. `return expression` validates the expression against and returns
through the named result type. Fallible functions use a named `!T` result.

A named-result function may use one same-line expression body without braces;
the named result must still be assigned on every path. All general
multi-statement function bodies and control-flow blocks use braces.

Functions may be declared before or after their uses. Overloading is not
supported. Generated internal symbols are deterministic and encode declaration
path segments without underscore-collision ambiguity. Only extern or explicit
link names form a stable foreign ABI.

## 7. Memory realms

Every function has one realm:

| Syntax | Realm | `alloc` behavior |
|---|---|---|
| `f`, `stack f` | stack | no owning allocator |
| `dynamic f` | raw heap | raw allocation |
| `regional f` | arena | active arena allocation |
| `gc f` | managed | tracked GC allocation |
| `flex f` | inherited | caller's active realm |

Legal calls/nesting are:

| Caller | May call |
|---|---|
| root `main`, `dynamic f` | stack, flex, dynamic, regional, GC |
| stack `f` | stack, flex |
| `regional f` | stack, flex, regional |
| `gc f` | stack, flex, GC |

A root regional invocation creates an arena. Each nested regional invocation
creates a child attached to its parent. Children remain alive after return and
the root destroys the full tree on every exit path.

References cannot escape shorter-lived storage through return, assignment,
globals, aggregates, variants, interfaces, slices, or closures. Inline values
return by value and do not need a realm conversion.

`raw_alloc` is always raw ownership and requires explicit `raw_free`.

## 8. Promotion

`promote(value) as dynamic` and `promote(value) as gc` transfer arena-derived
ownership by deep cloning. No omitted target is allowed.

The clone preserves cycles and aliases. It traverses compiler-known owned
edges in pointers, arrays, slices, strings, structs, variants, tuples,
interfaces, and closure environments. Borrowed, raw, external, MMIO, and GC
edges remain pointer values and are not recursively claimed.

Promotion is rejected outside an arena ownership context and for values that do
not represent arena-backed data.

## 9. Scoped garbage collection

There is one precise, non-moving mark/sweep heap owned by one OS thread. Any
number of `gc f` functions on that thread share it.

Collection occurs only on a GC allocation threshold/slow path or explicit
collection call. The compiler emits exact type descriptors, shadow-stack
frames, roots, transient protections, and return-value protection. There are no
read/write barriers or asynchronous pauses in non-GC code.

GC references may not cross OS threads. v0.1 has no finalizers, weak
references, public free, generations, compaction, incremental collection, or
concurrent collection. Regional code cannot call GC-capable code and therefore
does not hold GC edges across a safepoint.

## 10. Pointers and unsafe operations

`*T` excludes null. `?*T` admits `null`; `unwrap` checks and returns `*T`.
Nullable pointers cannot be dereferenced or used arithmetically.

The following require lexical `unsafe`:

- pointer dereference;
- pointer arithmetic;
- volatile/MMIO access through pointers;
- integer-to-pointer construction and unsafe pointer casts;
- calls through arbitrary external function declarations;
- raw pointer/length slice construction;
- direct string pointer access and byte-pointer-to-string conversion;
- inline assembly.

Taking an address and passing a typed pointer are not inherently unsafe.
Entering `unsafe` does not disable arithmetic or bounds checks.
Compiler-lowered intrinsics and reserved `runes_` runtime functions are trusted
compiler/runtime contracts, not arbitrary foreign calls.

## 11. Structs, variants, and interfaces

Structs are nominal named field aggregates. Duplicate fields and direct
by-value recursive layout are errors. Constructors validate field names,
arity, defaults, and types. Structs have braced and same-line compact
declaration forms. Fields may declare defaults or use the `volatile` storage
modifier.

Variants are nominal tagged unions with zero or more ordered payloads per arm.
Constructors validate exact arm payloads. `match` supports variant, literal,
binding, wildcard, and guarded patterns. A value-producing match requires
compatible results.

Interfaces are nominal method sets. An implementation must exactly match
receiver, parameters, result, fallibility, and realm. Concrete-to-interface
conversion produces a data/vtable value and retains the concrete value's
provenance.

## 12. Generics

Functions, structs, variants, and methods may declare type parameters. A type
parameter may have an interface constraint. The generic body may use only
operations valid for its parameters and declared constraints.

Calls infer function type arguments when the mapping is unique, or accept
explicit type arguments. Each concrete use is monomorphized before resolution
and type checking and receives a collision-safe symbol.

Const generics, higher-kinded types, specialization, variance, and runtime type
erasure are outside v0.1.

## 13. Closures

Nested functions may capture lexical bindings by reference. Such a closure may
mutate mutable captures but cannot escape any captured binding's lifetime.

`move f` is legal only for a nested function and captures values into an
allocated environment. Its environment follows the active memory realm and may
escape only when that realm and all captured values permit it.

Closure values are first class: they may be parameters, results, variables,
array/tuple/struct fields, or variant payloads. Invocation evaluates the callee
expression exactly once. Arena closure environments participate in deep
promotion; GC environments participate in precise tracing.

## 14. Control flow and errors

`if`, `while`, `loop`, range/array/slice `for`, `break`, `continue`, `return`,
and `match` are implemented. `if` and `match` may produce values where every
path has a compatible result. Array and slice iteration supports value,
value/index, pointer, and pointer/index captures. Pointer capture is rejected
for ranges and read-only slices.

Error sets are nominal. `!T` is a result carrying `T` or an error. `try`
propagates an error from the current fallible function. `catch` handles the
error inline and may bind it. Error-set and success types must match exactly;
unrelated sets are not structurally interchangeable.

## 15. Modules

An inline module is `mod name { declarations }`. An external declaration
`mod name` first searches beside the declaring file, then configured project
module roots, for exactly one form:

1. `name.runes`, or
2. `name/mod.runes`.

Both existing is an ambiguity. Neither existing is an error. Multiple matches
among configured roots are ambiguous. Nested flat modules resolve children
beneath a same-named directory. Canonically completed modules may be reused;
re-entering a module that is still loading is a cycle and is rejected.

Members are private unless `pub`. Qualified access uses dot-separated paths.
`use path.member` imports the final public member into the current scope.

Projects use a strict `runes.toml` with a required project name and entry,
optional module roots, and named local path dependencies. A dependency creates
a top-level module namespace. The reserved `std` namespace is loaded from the
configured, environment, development, or installed standard-library root.
Normal driver builds load the runtime prelude unless `--no-prelude` is used.

Visibility is enforced for functions, types/variants, interfaces, error sets,
and child modules. Fields and variant arms follow their containing type. v0.1
currently exposes extern declarations, keeps module globals private, and does
not consistently enforce method visibility or support public `use` re-exports.

## 16. Foreign and systems ABI

`extern f` and extern variables declare host symbols. There are no variadic
function declarations in v0.1. The implemented target attributes are:

- struct: `#[repr(C)]`, `#[packed]`, `#[align(N)]`;
- global: `#[section("...")]`, `#[align(N)]`, `#[link_name("...")]`;
- function: `#[section("...")]`, `#[link_name("...")]`,
  `#[callconv("sysv64")]`, `#[callconv("win64")]`;
- extern function: link name and calling convention.

An extern function is unsafe to call by default. `#[safe]` is permitted only as
a marker on an extern function and asserts that the binding author guarantees a
safe wrapper contract. It changes call-site checking but emits no ABI attribute.

Unknown, duplicate, malformed, or inapplicable attributes are errors. Field
attributes other than the `volatile` keyword are rejected. `#[interrupt]`
signatures are validated, but C emission is unsupported and fails explicitly;
an external assembly entry stub is required.

Inline assembly uses the host compiler's GNU assembly syntax and is
target-specific.

## 17. Deliberate omissions

v0.1 has a standard-library namespace and local path project dependencies, but
no complete standard library, registry/package fetcher, version solver, or
lockfile. It also has no variadics, overloads, const generics, async, macros,
cleanup/defer construct, deterministic destructors, public foreign GC-root API,
native object backend, or complete freestanding runtime.

Pipeline syntax is deferred. No incomplete linear pipe form is part of v0.1.
