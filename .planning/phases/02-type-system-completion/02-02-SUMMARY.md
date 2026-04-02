---
phase: 02-type-system-completion
plan: 02
subsystem: type-checker
tags: [struct-validation, variant-payload, const-check, bool-strictness, typecheck]

requires:
  - phase: 01-foundation-fixes
    provides: "TY_INFER_ERROR poison type, type_is_resolved guard pattern, strict binary expression checking"
provides:
  - "Struct constructor missing-field detection with default-value awareness"
  - "Variant arm payload type and count validation"
  - "Const reassignment rejection in AST_ASSIGN"
  - "4 expected-failure tests proving struct, variant, const, and bool strictness"
affects: [03-pattern-matching, 04-interface-system]

tech-stack:
  added: []
  patterns: ["field_provided bool array tracking in struct constructor", "variant arm name resolution via callee AST kind"]

key-files:
  created:
    - src/tests/samples/type_struct_fields.runes
    - src/tests/samples/type_variant_payload.runes
    - src/tests/samples/type_const_assign.runes
    - src/tests/samples/type_bool_strict.runes
  modified:
    - src/typecheck.c
    - src/tests/samples/02_functions.runes
    - src/tests/samples/12_builtins.runes

key-decisions:
  - "Variant arm name determined from callee AST kind (AST_IDENTIFIER or AST_FIELD_EXPR)"
  - "Struct missing-field check walks AST_TYPE_DECL fields to detect defaults via field_decl.default_val"

patterns-established:
  - "field_provided tracking: arena-allocated bool array indexed by struct field position"
  - "Variant payload validation: single vs tuple vs no-payload branching"

requirements-completed: [TYPE-05, TYPE-06, TYPE-08, TYPE-09]

duration: 5min
completed: 2026-04-02
---

# Phase 2 Plan 2: Struct/Variant/Const/Bool Strictness Summary

**Struct missing-field detection, variant payload type validation, const reassignment rejection, and boolean condition strictness with 4 new expected-failure tests**

## Performance

- **Duration:** 5 min
- **Started:** 2026-04-02T21:39:38Z
- **Completed:** 2026-04-02T21:44:18Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Struct constructors now detect missing required fields (respecting defaults) and report which field is missing
- Variant constructors validate payload type and count (single, tuple, and no-payload arms)
- Const reassignment produces clear "Cannot reassign constant" error
- Bool strictness in if/while conditions was already implemented; now tested with dedicated test file
- All 36 tests pass (27 normal + 9 expected failures), zero compiler warnings

## Task Commits

Each task was committed atomically:

1. **Task 1: Struct missing-field detection, variant payload validation, const reassignment** - `b73af79` (feat)
2. **Task 2: Create test files for struct, variant, const, and bool strictness** - `18c6ac9` (test)

## Files Created/Modified
- `src/typecheck.c` - Added struct field tracking, variant payload validation, const reassignment check
- `src/tests/samples/type_struct_fields.runes` - Expected-failure test for missing struct fields
- `src/tests/samples/type_variant_payload.runes` - Expected-failure test for variant payload type mismatch
- `src/tests/samples/type_const_assign.runes` - Expected-failure test for const reassignment
- `src/tests/samples/type_bool_strict.runes` - Expected-failure test for non-bool if conditions
- `src/tests/samples/02_functions.runes` - Fixed to provide all required struct fields (Task type)
- `src/tests/samples/12_builtins.runes` - Fixed to provide all required struct fields (Node type)

## Decisions Made
- Variant arm name resolved from callee AST kind: AST_IDENTIFIER for bare names, AST_FIELD_EXPR for qualified names (Shape.Circle)
- Struct missing-field check looks up AST_TYPE_DECL via symbol table to check field_decl.default_val for default awareness
- Variant payload test uses qualified syntax (Shape.Circle) because bare variant arm names require resolver support not yet present

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed test files using incomplete struct constructors**
- **Found during:** Task 1 (struct missing-field implementation)
- **Issue:** 02_functions.runes constructed Task(id: 1, name: "shell") missing required `state` field; 12_builtins.runes constructed Node(val: 42) missing required `next` field
- **Fix:** Added missing fields: `state: 0` for Task, `next: 0 as *Node` for Node
- **Files modified:** src/tests/samples/02_functions.runes, src/tests/samples/12_builtins.runes
- **Verification:** All 32 existing tests pass after fix
- **Committed in:** b73af79 (Task 1 commit)

**2. [Rule 1 - Bug] Fixed variant test syntax**
- **Found during:** Task 2 (test file creation)
- **Issue:** Used curly-brace variant syntax `type Shape = { | Circle(f64) }` which parser doesn't support; also bare `Circle(...)` not resolved by resolver
- **Fix:** Used correct syntax `type Shape = | Circle(f64) | Square(f64)` and qualified constructor `Shape.Circle("hello")`
- **Files modified:** src/tests/samples/type_variant_payload.runes
- **Verification:** Test correctly produces "expects payload type" error and passes as expected failure
- **Committed in:** 18c6ac9 (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (2 bugs)
**Impact on plan:** Both fixes necessary for correctness. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviations above.

## User Setup Required
None - no external service configuration required.

## Known Stubs
None - all implementations are complete and wired.

## Next Phase Readiness
- Struct field validation, variant payload checking, const enforcement, and bool strictness are complete
- Ready for Plan 02-03 (tuple checking, schema inheritance) and Plan 02-04 (return path analysis)
- Pattern matching (Phase 3) can now rely on correctly validated variant payloads

---
*Phase: 02-type-system-completion*
*Completed: 2026-04-02*
