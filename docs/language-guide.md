# Runes v0.1 Language Usage Guide

This guide describes the behavior implemented by the bootstrap compiler on the
`v0-1-exp` branch. Runes currently emits hosted C11 for GCC or Clang on Linux
x86-64. It is experimental, not self-hosted, and does not ship a standard
library. `src/runtime.c` contains only compiler-required runtime machinery.

## 1. Build and run

Requirements: Bash, Make, and GCC or Clang.

```bash
make
./runec check app.runes
./runec run app.runes -- argument1 argument2
./runec build app.runes -o build/app
./runec emit-c app.runes -o build/app.c
```

`runec` rebuilds `./runes` when compiler sources are newer. `build` and `run`
link generated C with `src/runtime.c`, `src/utils/arena.c`, and `-lm`. Set `CC`
or `CFLAGS` to change the host compilation:

```bash
CC=clang CFLAGS="-O0 -g" ./runec build app.runes -o build/app
./runec build app.runes -o build/app -- -lpthread
```

Arguments after `--` are linker flags for `build` and program arguments for
`run`. `--prelude` adds `src/std/prelude.runes`, which declares the compiler
runtime ABI; it is not a standard library.

The lower-level compiler commands are:

```bash
./runes app.runes                         # resolve and type-check
./runes --lex-only app.runes              # print tokens
./runes --parse-only app.runes            # parse, including external modules
./runes --dump-ast app.runes              # print the AST
./runes app.runes --emit-c build/app.c    # generate C
```

## 2. First program

```runes
f add(left: i32, right: i32) = result: i32 {
    result = left + right
}

f main() {
    print("answer: ", add(20, 22))
}
```

A value-returning function declares a named result after `=`. Assigning that
name determines the returned value. A void function omits the return clause.

Only a root-level `f main()` is the process entry point. It must have no type
parameters, value parameters, or return value. It is an orchestrator realm and
may call every implemented memory realm. A module member or nested function
named `main` is an ordinary function.

`print` writes arguments consecutively and then exactly one newline. It never
inserts spaces or commas:

```runes
print("hello, ", name)  -- programmer supplies the separator
```

## 3. Source structure

Runes files use `.runes`. Statements normally end at an unambiguous newline;
semicolon is also accepted. Newlines inside `()`, `[]`, and expression
continuations do not terminate the statement.

```runes
-- one-line comment
---
block comment
---
```

Identifiers use ASCII letters, digits, and `_`, and cannot start with a digit.
Text and character literals may contain UTF-8.

## 4. Types and literals

Primitive types:

| Type | Meaning |
|---|---|
| `i8`, `i16`, `i32`, `i64` | Signed integers |
| `u8`, `u16`, `u32`, `u64` | Unsigned integers |
| `usize` | Target-sized unsigned integer; `u64` on the current target |
| `f32`, `f64` | IEEE-style floating point through the C backend |
| `bool` | `true` or `false` |
| `char` | Unicode scalar value represented as `u32` |
| `str` | Immutable length-bearing UTF-8 byte view |
| `void` | No value, mainly in FFI and fallible-void types |

Composite types:

```runes
*i32                 -- non-null raw/owned pointer
?*i32                -- nullable pointer
[4]i32               -- fixed array
[]i32                -- mutable non-owning slice
[]const i32          -- read-only non-owning slice
(i32, bool)          -- tuple
f(i32) -> i32        -- closure/function value
[regional] f(i32) -> i32 -- realm-qualified function type
!i32                 -- fallible i32
```

Integer literals support decimal and the implemented base prefixes. Numeric
conversions are explicit with `as`. Narrowing an integer literal is allowed
only when its value fits; other implicit narrowing is rejected.

String literals carry a byte pointer and byte length. Embedded NUL is valid.
Characters must be Unicode scalar values; surrogate code points and values
above `U+10FFFF` are rejected or trap when constructed dynamically.

## 5. Variables and constants

```runes
i32 count = 0
name := "Runes"
const usize BUFFER_SIZE = 4096
volatile *u32 uart = 0x10000000 as *u32
```

`:=` infers the type. `const` prevents assignment. `volatile` is emitted for
storage and struct fields and is intended for FFI or MMIO. Reading or writing
through a raw pointer requires `unsafe`.

Fixed arrays copy by value. Structs, variants, and tuples also have value
semantics; pointer and slice values copy their address/view, not their backing
storage.

## 6. Operators and checks

Implemented operators include:

- arithmetic: `+ - * / %`;
- comparison: `== != < <= > >=`;
- boolean: `and or !`;
- bitwise: `& | ^ << >>`;
- assignment: `=`;
- range: `start..end` and `start..=end`;
- cast: `value as Type`.

Ordinary integer add, subtract, multiply, divide, remainder, negate, and left
shift are checked. Overflow, division by zero, invalid shift counts, and
`MIN / -1` trap with a source location. Safe array, slice, and string indexing
also traps on an invalid bound. `unsafe` does not disable these checks.

`str + str` concatenates in the active realm. String equality and ordering are
byte-wise and length-aware, including embedded NUL bytes.

Explicit wrapping/saturating arithmetic and an unchecked-indexing operation are
not yet implemented.

## 7. Functions and memory realms

```runes
f local_work() {}
stack f strict_leaf() {}
dynamic f raw_heap_work() {}
regional f arena_work() {}
gc f managed_work() {}
flex f inherited_work() {}
```

| Form | Allocation selected by `alloc` | Lifetime |
|---|---|---|
| `f` / `stack f` | no owning allocator; local values are stack values | call |
| `dynamic f` | raw heap | explicit `raw_free` |
| `regional f` | a new arena or attached child arena | root regional exit |
| `gc f` | process GC heap | reachability |
| `flex f` | active caller arena/GC, otherwise raw behavior | inherited |

`f` and `stack f` use the same v0.1 realm. They may call stack/flex functions.
`regional f` may call stack/flex/regional functions. `gc f` may call
stack/flex/gc functions. `dynamic f` and root `main` may call every realm.
Illegal calls are rejected statically.

Every root regional call creates an arena. A regional call made inside it
creates a child arena which remains attached after the child returns. The full
tree is destroyed when the root regional call exits, including early and
fallible exits. This permits a child to return arena data to its regional
parent but not outside the regional tree.

`raw_alloc` and `raw_alloc_aligned` always allocate raw memory and
`raw_free` releases it. Arena destruction only releases memory; files, sockets,
locks, and other resources still need explicit cleanup.

## 8. Allocation and promotion

The allocation operations require ABI declarations, normally through
`--prelude`:

```runes
extern f alloc(size: usize) = result: *void
extern f raw_alloc(size: usize) = result: *void
extern f raw_alloc_aligned(size: usize, align: usize) = result: *void
extern f raw_free(pointer: *void)
```

Cast the returned pointer to the intended type before dereferencing:

```runes
regional f make_value() = result: *i32 {
    unsafe {
        result = alloc(sizeof(i32)) as *i32
        *result = 42
    }
}
```

Arena-backed references cannot escape their regional tree implicitly.
`promote(value) as dynamic` or `promote(value) as gc` deep-clones an
arena-owned graph into the selected longer-lived realm. Descriptors emitted by
the compiler traverse structs, variants, arrays, slices, strings, and closure
environments. The clone map preserves aliases and cycles. Borrowed, external,
raw, and GC edges are not recursively claimed as arena ownership.

Promotion only applies to arena-derived data. Inline values already leave by
value copy, and raw/borrowed/GC values do not need or permit arena promotion.

## 9. Garbage collection

v0.1 implements one precise, non-moving mark/sweep heap per process, owned by
one OS thread. Any number of `gc f` calls on that thread share the heap.

Collection is synchronous and cooperative. It occurs only on a GC allocation
slow path or an explicit `runes_gc_collect()` call. Code that cannot reach such
a safepoint has no GC poll or barrier. The compiler emits type descriptors and
shadow-stack roots for live locals, parameters, globals, closure environments,
interfaces, aggregate contents, and protected return/temporary values.

There are no read or write barriers, moving/compaction, finalizers, weak
references, generations, concurrent marking, or public `gc_free`. GC
references cannot cross OS threads. Current realm rules also prevent regional
code from calling GC-capable code, so regional arenas are not GC root
containers in v0.1.

Runtime diagnostic functions declared by the bootstrap prelude are
`runes_gc_collect`, `runes_gc_debug_object_count`, and
`runes_gc_debug_collection_count`.

## 10. Arrays and slices

```runes
[4]i32 values = [10, 20, 30, 40]
[4]i32 zeros = []
[]i32 mutable = values[..]
[]const i32 tail = values[1..]
[]const i32 first_two = values[..2]
[]const i32 inclusive = values[1..=2]
```

An explicit fixed-array initializer must contain exactly its declared number
of same-typed elements. `[]` zero-initializes a declared array. Array indexes
known to be invalid are rejected; dynamic indexes are checked at runtime.

Slices are `{ pointer, length }` views with no allocation or ownership. Arrays
coerce to slices, mutable slices coerce to read-only slices, and the reverse is
rejected. Sub-slicing preserves provenance and checks its range. A slice cannot
escape the lifetime of stack, arena, raw, GC, or external storage it views.

For external buffers, `slice(pointer, length)` and
`const_slice(pointer, length)` construct typed views inside `unsafe`. Their
result type is inferred from context.

Arrays and slices support `.len`, indexing, sub-slicing, and iteration:

```runes
f sum(values: []const i32) = result: i32 {
    result = 0
    for (values) |value| {
        result = result + value
    }
}
```

## 11. Pointers and nullability

`*T` is non-null. `?*T` can contain `null`. Use `unwrap(pointer)` to obtain a
non-null pointer; it traps if null. Nullable pointers cannot be dereferenced or
used in pointer arithmetic before unwrapping.

Address-of requires assignable storage. Pointer arithmetic is limited to
pointer-plus-integer, integer-plus-pointer, and pointer-minus-integer, and is
allowed only in `unsafe`. Dereference and integer-to-pointer casts are also
unsafe. `*void` is the untyped FFI/allocation pointer and explicitly converts
to or from typed pointers.

## 12. Structs, methods, and interfaces

```runes
type Vec2 = { x: f32, y: f32 }

method Vec2 {
    f sum(self) = result: f32 {
        result = self.x + self.y
    }
}

interface Value {
    f value(self) = result: i32
}

method Value for Counter {
    f value(self) = result: i32 { result = self.current }
}
```

Construct structs with named fields. Field names are checked, defaults are
supported where declared, and duplicate or recursive by-value fields are
rejected. Interface implementations must exactly match receiver,
parameter, result, fallibility, and realm signatures. Converting a concrete
value to an interface creates a data/vtable pair and participates in lifetime
checking.

Methods can be generic. Overloads are not supported; generated names include
module and owner identity to prevent C symbol collisions.

## 13. Variants and pattern matching

```runes
type Message =
    | Quit
    | Move(i32, i32)
    | Text(str)

f describe(message: Message) {
    match message {
        Quit -> print("quit"),
        Move(x, y) if x == y -> print("diagonal ", x),
        Move(x, y) -> print(x, ",", y),
        Text(value) -> print(value),
    }
}
```

Variant constructors enforce exact payload count and types. `match` supports
variant, literal, binding, wildcard, and guarded arms and can be used as a
statement or value where all paths produce a compatible value.

## 14. Generics

Functions, structs, variants, and methods support compile-time
monomorphization:

```runes
interface Value { f value(self) = result: i32 }
type Pair<T, U> = { first: T, second: U }
type Maybe<T> = | None | Some(T)

f identity<T>(value: T) = result: T { result = value }
f read<T: Value>(value: T) = result: i32 { result = value.value() }
```

Type arguments may be explicit, and function arguments can infer them where
the mapping is unambiguous. Constraints are interfaces and are checked exactly.
Every concrete instantiation receives a deterministic internal symbol.

v0.1 does not implement const generics, higher-kinded types, specialization,
variance, or runtime generic erasure.

## 15. Nested functions and closures

Nested functions capture referenced outer bindings by reference:

```runes
f apply(callback: f(i32) -> i32, value: i32) = result: i32 {
    result = callback(value)
}

f example(base: i32) = result: i32 {
    f add(value: i32) = answer: i32 { answer = base + value }
    result = apply(add, 2)
}
```

Borrowing closures may mutate mutable captures but cannot outlive them.
`move f` copies captures into an allocated closure environment and is allowed
only for nested functions:

```runes
dynamic f make_adder(base: i32) = result: f(i32) -> i32 {
    move f add(value: i32) = answer: i32 { answer = base + value }
    result = add
}
```

Function values can be passed, returned, stored in arrays, structs, tuples, and
variant payloads, and invoked through fields or indexes. Environment lifetime
follows its realm. An arena closure requires deep promotion to escape its
regional tree; a GC closure is traced precisely.

## 16. Control flow

```runes
if ready { start() } else { wait() }
while count > 0 { count = count - 1 }
loop { if done { break } }

for (0..10) |index| { print(index) }
for (values) |value| { print(value) }
for (values) |value, index| { print(index, ":", value) }
```

`break` and `continue` are loop-only. `if` and `match` can initialize a value
when their branches produce compatible results. `return` performs deterministic
arena and GC frame cleanup before leaving the function.

## 17. Errors, `try`, and `catch`

```runes
error ParseError = { | Empty | Invalid }

f parse(value: str) = result: !i32 {
    if value.len == 0 {
        result = error.ParseError.Empty
    } else {
        result = 42
    }
}

f load(value: str) = result: !i32 {
    i32 parsed = try parse(value)
    result = parsed
}

f main() {
    i32 parsed = parse("") catch |error| {
        print("parse failed")
        0
    }
    print(parsed)
}
```

Error sets are nominal. `!T` carries either `T` or an error. `try` propagates
the error from a fallible function; `catch` handles it and may bind the error.
Fallible `void` is written `!void`.

## 18. Modules and visibility

Inline modules:

```runes
mod math {
    pub f double(value: i32) = result: i32 { result = value * 2 }
}

use math.double
```

External modules use `mod name` and are resolved relative to the declaring
file as either `name.runes` or `name/mod.runes`. If both exist the compiler
reports an ambiguity; if neither exists it reports a missing module. Nested
flat modules resolve children under a directory named after the flat module.
Cycles or loading the same canonical file twice are rejected.

Only `pub` members cross module boundaries. Qualified access uses dots, such as
`math.double(21)`. Multiple root files may still be supplied explicitly and
are analyzed as one program.

## 19. FFI, attributes, and unsafe code

```runes
extern f read(fd: i32, buffer: *u8, count: usize) = result: i64
extern u64 external_counter

#[safe]
#[link_name("abs")]
extern f c_abs(value: i32) = result: i32
```

Extern signatures use the platform C ABI unless `#[callconv("sysv64")]` or
`#[callconv("win64")]` is supplied. Foreign strings use the Runes `RunesStr`
ABI unless you explicitly pass a C pointer using the runtime conversion
functions. Variadic C functions are not supported; use a fixed-signature C
wrapper.

Extern calls require `unsafe` by default. `#[safe]` on an extern function is an
explicit assertion by the binding author that its complete contract is safe to
call with type-checked arguments; it does not validate the foreign
implementation.

Implemented Linux x86-64 attributes:

- `#[repr(C)]`, `#[packed]`, and `#[align(N)]` on structs;
- `#[section("name")]` and `#[align(N)]` on global storage;
- `#[section("name")]`, `#[link_name("symbol")]`, and `#[callconv(...)]` on
  functions where applicable;
- `#[link_name(...)]` and `#[callconv(...)]` on extern functions.

Unknown, duplicate, malformed, or inapplicable attributes are rejected.
`#[interrupt]` signatures are recognized but the v0.1 C backend deliberately
rejects emission; use an external assembly entry stub.

Unsafe operations are lexically isolated:

```runes
unsafe {
    *device = 0x41
    *i32 next = pointer + 1
    asm { "nop" }
}
```

Dereference, pointer arithmetic, volatile/MMIO access, integer-to-pointer
casts, arbitrary foreign calls, raw slice construction, and inline assembly
require `unsafe`. Compiler-lowered intrinsics and reserved `runes_` runtime
functions are checked contracts rather than arbitrary FFI. Inline assembly is
GNU-style backend assembly and therefore target-specific.

## 20. Reading user input

Input is currently ordinary FFI, not a compiler builtin. On Linux, declare
`read`, read bytes, remove the terminal newline yourself, and construct or view
text according to your chosen library policy:

```runes
extern f read(fd: i32, buffer: *u8, count: usize) = result: i64

f main() {
    [64]u8 input = []
    print("What's your name?: ")
    i64 count = 0
    unsafe { count = read(0, &input, 63) }
    if count > 0 {
        usize length = count as usize
        if input[length - 1] == '\n' { length = length - 1 }
        input[length] = 0
        print("hello, ", (&input) as str)
    }
}
```

The raw terminal line includes `\n`, and `print` adds its own final newline.
C-style variadic `scanf` is intentionally not built in. A user standard
library can provide `read_line` and typed parsers over byte slices.

## 21. Runtime ABI versus standard library

The compiler-required runtime provides traps, raw allocation helpers, arenas,
deep promotion, the scoped collector, string primitives, and generated-code
support. It does not provide collections, filesystem/path APIs, sockets, HTTP,
format strings, scanners, threading, tensor operations, or ML code. Those are
standard-library or application responsibilities.

See [v0.1-runtime-requirements.md](v0.1-runtime-requirements.md) for the exact
runtime boundary and exported C ABI.

## 22. Current limits

- hosted C11 backend only; no native object backend or freestanding profile;
- one owning OS thread for all GC references and allocations;
- no variadic functions, function overloading, async, cleanup/defer construct,
  or deterministic object destructors;
- no const generics, higher-kinded generics, or generic specialization;
- no public registered-root API in the language surface;
- no automatic C-string conversion or lifetime extension across FFI;
- `#[interrupt]` requires an external assembly stub;
- pipeline syntax is deferred and is not reserved as an incomplete feature.

## 23. Verification and editor support

```bash
make test                 # unit, core, tooling, scale, differential
make test-samples         # positive and expected-failure language inventory
make test-codegen         # executable generated C under -Werror
make test-sanitize        # compiler and runtime ASan/UBSan coverage
make fuzz-smoke           # lexer through codegen fuzz smoke
make test-zed             # Zed extension checks
make install-zed          # install local Zed highlighting
```

Restart Zed after installation and open a `.runes` file. The local extension is
under `editors/zed/`; VS Code support is under `runes-lang/`.

## 24. Keywords

```text
and as asm bool break catch char const continue dynamic else error extern
f f32 f64 false flex for gc i8 i16 i32 i64 if interface loop match method
mod move null or promote pub regional return self sizeof alignof stack str
true try type u8 u16 u32 u64 unsafe use usize void volatile while
```
