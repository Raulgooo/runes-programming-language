# Syntax Reference

This chapter describes every source form accepted as part of the implemented
v0.1 language. Semantic restrictions are collected in
[semantics.md](semantics.md) and [memory-and-unsafe.md](memory-and-unsafe.md).

## Lexical structure

Runes source is UTF-8. Identifiers themselves are ASCII:

```text
identifier = (ASCII-letter | "_") (ASCII-letter | digit | "_")*
```

Keywords cannot be used as ordinary identifiers. The complete keyword set is:

```text
and as asm bool break catch char const continue defer dynamic else error except extern
f f32 f64 false flex for gc i8 i16 i32 i64 if in interface loop match method
mod move null or promote pub regional return self sizeof alignof stack str
true try type u8 u16 u32 u64 unsafe use usize void volatile when realm while
```

Whitespace separates tokens. A newline normally terminates a statement, but is
treated as whitespace inside `()`, `[]`, or `{}`, and after syntax that is
visibly incomplete, such as an operator or comma. A semicolon can terminate a
statement explicitly. Multiple statements may therefore share a line:

```runes
i32 left = 20; i32 right = 22
```

Line comments begin with `--`. Block comments begin and end with `---`:

```runes
-- one line
---
several lines
---
```

## Import declarations

```text
use-declaration = "use" import-path ("as" identifier)?
import-path = identifier ("." identifier)*
```

Without `as`, the final path segment becomes the local name. With `as`, the
following identifier becomes the local name:

```runes
use std.bytes.find
use std.bytes.find as find_byte
use std.core.Option as Maybe
```

Imports are allowed only at file or module scope and are private to that
module. `pub use`, grouped imports, and wildcard imports are not accepted.
Path resolution, visibility, collisions, and module-loading behavior are
specified in [Modules and names](modules-ffi-tooling.md#modules-and-names).

## Literals

### Integers and floats

```runes
42
0x2a
0X2A
3.5
1.25e3
1.25E-3
```

Integer literals are decimal or hexadecimal. Binary (`0b`), octal (`0o`), and
digit-separator forms are not implemented. A floating literal requires digits
on both sides of the decimal point. An optional exponent is accepted only
after that fractional part. A leading sign is a unary operator, not part of the
literal token.

An integer literal is checked against the type inferred from its context. An
out-of-range literal is a compile-time error.

### Strings and characters

```runes
"Runes"
"line one\nline two"
'R'
'界'
'\u{1F30D}'
```

The implemented escapes are:

| Escape | Value |
|---|---|
| `\n` | newline |
| `\t` | horizontal tab |
| `\r` | carriage return |
| `\\` | backslash |
| `\"` | double quote |
| `\'` | single quote |
| `\0` | NUL byte/scalar |
| `\u{HEX}` | Unicode scalar value |

Strings may contain UTF-8 and embedded NUL. A character literal must decode to
exactly one Unicode scalar value; surrogates and values above `U+10FFFF` are
rejected.

Boolean literals are `true` and `false`. `null` is the null value for nullable
pointer types.

## Types

```text
type = primitive
     | qualified-name type-arguments?
     | "*" type
     | "*const" type
     | "?*" type
     | "?*const" type
     | "[" integer-literal "]" type
     | "[]" type
     | "[]const" type
     | "(" type ("," type)+ ")"
     | realm-function-type
     | "!" type
```

Examples:

```runes
i32
domain.Item
Box<i32>
*u8
*const u8
?*Node
?*const Node
[16]u8
[]u8
[]const u8
(i32, bool)
f(i32) -> bool
[regional] f(*Node) -> *Node
!i32
!void
```

The available function-type realm tags are `[stack]`, `[dynamic]`,
`[regional]`, `[gc]`, and `[flex]`. An unqualified `f(...)` is stack-realm.

## Variables and storage declarations

```text
variable-declaration = ("const" | "volatile")? type name ("=" expression)?
                     | name ":=" expression
tuple-declaration    = typed-name ("," typed-name)+ "=" expression
typed-name           = type name
```

Examples:

```runes
i32 count = 0
name := "Runes"
const usize capacity = 4096
volatile u32 device_status = 0
i32 number, bool ready = (42, true)
```

Declarations at file or module scope create globals. Globals use the same
surface syntax but have additional visibility and lifetime restrictions.
Non-volatile constants may be exported as `pub const T NAME = value`.
Mutable and volatile public globals are rejected.
Attributes may precede a global where permitted by the
[attribute matrix](modules-ffi-tooling.md#attribute-matrix).

A declaration using `:=` must have an initializer.

## Functions

```text
function = "pub"? realm? "f" name generic-parameters?
           "(" parameters? ")" named-result? function-body
realm = "stack" | "dynamic" | "regional" | "gc" | "flex"
named-result = "=" name ":" type
function-body = block | same-line-result-expression
```

An omitted realm means stack. A void function omits the named result:

```runes
f log(value: i32) { print(value) }

f add(left: i32, right: i32) = result: i32 {
    result = left + right
}
```

### Realm declaration modifiers

The parser accepts optional realm applicability modifiers before functions,
methods, types, and interfaces:

```text
realm-overload = "in" concrete-realm
realm-exclusion = "except" "(" concrete-realm
                  ("," concrete-realm)* ")"
concrete-realm = "stack" | "dynamic" | "regional" | "gc"
```

Examples:

```runes
except(stack)
flex f build<T>(value: T) = result: T {
    result = value
}

except(stack)
in gc f build<T>(value: T) = result: T {
    result = value
}
```

`in gc f` marks the definition selected for GC dispatch; it is separate from
the existing `gc f` syntax, which enters GC execution. `flex` and `main` are
not valid applicability cases. An exact definition cannot exclude its own
realm.

At a direct call, the compiler infers the effective execution realm, selects
an exact definition for that realm when present, and otherwise uses the shared
definition. A family containing only exact definitions is an implicit
allowlist. `except(...)` is checked before fallback selection.

Selection is compile time. Generated C contains a deterministic concrete
instance and no runtime realm branch. Generic type arguments and the inferred
realm are both part of that instance's identity. Realm-overloaded function
values remain unsupported because erasure would discard the static selection
context.

For an overloaded definition with no explicit execution-realm prefix, its body
uses the selected dispatch realm. An explicit prefix remains independent:
`in dynamic gc f work()` selects the definition for dynamic dispatch and then
enters GC execution. This preserves the existing meaning of `gc f`.

Realm-specific struct and variant families use the same exact-then-fallback
selection. Construction and type annotations infer the effective realm and
produce a hidden concrete type identity. Generic arguments, layouts,
descriptors, constructors, and generated ABI names are specialized together.
Only demanded variants are emitted.

A value retains that hidden identity through assignment-compatible parameters
and returns. Ordinary functions with a realm-specific parameter are
automatically specialized for the concrete argument variant; the realm is not
written at the call site. Structs or variants containing a realm-specific type
are specialized transitively. Assigning or merging incompatible variants is a
type error.

Realm-overloaded receiver methods dispatch from a realm-specific receiver's
persistent owner variant, even when the call expression occurs under another
effective execution realm. Methods on ordinary types continue to dispatch
from effective execution realm.

Interface modifiers are parsed and family-validated for forward
compatibility, but interface semantic selection is still rejected explicitly.

A value-returning function permits one same-line expression without braces. It
must have a named result, the expression must begin on the declaration line,
and normal result-assignment rules still apply. The useful canonical form is an
assignment to the named result:

```runes
f double(value: i32) = result: i32 result = value * 2
```

Interface method signatures omit the body. `extern f` uses the same parameter
and named-result syntax but has no Runes body.

Parameters use `name: Type`. A method receiver uses `self` or `self: Type`.
Generic parameters use `<T, U>` and constraints use `<T: Interface>`.

The process entry point is exactly a root `f main()` with no parameters,
generic parameters, or result.

## Structs

Braced declaration:

```runes
type Position = {
    x: i32 = 0
    y: i32 = 0
    volatile flags: u32
}
```

Compact declaration:

```runes
type Position = x: i32, y: i32
```

Fields have `name: Type` syntax, may have `= default`, and may use the
`volatile` keyword. Within braces, commas or unambiguous newlines separate
fields. Compact declarations end at the statement boundary.

Construction uses named arguments:

```runes
Position origin = Position()
Position point = Position(x: 20, y: 22)
```

Defaulted fields may be omitted. Required fields may not. Unknown, duplicate,
or wrongly typed constructor arguments are errors.

## Variants

```runes
type Message =
    | Quit
    | Move(i32, i32)
    | Text(str)
```

Variant arms may have zero or more ordered payload types. Construct with a
qualified arm name when needed:

```runes
Message first = Message.Quit
Message second = Message.Move(10, 20)
```

Within a context where the variant type is already known, an unqualified arm
name can also be resolved, including in a pattern.

## Interfaces and methods

```runes
interface Readable {
    f read(self) = result: i32
}

method Counter {
    f read(self) = result: i32 { result = self.value }
    f set(self: *Counter, value: i32) {
        unsafe { self.value = value }
    }
}

method Readable for Counter {
    f read(self) = result: i32 { result = self.value }
}
```

The first method block adds inherent methods. The `Interface for Concrete`
form supplies an interface implementation. A generic owner is written
`method Box<T>`, and individual methods may introduce additional type
parameters.

Method bodies use the ordinary function syntax and may use realm qualifiers.
Method visibility parsing exists, but enforcement remains incomplete; see
[implementation status](implementation-status.md#visibility-gaps).

## Error sets

```runes
error ParseError = {
    | Empty
    | InvalidDigit
}
```

Construct an error with `error.SetName.Member`, for example
`error.ParseError.Empty`. Error sets are nominal.

## Operators and precedence

From highest to lowest precedence:

| Level | Forms | Associativity |
|---|---|---|
| Primary/postfix | literals, names, grouping, calls, fields, indexing, ranges | left |
| Cast | `as Type` | left |
| Unary | `! - ~ & *` | right |
| Multiplicative | `* / %` | left |
| Additive | `+ -` | left |
| Bitwise/shift | `& ^ \| << >>` | left, one shared level |
| Comparison | `< <= > >=` | left |
| Equality | `== !=` | left |
| Boolean AND | `and` | left, short-circuiting |
| Boolean OR | `or` | left, short-circuiting |
| Error propagation | prefix `try` | right |
| Error recovery | `catch` | left |
| Assignment | `=` | right |

Use parentheses when mixing bitwise operators or chained comparisons; their
shared/left-associative parsing is exact but can be visually surprising.

Valid assignment targets are mutable variables, fields, indexes, and unsafe
pointer dereferences. Assignment does not produce a generally reusable value.

## Postfix expressions

```runes
function(arguments)
identity<i32>(42)
tools.identity<domain.Item>(item)
value.method(arguments)
box.replace<bool>(false)
value.field
array[index]
array[start..end]
array[start..=end]
array[..end]
array[start..]
array[..]
```

Calls may span lines inside parentheses. Generic argument lists are
disambiguated from comparison operators by the following `(`. Indexing uses one
expression. A range in brackets creates a slice or string view.

## Control flow

Conditions do not use parentheses and must have type `bool`:

```runes
if ready {
    start()
} else if waiting {
    poll()
} else {
    stop()
}

i32 magnitude = if value < 0 { -value } else { value }
```

A value-producing `if` requires an `else`, and every path must produce a
compatible value.

`when realm` selects a statement block at compile time:

```runes
when realm regional {
    record_abandoned_capacity()
} else {
    record_released_capacity()
}
```

Valid cases are `stack`, `dynamic`, `regional`, and `gc`. `flex` is not a case
because specialization resolves it to a concrete effective realm. The `else`
block is optional. Inactive blocks are discarded before name resolution and
type checking, and no runtime condition is emitted. A flex function containing
`when realm` currently requires direct calls; realm-polymorphic function-value
erasure is not implemented.

`return` can be bare or carry an expression:

```runes
result = 42
return

return 42
```

In a value-returning function, a return expression must match the named result
type. Both forms perform compiler-managed cleanup. `break` and `continue` apply
to the nearest loop.

`defer expression` schedules one expression for the end of the current
lexical block:

```runes
defer close(descriptor)
```

Deferred expressions execute in last-in, first-out order on normal block exit,
`return`, propagated errors, `break`, and `continue`. A return value is
evaluated before cleanup begins. `defer` is valid only inside a function.

Current backend limitation: a value-producing `if` or `match` cannot be emitted
directly as the operand of `return`. Assign it to the named result and reach the
end or use bare `return` instead.

## Loops

```runes
while condition { work() }
loop { if done { break } }
for (0..10) |index| { print(index) }
for (values) |value| { print(value) }
for (values) |value, index| { print(index, value) }
for (values) |*value| { unsafe { *value = 0 } }
for (values) |*value, index| { unsafe { *value = index as i32 } }
```

Unlike `if` and `while`, `for` requires parentheses around its iterable.
Ranges, fixed arrays, and slices are iterable. The optional index capture has
type `usize`. Pointer captures work for fixed arrays and mutable slices; they
are rejected for ranges and read-only slices.

## Patterns and match

```runes
str description = match value {
    Message.Quit -> "quit",
    Move(x, y) if x == y -> "diagonal",
    Move(_, _) -> "move",
    Text(text) -> text,
}
```

Implemented pattern forms are:

- literal patterns such as `0`, `true`, `'x'`, and `"text"`;
- binding names such as `value`;
- wildcard `_`;
- variant patterns such as `Some(value)`;
- tuple-shaped patterns;
- struct patterns such as `Point(x: 0, y)`;
- qualified error or variant members;
- an optional boolean guard after `if`.

Struct patterns accept named fields (`x: pattern`) and positional bindings
whose name selects the corresponding field (`Point(x, y)`). Pattern bindings
exist only in that arm. Match arms may contain an expression or a braced block,
and commas between arms are optional when newlines make the boundary clear.

A value-producing match requires compatible arm values. Variant matches are
checked for exhaustiveness. A guarded arm does not prove that its variant is
fully covered.

## Error-flow expressions

```runes
i32 value = try parse(text)
i32 fallback = parse(text) catch 0
i32 logged = parse(text) catch |problem| {
    print(problem)
    0
}
```

`try` propagates failure from the current fallible function. `catch` either
uses a fallback expression or binds the error for a handler block. Catch
expressions can be chained left-to-right.

## Unsafe and systems expressions

```runes
unsafe {
    *pointer = 42
    *u32 register = 0x10000000 as *u32
    asm { "mov %cr3, %rax" } -> value
}

usize bytes = sizeof(Packet)
usize alignment = alignof(Packet)
*Node promoted = promote(&node) as dynamic
```

The exact operations requiring `unsafe` and their invariants are in
[memory-and-unsafe.md](memory-and-unsafe.md). Attributes and foreign
declarations are in [modules-ffi-tooling.md](modules-ffi-tooling.md).
