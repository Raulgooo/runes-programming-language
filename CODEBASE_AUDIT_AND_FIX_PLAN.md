# Runes Codebase Audit and Fix Plan

Updated: 2026-07-20

## Verified State

Runes is a dependency-free C bootstrap compiler with lexer, parser, resolver,
typechecker, and a C bootstrap backend.

Verified gates:

```bash
make
make test
make test-samples
make test-codegen
make test-sanitize
```

Current results:

- Compiler build passes with C11, `-Wall`, and `-Wextra`.
- Lexer and parser unit tests pass.
- Focused semantic and executable C bootstrap tests pass.
- Integration inventory: `57` positive passes, `42` expected-negative passes,
  `0` unexpected failures, `99` total programs.
- Generated-C inventory: all `57` positive programs emit C accepted by GCC.
- ASan/UBSan reports no issue across the integration inventory. Leak detection
  is disabled because LeakSanitizer cannot run under the managed tracer.

## Stabilized Behavior

- Fixed-size arrays require a positive literal length. Contextual array
  literals check arity and element type; `[]` zero-initializes the declared
  length. Array indexes must be integers and literal indexes are bounds-checked.
- Raw pointers support dereference, address-of, typed indexing, and addition or
  subtraction by an integer. Address-of and assignment enforce lvalues.
- `*void` is the untyped allocation/FFI pointer. Other pointer element types are
  invariant. On the 64-bit bootstrap target, `usize` aliases `u64`.
- Local functions are visible throughout their block. Inferred symbols retain
  their types across nested scopes.
- Variant payloads preserve zero, one, or multiple ordered field types.
- `print(value, ...)` is a variadic builtin for primitive values and pointers.
- Modules have semantic scopes. Qualified functions/types and direct
  `use module.member` imports resolve to checked member declarations.
- JSON/schema and pipeline language features have been removed. `|` remains
  for variants, captures, catch bindings, and bitwise OR.
- The CLI releases resources on file, read, and parse failures.

## C Bootstrap Backend

Executable coverage currently proves:

- primitive declarations, inferred locals, assignments, and expressions;
- forward function calls and named returns;
- typed variadic print lowering;
- casts, `if`, `while`, range/array `for`, infinite `loop`, break/continue;
- fixed arrays, indexing, pointer arithmetic, dereference, and address-of;
- structs, named constructors, field access, and pointer field access;
- variants and exhaustive match lowering;
- fallible values, propagation, catch chains, and named error sets;
- modules, receiver methods, fat-pointer interfaces, and generated adapters;
- fixed-array return wrappers, strings, globals, unsafe blocks, and inline asm.

Unsupported AST nodes fail with a code-generation diagnostic instead of being
silently omitted.

## Remaining Work

1. A filesystem module loader with visibility enforcement and cycle detection;
   multiple CLI files are currently merged into one program.
2. Error-set identity and compatibility instead of the remaining generic error
   compatibility rule.
3. Complete interface conformance diagnostics, especially signature mismatch
   reporting and non-addressable coercions.
4. Runtime/standard-library modules for files, sockets, allocation strategies,
   and platform startup; current facilities are primarily extern contracts.
5. Property/fuzz testing for lexer/parser/type invariants and generated-C
   differential tests.
6. A self-hosted compiler source once the bootstrap backend can lower every
   construct required by that compiler.

## Next Order

1. Build a small Linux HTTP server as the runtime/FFI usability proof.
2. Add filesystem-backed modules and a minimal platform library.
3. Build a representative Runes compiler slice with the C backend.
4. Harden diagnostics and add fuzz/property tests around that slice.
