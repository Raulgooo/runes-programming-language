# Learn Runes

This handbook teaches the Runes language implemented on the `v0-1-exp`
branch. It assumes you know how to edit a text file and run a terminal command,
but it does not assume prior experience with compilers, type systems, manual
memory management, or systems programming.

Runes is experimental. It compiles to C11, currently targets hosted Linux
x86-64, and is not self-hosted. Its application foundation now includes
borrowed and owning UTF-8 text, realm-aware vectors, typed formatting and
parsing, static readers/writers, explicit buffering, bounded line input, and
safe standard-terminal I/O, byte-preserving lexical paths, and owning hosted
files. Networking, concurrency, and broader application libraries still need
work.

## Reading order

1. [Getting started](01-getting-started.md): build the compiler and run a
   program.
2. [Values, types, and variables](02-values-and-types.md): store and calculate
   data.
3. [Functions, control flow, and errors](03-functions-and-control-flow.md):
   organize behavior and handle failure.
4. [Data modeling](04-data-modeling.md): arrays, slices, structs, variants,
   methods, interfaces, generics, and closures.
5. [Projects, modules, and visibility](05-projects-and-modules.md): split real
   programs across files and packages.
6. [Memory realms](06-memory-realms.md): choose stack, explicit heap, arena, or
   garbage-collected allocation per function.
7. [Pointers, unsafe code, FFI, and input](07-unsafe-ffi-and-io.md): cross the
   boundary into operating-system and C APIs safely.
8. [Language and tooling reference](08-reference.md): attributes, builtins,
   commands, limitations, and keywords.
9. [Glossary](09-glossary.md): plain-language definitions of compiler and
   systems terms used throughout the handbook.
10. [Using the standard library](10-using-the-standard-library.md): combine
    text, parsing, `Vec<T>`, `String`, formatting, writers, terminal I/O, and
    lexical paths and files.

You do not need to understand memory realms or pointers before writing ordinary
calculations, parsers, data transformations, and command-line logic. Begin with
chapters 1 through 4. Read chapters 6 and 7 before writing allocation-heavy,
FFI, operating-system, networking, or runtime code.

## Documentation conventions

Code marked **works now** is intended to match the current compiler. A section
marked **current limitation** describes behavior that is incomplete or
deliberately absent. “Runtime” means the small C support layer required by
compiled programs. “Standard library” means reusable Runes modules such as
collections, files, networking, and text utilities.

The handbook explains how to use the language. The normative compact rules are
in [specv0_1.md](../specv0_1.md). Project manifest details are also collected
in [projects-and-modules.md](../projects-and-modules.md), and future library
work is tracked in the
[standard-library internal documentation](../internal/stdlib/README.md).

For exhaustive syntax and semantics, use the
[complete language reference](../reference/README.md). Unlike this progressive
handbook, the reference includes every implemented form, conversion rule,
runtime check, and implementation limitation.

## Fast lookup

| Question | Go to |
|---|---|
| How do I run one file? | [Getting started](01-getting-started.md#check-run-and-build) |
| Why does a function use `= result: T`? | [Functions](03-functions-and-control-flow.md#returning-a-value) |
| Does `print` insert spaces? | [Getting started](01-getting-started.md#printing) |
| What is the difference between an array and a slice? | [Data modeling](04-data-modeling.md#arrays-and-slices) |
| What does `pub` change? | [Visibility](05-projects-and-modules.md#public-and-private-declarations) |
| Which function realm should I choose? | [Realm selection](06-memory-realms.md#choosing-a-realm) |
| Why does `read` require `unsafe`? | [Safe I/O wrappers](07-unsafe-ffi-and-io.md#safe-wrappers-around-unsafe-ffi) |
| How do I parse and format application data? | [Using the standard library](10-using-the-standard-library.md) |
| How do I join or normalize paths? | [Using the standard library](10-using-the-standard-library.md#work-with-paths) |
| How do I safely open and close a file? | [Using the standard library](10-using-the-standard-library.md#open-and-use-files) |
| What is implemented but incomplete? | [Current limitations](08-reference.md#current-limitations) |
| What does an unfamiliar term mean? | [Glossary](09-glossary.md) |
