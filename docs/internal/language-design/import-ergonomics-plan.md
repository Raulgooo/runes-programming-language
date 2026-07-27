# Import Ergonomics: Current Contract and Next Steps

Status: import aliases and direct imported-generic specialization are
implemented on `v0-1-exp`. The remaining sections are an ordered implementation
plan, not accepted syntax until their milestone lands with tests and public
documentation.

## What works now

The implemented forms are:

```runes
use std.bytes.find
use std.bytes.find as find_byte
use std.bytes as byte_ops
use std.core.Option
use std.core.Option as Maybe
```

Aliases work for modules, functions, types, generic declarations, interfaces,
error sets, and child modules.

The current semantic contract is:

- without `as`, the final path segment is the local name;
- with `as`, the following identifier is the local name;
- the target must already exist in the loaded module graph and be public;
- an alias changes only the local source name;
- aliases preserve declaration identity, nominal type identity, generic
  specialization identity, and emitted ABI names;
- imports are module-scope declarations and are private;
- an import/local or import/import name collision is an error;
- methods cannot be imported independently from their receiver type;
- `pub use`, grouped imports, and wildcard imports are rejected or unavailable.

## Compiler foundation now in place

Before generic inference and specialization, the monomorphizer records each
valid import's local name and target declaration. Generic lookup consults that
binding instead of assuming every unqualified generic belongs to the current
module.

This makes these equivalent:

```runes
use std.core.Option
use std.core.Option as Maybe

Option<i32> first = Option.Some(1)
Maybe<i32> second = Maybe.Some(2)
```

Both names specialize the original `std.core.Option` declaration. They do not
generate unrelated nominal types.

The full body resolver and type checker still run after specialization.
Non-generic aliases are connected to their target types before function
signatures are resolved, allowing aliased interfaces and types in public or
private signatures.

## Known architectural debt

Some compiler subsystems still represent a module by its final textual segment
instead of a canonical declaration identity or complete path. Two unrelated
nested modules with the same leaf name can therefore remain ambiguous in
generic lookup even though ordinary module loading knows their canonical
files.

Before adding complex import trees, replace leaf-name module tracking with one
of:

1. the owning `AST_MOD_DECL` identity during bootstrap compilation; or
2. a canonical module record containing its full logical path and source
   identity.

Generic templates, import bindings, method plans, and generated-declaration
ownership must all use that same identity.

## Milestone 1: canonical module identity

### Goal

Make aliases and generic specialization correct when separate module trees
contain identical leaf names.

### Work

- introduce one canonical module identity shared by loading, binding, and
  monomorphization;
- replace `current_module` leaf-name comparisons;
- store the canonical owner on generic templates and method plans;
- resolve qualified generic calls through module declarations rather than
  strings;
- preserve canonical paths in diagnostics.

### Acceptance

- two dependencies may each contain `core.Option<T>`;
- aliases can select each type without ambiguity;
- specializing one does not reuse the other's generated declaration;
- generated names remain deterministic;
- private members remain inaccessible through either path.

## Milestone 2: grouped imports

Proposed syntax:

```runes
use std.bytes.{copy, equal, find as find_byte}
```

Keep the first implementation deliberately flat:

```text
grouped-use = "use" path ".{" item ("," item)* ","? "}"
item = identifier ("as" identifier)?
```

Nested groups and wildcard items should not be included.

The parser may lower a group into ordinary `AST_USE_DECL` nodes so the
resolver, visibility checks, collision rules, and monomorphizer keep one import
model.

Acceptance requires:

- plain and aliased members in one group;
- a trailing comma;
- generic types in groups;
- duplicate item and local-name collision diagnostics;
- private and missing member diagnostics at the member location.

## Milestone 3: public re-exports

Proposed syntax:

```runes
pub use std.core.Option
pub use internal.parser.parse as parse
```

Rules:

- only module-scope imports may be public;
- every crossed path segment and final target must be public;
- a re-export becomes a public member of the containing module;
- downstream users see the re-exporting module as the source-level path while
  declaration and type identity remain canonical;
- collisions with declarations and other re-exports are errors;
- re-export cycles must be diagnosed with the involved paths.

Do not implement re-exports by copying declarations into the façade module.

## Milestone 4: contextual generic inference

Once imported generic identity is stable, remove avoidable type-argument noise.

Targets:

```runes
use std.core.Option as Maybe

Maybe<i32> empty = Maybe.None()
Maybe<i32> present = Maybe.Some(42)
Maybe<bool> checked = present.map(is_positive)
```

Inference sources:

- constructor payload types;
- the expected destination or named-result type;
- function parameter and return types;
- the expected result of a generic method call.

Explicit arguments remain valid and are required when the context is
ambiguous. Diagnostics must name the uninferred parameter and show the
canonical declaration plus the local alias used at the call site.

## Milestone 5: import-driven module loading

Today `use` only resolves declarations already present through `mod`, `std`, or
a manifest dependency. Consider allowing a `use` path to load a local module
only after canonical module identity is complete.

This milestone must preserve:

- deterministic file selection;
- the existing `name.runes` versus `name/mod.runes` ambiguity diagnostic;
- dependency-cycle detection;
- one canonical identity when both `mod` and `use` mention the same module;
- explicit project-root and dependency boundaries.

This is lower priority than grouped imports, re-exports, and inference because
it changes module graph construction rather than only local naming.

## Tooling work for every milestone

Each syntax milestone must update:

- the parser-backed LSP diagnostics;
- completion after `use`, `as`, and grouped-import separators;
- hover and go-to-definition through aliases;
- document symbols where imports are displayed;
- editor grammar tests;
- the specification, reference, handbook, feature matrix, and implementation
  status.

Cross-file rename should treat an alias as its own local binding: renaming the
alias must not rename the exported declaration.

## Deliberate non-goals

- wildcard imports;
- importing a method as a free function;
- aliases that create new nominal types;
- aliases that change link names;
- nested grouped-import trees in the first grouped-import implementation;
- silently shadowing a declaration with an import.

## Recommended execution order

1. canonical module identity;
2. flat grouped imports;
3. public re-exports;
4. contextual generic constructor and method inference;
5. import-driven module loading;
6. richer LSP navigation and rename after resolver data is exposed to the
   language server.

Every milestone should land independently with positive codegen coverage,
expected-failure diagnostics, documentation updates, and a full `make test`
pass.
