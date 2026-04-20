# Phase 1: Foundation Fixes - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-02
**Phase:** 01-foundation-fixes
**Areas discussed:** Error type strategy, Type promotion rules, Test failure handling, Debug cleanup scope

---

## Error Type Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Suppress all | TY_ERROR is a poison type — any check involving TY_ERROR silently passes | ✓ |
| Suppress mismatches only | TY_ERROR suppresses type mismatch errors but still enforces other checks | |
| You decide | Claude picks the approach that matches common compiler practice | |

**User's choice:** Suppress all (Recommended)
**Notes:** TY_ERROR should prevent all cascading error noise.

| Option | Description | Selected |
|--------|-------------|----------|
| Minimal + file a bug | "internal error: unresolved type at line X — please report this bug" | ✓ |
| Verbose with context | Include node kind, expression text, and surrounding context | |
| Assert + abort | Hard assert() crash with stack trace | |

**User's choice:** Minimal + file a bug
**Notes:** User-friendly ICE message, not a crash.

---

## Type Promotion Rules

| Option | Description | Selected |
|--------|-------------|----------|
| Error, require cast | Mixed sign is always an error — user must explicitly cast | ✓ |
| Promote to signed | u32 widens to i64 to preserve both value ranges | |
| You decide | Claude picks based on Runes safety philosophy | |

**User's choice:** Error, require cast
**Notes:** Maximum strictness for mixed-sign operations.

| Option | Description | Selected |
|--------|-------------|----------|
| Silent promotion | i8 + i32 silently yields i32 | |
| Warn on promotion | Compile succeeds but warns about implicit widening | |
| Error, require cast | Even same-family widening requires explicit cast | ✓ |

**User's choice:** Error, require cast
**Notes:** ALL implicit promotion rejected — maximum strictness. User clarified that type casting (numeric `as`) and realm promotion (`promote() as`) are separate concepts.

---

## Test Failure Handling

| Option | Description | Selected |
|--------|-------------|----------|
| Delete the test file | Remove 08_schema_json.runes entirely | ✓ |
| Strip JSON parts | Keep schema struct/inheritance tests, remove JSON calls | |
| Keep as expected-fail | Mark as known-failure test | |

**User's choice:** Delete the test file
**Notes:** JSON/schema features are deprecated.

| Option | Description | Selected |
|--------|-------------|----------|
| Real bugs to fix | Compiler incorrectly rejecting valid code | |
| Expected rejections | These ARE invalid programs — convert to expected-failure | ✓ |
| Need to check | Let Claude determine | |

**User's choice:** Expected rejections

| Option | Description | Selected |
|--------|-------------|----------|
| Delete stray line | Remove line 11, keep as passing positive test | |
| Make negative test | Keep overflow line, convert to expected-failure test | ✓ |

**User's choice:** Make negative test
**Notes:** Line 11 (f32 overflow = 3.5e38) exceeds f32 max. Convert to expected-failure test.

---

## Debug Cleanup Scope

| Option | Description | Selected |
|--------|-------------|----------|
| Remove all debug prints | Clean stderr — only real error messages | |
| Keep behind #ifdef DEBUG | Wrap in #ifdef DEBUG, compile out by default | ✓ |
| You decide | Claude picks whatever keeps codebase clean | |

**User's choice:** Keep behind #ifdef DEBUG

---

## Claude's Discretion

- Implementation details of TY_UNKNOWN → TY_ERROR migration
- Static Symbol bug fix approach
- type_promote() function design
- Variant arm payload type resolution approach

## Deferred Ideas

None — discussion stayed within phase scope.
