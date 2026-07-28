# Implementation Status and Limitations

This page distinguishes complete language behavior from partial backend,
tooling, and library support. A recognized form is not necessarily executable
on every target.

## Backend and platform

- Hosted C11 is the only backend.
- Linux x86-64 with GCC or Clang is the primary tested target.
- Raw Linux x86-64 syscalls use an external assembly register bridge. The
  command driver selects target-specific link inputs from the resolved target.
- Explicit targets are selected by CLI, manifest, or host default. The current
  exact triples are `x86_64-unknown-linux-gnu`,
  `x86_64-unknown-linux-none`, and `x86_64-unknown-runes-none`.
- There is no native object-code backend.
- The freestanding/kernel profile is incomplete and requires manual runtime and
  ABI work.
- Generated internal symbols are deterministic, but only extern declarations
  and explicit link names are stable foreign ABI surfaces.
- `#[interrupt]` signatures are checked, but C emission always fails; use an
  external assembly entry stub.
- A value-producing `if` or `match` used directly as `return`'s expression
  passes frontend checking but is not lowered. Assign it to the named result
  first.
- Inline assembly and calling conventions are target/compiler dependent.

## Realm specialization

- Direct generic and non-generic `flex` free-function calls are specialized by
  the caller's effective stack, dynamic, regional, or GC realm.
- Nested, recursive, imported, and aliased direct calls retain that inferred
  realm, and generated symbols are deterministic.
- Explicit `alloc()` in those specializations lowers directly to the selected
  dynamic, regional, or GC operation. An allocating stack specialization is a
  compile-time error.
- `when realm stack|dynamic|regional|gc` blocks in free functions are selected
  during specialization. Inactive blocks are removed before resolution and
  type checking, and optional `else` is supported.
- `in <realm>` declarations and `except(...)` exclusions are parsed for
  functions, methods, types, and interfaces. The compiler validates overload
  families for duplicate cases, fallback conflicts, visibility, generic arity,
  and exclusions.
- Direct free-function and method calls select an exact realm definition first,
  then a shared fallback. Generic arguments combine with the inferred realm,
  only demanded instances are emitted, and exclusions or missing definitions
  fail during specialization.
- Struct and variant families now select demanded realm-specific layouts,
  constructors, descriptors, and deterministic hidden type identities.
  Generic families, imported aliases, transitive aggregate fields, missing
  variants, and blacklists are covered.
- Hidden variants propagate through constructors, returns, generic arguments,
  and automatically specialized ordinary function parameters. Incompatible
  assignments are rejected by nominal type checking.
- Realm-overloaded methods on realm-specific receivers dispatch from the
  receiver's persistent owner variant. Methods on ordinary receivers continue
  to use effective execution realm.
- Shared and generic methods on generic realm-specific owner types retain
  their owner type arguments. Owner-sensitive calls continue to select the
  dynamic, regional, or GC implementation after the value passes through an
  automatically specialized ordinary function.
- Typed fallible array allocation, resize, and release are implemented through
  `std.allocation`. Lowering is static, GC sequences receive an explicit
  initialized count, failed growth preserves the old allocation, and nested
  regional owner mismatches are rejected by the runtime. A public `Vec<T>` is
  still library work.
- Realm-specific interface contracts remain planned work.
- Erased flex function values retain the earlier runtime behavior; direct-call
  specialization does not yet define a realm-polymorphic function-value ABI.
  A flex declaration containing `when realm` therefore cannot currently be
  converted to a function value; it requires a direct statically specialized
  call.

## Visibility gaps

- Extern declarations are exposed across their containing module.
- Constant module globals can be public with `pub const`. Mutable and volatile
  public globals remain rejected.
- Method visibility markers are parsed but not consistently enforced.
- Fields and variant arms follow their containing type; they have no separate
  visibility.
- `use` is private and cannot re-export.
- Grouped imports and wildcard imports are unavailable.

These are implementation gaps, not recommendations for long-term API design.

## Prelude and runtime surface

The default prelude is intentionally small. It declares compiler-required
allocation, GC, memory, math, and UTF-8 contracts. Most are low-level ABI
surfaces rather than ergonomic application APIs.

The compiler/runtime recognizes `raw_alloc_aligned`, but the default prelude
does not currently declare it. A program must explicitly provide the matching
extern declaration. The reference calls this out instead of pretending it is
automatically in scope.

There is no public foreign registered-root API, public `gc_free`, or automatic
C-string conversion.

## Garbage collector

- one owning OS thread;
- no cross-thread GC references;
- no concurrent or incremental marking;
- no generations or compaction;
- no weak references or finalizers;
- no public manual free;
- collection only at allocation slow paths or explicit collection calls.

## Standard library

The `std` namespace, module loader, and prelude exist. The currently tested
library surface is:

- `std.core`: `Option<T>`, `Result<T, E>`, their initial methods, and portable
  foundational errors;
- `std.bytes`: allocation-free `fill`, `copy`, `equal`, `find`, and
  `starts_with`;
- `std.io`: allocation-free safe stdin/stdout/stderr operations with portable
  `Result<usize, IoError>` signatures on hosted Linux x86-64;
- `std.os.linux`: raw syscall, errno-preserving result, descriptor, and virtual
  memory operations for Linux x86-64.

Target selection and declaration-level `#[cfg]` bind `std.io` to its backend
at compile time. Other hosted operating systems and freestanding targets do not
yet provide `std.io` operations.
See the [current standard-library reference](standard-library.md) for exact
signatures and behavior. A broad general-purpose standard library does not
yet exist.

Not currently supplied as complete safe library APIs:

- owning/growable strings and collections;
- filesystem and path operations;
- buffered input/output and formatting;
- networking, HTTP, and protocol helpers;
- threads, synchronization, or async runtime;
- graphics, numerical arrays, tensors, or ML APIs;
- package registry or remote package client.

The Linux layer encapsulates its syscall FFI, but its mapping and raw-path
operations still require caller-maintained pointer and lifetime invariants.

## Unsupported language features

v0.1 deliberately has no:

- variadic functions;
- function or method overloading;
- default function arguments;
- macros or general compile-time evaluation;
- const generics, higher-kinded types, specialization, or variance;
- runtime generic type erasure;
- async/await or language concurrency model;
- deterministic destructors or compiler-enforced move-only resources
  (`defer expression` exists as an explicit scoped cleanup mechanism);
- wrapping/saturating arithmetic syntax or unchecked indexing;
- pipeline syntax;
- grouped imports, wildcard imports, or public re-exports.

## Lexical limitations

- identifiers are ASCII even though string/character contents support UTF-8;
- integers are decimal or hexadecimal only;
- binary/octal prefixes and numeric digit separators are not implemented;
- floating exponent notation follows a fractional part;
- strings are borrowed length-bearing views, not owning language strings.

## Evaluation caveat

The hosted C backend does not promise a portable left-to-right order for every
independent operand or function argument. Code should sequence observable side
effects in separate statements instead of depending on host C evaluation order.

## Tooling limitations

- no package registry, remote fetching, semantic-version solver, or lockfile;
- no build scripts;
- the initial AST-backed language server supports parser diagnostics, document
  symbols, same-document hover/definition, and language completions, but not
  resolver/type diagnostics, cross-file navigation, references, rename,
  semantic tokens, or formatting;
- editor integrations support syntax highlighting, nested indentation, bracket
  metadata, file icons, and LSP startup;
- diagnostics are compiler-oriented and do not yet have stable error codes.

## Remaining import ergonomics

The current `use module.member` form can import one public function, type,
interface, error set, or child module after its root module is present in the
module graph. `use path as alias` supports explicit local aliases, including
direct imports and aliases of generic declarations. Aliases retain the
original declaration and specialization identity.

It has no grouped-import or public re-export syntax, and `use` does not
independently load an arbitrary filesystem module.

Planned follow-up work should evaluate:

- grouped imports from one module;
- explicit public re-exports for package façade modules;
- canonical full-path module identity in every compiler phase;
- contextual inference that removes unnecessary constructor and method type
  arguments;
- allowing `use` to resolve/load a module without a separate `mod`
  declaration, while preserving deterministic module identity and ambiguity
  diagnostics.

Methods should remain receiver-based rather than independently importable.
Importing `Type.method` as a free function would blur method and function
lookup, complicate generic/interface dispatch, and create avoidable name
collisions. Users should import the owning type or interface and invoke the
method through its receiver.

The ordered implementation plan is tracked in the internal
[import ergonomics plan](../internal/language-design/import-ergonomics-plan.md).

## Status discipline

When an implementation restriction changes, update this page, the normative
specification if applicable, the [feature matrix](../feature-matrix.md), and an
executable positive or negative test in the same change.
