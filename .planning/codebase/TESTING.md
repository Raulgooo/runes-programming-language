# Testing Patterns

**Analysis Date:** 2026-04-02

## Test Framework

**Runner:**
- Custom C test harness (no external framework like CUnit or Google Test)
- Bash test orchestration script: `src/tests/tester.bash`

**Assertion Library:**
- Standard `assert.h` (from C standard library)
- Custom macro: `#define ASSERT_TOKEN(...)` in `src/tests/lexer_test.c`
- Manual assertions via `assert()` and `if` statements

**Run Commands:**
```bash
make test                    # Run single test via Makefile
./runes [--lex-only|--parse-only|--dump-ast] file.runes  # Run compiler phases on test file
bash src/tests/tester.bash  # Run all samples with pass/fail summary
```

## Test File Organization

**Location:**
- Test code co-located with source: `src/tests/` directory
- Organized by phase: `lexer_test.c`, `parser_test.c`, `typecheck_test.c`, `codegen_test.c`
- Runes language sample files: `src/tests/samples/` and `src/tests/samples2/`
- Resolver tests: `src/tests/resolver_tests/` (directory with individual test files)

**Naming:**
- Unit tests: `{module}_test.c` (e.g., `lexer_test.c`)
- Test functions: `test_{feature}()` (e.g., `test_memory_scopes()`, `test_os_features()`)
- Sample files: `{number}_{feature}.runes` (e.g., `01_variables.runes`, `02_functions.runes`)

**Structure:**
```
src/tests/
├── lexer_test.c           # Lexer unit tests
├── parser_test.c          # Parser unit tests
├── typecheck_test.c       # Type checker tests (empty)
├── codegen_test.c         # Code generation tests (empty)
├── tester.bash            # Bash orchestration script
├── samples/               # Runes language samples (numbered)
│   ├── 01_variables.runes
│   ├── 02_functions.runes
│   ├── 03_control_flow.runes
│   └── ... (11 samples total)
├── samples2/              # Additional samples
├── resolver_tests/        # Resolver test files
└── undef_error.runes      # Edge case test file
```

## Test Structure

**Suite Organization (from `src/tests/lexer_test.c`):**

```c
void test_memory_scopes() {
  printf("Running test_memory_scopes...\n");
  Lexer L;
  const char *source = "flex f poly() { regional f arena() {} }";
  lexer_init(&L, source, NULL);

  ASSERT_TOKEN(&L, TOKEN_FLEX, "flex");
  ASSERT_TOKEN(&L, TOKEN_F, "f");
  // ... more assertions

  printf("test_memory_scopes passed!\n");
}

int main() {
  printf("--- Starting Runes Lexer Tests ---\n");
  test_memory_scopes();
  test_os_features();
  test_control_flow_and_errors();
  test_dynamic_function();
  test_lexer_bugs();
  printf("--- All tests passed successfully! ---\n");
  return 0;
}
```

**Patterns:**

1. **Setup Pattern:**
   - Manual initialization: `Lexer L;` then `lexer_init(&L, source, NULL);`
   - Arena allocation: `Arena arena; arena_init(&arena);`
   - Direct struct instantiation (stack allocation preferred)

2. **Teardown Pattern:**
   - Arena cleanup: `arena_destroy(&arena);`
   - Implicit cleanup via return (locals stack-freed automatically)
   - No explicit deinitialization for Lexer/Parser (no resources to free)

3. **Assertion Pattern:**
   - Custom `ASSERT_TOKEN(lexer, expected_kind, expected_lexeme)` macro
   - Macro expands to manual token comparison with error reporting
   - Simple `assert()` for truth conditions

Example from `src/tests/lexer_test.c` lines 7-26:
```c
#define ASSERT_TOKEN(L, expected_kind, expected_lexeme) \
  do { \
    Token token = lexer_next_token(L); \
    printf("[%s] '%.*s'\n", token_kind_to_string(token.kind), \
           (int)token.length, token.start); \
    if (token.kind != expected_kind) { \
      fprintf(stderr, "Test failed: line %d. Expected kind %s, got %s\n", \
              __LINE__, token_kind_to_string(expected_kind), \
              token_kind_to_string(token.kind)); \
      assert(token.kind == expected_kind); \
    } \
    if (strncmp(token.start, expected_lexeme, token.length) != 0 || \
        strlen(expected_lexeme) != token.length) { \
      fprintf(stderr, "Test failed: line %d. Expected lexeme '%.*s', got '%.*s'\n", \
              __LINE__, (int)strlen(expected_lexeme), expected_lexeme, \
              (int)token.length, token.start); \
      assert(0); \
    } \
  } while (0)
```

## Mocking

**Framework:** Not used. Tests call real implementations.

**Patterns:**
- No mocks or stubs observed
- Tests instantiate real structs: `Lexer L;`, `Arena arena;`, `Parser parser;`
- NULL parameters substitute for unneeded dependencies
  - Example: `lexer_init(&L, source, NULL);` (NULL = no string interning)

**What to Mock:**
- Not applicable. Codebase uses composition of small, testable units.

**What NOT to Mock:**
- Never mock: Core parsing/lexing logic relies on real token stream
- Integration with Arena allocator needed for memory management in AST

## Fixtures and Factories

**Test Data (from `src/tests/lexer_test.c`):**

```c
void test_dynamic_function() {
  printf("Running test_dynamic_function...\n");
  Lexer L;

  const char *source = "dynamic f alloc_buf(size: u64) = ptr: *u8 {\n"
                       "    ptr = raw_alloc(size)\n"
                       "\n"
                       "    -- caller must free raw_alloc\n"
                       "}";

  lexer_init(&L, source, NULL);
  
  ASSERT_TOKEN(&L, TOKEN_DYNAMIC, "dynamic");
  // ... token assertions
}
```

**Pattern:**
- Inline source strings in test functions
- Multi-line strings built with C string concatenation
- No separate fixture files or factory functions

**Location:**
- Fixtures embedded in test functions (`src/tests/lexer_test.c`)
- Sample files as fixtures: `src/tests/samples/01_variables.runes`, etc.

## Coverage

**Requirements:** No coverage tracking or enforcement observed
- No `.gcov` files
- No coverage configuration in Makefile
- No CI pipeline with coverage reporting

**View Coverage:**
- Not configured. Coverage analysis not integrated into test infrastructure.

**Current Coverage Gaps (observed):**
- `typecheck_test.c` is empty (no type checker tests)
- `codegen_test.c` is empty (no code generation tests)
- Resolver phases tested indirectly via full samples, not unit tests

## Test Types

**Unit Tests:**

Lexer unit tests (`src/tests/lexer_test.c`):
- Test individual token recognition
- Test keyword detection
- Test escape sequences and string/char literals
- Test operator lexing
- Scope: Single module (Lexer), real state, real dependencies

Examples:
- `test_memory_scopes()` - lexes memory realm keywords
- `test_os_features()` - lexes OS-level syntax (extern, volatile, attributes)
- `test_control_flow_and_errors()` - lexes try/catch/error handling
- `test_lexer_bugs()` - regression tests for past bugs (keyword length, hex literals)

Parser unit tests (`src/tests/parser_test.c`):
- Test AST construction
- Test attribute parsing
- Scope: Parser phase only, with mock source strings

Example from `src/tests/parser_test.c`:
```c
void test_attributes() {
    Arena arena;
    arena_init(&arena);
    
    const char *source = 
        "#[align(4096)]\n"
        "type PageTable = { entries: [512]u64 }\n"
        "#[interrupt]\n"
        "f handle_irq() { }\n";

    Lexer lexer;
    lexer_init(&lexer, source, "test.runes");
    Parser parser;
    parser_init(&parser, &lexer, &arena, "test.runes", source);
    
    AstNode *program = parser_parse(&parser);
    assert(program != NULL);
    assert(program->kind == AST_PROGRAM);
    
    // Verify attributes were parsed correctly
    AstNode *decls = program->as.program.declarations;
    assert(decls->as.type_decl.attrs != NULL);
    assert(strcmp(decls->as.type_decl.attrs->name, "align") == 0);
}
```

**Integration Tests:**

Sample Runes files (`src/tests/samples/`):
- Full compilation pipeline: lexer → parser → resolver → type checker
- Test complete source programs
- Scope: Multiple phases, real error handling

Sample coverage:
- `01_variables.runes` - Variable declarations (typed, inferred, const)
- `02_functions.runes` - Function definitions and calls
- `03_control_flow.runes` - if/else, loops, match
- `04_types_interfaces.runes` - Type and interface declarations
- `05_error_handling.runes` - Try/catch, error types
- `06_pattern_matching.runes` - Pattern destructuring
- `07_unsafe_systems.runes` - Unsafe blocks, ASM, volatile
- `08_schema_json.runes` - JSON schema support
- `09_modules.runes` - Module declarations and use
- `10_kernel_bootstrap.runes` - Kernel-level features (memory realms)
- `11_edge_cases.runes` - Boundary conditions and corner cases

**E2E Tests:**
- Not formally defined
- Bash orchestration script (`src/tests/tester.bash`) runs all samples:
  ```bash
  for f in src/tests/samples/*.runes; do
    output=$(./runes src/std/prelude.runes "$f" 2>&1)
    status=$?
    if [ $status -eq 0 ]; then
      echo "✅ $name"
    else
      echo "❌ $name"
    fi
  done
  ```
- Success = return code 0; Failure = non-zero exit

## Common Patterns

**Async Testing:**
- Not applicable. C with no async runtime.

**Error Testing:**

Pattern 1: Negative test cases (test failure conditions):
- File: `src/tests/samples/03_invalid_names.runes` - tests that invalid identifiers fail
- File: `src/tests/samples/undef_error.runes` - tests undefined variable error
- Execution: `./runes file.runes 2>&1` captures stderr, check exit code

Pattern 2: Manual error verification:
```c
if (parser.had_error) {
  fprintf(stderr, "Compilation failed in %s with %d error(s)\n", filename,
          parser.error_count);
  return 1;
}
```

Pattern 3: Error counting:
```bash
errors=$(echo "$output" | grep -c "Error")
echo "❌ $name ($errors errors)"
```

**Lifecycle Testing:**

Arena allocation tests (implicit in all tests):
- Verify arena allocates and doesn't leak
- Example: `src/tests/parser_test.c` allocates arena, parses, then destroys
- If code runs without segfault, memory management working

**Test Isolation:**
- Each test function initializes fresh state (new Lexer, Arena, Parser)
- No shared test state across functions
- No test teardown hooks; cleanup via `arena_destroy()` call

## Test Execution Environment

**Compilation:**
```bash
make test  # Builds runes binary and runs single sample
```

**Executable:**
- Main compiler: `./runes` (built from Makefile)
- Lexer test: `gcc -Isrc -Wall -Wextra -g src/lexer.c ... src/tests/lexer_test.c -o lexer_test`
- Parser test: Similar compilation with `parser_test.c`

**Success Criteria:**
- Lexer/Parser tests: Exit code 0, all `assert()` macros pass
- Sample tests: Exit code 0 from `./runes file.runes` (indicates no compilation errors)
- Bash script: Count of ✅ vs ❌ symbols

---

*Testing analysis: 2026-04-02*
