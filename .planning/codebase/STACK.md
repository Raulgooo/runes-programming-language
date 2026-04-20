# Technology Stack

**Analysis Date:** 2026-04-02

## Languages

**Primary:**
- C (C11) - Compiler implementation and core runtime
  - Used for: Lexer, parser, AST, type checker, resolver, code generator
  - Location: `src/` directory

**Domain-Specific:**
- Runes - The language being implemented
  - Used for: Language specification, tests, examples
  - File extension: `.runes`
  - Location: `src/tests/samples/`, `src/examples/`, `src/std/`

**Secondary:**
- TypeScript/JavaScript - VSCode extension for language support
  - Used for: Syntax highlighting, language configuration
  - Location: `runes-lang/package.json`
  - VSCode target: v1.60.0+

## Runtime

**Environment:**
- Native binary (self-compiled C)
- Command-line interface: `./runes [options] <filename> [additional_files...]`

**Package Manager:**
- None (native C project, no external package dependencies)
- No lockfile required

## Frameworks

**Core Compiler Toolchain:**
- Custom-built compiler (no external frameworks)
- Single-pass lexer-to-codegen pipeline
- Memory arena for allocation
- String table for interned identifiers

**Build Tool:**
- GNU Make - Makefile-based build system
- Location: `/home/raul/Desktop/runes/Makefile`
- Build target: `runes` (executable binary)

**Development:**
- VSCode Extension framework - For language server integration
  - Package: `runes-lang`
  - Extension ID: `runes`
  - Config: `language-configuration.json`
  - Syntax highlighting: TextMate grammar (JSON)

**Testing:**
- Custom test framework (assertion-based, built-in)
- Test files: `src/tests/*.c` files
- Test samples: `src/tests/samples/*.runes` files

## Key Dependencies

**Critical (Built-in C Standard Library):**
- `<stdio.h>` - Input/output, file operations
- `<stdlib.h>` - Memory allocation, general utilities
- `<string.h>` - String manipulation
- `<ctype.h>` - Character classification
- `<stdbool.h>` - Boolean type support
- `<stdint.h>` - Fixed-width integer types
- `<stddef.h>` - Standard type definitions (size_t, etc.)
- `<errno.h>` - Error reporting
- `<assert.h>` - Assertions for testing

**No External Dependencies:**
- Project has zero external library dependencies
- All functionality implemented from scratch
- Fully self-contained compiler

## Configuration

**Environment:**
- Command-line arguments only
- No environment variables required
- No `.env` files

**Build Configuration:**
- Compiler: `gcc`
- CFLAGS: `-Isrc -Wall -Wextra -g`
- Output: Single executable binary `runes`

**Runtime Options:**
- `--lex-only` - Run lexer only, dump tokens
- `--parse-only` - Run parser only, check syntax
- `--dump-ast` - Parse and dump Abstract Syntax Tree

## Platform Requirements

**Development:**
- GCC C compiler (or compatible C11 compiler)
- GNU Make build system
- POSIX-compatible filesystem
- Linux/Unix environment (or Windows with MinGW/Cygwin)

**Compilation:**
- C11 standard support required
- Standard C library (libc) required
- No additional system libraries needed

**Production/Distribution:**
- Compiled binary executable
- No runtime dependencies beyond C standard library
- Deployable as single `runes` binary
- Supports compiling Runes source files (`.runes` extension)

## Language Features (as Target Language)

**Memory Strategies:**
- Stack allocation (default)
- Explicit stack
- Dynamic (heap)
- Regional (arena)
- Garbage-collected (GC)
- Flex (inherits caller's strategy)

**Type System:**
- Primitive types: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, `str`, `char`, `usize`, `void`
- Composite: pointers, arrays, tuples, functions, variants, structs, interfaces
- Fallible types (`!T`)
- Error types
- Schema types (JSON-related)

**Paradigms:**
- Procedural
- Functional (pattern matching, type variants)
- Systems-level (unsafe, inline assembly, raw pointers)
- Object-oriented (methods, interfaces)

---

*Stack analysis: 2026-04-02*
