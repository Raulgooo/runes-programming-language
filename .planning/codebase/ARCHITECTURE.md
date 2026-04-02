# Architecture

**Analysis Date:** 2026-04-02

## Pattern Overview

**Overall:** Multi-stage compiler with distinct separation of concerns across lexical analysis, parsing, symbol resolution, and type checking.

**Key Characteristics:**
- Clear pipeline stages with explicit error handling at each phase
- Memory-realm-aware type system (STACK, ARENA, HEAP, GC, FLEX) enforcing allocation strategy rules
- Memory safety through arena-based allocation with no manual heap deallocation
- Comprehensive symbol table with scope hierarchy for name resolution

## Layers

**Lexical Analysis Layer:**
- Purpose: Tokenize source code into a stream of tokens with metadata
- Location: `src/lexer.c`, `src/lexer.h`
- Contains: Token scanning, keyword recognition, string interning via string table
- Depends on: `src/utils/strtab.h` for identifier/string interning
- Used by: Parser

**Parsing Layer:**
- Purpose: Transform token stream into an Abstract Syntax Tree (AST)
- Location: `src/parser.c`, `src/parser.h`
- Contains: Recursive descent parser with lookahead (current, next, next2 tokens), AST node construction
- Depends on: Lexer, Arena allocator, AST definitions
- Used by: Main compiler driver

**AST Representation:**
- Purpose: Unified in-memory representation of program syntax and semantics
- Location: `src/ast.h`, `src/ast.c`
- Contains: 150+ node kinds covering declarations, statements, expressions, type expressions; memory realms and attributes
- Depends on: Lexer (TokenKind), Arena allocator
- Used by: Parser (construction), Resolver, Type Checker

**Name Resolution Layer:**
- Purpose: Link identifier references to their declarations and enforce scoping rules
- Location: `src/resolver.c`, `src/resolver.h`
- Contains: Symbol table population, scope push/pop, name lookup
- Depends on: AST, Symbol Table, Memory Realms
- Used by: Type Checker (post-resolution names)

**Symbol Table:**
- Purpose: Track declared names across scopes with type information
- Location: `src/symbol_table.c`, `src/symbol_table.h`
- Contains: Hash-table-based scope stack, symbol kinds (VAR, FUNC, TYPE, SCHEMA, IFACE, MOD, ERROR)
- Depends on: Arena allocator
- Used by: Resolver, Type Checker

**Type System Layer:**
- Purpose: Define and manage semantic types for type checking
- Location: `src/types.c`, `src/types.h`
- Contains: Type representation (primitives, pointers, arrays, functions, structs, variants, interfaces, fallibles), type context for caching
- Depends on: AST (MemoryRealm, MemoryStrategy)
- Used by: Type Checker

**Type Checking Layer:**
- Purpose: Validate type safety, enforce memory realm nesting rules, and infer types for expressions
- Location: `src/typecheck.c`, `src/typecheck.h`
- Contains: Expression type inference, pattern type checking, memory realm nesting validation, error reporting
- Depends on: AST, Symbol Table, Types, Memory Realms
- Used by: Main compiler driver (final validation before code generation)

**Utilities:**
- Purpose: Memory and string management infrastructure
- Location: `src/utils/arena.h`, `src/utils/arena.c`, `src/utils/strtab.h`, `src/utils/strtab.c`
- Contains: Arena-based bump allocator with snapshots, string interning table
- Used by: All layers

**AST Printing (Debugging):**
- Purpose: Dump AST structure for debugging and testing
- Location: `src/tools/ast_print.c`, `src/tools/ast_tool.c`
- Used by: Main compiler with `--dump-ast` flag

## Data Flow

**Phase 1: Lexical Analysis**

1. `main()` reads source file into memory
2. Initialize `Lexer` with source and `StrTab` (string table)
3. For `--lex-only` mode: Call `lexer_next_token()` repeatedly, print tokens, exit
4. Otherwise: Pass lexer to `Parser`

**Phase 2: Parsing**

1. `parser_init()` initializes parser with lexer and arena
2. `parser_parse()` performs recursive descent parsing, building AST
3. Parser maintains 3-token lookahead (current, next, next2)
4. All AST nodes allocated in arena (no manual free)
5. For `--parse-only` mode: Return after parse completes
6. For `--dump-ast` mode: Call `ast_print()` to visualize tree
7. Otherwise: Continue to name resolution

**Phase 3: Name Resolution**

1. `resolver_init()` with symbol table
2. `resolver_resolve()` traverses AST recursively:
   - For declarations: Define symbols in current scope
   - For references (identifiers): Validate name exists via `symbol_table_lookup()`
   - For scoped blocks (functions, match arms): Push scope, resolve contents, pop scope
   - Enforce memory realm nesting rules for function declarations
3. Error accumulation (error_count, had_error)
4. Halt compilation if errors found

**Phase 4: Type Checking**

1. `type_context_init()` creates type system cache
2. `typechecker_init()` with arena, type context, symbol table
3. `typechecker_check()` traverses AST:
   - `typechecker_infer_expr()` recursively determines expression types
   - For function decls: Save/restore current_realm, validate nesting legality
   - For match statements: Check pattern exhaustiveness, bind pattern variables
   - For operations: Verify operand types match expected operator semantics
4. Error accumulation
5. Halt compilation if errors found

**Error Handling Flow:**

Each phase maintains `error_count` and `had_error` flag. Main driver checks after each phase and either continues or exits with status 1.

**State Management:**

- **Arena**: Single arena shared across all phases, holds all AST and symbol table data
- **Symbol Table**: Scope stack maintains parent pointers for hierarchical lookup
- **Type Context**: Caches resolved types for reuse (implements memoization)
- **Memory Realms**: Tracked in resolver and type checker to validate nesting legality

## Key Abstractions

**AstNode:**
- Purpose: Universal representation of all syntactic and semantic program elements
- Examples: `src/ast.h` defines 150+ node kinds via `AstKind` enum
- Pattern: Tagged union with `kind` discriminator and anonymous `as` union for type-safe field access

**Token:**
- Purpose: Lexeme with metadata (kind, location, interned string/numeric value)
- Examples: `src/lexer.h` TokenKind enum covers keywords, operators, literals
- Pattern: Value-semantic structure, no allocation

**Symbol:**
- Purpose: Name-to-declaration binding with visibility and type annotation
- Examples: `src/symbol_table.h` Symbol struct: name, kind, node pointer, type pointer
- Pattern: Immutable after insertion; symbol_table_lookup returns pointer to existing

**Type:**
- Purpose: Semantic type for type checking (distinct from AST type expressions)
- Examples: TY_PRIMITIVE (i32, str, bool), TY_POINTER, TY_STRUCT, TY_FALLIBLE (!T)
- Pattern: Recursive union-based structure, all allocated in arena

**MemoryRealm:**
- Purpose: Specifies allocation strategy for functions (STACK, ARENA, HEAP, GC, FLEX)
- Examples: `f add(a: i32, b: i32) = a + b` implies REALM_STACK
- Pattern: Enum used in AST_FUNC_DECL, checked during type checking for nesting legality

## Entry Points

**main.c:**
- Location: `src/main.c`
- Triggers: Command line execution
- Responsibilities: 
  - Parse arguments (--lex-only, --parse-only, --dump-ast)
  - Open and read source files
  - Initialize arena and string table
  - Orchestrate lexer → parser → resolver → type checker pipeline
  - Accumulate and report errors
  - Free resources and exit with status code

## Error Handling

**Strategy:** Three-stage accumulation with early exit

**Patterns:**
- **Lexer**: No errors (tokenization is forgiving)
- **Parser**: Maintains `error_count` and `had_error` flag; uses panic mode for recovery
- **Resolver**: Accumulates errors without halting; checks `had_error` post-traversal
- **Type Checker**: Accumulates type errors; final check before success

**Reporting:** All errors print to stderr with format `Error at line:col: message`

## Cross-Cutting Concerns

**Logging:** None in production path; `--dump-ast` uses `ast_print()` for debugging

**Validation:** 
- Type safety: In type checker via type unification and realm nesting rules
- Memory safety: Arena ensures no UAF, no double-free; realm system prevents unsafe allocations

**Authentication:** Not applicable (compiler, not a runtime system)

---

*Architecture analysis: 2026-04-02*
