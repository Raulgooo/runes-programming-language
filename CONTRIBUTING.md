# Contributing to Runes

Thanks for your interest in contributing to Runes! This guide covers how to
build the project, run the tests, build with sanitizers, and add new tests.

## Prerequisites

- A C compiler (`gcc` or `clang`)
- `bash` (for the test runner)
- `make`

## Build

The default build produces the `runes` executable:

```bash
make
```

This compiles the compiler front-end and back-end into `./runes`.

## Run the tests

The test suite lives in `src/tests/samples/` and is driven by a `bash`
harness:

```bash
bash src/tests/tester.bash
```

The runner prints `PASS`, `FAIL`, or `PASS (expected failure)` for every
sample and a summary at the end. The suite is under active development, so
some samples are expected to fail until their features land.

## AddressSanitizer / UndefinedBehaviorSanitizer build

To build a sanitized binary for debugging memory and undefined-behavior
issues:

```bash
gcc -Isrc -fsanitize=address,undefined -o runes_asan \
    src/main.c src/lexer.c src/parser.c src/ast.c \
    src/resolver.c src/symbol_table.c src/types.c src/typecheck.c \
    src/realm_check.c src/codegen.c src/tools/ast_print.c \
    src/utils/arena.c src/utils/strtab.c
```

Then run it the same way you would run `./runes`, e.g.:

```bash
./runes_asan src/std/prelude.runes src/tests/samples/01_variables.runes
```

## How to add a new test

Tests are `.runes` source files placed in `src/tests/samples/`.

### Positive test

A normal sample is expected to compile and run successfully (the `runes`
process exits with status `0`). Just drop a new `.runes` file in
`src/tests/samples/` and it will be picked up automatically by the runner.

### Expected-failure test

To assert that a program is *rejected*, put the following as the very first
line of the file:

```runes
-- EXPECT FAIL: <pattern>
```

where `<pattern>` is a `grep`-compatible regular expression that must appear
in the compiler's error output. The runner marks the test as passing only if
the compiler fails *and* the pattern is found in its output.

## Pipeline overview

A Runes source file flows through the following stages (see `src/`):

1. **Lexer** (`lexer.c`) — turns source text into a stream of tokens.
2. **Parser** (`parser.c`, `ast.c`) — builds the abstract syntax tree.
3. **Resolver** (`resolver.c`, `symbol_table.c`) — resolves names and scopes.
4. **Type checker** (`typecheck.c`, `types.c`, `realm_check.c`) — validates
   types and realm constraints.
5. **Code generator** (`codegen.c`) — emits the target output.

Supporting utilities live under `src/utils/` (an arena allocator in
`arena.c` and a string table in `strtab.c`), and developer tools under
`src/tools/` (`ast_print.c`, `ast_tool.c`).
