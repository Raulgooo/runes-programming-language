# Codebase Concerns

**Analysis Date:** 2026-04-02

## Tech Debt

**Debug Statements Left in Production Code:**
- Issue: Printf debug statements remain in type checker assignment validation
- Files: `src/typecheck.c` (lines 580-584)
- Impact: Pollutes stderr output during normal compilation, confuses users with "DEBUG ASSERT" messages
- Fix approach: Remove all debug printf statements and replace with proper error messages only

**Incomplete Type Checker Implementation:**
- Issue: Multiple core type checking features are partially implemented or stubbed
- Files: `src/typecheck.c`, `src/types.c`
- Impact: Variant arm construction, struct pattern destructuring, error set checking not fully functional
- Fix approach: Complete variant arm resolution, implement full struct field pattern matching, implement proper error set validation

**Empty Placeholder Files:**
- Issue: `src/codegen.c` and `src/realm_check.c` are empty stubs
- Files: `src/codegen.c`, `src/realm_check.c`
- Impact: Code generation and memory realm validation are not implemented; compiler cannot produce runnable code
- Fix approach: Implement AST-to-code generation and memory realm checking logic

**Integer Literal Type Coercion Missing Range Checking:**
- Issue: Integer literals default to i32 and are permissively assigned to other integer types without validating the value fits in the target type
- Files: `src/types.c` (line 179)
- Impact: Code like `u8 x = 300` compiles without error despite overflow risk
- Fix approach: Implement range checking for integer literal assignments

## Known Bugs

**Incomplete Error Set Validation:**
- Symptoms: Error variants are assigned permissively without proper error set checking
- Files: `src/types.c` (line 207)
- Trigger: Any assignment of an Error type to another Error type
- Workaround: Manually verify error types match semantically

**Struct Pattern Destructuring Not Fully Resolved:**
- Symptoms: Pattern matching on struct fields binds fields as unknown type instead of inferring actual field types
- Files: `src/typecheck.c` (line 142)
- Trigger: Using struct pattern in match statement like `match p { Point(x: a, y: b) -> ... }`
- Workaround: Use named field access instead of pattern destructuring

**Variant Arm Resolution Incomplete:**
- Symptoms: Variant constructors like `RGB(255, 0, 0)` parse but don't validate argument types against arm signature
- Files: `src/typecheck.c` (line 696)
- Trigger: Calling variant constructors with arguments
- Workaround: None; feature not yet functional

## Security Considerations

**Unsafe Memory Management Without Bounds Checking:**
- Risk: Array indexing and pointer arithmetic not validated; can read/write out of bounds
- Files: `src/typecheck.c` (lines 739-761 for array indexing)
- Current mitigation: Type checking ensures index is integer type, but no bounds validation
- Recommendations: Add runtime bounds checking or static bounds analysis; document unsafe regions clearly

**String Escape Sequence Handling Incomplete:**
- Risk: Unicode escape sequences (`\uXXXX`) only return low byte, losing data and enabling encoding attacks
- Files: `src/lexer.c` (lines 481-489)
- Current mitigation: None
- Recommendations: Implement full UTF-8 encoding for Unicode escapes; validate escape sequences properly

**Unvalidated External Function Calls:**
- Risk: No validation of `extern` FFI function signatures; type mismatches could cause memory corruption
- Files: No validation implemented yet
- Current mitigation: Parser accepts `extern` declarations but no semantic checking
- Recommendations: Implement FFI type checking and ABI validation

## Performance Bottlenecks

**Arena Allocation O(k) Lookup:**
- Problem: `arena_alloc_aligned()` walks block chain linearly to find fit
- Files: `src/utils/arena.c` (lines 68-78)
- Cause: No free list or tracking of available blocks; sequential scan after each reset
- Improvement path: Maintain free list or size hints for blocks; implement first-fit or best-fit strategy

**Type Comparison Overhead:**
- Problem: Type equality checking may recursively traverse complex type structures
- Files: `src/types.c` (type comparison functions)
- Cause: No type canonicalization or caching of type comparisons
- Improvement path: Intern types or use hash-based comparison; cache recent comparisons

## Fragile Areas

**Parser Operator Precedence and Associativity:**
- Files: `src/parser.c` (lines 2000-2200 for expression parsing)
- Why fragile: Operator precedence is encoded through function call order; easy to introduce precedence bugs when adding new operators
- Safe modification: Document precedence levels clearly; add test cases for all operator combinations; consider building a precedence table
- Test coverage: Basic lexer tests exist but no comprehensive operator precedence tests

**Memory Strategy Nesting Rules:**
- Files: `src/typecheck.c` (lines 33-44)
- Why fragile: Nesting rules are hardcoded conditionals; spec defines complex matrix of valid combinations
- Safe modification: Create a lookup table for realm nesting; add comprehensive test matrix
- Test coverage: No dedicated realm nesting tests exist

**Type Inference for Method Calls:**
- Files: `src/typecheck.c` (lines 603-629)
- Why fragile: Implicit address-of and dereference for method receivers; complex type compatibility logic
- Safe modification: Add unit tests for all method receiver type combinations; document implicit conversions
- Test coverage: Limited to type checking tests; no dedicated method call tests

## Scaling Limits

**Single-Pass Compiler Without Lookahead:**
- Current capacity: Single-pass lexing and parsing; no symbol table pre-pass
- Limit: Two-phase compilation (resolve then type-check) requires complete AST in memory; forward references may fail
- Scaling path: Implement multi-pass compilation or symbol table pre-scan to enable forward references and separate modules

**Arena Memory Fixed Block Size:**
- Current capacity: `ARENA_BLOCK_SIZE` (likely 16KB or 64KB)
- Limit: Large compilation units create many block allocations; each allocation may fragment arena
- Scaling path: Tune block size dynamically based on input size; implement block reuse strategy

## Dependencies at Risk

**No External Dependencies:**
- Risk: Project uses only libc; no third-party libraries to track
- Impact: Good for compiler bootstrap; limits available optimizations
- Migration plan: If adding dependencies, implement vendoring or lock-file strategy

## Missing Critical Features

**Code Generation:**
- Problem: No code generation implemented; `src/codegen.c` is empty stub
- Blocks: Cannot produce executable output; compiler cannot run Runes programs

**Realm Checking:**
- Problem: Memory strategy nesting validation not implemented; `src/realm_check.c` is empty stub
- Blocks: Cannot enforce memory safety contracts; region/GC nesting violations not caught

**Standard Library:**
- Problem: No stdlib implemented; only skeleton in `src/std/`
- Blocks: Cannot write practical programs; no I/O, memory allocation, or string manipulation utilities

**Module System:**
- Problem: Parser supports `mod` declarations but resolver does not implement module loading
- Blocks: Cannot split code into separate modules; all code must be in single compilation unit

## Test Coverage Gaps

**Type Checker Tests Missing:**
- What's not tested: Type inference, unification, assignment validity, function calls, method calls
- Files: `src/tests/typecheck_test.c` (empty file)
- Risk: Type checking bugs only discovered at manual testing; regressions introduced silently
- Priority: High - Type checker is core to compiler correctness

**Code Generation Tests Missing:**
- What's not tested: Any code generation (feature not implemented)
- Files: `src/tests/codegen_test.c` (empty file)
- Risk: No validation that generated code is correct; crashes or miscompilation may go undetected
- Priority: High - Required before any output can be trusted

**Parser Coverage Incomplete:**
- What's not tested: Most statement types, complex expressions, error recovery, edge cases
- Files: `src/tests/parser_test.c` (only 59 lines)
- Risk: Parser bugs with complex input; syntax errors may not be detected properly
- Priority: Medium - Parser is stable but lacks comprehensive coverage

**Memory Realm Validation Tests Missing:**
- What's not tested: Realm nesting rules, strategy transitions, invalid combinations
- Files: No dedicated realm tests
- Risk: Memory safety violations may compile; execution safety not validated
- Priority: High - Core language feature for safety

**Operator Precedence Tests Missing:**
- What's not tested: All operator combinations, associativity, mixed operators
- Files: No operator precedence tests
- Risk: Operator precedence bugs cause silent semantic errors
- Priority: Medium - Affects correctness but less frequent than type errors

---

*Concerns audit: 2026-04-02*
