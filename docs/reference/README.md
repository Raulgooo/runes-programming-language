# Runes v0.1 Complete Language Reference

This reference describes the language accepted by the current C bootstrap
compiler. It is organized for lookup rather than as a tutorial. New readers
should begin with the [handbook](../guide/README.md).

## Status and authority

Runes v0.1 is experimental. The compiler emits hosted C11 and is primarily
tested on Linux x86-64 with GCC and Clang.

The normative source of truth is the v0.1 specification. Use this order when
deciding what the language promises and what the bootstrap can execute today:

1. the normative [v0.1 specification](../specv0_1.md) for language behavior;
2. [implementation status](implementation-status.md) and passing compiler
   tests for current bootstrap availability;
3. this complete reference;
4. the tutorial handbook and examples;
5. editor grammars and highlighting.

A mismatch remains a defect even when it is a documented backend limitation.
Record it in the
[feature matrix](../feature-matrix.md) rather than silently relying on the
lower-authority document. Accidental or untested compiler behavior is not
automatically a language guarantee.

## Reference map

| Need | Document |
|---|---|
| Tokens, literals, grammar, declarations, expressions, statements, patterns | [Syntax](syntax.md) |
| Types, conversions, evaluation, functions, aggregates, generics, errors | [Semantics](semantics.md) |
| `alloc()`, ownership, cleanup, and raw-allocation differences | [Allocation](allocation.md) |
| Realms, allocation, lifetimes, promotion, GC, pointers, and unsafe operations | [Memory and unsafe](memory-and-unsafe.md) |
| Modules, projects, visibility, FFI, ABI attributes, builtins, and commands | [Modules, FFI, and tooling](modules-ffi-tooling.md) |
| Implemented standard-library modules and APIs | [Standard library](standard-library.md) |
| Borrowed UTF-8 search, views, trimming, splitting, and traversal | [Borrowed text](text.md) |
| Owning realm-aware growable UTF-8 text | [`String`](string.md) |
| Typed formatting into buffers, `String`, and static writers | [Formatting](format.md) |
| Allocation-free integer, boolean, and Unicode-scalar parsing | [Parsing](parse.md) |
| Readers, writers, buffering, line input, and standard streams | [I/O](io.md) |
| Byte-preserving lexical paths and syscall representation | [Paths](path.md) |
| Owning hosted files, metadata, bounded reads, and path operations | [Filesystem](fs.md) |
| Complete `Vec<T>` API, ownership, realm, and safety contract | [`Vec<T>`](vec.md) |
| Supported, partial, unsupported, and platform-dependent behavior | [Implementation status](implementation-status.md) |
| Normative compact rules | [Specification](../specv0_1.md) |
| Coverage evidence and documentation ownership | [Feature matrix](../feature-matrix.md) |

## Notation

Grammar fragments use the following notation:

- quoted text is literal source text;
- `name` denotes a grammar production or lexical category;
- `(x)` groups grammar terms;
- `x?` means zero or one occurrence;
- `x*` means zero or more occurrences;
- `x+` means one or more occurrences;
- `x | y` means either alternative;
- prose constraints beneath a production are part of the rule.

The grammar is descriptive. Where newline sensitivity makes a compact grammar
misleading, the rule is stated in prose and accompanied by examples.

## Completeness contract

This reference is considered complete only while all of these remain true:

- every accepted token and user-facing AST form has a syntax entry;
- every enforced typing, lifetime, realm, and runtime-checking rule has a
  semantics entry;
- every public builtin, prelude contract, attribute, project option, and
  compiler command has an entry;
- positive documentation examples pass `runec check`;
- negative documentation examples fail with their declared diagnostic;
- partial and backend-rejected forms are labeled where they are introduced;
- `make test-docs` succeeds.

See [contributing-docs.md](../contributing-docs.md) for the update checklist.

## Quick indexes

### Declarations

`const`, `error`, `extern`, `f`, `interface`, `method`, `mod`, `pub`, `type`,
`use`, `volatile`, plus the function realm qualifiers `stack`, `dynamic`,
`regional`, `gc`, and `flex`.

### Control flow

`if`, `else`, `while`, `loop`, `for`, `break`, `continue`, `return`, `match`,
`try`, and `catch`.

### Low-level forms

`unsafe`, pointer types and operations, `asm`, `volatile`, `extern`, attributes,
`sizeof`, `alignof`, raw slices, allocation, and promotion.

### Builtins and prelude contracts

`print`, `sizeof`, `alignof`, `unwrap`, `slice`, `const_slice`, `promote`,
`alloc`, `raw_alloc`, `raw_free`, GC controls, memory primitives, and UTF-8
runtime primitives. The last group is compiler/runtime infrastructure rather
than a general-purpose standard library.
