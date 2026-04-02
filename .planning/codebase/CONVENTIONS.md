# Coding Conventions

**Analysis Date:** 2026-04-02

## Naming Patterns

**Files:**
- Source files: `*.c` and `*.h` (C language)
- Headers: `module_name.h` (e.g., `lexer.h`, `parser.h`, `typecheck.h`)
- Test files: `*_test.c` (e.g., `lexer_test.c`, `parser_test.c`)
- Runes language samples: `*.runes` (e.g., `01_variables.runes`, `02_functions.runes`)

**Functions:**
- Snake case: `lexer_init()`, `parser_parse()`, `ast_new_func_decl()`, `typechecker_error()`
- Static helper functions prefixed with module name: `is_at_end()`, `peek()`, `advance()`
- Enum-to-string converters: `token_kind_to_string()`, `realm_name()`

**Variables:**
- Snake case: `source`, `current`, `line`, `column`, `had_error`
- Struct abbreviations: `L` for Lexer, `p` for Parser, `tc` for TypeChecker
- Loop variables: `i`, `b`, `n`, `r` (for Resolver)
- Single-letter pointer receivers (C convention): `L->current`, `p->panic_mode`

**Structs:**
- PascalCase: `Lexer`, `Token`, `Parser`, `AstNode`, `TypeChecker`, `Arena`
- Typedef'd: `typedef struct { ... } StructName;`
- Nested structs use union pattern: `node->as.func_decl.name`, `node->as.int_literal.value`

**Enums:**
- PascalCase enum name: `TokenKind`, `AstKind`, `MemoryRealm`, `SemTypeKind`, `TypeKind`
- SCREAMING_SNAKE_CASE enum values: `TOKEN_EOF`, `TOKEN_IDENTIFIER`, `AST_PROGRAM`, `REALM_STACK`

**Constants:**
- SCREAMING_SNAKE_CASE: `ARENA_BLOCK_SIZE` (128KB default block size)

## Code Style

**Formatting:**
- No formatter configured (no `.clang-format` or similar)
- Observed style: 2-space indentation
- Brace style: K&R (opening brace on same line)
- Line length: Generally 80-100 characters, flexible for readability

**Linting:**
- Compiler flags in `Makefile`: `-Wall -Wextra -g`
- No external linter (eslint, clang-tidy, etc.)
- Type-aware checking via typechecker phase (not compile-time C warnings)

## Import Organization

**Header Guards:**
- Pattern: `#ifndef RUNES_MODULE_H` / `#define RUNES_MODULE_H` / `#endif`
- Example: `src/lexer.h` uses `#ifndef RUNES_LEXER_H`

**Include Order (observed pattern):**
1. Local headers first (double-quoted): `#include "lexer.h"`
2. Relative paths with slash: `#include "utils/arena.h"`
3. Standard library headers: `#include <stdio.h>`, `#include <stdlib.h>`, `#include <string.h>`
4. System headers: `#include <stdbool.h>`, `#include <stdint.h>`

Example from `src/parser.c`:
```c
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "utils/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

**Path Aliases:**
- None observed. Paths are relative: `#include "utils/arena.h"`, `#include "../lexer.h"`

## Error Handling

**Patterns:**

1. **Parser/Lexer Errors:**
   - Use `parser_error(Parser *p, const char *msg)` in `src/parser.c`
   - Sets `p->panic_mode = true` and increments `p->error_count`
   - Errors printed to `stderr` with format: `[Error] filename:line:column: message`

2. **Type Checker Errors:**
   - Use `typechecker_error(TypeChecker *tc, uint32_t line, uint32_t col, const char *fmt, ...)`
   - Variadic argument support for formatted messages
   - Prints to stderr with format: `[Type Error] line:col: message`
   - Example from `src/typecheck.c` line 18-29:
     ```c
     void typechecker_error(TypeChecker *tc, uint32_t line, uint32_t col,
                            const char *fmt, ...) {
       va_list args;
       va_start(args, fmt);
       fprintf(stderr, "[Type Error] %u:%u: ", line, col);
       vfprintf(stderr, fmt, args);
       fprintf(stderr, "\n");
       va_end(args);
       tc->error_count++;
       tc->had_error = true;
     }
     ```

3. **Resolver Errors:**
   - Use static `error()` function in `src/resolver.c` line 7-13
   - Prints `Error at line:col: message`

4. **State Tracking:**
   - All phases track: `error_count` (int), `had_error` (bool)
   - Main loop checks `had_error` flags after each phase before proceeding

5. **Panic Mode (Parser):**
   - Parser enters panic mode on error, discards subsequent errors
   - Synchronizes to safe recovery points (declaration keywords, closing braces)

## Logging

**Framework:** `printf()` and `fprintf()` to stderr

**Patterns:**

1. **Token Output (Lexer testing):**
   ```c
   printf("[%s] '%.*s'\n", token_kind_to_string(token.kind),
          (int)token.length, token.start);
   ```

2. **Error Location:**
   ```c
   fprintf(stderr, "[Error] %s:%d:%d: %s\n", p->filename, p->current.line,
           p->current.column, msg);
   ```

3. **Phase Progress (main.c):**
   ```c
   fprintf(stderr, "Compilation failed in %s with %d error(s)\n", filename,
           parser.error_count);
   ```

4. **Test Output:**
   ```c
   printf("Running test_memory_scopes...\n");
   printf("test_memory_scopes passed!\n");
   ```

No structured logging or log levels. All output is diagnostic/debug.

## Comments

**When to Comment:**

1. **Section Dividers (Used throughout):**
   - Decorative separators mark logical sections
   - Pattern: `// ── Section Name ────────────────────────`
   - Example from `src/lexer.c` line 7-8, 19-20, 134
   ```c
   // ── init
   // ──────────────────────────────────────────────────────────────────────

   // ── helpers
   // ───────────────────────────────────────────────────────────────────
   ```

2. **Implementation Notes:**
   - Comments explain non-obvious logic or complex decisions
   - Example from `src/arena.c` line 63-68:
   ```c
   // Walk the chain from current forward looking for a block with enough
   // space. This is O(k) where k is the number of blocks skipped — in
   // practice almost always 0 or 1 after a reset, never pathological during
   // normal forward allocation.
   ```

3. **Inline Comments:**
   - Rare. Used for disambiguation only
   - Example from `src/parser.c` line 103: `advance(p); // skip the fucked up token`

4. **Matrix/Reference Comments (AST):**
   - Comments show state tables and reference information
   - Example from `src/ast.h` line 22-30 (nesting legality matrix with emoji)

5. **Algorithm Descriptions:**
   - Comments explain parsing strategy or semantic rules
   - Example from `src/ast.h` line 160-166 (intrusive linked list explanation)

**JSDoc/TSDoc:**
- Not used in C codebase
- Function signatures documented via comments above declarations
- Example: `/* Pass strtab=NULL if you do not need interned identifiers/strings */` in `src/lexer.h` line 146

**TODO/FIXME Comments:**
- Sparse usage (6 found across codebase)
- Format: `// TODO: description` or `// FIXME: description`
- Examples:
  - `src/types.c:179`: `// TODO: Implement range checking.`
  - `src/typecheck.c`: `// Bind fields as unknown for now (full struct field lookup is TODO)`

## Function Design

**Size:**
- Varies: 5-line utility functions to 200+ line complex parsers
- No strict limit; functions sized by logical responsibility
- Helper functions extracted when repeating logic

**Parameters:**
- Pointer-to-struct convention for state: `parser_init(Parser *p, ...)`
- Structs rarely passed by value; always by pointer
- Output via pointer parameters: `void arena_alloc_aligned(Arena *a, size_t size, ...)`
- Boolean flags for mode selection: `parse_func_decl(..., bool is_pub, bool body_allowed)`

**Return Values:**
- `NULL` for parse failures or missing optional data
- Pointer returns: `AstNode *`, `Token`, `Type *`
- Boolean returns for checks: `bool is_at_end(Lexer *L)`
- Void for initialization/state mutation functions: `void parser_init(Parser *p, ...)`

**Example from `src/parser.c` lines 304-344 (parse_func_decl):**
```c
static AstNode *parse_func_decl(Parser *p, bool is_pub, MemoryRealm realm,
                                Attr *attrs, bool body_allowed) {
  // caller verified current == TOKEN_F
  Token f_tok = advance(p);
  Token name_tok = expect(p, TOKEN_IDENTIFIER, "expected function name");
  if (p->panic_mode)
    return NULL;
  // ... (parameter list, return type, body parsing)
}
```

## Module Design

**Exports (Header Pattern):**
- Public API functions declared in `.h` files
- Static helpers confined to `.c` files
- No visibility modifiers (C limitation); convention is header declarations = public

**Example from `src/lexer.h` lines 147-150:**
```c
void lexer_init(Lexer *l, const char *source, StrTab *strtab);
Token lexer_next_token(Lexer *l);
const char *token_kind_to_string(TokenKind kind);
#endif
```

**Initialization Pattern (Lifecycle):**
- Every module with state has `module_init()` function
- Examples: `lexer_init()`, `parser_init()`, `arena_init()`, `typechecker_init()`
- Cleanup functions: `arena_destroy()` (if needed)

**Barrel Files:**
- Not applicable to C
- Main entry point is `src/main.c` which imports all phases

## Conventions for Runes Language (Test/Sample Files)

**Variable declarations (from `src/tests/samples/01_variables.runes`):**
- Explicit typed: `i32 x = 5` (type before name)
- Inferred: `inferred_int := 42` (walrus operator)
- Constants: `const i32 MAX_PAGES = 512` or `const LIMIT = 1024`

**Memory realms (keywords in function signatures):**
- Default: `f function_name()` (stack)
- Dynamic heap: `dynamic f alloc_buf()` 
- Regional/Arena: `regional f arena_fn()`
- GC: `gc f collect()`
- Flexible: `flex f poly()`

**Comment style in .runes files:**
- Single-line: `-- comment text`
- Multi-line: `--- ... ---`

---

*Convention analysis: 2026-04-02*
