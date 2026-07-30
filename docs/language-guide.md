# Runes v0.1 Language Guide

The language guide is now a chapter-based handbook so that new programmers do
not have to learn syntax, module loading, memory management, garbage collection,
and C interoperability all at once.

Start here:

## [Learn Runes](guide/README.md)

The recommended reading order is:

1. [Getting started](guide/01-getting-started.md)
2. [Values, types, and variables](guide/02-values-and-types.md)
3. [Functions, control flow, and errors](guide/03-functions-and-control-flow.md)
4. [Data modeling](guide/04-data-modeling.md)
5. [Projects, modules, and visibility](guide/05-projects-and-modules.md)
6. [Memory realms](guide/06-memory-realms.md)
7. [Pointers, unsafe code, FFI, and input](guide/07-unsafe-ffi-and-io.md)
8. [Language and tooling reference](guide/08-reference.md)
9. [Glossary](guide/09-glossary.md)
10. [Using the current standard library](guide/10-using-the-standard-library.md)

## Which document should I use?

| Need | Document |
|---|---|
| Learn the language from the beginning | [Handbook](guide/README.md) |
| Look up syntax, commands, attributes, or limitations | [Reference](guide/08-reference.md) |
| Build with text, collections, formatting, parsing, streams, paths, and files | [Using the standard library](guide/10-using-the-standard-library.md) |
| Read the exhaustive syntax and semantics reference | [Complete reference](reference/README.md) |
| Configure a `runes.toml` project | [Projects and modules](projects-and-modules.md) |
| Implement compiler-required C runtime support | [Runtime requirements](v0.1-runtime-requirements.md) |
| Build the future standard library | [Stdlib internal documentation](internal/stdlib/README.md) |
| Read compact normative v0.1 rules | [Language specification](specv0_1.md) |
| Review locked design decisions | [Hardening decisions](hardening-decisions.md) |

## Current status

Runes currently emits hosted C11 and is primarily tested on Linux x86-64 with
GCC and Clang. The v0.1 language core, project loader, compiler runtime, arenas,
deep promotion, and scoped collector are implemented and under hardening. The
`std` application foundation now includes bytes, borrowed/owning UTF-8 text,
realm-aware vectors, typed allocation, formatting, parsing, static
readers/writers, explicit buffering, bounded line input, and safe hosted
terminal I/O, byte-preserving lexical paths, and owning hosted files.
Networking, concurrency, and broader application libraries remain unfinished.
Runes is experimental and is not self-hosted or production-ready.

For the shortest path to a running program:

```bash
make
./runec run src/examples/hello_codegen.runes
```
