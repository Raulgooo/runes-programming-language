# Phase 1: Foundation Fixes - Context

**Gathered:** 2026-04-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix 4 critical bugs that corrupt type inference, module lookups, variant resolution, and binary expression types. Clean up compiler warnings and test failures. The goal is a correct, clean baseline that all downstream phases can rely on.

Requirements: FOUND-01 through FOUND-06.

</domain>

<decisions>
## Implementation Decisions

### Error Type Strategy
- **D-01:** Split TY_UNKNOWN into TY_UNKNOWN (not yet inferred) and TY_ERROR (inference failed, already reported)
- **D-02:** TY_ERROR is a poison type — any check involving TY_ERROR silently passes (suppress ALL downstream checks, not just type mismatches). This prevents cascading error noise.
- **D-03:** If TY_UNKNOWN reaches validation (compiler bug), emit a minimal ICE: "internal error: unresolved type at line X — please report this bug". Do not crash with assert().

### Type Promotion Rules
- **D-04:** ALL implicit numeric promotion is rejected. Mixed-width operations (e.g., i8 + i32) require an explicit `as` cast. This applies to same-family widening too — Runes enforces maximum strictness.
- **D-05:** Mixed-sign operations (e.g., i32 + u32) are always an error requiring explicit cast.
- **D-06:** Type casting (`as` for numeric conversion) and realm promotion (`promote(&t) as dynamic` for memory realm transfer) are completely separate concepts that happen to share the `as` keyword.

### Test Failure Handling
- **D-07:** Delete 08_schema_json.runes entirely — it tests deprecated JSON/schema features that will not be implemented.
- **D-08:** test_type_safety.runes errors (duplicate variant arm, method/field name conflict) are expected rejections — convert to expected-failure test.
- **D-09:** float_range_tests.runes: keep the overflow line (3.5e38 > f32 max), convert to expected-failure test that validates overflow detection.

### Debug Cleanup
- **D-10:** Wrap existing debug printf statements in `#ifdef DEBUG` guards (compile out by default, re-enable with `-DDEBUG` flag). Do not delete them.
- **D-11:** Fix all compiler warnings (sign comparison, unused variables) — zero warnings on clean build.

### Claude's Discretion
- Implementation details of the TY_UNKNOWN → TY_ERROR migration (which guards to update, ordering)
- Exact approach for fixing the static Symbol bug in resolver.c (arena allocation specifics)
- How to structure the type_promote() function in types.c (rank table design)
- How to complete variant arm payload type resolution

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Language Specification
- `docs/specv0_1.md` — Authoritative language spec; sections 3 (types), 5 (memory model), 7 (user types) are most relevant for foundation fixes

### Implementation Guides
- `type_checker_guide1.md` — Type checker implementation roadmap with phase breakdown

### Research (bug locations and algorithms)
- `.planning/research/PITFALLS.md` — Critical bugs with exact line numbers (TY_UNKNOWN at 15+ guards, static Symbol at resolver.c:159, variant payloads TY_UNKNOWN, binary expression result at typecheck.c:540)
- `.planning/research/STACK.md` — Numeric rank table algorithm for type promotion
- `.planning/research/SUMMARY.md` — Synthesis with build order and confidence assessment

### Codebase Analysis
- `.planning/codebase/CONCERNS.md` — Known bugs, debug statements at typecheck.c:580-584, integer literal range checking gaps

</canonical_refs>

<code_context>
## Existing Code Insights

### Key Files to Modify
- `src/types.h` / `src/types.c` — Add TY_ERROR to TypeKind enum, add type_promote() function with rank table
- `src/typecheck.c` — Update all `if (kind != TY_UNKNOWN)` guards (15+ locations), fix binary expression result type at line 540, wrap debug prints in #ifdef DEBUG
- `src/resolver.c` — Fix static Symbol at line 159, replace with arena-allocated symbol
- `src/symbol_table.h` — May need minor changes for module scope persistence (prep for Phase 5)

### Established Patterns
- Arena allocator (`arena_alloc()`) used exclusively — the static Symbol fix should follow this pattern
- Error reporting via `fprintf(stderr, ...)` with line/column tracking
- Module-prefixed function naming (`typechecker_*`, `resolver_*`)

### Integration Points
- `src/tests/tester.bash` — Needs to support expected-failure tests (exit 1 + error pattern matching)
- `Makefile` — May need -DDEBUG flag support for debug builds

</code_context>

<specifics>
## Specific Ideas

- Type promotion is maximally strict: no implicit widening at all, even within the same sign family. This is a deliberate language design choice for Runes, not a compiler limitation.
- The `as` keyword serves dual purpose (numeric cast + realm promote) but these are semantically distinct operations handled by different compiler phases.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-foundation-fixes*
*Context gathered: 2026-04-02*
