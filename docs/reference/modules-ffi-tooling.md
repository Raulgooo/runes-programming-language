# Modules, Projects, FFI, Builtins, and Tooling

## Modules and names

An inline module contains declarations directly:

```runes
mod math {
    pub f double(value: i32) = result: i32 { result = value * 2 }
}
```

An external declaration has no body:

```runes
mod math
```

For an external `mod name`, the loader searches for exactly one of
`name.runes` or `name/mod.runes`, first beside the declaring file and then in
configured module roots. Both forms at one location are ambiguous. Multiple
root matches are ambiguous. No match is an error.

A flat `name.runes` module resolves child modules beneath `name/`. Canonical
paths identify loaded modules: a completed module can be reused, while
re-entering a module still being loaded is a dependency cycle.

Qualified access uses dots:

```runes
math.double(21)
domain.Box<i32>
```

`use path.member` imports the final public member into the current scope. The
root namespace must already exist through `mod`, `std`, or a manifest
dependency. There are no aliases, wildcard imports, or public re-exports.

## Visibility

Declarations inside a module are private unless their declaration kind accepts
`pub`.

| Declaration | Public syntax | Current behavior |
|---|---|---|
| Function | `pub f name()` | Enforced |
| Struct/variant | `pub type Name` | Enforced |
| Interface | `pub interface Name` | Enforced |
| Error set | `pub error Name` | Enforced |
| Child module | `pub mod name` | Enforced |
| Fields/variant arms | no independent marker | Follow accessible containing type |
| Extern function/variable | no private form | Exposed; known gap |
| Module global | no working public form | Private; known gap |
| Method | marker parsed | Enforcement incomplete |
| `use` | no public form | Private import only |

Every module segment crossed from another module must be public. Visibility
does not relax type, unsafe, realm, or lifetime rules.

## Projects

A project uses a strict `runes.toml`:

```toml
[project]
name = "server"
entry = "src/main.runes"

[modules]
roots = ["src", "modules"]

[dependencies]
shared = { path = "packages/shared/src" }
logging = "../logging/src"
```

`[project]` requires `name` and `entry`. The default module root is `src` when
`[modules]` is absent. Unknown sections and fields are errors. Manifest paths
are relative to the manifest, and the driver discovers the nearest manifest by
walking upward unless `--project` selects one explicitly.

A dependency name must be a unique Runes identifier and cannot be `std`. It
creates a top-level namespace automatically; do not redeclare it with `mod`.
The path may name a `.runes` file or a directory containing `mod.runes`.

Only local path dependencies exist. There is no registry, download, Git source,
version solver, lockfile, or build-script execution.

Additional roots come from repeated `--module-path` options and the
colon-separated `RUNES_PATH` environment variable. Effective lookup order is:

1. declaring file's directory;
2. manifest roots;
3. command-line roots;
4. `RUNES_PATH` roots.

## Standard namespace and prelude

`std` is a reserved top-level namespace. Its root is selected in this order:

1. `--stdlib DIR`;
2. `RUNES_STDLIB`;
3. repository `src/std` beside a development compiler;
4. installed `../lib/runes/std` relative to the compiler.

Normal `runec` commands load `std/prelude.runes`. The prelude declares
compiler/runtime ABI contracts; it does not import a general standard library.
Use `--no-prelude` when supplying every contract manually. The lower-level
`runes` binary loads the prelude only with `--prelude`, except for applicable
manifest-driven behavior.

## Foreign declarations

```runes
extern f read(fd: i32, buffer: *u8, count: usize) = result: i64
extern u64 external_counter
```

An extern function or variable declares a host symbol; it does not define it.
The compiler checks the Runes declaration and calls but cannot verify that the
linked symbol has the same ABI, that passed pointers are valid, or that foreign
code obeys Runes lifetime and thread rules.

Calling an ordinary extern function requires `unsafe`. `#[safe]` is an
assertion by the binding author that every well-typed call is safe:

```runes
#[safe]
#[link_name("abs")]
extern f c_abs(value: i32) = result: i32
```

`#[safe]` changes call-site checking only. It emits no ABI property and should
not be used for functions whose safety depends on unverified pointer/length or
lifetime contracts.

Variadic foreign functions are unsupported. Bind a fixed-signature wrapper.
Runes `str` is a pointer/length pair, not `char *`; conversion must be explicit.

## Attribute matrix

| Attribute | Valid targets | Contract |
|---|---|---|
| `#[safe]` | extern function | Suppresses unsafe call-site requirement |
| `#[link_name("name")]` | Runes function, global, extern function | Selects emitted/foreign symbol; invalid on `main` |
| `#[section("name")]` | Runes function or global | Selects emitted object section |
| `#[callconv("sysv64")]` | Runes/extern function | System V x86-64 convention |
| `#[callconv("win64")]` | Runes/extern function | Windows x64 convention |
| `#[align(N)]` | struct or global | Nonzero power-of-two alignment, at most `2^28` |
| `#[packed]` | struct | Packed C backend layout |
| `#[repr(C)]` | struct | C field-order/layout policy |
| `#[interrupt]` | Runes/extern function | Requires no parameters/result; C emission rejected |

Unknown, duplicate, malformed, and inapplicable attributes are errors. String
arguments must be nonempty and contain no NUL. `#[interrupt]` is intentionally
partial: its signature is checked, but an external assembly entry stub is
required because the v0.1 C backend refuses to emit it.

## Inline assembly

```runes
unsafe { asm { "cli; hlt" } }

u64 value = 0
unsafe { asm { "mov %cr3, %rax" } -> value }
```

Assembly uses target-specific GNU compiler syntax. An optional output names an
existing mutable integer or pointer binding. Undefined, const, unsupported, or
wrongly typed outputs are errors. Assembly strings cannot contain NUL.

The compiler does not model arbitrary assembly side effects. Correct
instructions, clobbers, privileges, memory effects, and platform applicability
are the programmer's responsibility.

## Language builtins

| Form | Type/behavior |
|---|---|
| `print(values...)` | At least one primitive, pointer, or error value; writes consecutively then newline |
| `sizeof(Type)` | Target C size as `usize` |
| `alignof(Type)` | Target C alignment as `usize` |
| `unwrap(value)` | Exactly one `?*T`; traps on null and yields `*T` |
| `slice(pointer, length)` | Unsafe mutable slice from non-null pointer |
| `const_slice(pointer, length)` | Unsafe read-only slice from non-null pointer |
| `promote(pointer) as dynamic` | Deep arena-to-raw graph clone |
| `promote(pointer) as gc` | Deep arena-to-GC graph clone |

`print` inserts no separators. `str` values are length-aware. Aggregates and
closures are not printable directly.

`alloc(size)` is a compiler-recognized prelude contract rather than a lexical
keyword. Its ownership changes with the active realm; see the dedicated
[`alloc()` reference](allocation.md).

## Default prelude contracts

The default prelude currently declares:

- `raw_alloc`, `raw_free`, and realm-sensitive `alloc`;
- `runes_gc_collect` and GC diagnostic counters;
- `memset`, `memcpy`, and `sqrt`;
- length-aware string comparison, UTF-8 validation/decoding/encoding, hashing,
  checked string construction, and raw C-string conversion.

These are trusted compiler/runtime ABI contracts, not a complete standard
library. Reserved `runes_` functions may receive special lowering and safety
treatment. Application code should prefer eventual safe standard-library
wrappers where available.

The exact signatures are authoritative in
[`src/std/prelude.runes`](../../src/std/prelude.runes).

## Compiler commands

```bash
runec check [OPTIONS] [FILE...]
runec emit-c [OPTIONS] [FILE...] [-o OUTPUT.c]
runec build [OPTIONS] [FILE...] [-o PROGRAM] [-- CC_FLAGS...]
runec run [OPTIONS] [FILE...] [-- PROGRAM_ARGS...]
runec project [OPTIONS]
```

- `check`: lex, parse, monomorphize, resolve, and type/realm-check.
- `emit-c`: additionally generate readable C.
- `build`: generate C and compile/link it with the host compiler.
- `run`: build a temporary executable and pass arguments following `--`.
- `project`: print resolved manifest, paths, prelude state, and dependencies.

Shared options are `--project`, repeated `--module-path`, `--stdlib`,
`--prelude`, and `--no-prelude`. `-o`/`--output` applies to emitted C or the
built program. Environment variables are `CC`, `CFLAGS`, `RUNES_PATH`, and
`RUNES_STDLIB`.

The lower-level compiler supports `--lex-only`, `--parse-only`, `--dump-ast`,
`--emit-c FILE`, the project/module options, and `--project-info`.
