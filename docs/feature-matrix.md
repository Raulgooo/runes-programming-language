# Language Feature Coverage Matrix

This matrix maps implemented user-facing behavior to its documentation and
verification evidence. It is the control document for preventing drift between
the compiler, specification, reference, and handbook.

Status meanings:

- **Complete**: implemented, tested, and covered by the complete reference.
- **Partial**: deliberately restricted or rejected by part of the toolchain.
- **Unsupported**: recognized only to produce a clear rejection, or absent by
  design.

The test column names representative coverage, not every test exercising the
feature.

## Source and declarations

| Feature | Implementation evidence | Representative tests | Reference | Status |
|---|---|---|---|---|
| Identifiers, keywords, comments, newlines | `lexer.c`, `parser.c` | `lexer_test.c`, `core_codegen_multiline_delimiters.runes` | [Lexical structure](reference/syntax.md#lexical-structure) | Complete |
| Decimal, hexadecimal, float, string, char literals | `lexer.c` | `numeric_literacy_tests.runes`, `core_codegen_unicode_strings.runes` | [Literals](reference/syntax.md#literals) | Complete |
| Local/global variables, inference, constants, volatile | `parser.c`, `typecheck.c` | `01_variables.runes`, `core_codegen_globals.runes` | [Variables](reference/syntax.md#variables-and-storage-declarations) | Complete |
| Public module constants | `parser.c`, `resolver.c`, `typecheck.c` | `core_codegen_public_constants.runes`, public-constant error tests | [Visibility](reference/modules-ffi-tooling.md#visibility) | Complete |
| Functions, named results, direct returns, shorthand bodies | `parser.c`, `typecheck.c` | `02_functions.runes`, `core_codegen_named_early_return.runes` | [Functions](reference/syntax.md#functions) | Complete |
| Direct return of value-producing `if`/`match` | frontend accepts; `codegen.c` lacks nested lowering | documentation regression coverage | [Returns](reference/semantics.md#functions-and-returns) | Partial: assign to named result first |
| Structs, compact structs, defaults, volatile fields | `parser.c`, `codegen.c` | `types.runes`, `test_struct_methods.runes` | [Structs](reference/syntax.md#structs) | Complete |
| Variants and payload constructors | `parser.c`, `typecheck.c` | `core_codegen_variants.runes` | [Variants](reference/syntax.md#variants) | Complete |
| Interfaces and implementation blocks | `parser.c`, `typecheck.c` | `core_codegen_interfaces.runes` | [Interfaces and methods](reference/syntax.md#interfaces-and-methods) | Complete |
| Error sets and fallible results | `parser.c`, `typecheck.c` | `core_codegen_errors.runes` | [Errors](reference/semantics.md#errors-and-fallible-values) | Complete |
| Inline/external modules, direct generic imports, and `use ... as ...` aliases | `project.c`, `resolver.c`, `monomorphize.c` | `core_codegen_import_aliases.runes`, module fixtures, import alias error tests | [Modules](reference/modules-ffi-tooling.md#modules-and-names) | Complete; grouped imports and re-exports remain unsupported |
| Extern functions and variables | `parser.c`, `codegen.c` | `core_codegen_extern.runes` | [FFI](reference/modules-ffi-tooling.md#foreign-declarations) | Complete |
| Attributes | `typecheck.c`, `codegen.c` | `core_codegen_systems_attributes.runes` | [Attributes](reference/modules-ffi-tooling.md#attribute-matrix) | Complete |
| `#[interrupt]` declaration validation | `typecheck.c`, `codegen.c` | `codegen_failures/interrupt.runes` | [Attributes](reference/modules-ffi-tooling.md#attribute-matrix) | Partial: emission rejected |

## Types and expressions

| Feature | Implementation evidence | Representative tests | Reference | Status |
|---|---|---|---|---|
| Primitive and constructed types | `types.c`, `typecheck.c` | `phase1_tests.runes`, `core_arrays_pointers.runes` | [Types](reference/semantics.md#type-system) | Complete |
| Tuples and typed destructuring | `parser.c`, `typecheck.c` | `type_tuples.runes` | [Aggregates](reference/semantics.md#aggregate-values) | Complete |
| Fixed arrays and zero initialization | `parser.c`, `codegen.c` | `core_codegen_array_copy.runes` | [Arrays and slices](reference/semantics.md#arrays-and-slices) | Complete |
| Mutable/read-only slices and coercions | `typecheck.c`, `codegen.c` | `core_codegen_slices.runes`, slice error tests | [Arrays and slices](reference/semantics.md#arrays-and-slices) | Complete |
| Mutable/read-only pointers and const-safe FFI | `types.c`, `typecheck.c`, `codegen.c` | `core_codegen_const_pointer_ffi.runes`, `core_readonly_pointer_mutation_error.runes` | [Pointers](reference/memory-and-unsafe.md#pointers) | Complete |
| Arithmetic, comparison, boolean, bitwise operators | `parser.c`, `typecheck.c` | `core_bitwise.runes`, arithmetic error tests | [Operators](reference/syntax.md#operators-and-precedence) | Complete |
| Checked integer operations | `codegen.c`, `runtime.c` | checked arithmetic trap tests | [Runtime checks](reference/semantics.md#runtime-checks) | Complete |
| Indexing and ranges | `typecheck.c`, `codegen.c` | bounds/range trap tests | [Postfix expressions](reference/syntax.md#postfix-expressions) | Complete |
| String operations and UTF-8 boundaries | `typecheck.c`, `runtime.c` | Unicode/string trap tests | [Strings](reference/semantics.md#strings-and-characters) | Complete |
| Numeric, character, and pointer casts | `typecheck.c`, `codegen.c` | char/pointer cast tests | [Conversions](reference/semantics.md#conversions-and-coercions) | Complete |
| `sizeof`, `alignof`, `unwrap`, raw slice constructors | `parser.c`, `typecheck.c` | `12_builtins.runes`, nullable/raw-slice tests | [Builtins](reference/modules-ffi-tooling.md#language-builtins) | Complete |

## Control, abstraction, and memory

| Feature | Implementation evidence | Representative tests | Reference | Status |
|---|---|---|---|---|
| Statement/value `if` | `parser.c`, `typecheck.c`, `codegen.c` | `core_codegen_if_values.runes` | [Control flow](reference/syntax.md#control-flow) | Complete |
| `while`, `loop`, range/array/slice `for` | `parser.c`, `typecheck.c` | `core_codegen_control.runes` | [Loops](reference/syntax.md#loops) | Complete |
| Value/index/pointer loop captures | `parser.c`, `typecheck.c` | `03_control_flow_fixed.runes` | [Loops](reference/syntax.md#loops) | Complete |
| Statement/value match, patterns, guards, exhaustiveness | `parser.c`, `typecheck.c` | `test_match_types.runes`, `core_codegen_variants.runes` | [Patterns](reference/syntax.md#patterns-and-match) | Complete |
| `try`, `catch`, `Ok`/`Err` matching | `parser.c`, `typecheck.c` | `core_codegen_errors.runes` | [Errors](reference/semantics.md#errors-and-fallible-values) | Complete |
| Lexical LIFO `defer expression` cleanup | `parser.c`, `codegen.c` | `core_codegen_defer.runes`, defer error tests | [Control flow](reference/syntax.md#control-flow) | Complete; not a destructor/ownership system |
| Generic functions/types/variants/methods | `monomorphize.c` | generic codegen/error tests | [Generics](reference/semantics.md#generics) | Complete |
| Nested functions and borrowing/move closures | `typecheck.c`, `codegen.c` | closure codegen/escape tests | [Closures](reference/semantics.md#nested-functions-and-closures) | Complete |
| Interfaces and dynamic dispatch | `typecheck.c`, `codegen.c` | interface codegen/error tests | [Methods and interfaces](reference/semantics.md#methods-and-interfaces) | Complete |
| Stack, dynamic, regional, GC, flex realms | `realm_check.c`, `typecheck.c` | realm and nesting tests | [Realms](reference/memory-and-unsafe.md#memory-realms) | Complete |
| Arena escape checks and deep promotion | `typecheck.c`, `runtime.c` | promotion and arena escape tests | [Promotion](reference/memory-and-unsafe.md#deep-promotion) | Complete |
| Precise scoped GC | `codegen.c`, `runtime.c` | GC graph/root tests | [GC](reference/memory-and-unsafe.md#scoped-garbage-collection) | Complete with documented limits |
| Non-null/nullable pointers and provenance | `typecheck.c` | nullable/pointer error tests | [Pointers](reference/memory-and-unsafe.md#pointers) | Complete |
| Unsafe operations, volatile/MMIO, inline assembly | `typecheck.c`, `codegen.c` | systems tests | [Unsafe](reference/memory-and-unsafe.md#unsafe-operations) | Complete, target-dependent |

## Tooling and deliberate omissions

| Feature | Implementation evidence | Representative tests | Reference | Status |
|---|---|---|---|---|
| Project manifests and local dependencies | `project.c`, `runec` | project fixtures | [Projects](reference/modules-ffi-tooling.md#projects) | Complete |
| Explicit targets and declaration `#[cfg]` | `target.c`, `main.c`, `runec` | target tooling tests, `core_codegen_cfg.runes` | [Attributes](reference/modules-ffi-tooling.md#attribute-matrix) | Complete for three x86-64 bootstrap triples |
| Import-driven std loading and reachability-based C emission | `main.c`, `reachability.c`, `codegen.c` | lazy-std fixture, `core_codegen_reachability.runes` | [Commands](reference/modules-ffi-tooling.md#compiler-commands) | Complete; explicit project modules remain eager |
| Check, run, build, emit-C, project commands | `runec`, `main.c` | `test-tooling` | [Commands](reference/modules-ffi-tooling.md#compiler-commands) | Complete |
| General standard library | `src/std/` | n/a | [Status](reference/implementation-status.md#standard-library) | Partial |
| Variadics, overloads, async, macros, const generics | deliberate omission | rejection/absence | [Unsupported features](reference/implementation-status.md#unsupported-language-features) | Unsupported |
| Native backend and complete freestanding target | deliberate omission | n/a | [Backend](reference/implementation-status.md#backend-and-platform) | Unsupported |
