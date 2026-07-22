# Projects and Modules

Runes projects use a strict `runes.toml` manifest. The compiler discovers the
nearest manifest by walking from the current directory toward the filesystem
root. Manifest paths are resolved relative to the manifest, so builds do not
depend on the directory from which individual source files are opened.

## Quick start

```text
hello/
|-- runes.toml
|-- src/
|   |-- main.runes
|   `-- server.runes
|-- modules/
|   `-- protocol/
|       `-- mod.runes
`-- packages/
    `-- shared/
        `-- src/
            `-- mod.runes
```

```toml
[project]
name = "hello"
entry = "src/main.runes"

[modules]
roots = ["src", "modules"]

[dependencies]
shared = { path = "packages/shared/src" }
```

Both dependency forms below are accepted:

```toml
[dependencies]
shared = { path = "packages/shared/src" }
logging = "../logging/src"
```

Only local path dependencies are implemented. Dependency names must be valid
Runes identifiers, must be unique, and cannot be `std`. Unknown manifest
sections and fields are errors. `[project]` requires both `name` and `entry`.
When `[modules]` is omitted, its root defaults to `src`.

From the project directory:

```bash
runec project
runec check
runec run
runec build -o build/hello
runec emit-c -o build/hello.c
```

`runec project` prints the selected manifest, canonical entry, standard-library
location, prelude state, module roots, and path dependencies. Pass
`--project path/to/runes.toml` to select a manifest explicitly.

## Local modules

`mod` declares a module. `use` imports a public member that is already present
in the module graph:

```runes
mod server
mod protocol

use server.start
use protocol.Message
```

For `mod server`, the loader first checks beside the declaring file:

```text
server.runes
server/mod.runes
```

If neither exists, it checks each configured module root for those same two
forms. A local match takes precedence over module roots. Both forms in one
location are ambiguous, and matches in multiple module roots are also
ambiguous. The compiler reports every conflicting path instead of selecting
one based on incidental directory order.

Nested flat modules use a same-named directory for children. For example,
`math.runes` containing `pub mod constants` resolves its child from
`math/constants.runes` or `math/constants/mod.runes`.

Only `pub` declarations cross module boundaries. `use math.answer` imports the
last path segment as `answer`; aliases, wildcard imports, and re-exports are not
implemented yet. Qualified access such as `math.answer()` does not require a
`use` declaration.

## Path dependencies

A manifest dependency creates a top-level module namespace automatically:

```runes
use shared.parse

f main() {
    print(parse("42"))
}
```

Do not add `mod shared`; the manifest already defines that root namespace. A
dependency path may name a `.runes` file or a directory containing
`mod.runes`. Its nested modules follow the normal file rules.

All files are identified by canonical paths. Reusing an already loaded module
is allowed. Entering a module that is still being loaded is a dependency cycle
and produces a diagnostic containing the loading chain.

There is no registry, downloading, semantic-version solver, Git dependency,
lockfile, or build-script execution in v0.1. Keeping dependencies path-only
makes bootstrap builds deterministic and offline.

## Standard library

`std` is a reserved top-level namespace and cannot be shadowed by a path
dependency. Its root is [src/std/mod.runes](../src/std/mod.runes), which exposes
the library modules implemented under `src/std/`.

The compiler searches for the standard library in this order:

1. `--stdlib DIR`;
2. `RUNES_STDLIB`;
3. `src/std` beside a development compiler binary;
4. `../lib/runes/std` relative to an installed compiler binary.

Normal `runec` commands load `std/prelude.runes` automatically. The prelude
contains only compiler/runtime ABI declarations; it does not import the rest of
the standard library. Use `--no-prelude` for kernels, freestanding targets, or
programs that declare every required contract themselves.

The lower-level `runes` compiler does not load the prelude unless passed
`--prelude`, except that manifest-driven builds enable it by default.

## Additional module roots

For one-off builds, add roots without editing the manifest:

```bash
runec check --module-path ../shared/modules src/main.runes
```

`--module-path` may be repeated. `RUNES_PATH` provides additional roots as a
colon-separated list on Unix:

```bash
RUNES_PATH=../shared:../generated runec check src/main.runes
```

The effective order is the importing file's directory, manifest module roots,
explicit `--module-path` values, and then `RUNES_PATH`. Local files have
precedence; ambiguity among configured roots is rejected.

## Multiple root files

Explicit root files remain supported:

```bash
runec check declarations.runes app.runes
```

Their top-level declarations are merged into one program. This is useful for
small experiments and generated declarations, but named modules and a manifest
are preferable for real projects because they preserve namespaces and
visibility boundaries.
