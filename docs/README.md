# Runes Documentation

This directory contains the learning material, complete language reference,
normative specification, implementation notes, project documentation, and
executable examples for Runes v0.1.

Runes is experimental. It currently compiles to hosted C11 and is primarily
tested on Linux x86-64 with GCC and Clang.

## Start here

| Goal | Document |
|---|---|
| Install the compiler and run a first program | [Getting started](getting-started.md) |
| Learn Runes progressively | [Learn Runes handbook](guide/README.md) |
| Look up exact syntax or semantics | [Complete language reference](reference/README.md) |
| See the standard library that works today | [Current standard-library reference](reference/standard-library.md) |
| Read the normative language rules | [Runes v0.1 specification](specv0_1.md) |
| Configure modules and a `runes.toml` project | [Projects and modules](projects-and-modules.md) |
| Understand what is implemented or incomplete | [Implementation status](reference/implementation-status.md) |
| Configure editor support and the language server | [Editor support and LSP](tooling/language-server.md) |
| Work on the internal standard-library design | [Stdlib internal documentation](internal/stdlib/README.md) |

The [language guide entry point](language-guide.md) also explains which
documentation layer to use for different tasks.

## Learn the language

The beginner-first handbook is designed to be read in order:

1. [Getting started](guide/01-getting-started.md)
2. [Values, types, and variables](guide/02-values-and-types.md)
3. [Functions, control flow, and errors](guide/03-functions-and-control-flow.md)
4. [Data modeling](guide/04-data-modeling.md)
5. [Projects, modules, and visibility](guide/05-projects-and-modules.md)
6. [Memory realms](guide/06-memory-realms.md)
7. [Pointers, unsafe code, FFI, and input](guide/07-unsafe-ffi-and-io.md)
8. [Language and tooling quick reference](guide/08-reference.md)
9. [Glossary](guide/09-glossary.md)

Use the handbook to learn concepts and idioms. Use the complete reference when
you need every accepted form, restriction, or runtime rule.

## Complete reference

| Topic | Document |
|---|---|
| Tokens, literals, declarations, expressions, statements, patterns, precedence | [Syntax](reference/syntax.md) |
| Types, conversions, evaluation, aggregates, functions, interfaces, generics, closures, errors | [Semantics](reference/semantics.md) |
| Realm-sensitive `alloc()`, ownership, cleanup, and raw allocation | [Allocation](reference/allocation.md) |
| Realms, provenance, lifetimes, promotion, GC, pointers, and unsafe operations | [Memory and unsafe](reference/memory-and-unsafe.md) |
| Modules, projects, visibility, FFI, ABI attributes, builtins, and commands | [Modules, FFI, and tooling](reference/modules-ffi-tooling.md) |
| Implemented `std.core` and `std.bytes` APIs | [Standard library](reference/standard-library.md) |
| Backend, platform, visibility, library, and tooling limitations | [Implementation status](reference/implementation-status.md) |

The [feature coverage matrix](feature-matrix.md) maps language features to
compiler implementation, representative tests, and their reference sections.

## Executable examples

Canonical positive and expected-failure programs live under
[`examples/`](examples/README.md). They include:

- a broad language-reference program;
- pointer, promotion, and lifetime examples;
- stack, dynamic, regional, GC, and flex allocation comparisons;
- expected diagnostics for invalid literals, return types, pointer captures,
  and stack allocation.

Run every documentation example and link check with:

```bash
make test-docs
```

## Specification and design

| Document | Purpose |
|---|---|
| [Runes v0.1 specification](specv0_1.md) | Normative language behavior |
| [Hardening decisions](hardening-decisions.md) | Locked implementation and design decisions |
| [Runtime requirements](v0.1-runtime-requirements.md) | C runtime ABI and behavior required by generated programs |
| [Stdlib internal documentation](internal/stdlib/README.md) | Building guide, implementation plan, and long-term roadmap |
| [Freestanding runtime plan](internal/runtime/README.md) | Internal allocation decoupling, freestanding C, migration, and testing plan |
| [Internal language-design notes](internal/language-design/README.md) | Open compiler, ABI, value-passing, and pointer-design questions |
| [Projects and modules](projects-and-modules.md) | Manifest and module-loading details |

The specification defines what the language promises. Current bootstrap gaps
and workarounds are recorded in the
[implementation-status reference](reference/implementation-status.md).

## Contributing to the documentation

Read [Contributing language documentation](contributing-docs.md) before changing
syntax, semantics, builtins, realms, modules, or public runtime contracts.

A user-visible language change should normally update:

- the normative specification;
- the complete reference;
- the feature matrix;
- positive and negative examples;
- the tutorial where appropriate;
- editor grammar/highlighting where applicable.

Validate documentation changes with `make test-docs` and compiler changes with
the relevant test targets described in the
[tooling reference](reference/modules-ffi-tooling.md#compiler-commands).
