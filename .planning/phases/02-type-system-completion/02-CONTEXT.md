# Phase 2: Type System Completion - Context

**Gathered:** 2026-04-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Complete the type checker so all numeric operations, literal assignments, struct/variant/tuple constructions, and control flow expressions type-check correctly per spec v0.1. This builds on Phase 1's foundation (poison type, strict binary expressions, variant payload resolution).

Requirements: TYPE-01 through TYPE-11.

</domain>

<decisions>
## Implementation Decisions

### Numeric Promotion Strategy
- **D-01:** ALL implicit numeric promotion is rejected — same-sign widening (i8 + i32) requires explicit `as` cast. Carries forward from Phase 1 D-04.
- **D-02:** Mixed-sign operations (i32 + u32) always error requiring explicit cast. Carries forward from Phase 1 D-05.
- **D-03:** Float + int operations (f64 + i32) always error — no implicit cross-family conversion.
- **D-04:** Type mismatch errors on mixed-width ops suggest the widening cast direction. E.g., "Type mismatch: i8 and i32. Use explicit cast: (x as i32) + y".
- **D-05:** TYPE-01/TYPE-02 requirements are reinterpreted as: the compiler correctly rejects mixed-width operations with clear, helpful error messages suggesting the needed cast. Not auto-widening.

### Literal Inference Rules
- **D-06:** Contextual typing for integer literals — `u8 x = 255` works (literal adopts target type if value fits). `u8 x = 256` is an overflow error.
- **D-07:** Uncontextualized integer literals default to i32. E.g., `x := 42` infers i32.
- **D-08:** Uncontextualized float literals default to f64. E.g., `x := 3.14` infers f64.
- **D-09:** Unary negation fused in typed context — `i8 x = -128` recognizes -128 as valid i8 (type checker sees the negation + literal as a unit). In uncontextualized context, `-128` defaults to i32.

### Struct/Variant/Tuple Diagnostics
- **D-10:** Struct construction missing fields: error names each missing field. E.g., "Struct Point missing required fields: y, z".
- **D-11:** Extra fields on struct construction: error on unknown fields. E.g., "Unknown field z on struct Point".
- **D-12:** Variant payload mismatch: show expected vs actual. E.g., "Variant arm Some expects payload type i32, got str".

### Control Flow Strictness
- **D-13:** Boolean strictness enforced — if/while conditions must be bool. `if x` where x is i32 is an error: "Condition must be bool, got i32". No implicit truthiness.
- **D-14:** Const reassignment: simple error. "Cannot reassign constant x". No suggestions or back-references.
- **D-15:** Return type verification on ALL code paths. Missing return at end of non-void function is an error.

### Claude's Discretion
- Implementation approach for literal range checking (compile-time evaluation vs lookup tables)
- How to implement all-paths return checking (control flow graph vs recursive descent)
- Schema inheritance chain walking implementation details
- Tuple type checking internals (construction, destructuring, field access)
- Exact order of implementation (which TYPE requirements to tackle first)
- How to extend the existing ICE whitelist as new expression kinds get handlers

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Language Specification
- `docs/specv0_1.md` — Authoritative spec; §3 (types), §4 (expressions/operators), §6 (statements/control flow), §7 (user types: structs, variants, schemas) are critical for this phase

### Implementation Guides
- `type_checker_guide1.md` — Type checker implementation roadmap with phase breakdown

### Phase 1 Context (carry-forward decisions)
- `.planning/phases/01-foundation-fixes/01-CONTEXT.md` — D-04 through D-06 on type strictness, D-01 through D-03 on error types
- `.planning/phases/01-foundation-fixes/01-03-SUMMARY.md` — Binary expression strictness implementation details, literal coercion approach

### Codebase Analysis
- `.planning/codebase/CONVENTIONS.md` — Naming patterns, code style
- `.planning/codebase/ARCHITECTURE.md` — Compiler pipeline, module boundaries
- `.planning/codebase/CONCERNS.md` — Known gaps in integer literal range checking

</canonical_refs>

<code_context>
## Existing Code Insights

### Key Files to Modify
- `src/typecheck.c` — Main type checker; needs literal inference, struct/variant validation, boolean condition checks, const enforcement, return path analysis
- `src/types.h` / `src/types.c` — May need numeric range constants, type utility functions
- `src/ast.h` — Has `is_const` on var_decl, `AST_SCHEMA_DECL`, tuple types already defined

### What Exists
- Binary expression strict type checking (Phase 1) — rejects i32+i64 with error message
- Variant arm payload resolution via `typechecker_resolve_type_expr` (Phase 1)
- Tuple destructuring partial support at typecheck.c:1180
- Struct/variant type checks in field_expr and call_expr handlers
- If/while statement handlers at lines 1240, 1295 (exist but don't enforce bool condition)
- Return statement handler at line 1214 (exists but no all-paths analysis)
- `is_const` field on AST var_decl nodes (parser sets it, type checker doesn't check it yet)
- Schema decl in AST and resolver but no type checker inheritance walking

### Established Patterns
- Error reporting via `typechecker_error(tc, line, col, "format", ...)` 
- Type comparison via `type_equals()`, `type_is_assignable()`, `type_is_comparable()`
- Poison type passthrough via `type_is_resolved()` check before any validation

### Integration Points
- ICE walk whitelist (`should_have_resolved_type`) needs expansion as new expression handlers are added
- Test suite: new test files for each TYPE requirement, expected-failure tests for error cases

</code_context>

<specifics>
## Specific Ideas

- The strict "no implicit promotion" philosophy is a deliberate Runes language design choice, not a compiler limitation. Error messages should guide users to the explicit cast rather than making them feel the language is broken.
- Literal contextual typing is the one exception to strict typing — `u8 x = 255` should feel natural, not require `u8 x = 255 as u8`.
- Unary negation fusion (`i8 x = -128`) is a type checker concern, not a lexer change — the parser already produces AST_UNARY_EXPR wrapping AST_INT_LITERAL.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 02-type-system-completion*
*Context gathered: 2026-04-02*
