# Runes v0.1 Language Usage Guide

This guide describes the language implemented by the current bootstrap
compiler. It covers what can be checked and executed today, including current
limitations. The draft specification contains design context; when it differs
from this guide, this guide follows tested compiler behavior.

## 1. Toolchain and execution model

Runes currently compiles through C:

1. `runes` lexes, parses, resolves, and type-checks all input files.
2. `--emit-c` lowers the checked program to readable C11.
3. `runec build` compiles and links that C with the host compiler.

Build the compiler with `make`. Use the higher-level driver for normal work:

```bash
./runec check app.runes
./runec run app.runes
./runec emit-c app.runes -o build/app.c
./runec build app.runes -o build/app
```

| Command | Purpose |
|---|---|
| `runec check FILE...` | Analyze without producing a binary. |
| `runec emit-c FILE...` | Emit C; default is `build/<first-file>.c`. |
| `runec build FILE...` | Emit C and compile it. |
| `runec run FILE...` | Build a temporary executable and run it. |

`runec` rebuilds `runes` when compiler sources are newer. Useful forms:

```bash
./runec check --prelude app.runes
./runec build app.runes -o build/app -- support.c -lpthread
CC=clang CFLAGS="-O0 -g" ./runec build app.runes -o build/app
./runec run app.runes -- program-argument
```

After `--`, `build` accepts C compiler/linker arguments while `run` accepts
program arguments. The low-level frontend also provides:

```bash
./runes --lex-only app.runes
./runes --parse-only app.runes
./runes --dump-ast app.runes
./runes app.runes --emit-c build/app.c
```

## 2. First program

```runes
f add(left: i32, right: i32) = result: i32 {
    result = left + right
}

f main() {
    i32 answer = add(20, 22)
    print("answer:", answer)
}
```

Run it with `./runec run hello.runes`. `main` is the entry point. The portable
v0.1 form is `f main()` with no parameters or named return; generated programs
return zero. A larger executable example is `src/examples/language_tour.runes`.

## 3. Files, names, comments, and statements

Source files use `.runes`. Identifiers begin with an ASCII letter or underscore
and continue with letters, digits, or underscores. Names are case-sensitive.
Newlines or semicolons separate statements; semicolons are optional.

```runes
-- A line comment

---
A multiline comment.
---
```

Top-level declarations include functions, variables, types, variants,
interfaces, methods, errors, modules, imports, and extern symbols. Executable
behavior should normally live in functions.

## 4. Types and literals

| Type | Current meaning |
|---|---|
| `i8`, `i16`, `i32`, `i64` | Signed integers. |
| `u8`, `u16`, `u32`, `u64` | Unsigned integers. |
| `usize` | Pointer-sized unsigned integer; currently 64-bit. |
| `f32`, `f64` | Floating-point numbers. |
| `bool` | `true` or `false`. |
| `char` | Character value; ASCII is the reliable executable subset. |
| `str` | NUL-terminated string data in the C backend. |
| `void` | No value, mainly in `!void` and `*void`. |

Integer literals default to `i32`; floating literals default to `f64`.
Compatible literals adapt to a declared destination:

```runes
i32 decimal = 42
u64 address = 0x1000
f32 ratio = 0.5
bool enabled = true
char marker = 'R'
str name = "Runes"
```

Decimal, hexadecimal, fractional, and exponent forms are accepted. String and
character escapes include `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\0`, and
`\uXXXX`. Non-ASCII `char` lowering is incomplete.

Composite forms are:

```runes
*i32          -- pointer
[4]i32        -- fixed array
(i32, str)    -- tuple
!i32          -- fallible value
!void         -- fallible operation without a success value
math.Point    -- qualified type
```

Array lengths must be positive integer literals. Generics and user-defined
aliases are not implemented.

Use `as` for explicit numeric and pointer casts:

```runes
u64 wide = 42 as u64
*u8 device = 0x10000000 as *u8
*void erased = device as *void
```

Casts are low-level; validity and alignment are not proven.

## 5. Variables, constants, and assignment

```runes
i32 count = 0
label := "ready"          -- inferred local
const i32 LIMIT = 10
const answer := 42
volatile i32 global_counter = 0
```

Assignment targets may be variables, fields, indexes, or dereferences:

```runes
count = count + 1
point.x = 10
values[0] = 7
*pointer = 9
```

Constants cannot be reassigned. Compound assignments and increment operators
do not exist; write `value = value + 1`.

## 6. Operators

Precedence, highest to lowest:

| Group | Operators |
|---|---|
| Postfix | calls, indexing, `.field`, `..`, `..=` |
| Cast | `as Type` |
| Unary | `!`, `-`, `~`, `&`, `*` |
| Multiplicative | `*`, `/`, `%` |
| Additive | `+`, `-` |
| Bitwise | `&`, `^`, `|`, `<<`, `>>` |
| Comparison | `<`, `<=`, `>`, `>=` |
| Equality | `==`, `!=` |
| Logical | `and`, then `or` |
| Errors | prefix `try`, then infix `catch` |
| Assignment | `=` |

Arithmetic is numeric; `%`, shifts, and bitwise operations require integers;
logical operations require booleans. Mixed widths normally need casts. Pointer
addition/subtraction accepts an integer offset. String `+` concatenates, while
`==` and `!=` compare contents.

## 7. Functions and returns

Parameters are typed. Void functions omit a return clause:

```runes
f log(message: str) {
    print(message)
}
```

Value-returning functions declare a writable named result:

```runes
f square(value: i32) = result: i32 {
    result = value * value
}
```

Falling off the end returns that variable. Assign it before a bare early
`return`:

```runes
f nonnegative(value: i32) = result: i32 {
    if value < 0 {
        result = 0
        return
    }
    result = value
}
```

A same-line shorthand is accepted:

```runes
f double(value: i32) = result: i32 result = value * 2
```

Functions are visible before declaration. Local functions are supported but
cannot capture surrounding locals; pass state explicitly.

### Memory realms

```runes
f ordinary() {}          -- stack by default
stack f explicit() {}
regional f arena_job() {}
dynamic f heap_job() {}
gc f managed_job() {}
flex f inherited_job() {}
```

| Caller | Allowed callees/nested functions |
|---|---|
| `main` | All realms |
| `stack` / plain `f` | `stack` only |
| `regional` | `stack` only |
| `dynamic` | All realms |
| `gc` | `stack` and `gc` |
| `flex` | Accepted as inheriting its caller |

These rules are checked, but the C backend does not yet create arenas or a GC.
Ordinary locals retain C storage behavior.

## 8. Built-in operations

### `print`

`print(value, ...)` accepts one or more primitives, pointers, or errors. It
separates arguments with spaces and adds a newline:

```runes
print("count", 42, true, 'x')
```

Pointers print as addresses and errors as numeric codes. Aggregate types cannot
be printed directly. `print` works only as a statement.

### `sizeof` and `alignof`

```runes
usize bytes = sizeof([4]i32)
usize alignment = alignof(i64)
```

Both return `usize` and reflect the host C ABI.

### `promote`

```runes
regional f make_value() = result: *i32 {
    i32 local = 42
    result = promote(&local) as dynamic
}
```

`promote(pointer) as dynamic|gc` copies the pointed-to object and returns the
same pointer type. It requires a pointer, rejects promotion from pure `stack`,
and accepts only `dynamic` or `gc`. Both targets currently use `malloc`; there
is no automatic destruction or garbage collection.

## 9. Prelude contracts

`--prelude` prepends these declarations:

```runes
extern f raw_alloc(size: usize) = result: *void
extern f raw_free(ptr: *void)
extern f alloc(size: usize) = result: *void
extern f memset(ptr: *void, value: i32, size: usize) = result: *void
extern f memcpy(dst: *void, src: *void, len: usize) = result: *void
extern f sqrt(value: f32) = result: f32
```

They are contracts, not a complete runtime. Host C provides memory functions
and `sqrt` maps to `sqrtf`; callers of `raw_alloc`, `raw_free`, or `alloc` must
link implementations. `*void` is the untyped FFI pointer:

```runes
*i32 value = raw_alloc(sizeof(i32)) as *i32
*value = 42
raw_free(value as *void)
```

## 10. Arrays and pointers

```runes
[4]i32 values = [10, 20, 30, 40]
[512]u64 zeroed = []
i32 first = values[0]
values[1] = 25
usize length = values.len
```

Initializers must match element type and length. Contextual `[]` zero-fills.
Literal out-of-range indexes are diagnosed; dynamic indexes have no runtime
bounds check. Arrays copy on assignment and return. `&array` decays to `*T`.

```runes
*i32 first_ptr = &values
first_ptr[1] = 7
*(first_ptr + 2) = 9
```

Pointers are invariant except for explicit `*void` use. Address-of requires an
assignable expression. Null is written `0 as *Type`. Lifetime, null, alignment,
aliasing, and dynamic bounds are the programmer's responsibility. Dynamic
arrays and slices are not built in; model them as pointer/length structs.

## 11. Strings, tuples, and destructuring

Strings expose byte-level properties:

```runes
str text = "ru" + "nes"
usize length = text.len
*u8 bytes = text.ptr
bool equal = text == "runes"
```

Concatenation allocates with `malloc` and is not automatically reclaimed.
`len` counts bytes.

Tuple syntax and typed destructuring:

```runes
(i32, str) pair = (42, "answer")
i32 number, str label = pair

f bounds() = result: (i32, i32) {
    result = (0, 10)
}
```

Destructuring arity and types must match. There is no numeric tuple-field
syntax; destructure instead.

## 12. Structs and methods

```runes
type Point = {
    x: i32,
    y: i32,
}

type Options = retries: i32 = 3, verbose: bool = false
```

Constructors require named arguments. Required fields cannot be omitted;
defaulted fields can:

```runes
Point point = Point(x: 20, y: 22)
Options options = Options(verbose: true)
```

Pointers auto-dereference for field access. Recursive structs must use a
pointer, not contain themselves by value.

```runes
method Point {
    f move(self: *Point, amount: i32) {
        self.x = self.x + amount
    }

    f sum(self) = result: i32 {
        result = self.x + self.y
    }

    f zero() = result: Point {
        result = Point(x: 0, y: 0)
    }
}
```

`self` may be an inferred value receiver or explicitly typed. Calls perform the
tested implicit adjustment between `T` and `*T`. A method without `self` is an
associated function, called as `Point.zero()`.

## 13. Interfaces

```runes
interface Value {
    f read_value(self) = result: i32
}

type Box = { value: i32 }

method Value for Box {
    f read_value(self) = result: i32 { result = self.value }
}

f read(value: Value) = result: i32 {
    result = value.read_value()
}
```

Concrete implementors coerce to interface values on assignment or calls. The C
backend generates a fat pointer and method adapters. Keep implementation
signatures exactly aligned; conformance diagnostics are still being hardened.
Avoid giving an inherent method and an interface implementation method the same
name on one concrete type; the current C name mangling collides in that case.

## 14. Variants and match

```runes
type Pixel =
    | Clear
    | Gray(u8)
    | RGB(u8, u8, u8)

Pixel clear = Pixel.Clear
Pixel gray = Pixel.Gray(7)
Pixel color = Pixel.RGB(10, 20, 30)
```

Payload arity and types are checked. `match` supports literals, wildcard `_`,
bindings, variant payloads, struct patterns, error members, and guards:

```runes
f score(pixel: Pixel) = result: i32 {
    result = match pixel {
        Clear -> 0,
        Gray(value) -> value as i32,
        RGB(red, green, blue) if red > 0 ->
            (red as i32) + (green as i32) + (blue as i32),
        RGB(_, _, _) -> -1,
    }
}
```

Arm bodies may be expressions or blocks; a value block yields its final
expression. Variant and boolean matches are checked for exhaustiveness. Guards
must be boolean and do not make an arm exhaustive. Struct patterns can use
`Point(x: 0, y)` or `Point(x, y)`.

## 15. Control flow

Conditions must be boolean. `if` is both a statement and a value:

```runes
if ready { work() } else { wait() }
str sign = if value < 0 { "negative" } else { "nonnegative" }
```

Value-producing `if` needs `else`, compatible branch values, and uses each
block's final expression.

```runes
while count < 10 { count = count + 1 }
loop {
    if done { break }
    work()
}
```

`break` and `continue` carry no value and are valid only in loops.

Ranges and fixed arrays are iterable:

```runes
for (0..10) |index| { print(index) }       -- excludes 10
for (0..=10) |index| { print(index) }      -- includes 10
for (values) |value| { print(value) }
for (values) |value, index| { print(index, value) }
for (values) |*value| { *value = *value + 1 }
for (values) |*value, index| { *value = index as i32 }
```

Range bounds are compatible integers. Array value capture copies; pointer
capture mutates in place. Capture indexes are `usize`.

## 16. Errors, `try`, and `catch`

```runes
error MathError = {
    | DivisionByZero
    | Overflow
}

f divide(a: i32, b: i32) = result: !i32 {
    if b == 0 {
        result = error.MathError.DivisionByZero
    } else {
        result = a / b
    }
}
```

Inside a fallible function, `try` unwraps success or propagates failure:

```runes
f doubled(a: i32, b: i32) = result: !i32 {
    i32 value = try divide(a, b)
    result = value * 2
}
```

`try` in a non-fallible function is rejected. Handle errors with a default or
a bound handler:

```runes
i32 fallback = divide(10, 0) catch -1
i32 handled = divide(10, 0) catch |err| {
    print("failed", err)
    return
}
```

`!void` models failure without a success payload. Fallible values match as
`Ok(value)` and `Err(error)`. Runtime error sets currently share one numeric
code space; strict error-set identity is unfinished.

## 17. Modules and multiple files

```runes
mod math {
    pub type Box = { value: i32 }
    pub f double(value: i32) = result: i32 {
        result = value * 2
    }
}

use math.double
math.Box box = math.Box(value: double(21))
```

Qualified and nested module paths work. `use module.member` imports one member.
`pub` records public intent, but visibility enforcement is incomplete.

There is no filesystem module discovery. List every file; they are analyzed as
one program:

```bash
./runec build types.runes math.runes app.runes -o build/app
```

## 18. FFI and linking

```runes
extern f abs(value: i32) = result: i32
extern f write_bytes(data: *u8, len: usize)
extern u64 KERNEL_START
```

External symbols must come from host libraries, object files, or C sources:

```bash
./runec build app.runes -o build/app -- support.c -lcurl
```

FFI follows the generated host C ABI. Fixed-width primitives and explicit
pointer casts are safest. A web server today should declare socket operations
with `extern`, use a small C shim for awkward platform APIs, and link it after
`--`; native networking modules are not included yet.

## 19. Systems syntax

`unsafe` scopes low-level code but is not yet a strict capability gate; pointer
operations and assembly are currently accepted outside it too.

```runes
unsafe {
    *u64 pointer = address as *u64
    *pointer = value
}
```

Volatile declarations and expressions parse and check:

```runes
volatile *u32 uart = 0x10000000 as *u32
*uart = 65
u32 status = volatile *uart
```

The C backend does not yet consistently preserve `volatile`; verify generated
C before using it for MMIO.

Inline assembly accepts a string and one optional output:

```runes
asm { "cli" }
u64 value = 0
asm { "mov $42, %rax" } -> value
```

It lowers to GNU-style host assembly. Inputs, clobber lists, and general
constraints are not exposed.

Attributes use `#[name]` or `#[name(argument)]`:

```runes
#[packed]
#[align(16)]
#[repr(C)]
type Frame = { ip: u64, sp: u64 }

#[section(".text.boot")]
#[link_name("_start")]
f entry() {}

#[callconv("sysv64")]
f syscall(number: u64) = result: u64 { result = number }

#[interrupt]
f timer_handler() {}
```

The checker validates string arguments for `section`/`link_name`, power-of-two
literal alignment, and parameterless/returnless interrupt functions. It parses
`packed`, `repr`, and `callconv`. The current C emitter does not yet lower these
attributes, symbol names, layouts, or calling conventions.

## 20. Executable surface and limits

The tested C backend executes primitives, globals, inference, named returns,
forward calls, non-capturing local functions, operators, casts, all control
flow above, arrays, pointers, strings, tuples, structs, methods, variants,
matches, errors, modules, interfaces, externs, `sizeof`, `alignof`, limited
assembly, `promote`, and `print`. Unsupported AST nodes fail emission rather
than being silently ignored.

Current boundaries:

- no filesystem module loader, packages, or dependency format;
- no native file/socket/process/thread/event-loop modules;
- no generics, capturing closures, slices, maps, dynamic arrays, or async;
- no real arena/GC runtime or automatic cleanup;
- no dynamic array bounds checks;
- incomplete `unsafe`, `volatile`, and systems-attribute enforcement/lowering;
- limited assembly and permissive error/interface diagnostics;
- incomplete `pub` visibility enforcement;
- host-dependent C ABI and target sizes.

See `CODEBASE_AUDIT_AND_FIX_PLAN.md` for readiness and next priorities.

## 21. Editors, testing, and further examples

Install VS Code highlighting with:

```bash
code --install-extension runes-lang/runes-lang-0.0.1.vsix
```

Install Zed highlighting with `make install-zed`, then restart Zed if needed.
Validate it with `make test-zed`.

Project verification commands:

```bash
make test
make test-samples
make test-codegen
make test-sanitize
```

Useful executable examples:

| Topic | File |
|---|---|
| Language tour | `src/examples/language_tour.runes` |
| Arrays/pointers | `src/tests/samples/core_arrays_pointers.runes` |
| Control flow | `src/tests/samples/core_codegen_control.runes` |
| Structs/methods | `src/tests/samples/core_codegen_methods.runes` |
| Variants/match | `src/tests/samples/core_codegen_variants.runes` |
| Errors | `src/tests/samples/core_codegen_errors.runes` |
| Interfaces | `src/tests/samples/core_codegen_interfaces.runes` |
| Modules | `src/tests/samples/core_codegen_modules.runes` |
| Systems | `src/tests/samples/core_codegen_systems.runes` |

## 22. Keywords

```text
f dynamic regional gc flex stack
method interface type error mod use pub const
match if else for while loop break continue return
try catch unsafe asm extern volatile promote sizeof alignof
self as true false and or
i8 i16 i32 i64 u8 u16 u32 u64 f32 f64
bool str char usize void
```

There is no `class`, `new`, `null`, `switch`, `throw`, `import`, or `fn`.
Use structs/methods, constructors, zero pointer casts, `match`, fallible values,
`use`, and `f` respectively.
