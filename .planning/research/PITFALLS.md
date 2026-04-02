# Pitfalls Research — Runes Compiler Frontend Completion

**Analysis Date:** 2026-04-02
**Confidence:** HIGH (all findings from direct codebase analysis)

## Critical Pitfalls

### 1. TY_UNKNOWN Swallows Errors (CRITICAL)

**The Problem:** The `if (kind != TY_UNKNOWN)` guard pattern used 15+ times in `typecheck.c` means unresolved types silently pass all checks. Any expression that fails to infer a type gets `TY_UNKNOWN`, and then every subsequent check against it succeeds — hiding real errors.

**Warning Signs:** Tests pass but programs with type errors compile without warnings.

**Prevention:** Split into `TY_UNKNOWN` (genuinely not yet inferred) and `TY_ERROR` (inference failed, already reported). `TY_ERROR` should propagate silently (suppress cascading errors). `TY_UNKNOWN` should trigger an internal compiler error if it reaches codegen.

**Phase:** Foundation / pre-work — must fix before any other type system work.

### 2. Static Symbol Bug in Resolver (CRITICAL)

**The Problem:** `resolver.c:159` uses `static Symbol tmp` for module path resolution. This will corrupt results under any nested resolution because the static variable is shared across all call frames.

**Warning Signs:** Module lookups return wrong symbols when modules are nested or when resolution triggers recursive lookups.

**Prevention:** Replace with arena-allocated symbols. The arena pattern is already used everywhere else in the compiler.

**Phase:** Foundation / pre-work — must fix before cross-file module work.

### 3. Exhaustiveness Depends on Incomplete Variant Types (CRITICAL)

**The Problem:** Variant arm payloads are currently stored as `TY_UNKNOWN`. Any exhaustiveness checking built on this foundation will produce wrong results — it can't verify that all constructors are covered if it doesn't know the constructors' types.

**Warning Signs:** Exhaustiveness checker reports "all arms covered" even when payload patterns don't match.

**Prevention:** Complete variant type resolution first. Every variant constructor must have a fully resolved payload type before exhaustiveness checking runs.

**Phase:** Must complete variant resolution before implementing exhaustiveness.

### 4. Binary Expressions Always Return Left Type (CRITICAL)

**The Problem:** `typecheck.c:540` — binary expressions always return `lty` (left operand type). This means `i32 + i64` infers `i32` (wrong — should promote to `i64`). Without type promotion, arithmetic on mixed types will produce silently wrong results.

**Warning Signs:** `i32 + i64` expressions don't error and infer `i32`.

**Prevention:** Implement `type_promote()` function in `types.c` with numeric rank table. Binary expressions should call this to determine result type.

**Phase:** Type promotion phase — early in the roadmap.

## High-Priority Pitfalls

### 5. Realm Analysis Pass Ordering

**The Problem:** Realm/lifetime analysis depends on complete type information. If realm checking runs before types are fully resolved, it can't determine whether a value is a copy type (safe to escape) or a pointer type (needs promote).

**Warning Signs:** Realm checker incorrectly allows pointer values to escape scope, or incorrectly rejects copy-type returns.

**Prevention:** Realm analysis must be the final semantic pass, after all type inference and resolution is complete. Keep realm nesting matrix in typecheck.c (already there), but add scope-exit/escape analysis as a final pass.

**Phase:** Last semantic phase.

### 6. Interface Satisfaction Without Method Resolution

**The Problem:** Interface satisfaction checking requires knowing all methods on a type. If struct methods aren't fully resolved (method blocks parsed but types not checked), interface checking will miss methods or accept wrong signatures.

**Warning Signs:** Types falsely satisfy interfaces, or methods with wrong signatures are accepted.

**Prevention:** Complete method type checking before implementing interface satisfaction. Method signatures must be fully resolved including parameter types and return types.

**Phase:** After type system completion, before module system.

### 7. Module Visibility Leaking

**The Problem:** Currently all symbols in all modules are visible to all code. When adding `pub`/private visibility, existing tests that rely on this open visibility will break.

**Warning Signs:** Mass test failures when visibility enforcement is added.

**Prevention:** Add visibility enforcement incrementally. First: mark everything correctly. Then: enforce at cross-module boundaries only. Ensure existing tests either use `pub` or access within the same module.

**Phase:** Module system phase.

### 8. Negative Literal Overflow

**The Problem:** The parser creates negative literals by applying unary minus to a positive literal. If overflow checking runs on the positive literal first, `-128` as `i8` will incorrectly report overflow (because `128` doesn't fit in `i8`, but `-128` does).

**Warning Signs:** `i8 x = -128` triggers false overflow error.

**Prevention:** Literal range checking must account for unary negation context. Either defer overflow checking until after unary operators are processed, or check the combined value.

**Phase:** Type promotion / literal checking phase.

## Medium-Priority Pitfalls

### 9. Pointer Equality for Type Comparison

**The Problem:** If type objects are duplicated (created separately with same content), pointer equality comparison will fail even though the types are structurally identical. The string table interns strings, but types may not be interned.

**Warning Signs:** Same type compared to itself returns "not equal".

**Prevention:** Use structural comparison for types (already done in `type_equals`). Only use pointer equality as an optimization after confirming type interning guarantees.

**Phase:** Ongoing concern during all type system work.

### 10. Flex Realm Complexity

**The Problem:** `flex f` functions inherit their caller's memory realm. Full verification requires re-checking the function body under each caller's context, which is expensive and complex.

**Warning Signs:** Flex functions accepted at compile time but fail at runtime with wrong allocation strategy.

**Prevention:** For v0.1, use conservative rule: flex function bodies may only perform realm-neutral operations (no direct allocation, no promote). This is simpler and correct, though restrictive.

**Phase:** Realm checking phase.

### 11. Match Guard Coverage

**The Problem:** Match arms with guards (`if condition`) must NOT count toward exhaustiveness coverage, because the guard may be false. A match with all variants covered but every arm guarded is not exhaustive.

**Warning Signs:** Guarded match considered exhaustive when it isn't.

**Prevention:** Exhaustiveness algorithm must treat guarded arms as non-contributing. Only unguarded arms establish coverage.

**Phase:** Exhaustiveness checking phase.

### 12. Cross-File Module Cycle Detection

**The Problem:** If file A imports module from file B, and file B imports from file A, naive resolution will infinite-loop or stack overflow.

**Warning Signs:** Compiler hangs or crashes on circular imports.

**Prevention:** Maintain a "currently loading" set during module resolution. If a module is encountered that's already in the set, report a cycle error.

**Phase:** Module system phase.

### 13. Arena Lifetime vs Module Scope

**The Problem:** Symbols from different files are allocated in the same arena. If module resolution creates symbols that reference other files' AST nodes, arena cleanup order matters.

**Warning Signs:** Use-after-free or corrupted data when processing multiple files.

**Prevention:** Use a single arena for the entire compilation (already the pattern). Don't try to free per-file arenas independently.

**Phase:** Module system phase.

### 14. Schema Inheritance Chain

**The Problem:** Schemas can inherit from other schemas. Type checking must walk the inheritance chain to validate field access and method resolution. Missing chain walking means derived schema fields are invisible.

**Warning Signs:** Fields from parent schema not accessible on derived schema.

**Prevention:** Build an inheritance chain during type resolution. Cache the flattened field set per schema.

**Phase:** Type system completion.

### 15. Error Type Propagation

**The Problem:** Fallible types (`!T`) must propagate correctly through expressions. `try` unwraps to `T`, `catch` handles the error. If propagation is incomplete, error types can leak into non-error contexts.

**Warning Signs:** Error types appearing where non-error types expected, without compiler warnings.

**Prevention:** Ensure every expression that produces `!T` is consumed by `try` or `catch`. Uncaught fallible types at statement level should be an error.

**Phase:** Type system completion.

### 16. Test Suite Regression During Refactoring

**The Problem:** As type system changes are made, existing passing tests may regress. Without CI or automated regression detection, regressions can go unnoticed.

**Warning Signs:** Previously passing tests failing after changes.

**Prevention:** Run `src/tests/tester.bash` after every significant change. Consider adding a make target that fails on any regression.

**Phase:** All phases — ongoing discipline.

## Recommended Fix Order

1. **Pre-work:** TY_UNKNOWN/TY_ERROR split, static symbol fix, pointer equality for primitives
2. **Type promotion:** `type_promote()`, split `type_is_assignable`, fix negative literal overflow
3. **Interface checking:** Dedicated `type_satisfies_interface()` ignoring memory strategy
4. **Module resolution:** Arena lifetime, visibility as post-resolution check
5. **Exhaustiveness:** Complete variant types first, per-subject-type rules
6. **Realm analysis:** Implement in `realm_check.c` as final pass
