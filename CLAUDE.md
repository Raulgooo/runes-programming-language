<!-- GSD:project-start source:PROJECT.md -->
## Project

**Runes Compiler — Frontend Completion**

The Runes bootstrap compiler, written in C, implementing a systems-level language with explicit memory realm strategies (stack, regional, dynamic, gc, flex). The compiler currently handles lexing, parsing, name resolution, and partial type checking. This project completes the entire frontend pipeline to full spec v0.1 compliance.

**Core Value:** Every valid Runes program (per spec v0.1, excluding deprecated features) passes through lex → parse → resolve → typecheck → realm-check without false positives or missed errors, with comprehensive test coverage proving it.

### Constraints

- **Language**: Must remain C — this is a bootstrap compiler
- **No new dependencies**: Pure C with standard library only, no LLVM or external libs
- **Spec compliance**: `docs/specv0_1.md` is the source of truth for language behavior
- **Memory**: Continue using arena allocator pattern — no heap allocation in compiler logic
- **Backwards compatible**: Existing passing tests must continue to pass
<!-- GSD:project-end -->

<!-- GSD:stack-start source:codebase/STACK.md -->
## Technology Stack

## Languages
- C (C11) - Compiler implementation and core runtime
- Runes - The language being implemented
- TypeScript/JavaScript - VSCode extension for language support
## Runtime
- Native binary (self-compiled C)
- Command-line interface: `./runes [options] <filename> [additional_files...]`
- None (native C project, no external package dependencies)
- No lockfile required
## Frameworks
- Custom-built compiler (no external frameworks)
- Single-pass lexer-to-codegen pipeline
- Memory arena for allocation
- String table for interned identifiers
- GNU Make - Makefile-based build system
- Location: `/home/raul/Desktop/runes/Makefile`
- Build target: `runes` (executable binary)
- VSCode Extension framework - For language server integration
- Custom test framework (assertion-based, built-in)
- Test files: `src/tests/*.c` files
- Test samples: `src/tests/samples/*.runes` files
## Key Dependencies
- `<stdio.h>` - Input/output, file operations
- `<stdlib.h>` - Memory allocation, general utilities
- `<string.h>` - String manipulation
- `<ctype.h>` - Character classification
- `<stdbool.h>` - Boolean type support
- `<stdint.h>` - Fixed-width integer types
- `<stddef.h>` - Standard type definitions (size_t, etc.)
- `<errno.h>` - Error reporting
- `<assert.h>` - Assertions for testing
- Project has zero external library dependencies
- All functionality implemented from scratch
- Fully self-contained compiler
## Configuration
- Command-line arguments only
- No environment variables required
- No `.env` files
- Compiler: `gcc`
- CFLAGS: `-Isrc -Wall -Wextra -g`
- Output: Single executable binary `runes`
- `--lex-only` - Run lexer only, dump tokens
- `--parse-only` - Run parser only, check syntax
- `--dump-ast` - Parse and dump Abstract Syntax Tree
## Platform Requirements
- GCC C compiler (or compatible C11 compiler)
- GNU Make build system
- POSIX-compatible filesystem
- Linux/Unix environment (or Windows with MinGW/Cygwin)
- C11 standard support required
- Standard C library (libc) required
- No additional system libraries needed
- Compiled binary executable
- No runtime dependencies beyond C standard library
- Deployable as single `runes` binary
- Supports compiling Runes source files (`.runes` extension)
## Language Features (as Target Language)
- Stack allocation (default)
- Explicit stack
- Dynamic (heap)
- Regional (arena)
- Garbage-collected (GC)
- Flex (inherits caller's strategy)
- Primitive types: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, `str`, `char`, `usize`, `void`
- Composite: pointers, arrays, tuples, functions, variants, structs, interfaces
- Fallible types (`!T`)
- Error types
- Schema types (JSON-related)
- Procedural
- Functional (pattern matching, type variants)
- Systems-level (unsafe, inline assembly, raw pointers)
- Object-oriented (methods, interfaces)
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

## Naming Patterns
- Source files: `*.c` and `*.h` (C language)
- Headers: `module_name.h` (e.g., `lexer.h`, `parser.h`, `typecheck.h`)
- Test files: `*_test.c` (e.g., `lexer_test.c`, `parser_test.c`)
- Runes language samples: `*.runes` (e.g., `01_variables.runes`, `02_functions.runes`)
- Snake case: `lexer_init()`, `parser_parse()`, `ast_new_func_decl()`, `typechecker_error()`
- Static helper functions prefixed with module name: `is_at_end()`, `peek()`, `advance()`
- Enum-to-string converters: `token_kind_to_string()`, `realm_name()`
- Snake case: `source`, `current`, `line`, `column`, `had_error`
- Struct abbreviations: `L` for Lexer, `p` for Parser, `tc` for TypeChecker
- Loop variables: `i`, `b`, `n`, `r` (for Resolver)
- Single-letter pointer receivers (C convention): `L->current`, `p->panic_mode`
- PascalCase: `Lexer`, `Token`, `Parser`, `AstNode`, `TypeChecker`, `Arena`
- Typedef'd: `typedef struct { ... } StructName;`
- Nested structs use union pattern: `node->as.func_decl.name`, `node->as.int_literal.value`
- PascalCase enum name: `TokenKind`, `AstKind`, `MemoryRealm`, `SemTypeKind`, `TypeKind`
- SCREAMING_SNAKE_CASE enum values: `TOKEN_EOF`, `TOKEN_IDENTIFIER`, `AST_PROGRAM`, `REALM_STACK`
- SCREAMING_SNAKE_CASE: `ARENA_BLOCK_SIZE` (128KB default block size)
## Code Style
- No formatter configured (no `.clang-format` or similar)
- Observed style: 2-space indentation
- Brace style: K&R (opening brace on same line)
- Line length: Generally 80-100 characters, flexible for readability
- Compiler flags in `Makefile`: `-Wall -Wextra -g`
- No external linter (eslint, clang-tidy, etc.)
- Type-aware checking via typechecker phase (not compile-time C warnings)
## Import Organization
- Pattern: `#ifndef RUNES_MODULE_H` / `#define RUNES_MODULE_H` / `#endif`
- Example: `src/lexer.h` uses `#ifndef RUNES_LEXER_H`
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
- None observed. Paths are relative: `#include "utils/arena.h"`, `#include "../lexer.h"`
## Error Handling
## Logging
## Comments
- Not used in C codebase
- Function signatures documented via comments above declarations
- Example: `/* Pass strtab=NULL if you do not need interned identifiers/strings */` in `src/lexer.h` line 146
- Sparse usage (6 found across codebase)
- Format: `// TODO: description` or `// FIXME: description`
- Examples:
## Function Design
- Varies: 5-line utility functions to 200+ line complex parsers
- No strict limit; functions sized by logical responsibility
- Helper functions extracted when repeating logic
- Pointer-to-struct convention for state: `parser_init(Parser *p, ...)`
- Structs rarely passed by value; always by pointer
- Output via pointer parameters: `void arena_alloc_aligned(Arena *a, size_t size, ...)`
- Boolean flags for mode selection: `parse_func_decl(..., bool is_pub, bool body_allowed)`
- `NULL` for parse failures or missing optional data
- Pointer returns: `AstNode *`, `Token`, `Type *`
- Boolean returns for checks: `bool is_at_end(Lexer *L)`
- Void for initialization/state mutation functions: `void parser_init(Parser *p, ...)`
## Module Design
- Public API functions declared in `.h` files
- Static helpers confined to `.c` files
- No visibility modifiers (C limitation); convention is header declarations = public
#endif
- Every module with state has `module_init()` function
- Examples: `lexer_init()`, `parser_init()`, `arena_init()`, `typechecker_init()`
- Cleanup functions: `arena_destroy()` (if needed)
- Not applicable to C
- Main entry point is `src/main.c` which imports all phases
## Conventions for Runes Language (Test/Sample Files)
- Explicit typed: `i32 x = 5` (type before name)
- Inferred: `inferred_int := 42` (walrus operator)
- Constants: `const i32 MAX_PAGES = 512` or `const LIMIT = 1024`
- Default: `f function_name()` (stack)
- Dynamic heap: `dynamic f alloc_buf()` 
- Regional/Arena: `regional f arena_fn()`
- GC: `gc f collect()`
- Flexible: `flex f poly()`
- Single-line: `-- comment text`
- Multi-line: `--- ... ---`
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

## Pattern Overview
- Clear pipeline stages with explicit error handling at each phase
- Memory-realm-aware type system (STACK, ARENA, HEAP, GC, FLEX) enforcing allocation strategy rules
- Memory safety through arena-based allocation with no manual heap deallocation
- Comprehensive symbol table with scope hierarchy for name resolution
## Layers
- Purpose: Tokenize source code into a stream of tokens with metadata
- Location: `src/lexer.c`, `src/lexer.h`
- Contains: Token scanning, keyword recognition, string interning via string table
- Depends on: `src/utils/strtab.h` for identifier/string interning
- Used by: Parser
- Purpose: Transform token stream into an Abstract Syntax Tree (AST)
- Location: `src/parser.c`, `src/parser.h`
- Contains: Recursive descent parser with lookahead (current, next, next2 tokens), AST node construction
- Depends on: Lexer, Arena allocator, AST definitions
- Used by: Main compiler driver
- Purpose: Unified in-memory representation of program syntax and semantics
- Location: `src/ast.h`, `src/ast.c`
- Contains: 150+ node kinds covering declarations, statements, expressions, type expressions; memory realms and attributes
- Depends on: Lexer (TokenKind), Arena allocator
- Used by: Parser (construction), Resolver, Type Checker
- Purpose: Link identifier references to their declarations and enforce scoping rules
- Location: `src/resolver.c`, `src/resolver.h`
- Contains: Symbol table population, scope push/pop, name lookup
- Depends on: AST, Symbol Table, Memory Realms
- Used by: Type Checker (post-resolution names)
- Purpose: Track declared names across scopes with type information
- Location: `src/symbol_table.c`, `src/symbol_table.h`
- Contains: Hash-table-based scope stack, symbol kinds (VAR, FUNC, TYPE, SCHEMA, IFACE, MOD, ERROR)
- Depends on: Arena allocator
- Used by: Resolver, Type Checker
- Purpose: Define and manage semantic types for type checking
- Location: `src/types.c`, `src/types.h`
- Contains: Type representation (primitives, pointers, arrays, functions, structs, variants, interfaces, fallibles), type context for caching
- Depends on: AST (MemoryRealm, MemoryStrategy)
- Used by: Type Checker
- Purpose: Validate type safety, enforce memory realm nesting rules, and infer types for expressions
- Location: `src/typecheck.c`, `src/typecheck.h`
- Contains: Expression type inference, pattern type checking, memory realm nesting validation, error reporting
- Depends on: AST, Symbol Table, Types, Memory Realms
- Used by: Main compiler driver (final validation before code generation)
- Purpose: Memory and string management infrastructure
- Location: `src/utils/arena.h`, `src/utils/arena.c`, `src/utils/strtab.h`, `src/utils/strtab.c`
- Contains: Arena-based bump allocator with snapshots, string interning table
- Used by: All layers
- Purpose: Dump AST structure for debugging and testing
- Location: `src/tools/ast_print.c`, `src/tools/ast_tool.c`
- Used by: Main compiler with `--dump-ast` flag
## Data Flow
- **Arena**: Single arena shared across all phases, holds all AST and symbol table data
- **Symbol Table**: Scope stack maintains parent pointers for hierarchical lookup
- **Type Context**: Caches resolved types for reuse (implements memoization)
- **Memory Realms**: Tracked in resolver and type checker to validate nesting legality
## Key Abstractions
- Purpose: Universal representation of all syntactic and semantic program elements
- Examples: `src/ast.h` defines 150+ node kinds via `AstKind` enum
- Pattern: Tagged union with `kind` discriminator and anonymous `as` union for type-safe field access
- Purpose: Lexeme with metadata (kind, location, interned string/numeric value)
- Examples: `src/lexer.h` TokenKind enum covers keywords, operators, literals
- Pattern: Value-semantic structure, no allocation
- Purpose: Name-to-declaration binding with visibility and type annotation
- Examples: `src/symbol_table.h` Symbol struct: name, kind, node pointer, type pointer
- Pattern: Immutable after insertion; symbol_table_lookup returns pointer to existing
- Purpose: Semantic type for type checking (distinct from AST type expressions)
- Examples: TY_PRIMITIVE (i32, str, bool), TY_POINTER, TY_STRUCT, TY_FALLIBLE (!T)
- Pattern: Recursive union-based structure, all allocated in arena
- Purpose: Specifies allocation strategy for functions (STACK, ARENA, HEAP, GC, FLEX)
- Examples: `f add(a: i32, b: i32) = a + b` implies REALM_STACK
- Pattern: Enum used in AST_FUNC_DECL, checked during type checking for nesting legality
## Entry Points
- Location: `src/main.c`
- Triggers: Command line execution
- Responsibilities: 
## Error Handling
- **Lexer**: No errors (tokenization is forgiving)
- **Parser**: Maintains `error_count` and `had_error` flag; uses panic mode for recovery
- **Resolver**: Accumulates errors without halting; checks `had_error` post-traversal
- **Type Checker**: Accumulates type errors; final check before success
## Cross-Cutting Concerns
- Type safety: In type checker via type unification and realm nesting rules
- Memory safety: Arena ensures no UAF, no double-free; realm system prevents unsafe allocations
<!-- GSD:architecture-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd:quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd:debug` for investigation and bug fixing
- `/gsd:execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd:profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
