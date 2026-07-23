# Implementation Status and Limitations

This page distinguishes complete language behavior from partial backend,
tooling, and library support. A recognized form is not necessarily executable
on every target.

## Backend and platform

- Hosted C11 is the only backend.
- Linux x86-64 with GCC or Clang is the primary tested target.
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

## Visibility gaps

- Extern declarations are exposed across their containing module.
- Module globals cannot currently be public.
- Method visibility markers are parsed but not consistently enforced.
- Fields and variant arms follow their containing type; they have no separate
  visibility.
- `use` is private and cannot re-export.
- Import aliases and wildcard imports are unavailable.

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

The `std` namespace, module loader, prelude, and a small set of initial modules
exist. A broad general-purpose standard library does not.

Not currently supplied as complete safe library APIs:

- owning/growable strings and collections;
- filesystem and path operations;
- buffered input/output and formatting;
- networking, HTTP, and protocol helpers;
- threads, synchronization, or async runtime;
- graphics, numerical arrays, tensors, or ML APIs;
- package registry or remote package client.

Unsafe FFI remains necessary for raw operating-system input until safe wrappers
are implemented.

## Unsupported language features

v0.1 deliberately has no:

- variadic functions;
- function or method overloading;
- default function arguments;
- macros or general compile-time evaluation;
- const generics, higher-kinded types, specialization, or variance;
- runtime generic type erasure;
- async/await or language concurrency model;
- `defer`, deterministic destructors, or general cleanup construct;
- wrapping/saturating arithmetic syntax or unchecked indexing;
- pipeline syntax;
- import aliases, wildcard imports, or public re-exports.

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
- no language server with semantic completion/refactoring in this repository;
- editor integrations currently focus on syntax, highlighting, brackets, and
  file icons;
- diagnostics are compiler-oriented and do not yet have stable error codes.

## Deferred import ergonomics

The current `use module.member` form can import one public function, type,
interface, error set, or child module after its root module is present in the
module graph. It has no alias, grouped-import, or public re-export syntax, and
`use` does not independently load an arbitrary filesystem module.

Future language-design work should evaluate, without committing v0.1 to a
specific syntax:

- import aliases for resolving collisions and shortening qualified names;
- grouped imports from one module;
- explicit public re-exports for package façade modules;
- allowing `use` to resolve/load a module without a separate `mod`
  declaration, while preserving deterministic module identity and ambiguity
  diagnostics.

Methods should remain receiver-based rather than independently importable.
Importing `Type.method` as a free function would blur method and function
lookup, complicate generic/interface dispatch, and create avoidable name
collisions. Users should import the owning type or interface and invoke the
method through its receiver.

This is a design note only. No parser, resolver, or visibility behavior is
scheduled or changed by this entry.

## Status discipline

When an implementation restriction changes, update this page, the normative
specification if applicable, the [feature matrix](../feature-matrix.md), and an
executable positive or negative test in the same change.
