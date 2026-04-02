# Codebase Structure

**Analysis Date:** 2026-04-02

## Directory Layout

```
runes/
├── src/                          # Core compiler source code
│   ├── lexer.c / lexer.h        # Tokenization
│   ├── parser.c / parser.h      # Parsing to AST
│   ├── ast.c / ast.h            # AST node definitions and constructors
│   ├── symbol_table.c / .h      # Name/scope tracking
│   ├── resolver.c / .h          # Name resolution pass
│   ├── types.c / types.h        # Semantic type system
│   ├── typecheck.c / .h         # Type checking pass
│   ├── realm_check.c / .h       # Memory realm validation (stub)
│   ├── codegen.c / codegen.h    # Code generation (stub)
│   ├── main.c                   # Compiler driver / entry point
│   ├── utils/                   # Utility infrastructure
│   │   ├── arena.c / arena.h    # Bump allocator
│   │   └── strtab.c / strtab.h  # String interning table
│   ├── tools/                   # Debugging and utility tools
│   │   ├── ast_print.c          # AST pretty-printer
│   │   └── ast_tool.c           # Standalone AST dumper
│   ├── tests/                   # Test files and samples
│   │   ├── lexer_test.c         # Lexer unit tests
│   │   ├── parser_test.c        # Parser unit tests
│   │   ├── codegen_test.c       # Code generation tests (stub)
│   │   ├── typecheck_test.c     # Type checking tests (stub)
│   │   ├── tester.bash          # Bash test runner
│   │   ├── samples/             # .runes source samples
│   │   ├── samples2/            # Additional .runes samples
│   │   ├── resolver_tests/      # Name resolution test cases
│   │   └── undef_error.runes    # Test for undefined name error
│   ├── examples/                # Example .runes programs
│   │   ├── hello.runes
│   │   ├── basic.runes
│   │   ├── types.runes
│   │   ├── match.runes
│   │   ├── memory.runes
│   │   └── ... (10+ examples)
│   └── std/                     # Standard library definitions (stub)
├── docs/                        # Documentation
│   └── specv0_1.md             # Language specification v0.1
├── .planning/                   # GSD planning output directory
│   └── codebase/               # Architecture/structure analysis
├── Makefile                     # Build configuration
├── README.md                    # Project readme
├── runes                        # Compiled executable
└── [test output files]         # .runes files, AST dumps, video recordings
```

## Directory Purposes

**src/**
- Purpose: All compiler source code in C
- Contains: Lexer, parser, AST definitions, symbol table, resolver, type checker, utilities
- Key files: `main.c` (entry), `lexer.c`, `parser.c`, `ast.h`, `resolver.c`, `typecheck.c`

**src/utils/**
- Purpose: Memory management and string interning infrastructure
- Contains: Arena bump allocator with snapshot/restore, hash-table-based string table
- Key files: `arena.h` (defines Arena, ArenaSnapshot), `strtab.h` (defines StrTab)

**src/tools/**
- Purpose: Debugging utilities for AST inspection
- Contains: Pretty-printer for AST visualization, standalone tool driver
- Key files: `ast_print.c` (recursive tree printer), `ast_tool.c` (CLI wrapper)

**src/tests/**
- Purpose: Test harness and sample programs
- Contains: Unit test files (.c), integration test samples (.runes), test runner script
- Key files: `tester.bash` (runs tests), `samples/*.runes` (language feature tests)

**src/examples/**
- Purpose: Reference programs demonstrating language features
- Contains: 10+ complete .runes programs covering functions, types, pattern matching, memory strategies
- Key files: `hello.runes`, `types.runes`, `match.runes`, `memory.runes`

**src/std/**
- Purpose: Standard library stubs (not yet implemented)
- Contains: Placeholder for built-in functions and types

**docs/**
- Purpose: Language specification and design documentation
- Contains: `specv0_1.md` detailing syntax, semantics, memory realms, type system

**.planning/codebase/**
- Purpose: GSD-generated architecture and structure analysis
- Contains: ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, CONCERNS.md

## Key File Locations

**Entry Points:**
- `src/main.c`: Compiler driver — reads args, orchestrates pipeline (lexer → parser → resolver → type checker)

**Configuration:**
- `Makefile`: Build rules, compiler flags (-Wall, -Wextra, -g), test targets

**Core Compilation Phases:**
- `src/lexer.c`: Phase 0 — tokenization
- `src/parser.c`: Phase 1 — parsing to AST
- `src/resolver.c`: Phase 2 — name resolution with symbol table
- `src/typecheck.c`: Phase 3 — type checking with memory realm validation

**Data Structures:**
- `src/ast.h`: All AST node kinds (150+) and memory realm enums
- `src/symbol_table.h`: Scope stack and symbol lookup
- `src/types.h`: Semantic type representation (distinct from AST type expressions)
- `src/utils/arena.h`: Bump allocator with snapshot/restore for backtracking

**Utilities:**
- `src/utils/strtab.h`: String interning (deduplicates identifier/string storage)
- `src/tools/ast_print.c`: Recursive pretty-printer for debugging

**Tests:**
- `src/tests/samples/`: 40+ .runes files testing language features (variables, functions, control flow, types, error handling, etc.)
- `src/tests/resolver_tests/`: Name resolution test cases
- `src/tests/lexer_test.c`: Lexer unit tests
- `src/tests/parser_test.c`: Parser unit tests

**Examples:**
- `src/examples/hello.runes`: "Hello, World" equivalent
- `src/examples/types.runes`: Type system showcase
- `src/examples/memory.runes`: Memory realm strategies
- `src/examples/match.runes`: Pattern matching

## Naming Conventions

**Files:**
- Headers: `.h` for public interfaces, paired with `.c` implementation
- Test files: `*_test.c` for unit tests, `*.runes` for integration samples
- Pattern: One logical module per file pair (e.g., `lexer.h`/`lexer.c`, `parser.h`/`parser.c`)

**Directories:**
- Lowercase with underscores: `src/utils/`, `src/tools/`, `src/tests/`, `src/examples/`
- Hierarchical by concern: utility vs. compiler vs. test code

**Functions:**
- Prefix with module name: `lexer_init()`, `parser_parse()`, `symbol_table_define()`, `typechecker_check()`
- Pattern: `module_action()` or `module_action_variant()`

**Structs/Enums:**
- Pascal case: `Lexer`, `Parser`, `AstNode`, `Token`, `Symbol`, `SymbolTable`
- Type enums use capitals: `TokenKind`, `AstKind`, `SymbolKind`, `MemoryRealm`, `SemTypeKind`

**Macros:**
- Uppercase with underscores: `ARENA_ALLOC()`, `ARENA_ALLOC_N()`, `ARENA_BLOCK_SIZE`

## Where to Add New Code

**New Compiler Phase (e.g., code generation):**
- Implementation: `src/codegen.c` / `src/codegen.h` (currently stubbed)
- Integration: Add phase to `main.c` pipeline after type checking
- Tests: `src/tests/codegen_test.c`

**New Language Feature (e.g., generics):**
- AST: Add node kinds to `src/ast.h` and constructor functions to `src/ast.c`
- Parser: Add parsing logic to `src/parser.c`
- Resolver: Add name binding logic to `src/resolver.c` if needed
- Type Checker: Add type inference/checking to `src/typecheck.c`
- Tests: Add test cases to `src/tests/samples/` as `.runes` files

**New Utility:**
- If memory/string related: Extend `src/utils/`
- If debugging-related: Add to `src/tools/`
- Pattern: Module-based with clear public API in `.h`

**Tests:**
- Unit tests: `src/tests/*_test.c` (C-based)
- Integration tests: `src/tests/samples/*.runes` (Runes language files)
- Test runner: `src/tests/tester.bash` (bash script)

## Special Directories

**src/utils/:**
- Purpose: Shared memory and string management infrastructure
- Generated: No
- Committed: Yes — essential utilities for all phases

**src/tools/:**
- Purpose: Debugging utilities, not part of compiler proper
- Generated: No (source files)
- Committed: Yes

**src/tests/samples/:**
- Purpose: Language feature test coverage
- Generated: No (manually authored)
- Committed: Yes — test suite for regression testing

**src/examples/:**
- Purpose: Reference programs for users and developers
- Generated: No
- Committed: Yes

**.planning/codebase/:**
- Purpose: GSD-generated analysis documents
- Generated: Yes (by mapper, updater, etc.)
- Committed: Yes (documents, not code)

---

*Structure analysis: 2026-04-02*
