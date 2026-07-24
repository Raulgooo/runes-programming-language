# Language and Tooling Reference

This chapter collects details that are useful to look up after reading the
tutorial chapters.

This is the handbook's quick-reference chapter. For exhaustive grammar,
semantics, conversion rules, and implementation evidence, see the
[complete language reference](../reference/README.md).

## Source forms

Top-level and module declarations include:

```runes
f function() {}
stack f stack_function() {}
dynamic f heap_function() {}
regional f arena_function() {}
gc f managed_function() {}
flex f inherited_function() {}

type Struct = { field: i32 }
type Variant = | First | Second(i32)
interface Behavior { f act(self) }
method Struct { f act(self) {} }
method Behavior for Struct { f act(self) {} }
error Failure = { | Invalid }
mod child
use child.member
extern f foreign()
```

`pub` is meaningful on functions, types/variants, interfaces, error sets, and
modules. See the implemented visibility table in chapter 5 for current gaps.

## Builtins and special forms

| Form | Purpose |
|---|---|
| `print(values...)` | Write supported values consecutively, then newline |
| `sizeof(Type)` | Target C size as `usize` |
| `alignof(Type)` | Target C alignment as `usize` |
| `unwrap(?*T)` | Obtain `*T`, trapping on null |
| `slice(pointer, length)` | Unsafe mutable external slice construction |
| `const_slice(pointer, length)` | Unsafe read-only external slice construction |
| `promote(pointer) as dynamic` | Deep-copy arena graph to raw/dynamic storage |
| `promote(pointer) as gc` | Deep-copy arena graph to GC storage |

Compiler-lowered allocation names and reserved `runes_` runtime functions use
checked contracts declared by the prelude. They are not general standard
library APIs.

## Properties and indexing

- arrays: `.len`, integer indexing, ranges, iteration;
- slices: `.len`, mutable `.ptr`, indexing, ranges, iteration;
- read-only slice raw pointers require an explicit FFI conversion policy;
- strings: `.len`, byte indexing, ranges, unsafe `.ptr`;
- structs: declared fields and methods;
- interfaces: declared methods;
- variants: constructors and methods.

String and slice lifetimes remain tied to their backing storage.

## Operator table

| Category | Operators |
|---|---|
| Arithmetic | `+ - * / %` |
| Equality/order | `== != < <= > >=` |
| Boolean | `and or !` |
| Bitwise | `& | ^ << >> ~` |
| Assignment | `=` |
| Exclusive/inclusive range | `..`, `..=` |
| Explicit cast | `as` |
| Address/dereference | `&`, `*` |

`+` also concatenates two strings. Pointer arithmetic is restricted to the
forms described in chapter 7.

## Attributes

| Attribute | Applies to | Meaning/status |
|---|---|---|
| `#[safe]` | extern function | Binding author asserts every typed call is safe |
| `#[link_name("name")]` | function/global/extern function | Select emitted or foreign symbol name |
| `#[callconv("sysv64")]` | function/extern | System V x86-64 calling convention |
| `#[callconv("win64")]` | function/extern | Windows x64 calling convention |
| `#[section("name")]` | function/global | Select object section |
| `#[align(N)]` | struct/global | Power-of-two alignment |
| `#[packed]` | struct | Packed backend layout |
| `#[repr(C)]` | struct | C-compatible field order/layout policy |
| `#[interrupt]` | function/extern | Signature checked; C emission unsupported |

Alignment above the v0.1 backend limit is rejected. `#[safe]` is valid only on
extern functions. `#[interrupt]` requires no parameters and no return value.

## Compiler commands

Normal driver:

```bash
runec check [OPTIONS] [FILE...]
runec run [OPTIONS] [FILE...] [-- PROGRAM_ARGS...]
runec build [OPTIONS] [FILE...] [-o PROGRAM] [-- LINKER_FLAGS...]
runec emit-c [OPTIONS] [FILE...] [-o OUTPUT.c]
runec project [OPTIONS]
```

Project options:

```text
--project FILE
--module-path DIR
--stdlib DIR
--prelude
--no-prelude
```

Environment:

```text
CC               host C compiler, default gcc
CFLAGS           additional host C flags
RUNES_PATH       colon-separated module roots
RUNES_STDLIB     standard-library root
```

Lower-level compiler:

```bash
runes --lex-only FILE
runes --parse-only FILE
runes --dump-ast FILE
runes [project/module options] FILE --emit-c OUTPUT.c
```

## Verification commands

```bash
make test          # unit, core, tooling, scale, differential tests
make test-samples  # positive and expected-failure source inventory
make test-codegen  # compile executable generated C with strict warnings
make test-sanitize # compiler/runtime ASan and UBSan coverage
make fuzz-smoke    # lexer-to-codegen fuzz smoke test
make test-zed      # Zed grammar, highlighting, and icon checks
make test-docs     # executable examples, reference coverage, and links
make install-zed   # install local Zed highlighting and file icons
```

Zed support lives under `editors/zed/`; VS Code support is under `runes-lang/`.
After installation, choose **Runes Material Icon Theme** in Zed's icon-theme
selector. Restart Zed after installing or updating its extensions.

## Runtime versus standard library

The compiler runtime provides only machinery required by generated programs:

- checked traps and indexing/arithmetic helpers;
- raw allocation;
- regional arenas;
- deep promotion;
- scoped garbage collection;
- string/UTF-8 primitives;
- generated-code descriptors and support.

Collections, filesystem/path APIs, buffered input, networking, HTTP, scanners,
formatting, threading, graphics, tensors, and ML belong in the standard library
or separate packages. See the runtime requirements and stdlib roadmap.

## Implemented feature inventory

This table distinguishes working language/compiler features from planned
library work.

| Area | Implemented now |
|---|---|
| Source | `.runes` files, newline/semicolon statements, line/block comments, UTF-8 literals |
| Scalars | fixed-width integers, `usize`, floats, `bool`, Unicode `char`, borrowed `str`, `void` |
| Variables | explicit types, `:=` inference, `const`, shadowing, globals, `volatile` storage |
| Expressions | checked arithmetic, comparison, boolean/bitwise operations, explicit casts, ranges |
| Functions | named results, early return cleanup, forward references, realm-qualified functions |
| Control | statement/value `if`, `while`, `loop`, `for`, `break`, `continue`, statement/value `match` |
| Errors | nominal error sets, `!T`, `!void`, `try`, `catch`, `Ok`/`Err` matching |
| Aggregates | tuples/destructuring, fixed arrays, mutable/read-only slices, structs, variants |
| Abstraction | methods, interfaces, generic functions/types/variants/methods, exact constraints |
| Closures | borrowing nested functions, `move f`, first-class storage and invocation |
| Modules | inline/external modules, visibility, qualified paths, private `use`, canonical loading |
| Projects | strict `runes.toml`, module roots, local path dependencies, automatic `std` namespace |
| Memory | stack checking, raw ownership, nested arenas, deep promotion, precise scoped GC, flex realm |
| Pointers | non-null/nullable pointers, address-of, unwrap, checked slices, unsafe arithmetic/casts |
| Systems | extern functions/globals, selected ABI/layout attributes, volatile/MMIO, inline assembly |
| Backend | readable C11 emission, GCC/Clang build driver, runtime linking |
| Tooling | check/run/build/emit-C/project commands, Zed and VS Code highlighting, test/fuzz targets |

Not listed as implemented: general collections, owning strings, filesystem,
safe input, networking, concurrency library APIs, graphics, numerical arrays,
ML, async, or a remote package ecosystem. Those are roadmap work rather than
hidden compiler builtins.

## Current limitations

Backend and platform:

- hosted C11 backend only;
- primary tested target is Linux x86-64 with GCC or Clang;
- no native object backend or complete freestanding profile;
- `#[interrupt]` requires an external assembly stub.

Language/tooling:

- no variadic functions or function overloading;
- no async language/runtime;
- no `defer`, deterministic destructors, or general cleanup construct;
- no const generics, higher-kinded types, specialization, or variance;
- no wrapping/saturating arithmetic forms or unchecked indexing;
- no import aliases, wildcard imports, or public re-exports;
- extern visibility is always public, global visibility is private-only, and
  method visibility is incomplete;
- no automatic C-string conversion or lifetime extension;
- no pipeline syntax in v0.1;
- no package registry, remote dependency fetching, version solver, or lockfile.

Garbage collection:

- one owning OS thread;
- no cross-thread GC references;
- no concurrent, generational, compacting, weak-reference, or finalizer support;
- no public registered-root or `gc_free` API.

Standard library:

- the `std` namespace and module root exist;
- most general library modules are not implemented yet;
- raw input currently uses unsafe FFI until safe stdlib wrappers are written.

## Keywords

```text
and as asm bool break catch char const continue dynamic else error extern
f f32 f64 false flex for gc i8 i16 i32 i64 if interface loop match method
mod move null or promote pub regional return self sizeof alignof stack str
true try type u8 u16 u32 u64 unsafe use usize void volatile while
```

## Related documents

- [Compact language specification](../specv0_1.md)
- [Project and manifest reference](../projects-and-modules.md)
- [Runtime implementation requirements](../v0.1-runtime-requirements.md)
- [Standard-library internal documentation](../internal/stdlib/README.md)
- [Locked hardening decisions](../hardening-decisions.md)

[Back to the handbook index](README.md)
