# Projects, Modules, and Visibility

## Why modules exist

A module groups related declarations under a name. It prevents every function
and type in a large program from sharing one global namespace and defines which
parts are available to other modules.

A project manifest tells the compiler where the entry file and module roots
are. A path dependency gives another project or library a stable top-level
module name.

## Inline modules

An inline module is written in one file:

```runes
mod math {
    f hidden_helper(value: i32) = result: i32 {
        result = value * 2
    }

    pub f double(value: i32) = result: i32 {
        result = hidden_helper(value)
    }
}

f main() {
    print(math.double(21))
}
```

`hidden_helper` is usable inside `math`. `double` has `pub`, so code outside
`math` can call it.

## External modules

Declare a module without a body:

```runes
mod math
```

The loader looks for exactly one of:

```text
math.runes
math/mod.runes
```

If both exist in the same location, the module is ambiguous and compilation
fails. If neither exists beside the declaring file, configured module roots
are searched. More than one matching root is also an error.

For a flat `math.runes` module, children live under `math/`. For example,
`pub mod constants` inside `math.runes` resolves from
`math/constants.runes` or `math/constants/mod.runes`.

## Qualified names and `use`

Call a public member through its complete path:

```runes
math.double(21)
```

Or import the final member name:

```runes
use math.double

f main() {
    print(double(21))
}
```

`use` does not load an arbitrary file. The root module must already exist
through `mod`, `std`, or a manifest dependency. It also does not copy code; it
adds a local name for the same declaration.

Aliases, wildcard imports, and public re-exports are not implemented. A `use`
is private to the scope containing it.

## Public and private declarations

Inside a module, declarations are private unless their kind supports and uses
`pub`.

```runes
mod account {
    f validate() = result: bool {
        result = true
    }

    pub f open() = result: bool {
        result = validate()
    }
}
```

This is allowed:

```runes
account.open()
```

This is rejected outside `account`:

```runes
account.validate()
```

Private does not mean “only the declaration itself.” It means “only code inside
the same module.” A private helper can be called by every function in that
module.

Each segment of a nested public path must be public:

```runes
mod library {
    pub mod text {
        pub f byte_length(value: str) = result: usize {
            result = value.len
        }
    }
}

f main() {
    print(library.text.byte_length("hello"))
}
```

`library` is declared in the same root scope as `main`, so it need not be
public there. If `library` were itself a child exposed by another module, that
parent declaration would also need `pub`.

### Implemented visibility table

| Declaration | Private form | Public form | Current behavior |
|---|---|---|---|
| Function | `f helper()` | `pub f helper()` | Enforced |
| Struct/variant | `type Name` | `pub type Name` | Enforced |
| Interface | `interface Name` | `pub interface Name` | Enforced |
| Error set | `error Name` | `pub error Name` | Enforced |
| Child module | `mod name` | `pub mod name` | Enforced |
| Struct fields/variant arms | no per-member marker | inherited | Accessible when the type is accessible |
| Extern function/variable | no working private form | always exposed | Visibility gap |
| Module-level variable | private | no working public form | Visibility gap |
| Method | marker is parsed | not consistently enforced | Visibility gap |
| `use` | private import | no re-export | Re-export missing |

`pub` changes reachability, not safety or memory lifetime. A public unsafe
foreign function still requires `unsafe`. A public arena reference still
cannot escape its arena. A private value is not encrypted or protected at
runtime; it is a compile-time API boundary.

## Creating a project

Recommended layout:

```text
server/
|-- runes.toml
|-- src/
|   |-- main.runes
|   `-- app.runes
|-- modules/
|   `-- protocol/
|       `-- mod.runes
`-- packages/
    `-- shared/
        `-- src/
            `-- mod.runes
```

Manifest:

```toml
[project]
name = "server"
entry = "src/main.runes"

[modules]
roots = ["src", "modules"]

[dependencies]
shared = { path = "packages/shared/src" }
```

`[project]` requires `name` and `entry`. If `[modules]` is absent, the module
root defaults to `src`. Unknown fields and sections are errors rather than
being silently ignored.

Paths are relative to `runes.toml`, not the current shell directory. The
compiler walks upward from the current directory to find the nearest manifest.

```bash
runec project
runec check
runec run
runec build -o build/server
```

`runec project` shows the selected manifest, canonical entry, stdlib path,
prelude state, module roots, and dependencies.

## Local path dependencies

A dependency becomes a top-level module automatically:

```toml
[dependencies]
shared = { path = "packages/shared/src" }
logging = "../logging/src"
```

Then source can use it directly:

```runes
use shared.parse
```

Do not write `mod shared`; the manifest already creates that namespace. The
path may name a `.runes` file or a directory containing `mod.runes`.

v0.1 dependencies are local paths only. There is no registry, network fetch,
Git dependency, semantic-version solver, or lockfile yet.

## Additional search paths

Add a temporary root on the command line:

```bash
runec check --module-path ../generated src/main.runes
```

Repeat `--module-path` as needed. On Unix, `RUNES_PATH` accepts a colon-separated
list:

```bash
RUNES_PATH=../shared:../generated runec check src/main.runes
```

Resolution order is:

1. the declaring file's directory;
2. manifest module roots;
3. explicit `--module-path` roots;
4. `RUNES_PATH` roots.

A local match wins. Ambiguity among configured roots is rejected.

## The `std` namespace

`std` is reserved for the standard library. The compiler finds it from:

1. `--stdlib DIR`;
2. `RUNES_STDLIB`;
3. repository `src/std` beside a development compiler;
4. installed `../lib/runes/std` relative to the compiler binary.

`src/std/mod.runes` declares standard-library child modules. The namespace and
loader exist now, but most library APIs remain to be written.

## Canonical identity and cycles

The loader canonicalizes paths. Referring to one completed module more than
once reuses it. Re-entering a module that is still loading is a dependency
cycle and is rejected with the path chain.

## Common mistakes

- Adding `pub` to a helper that should stay an implementation detail.
- Forgetting `pub` on one segment of a nested module path.
- Expecting `use` to discover/load an undeclared filesystem path.
- Writing both `name.runes` and `name/mod.runes`.
- Declaring a manifest dependency again with `mod`.
- Assuming current extern or method visibility gaps are the intended final
  design.

The complete manifest-only reference is in
[projects-and-modules.md](../projects-and-modules.md).

[Next: Memory realms](06-memory-realms.md)
