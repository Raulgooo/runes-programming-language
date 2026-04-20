# Phase 2: Type System Completion - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-02
**Phase:** 02-type-system-completion
**Areas discussed:** Numeric promotion strategy, Literal inference rules, Struct/variant/tuple diagnostics, Control flow strictness

---

## Numeric Promotion Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Strict: always explicit cast | i8 + i16 is a type error — user must write (x as i16) + y. Consistent with D-04. | ✓ |
| Auto-widen same-sign only | i8 + i16 auto-widens to i16. Mixed-sign still errors. | |
| Auto-widen all, error on mixed-sign | Same-sign widens freely. Only mixed-sign requires cast. | |

**User's choice:** Strict: always explicit cast
**Notes:** Confirms Phase 1 D-04 carries forward. No implicit widening at all.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Suggest widening cast | Error suggests (x as i32) + y — always widening direction | ✓ |
| Show both options | Error shows both cast directions | |
| No suggestion, just report mismatch | Minimal error, no cast hint | |

**User's choice:** Suggest widening cast
**Notes:** Error messages should be actionable, guiding users to the wider type.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Always error | f64 + i32 is a type error. No cross-family conversion. | ✓ |
| Auto-promote int to float | f64 + i32 auto-widens i32 to f64 | |

**User's choice:** Always error
**Notes:** Cross-family conversions (float+int) rejected, consistent with strict philosophy.

---

## Literal Inference Rules

| Option | Description | Selected |
|--------|-------------|----------|
| Contextual typing | u8 x = 255 works (literal adopts u8). u8 x = 256 overflows. | ✓ |
| Always default type, explicit cast | u8 x = 255 errors (255 is i32). Ultra-strict. | |
| Contextual with warning | Works but warns on narrowing. | |

**User's choice:** Contextual typing
**Notes:** Literals adopt target type if value fits — the one exception to strict typing.

---

| Option | Description | Selected |
|--------|-------------|----------|
| i32 | Uncontextualized integers default to i32 | ✓ |
| i64 | Default to i64 for max range | |
| Smallest fitting type | 42 → i8, 200 → u8, etc. | |

**User's choice:** i32
**Notes:** Standard choice, matches Rust/Go.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Fuse in typed context | i8 x = -128 fused; uncontextualized defaults to i32 | ✓ |
| Always two-step | Parse as unary minus + literal, then check | |
| Always fuse | Lexer produces negative literal tokens | |

**User's choice:** Fuse in typed context
**Notes:** Type checker handles fusion, not lexer.

---

| Option | Description | Selected |
|--------|-------------|----------|
| f64 | Uncontextualized float literals default to f64 | ✓ |
| f32 | Default to f32 | |

**User's choice:** f64
**Notes:** Standard choice.

---

## Struct/Variant/Tuple Diagnostics

| Option | Description | Selected |
|--------|-------------|----------|
| Name missing fields | "Struct Point missing required fields: y, z" | ✓ |
| Name + suggest similar | Adds Levenshtein-distance typo suggestions | |
| Simple count only | "expected 3 fields, got 1" | |

**User's choice:** Name missing fields
**Notes:** Actionable but not over-engineered.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Error on extra fields | "Unknown field z on struct Point" | ✓ |
| Ignore extra fields | Silently ignored | |

**User's choice:** Error on extra fields
**Notes:** Catches typos and wrong-struct usage.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Show expected vs actual | "Variant arm Some expects payload type i32, got str" | ✓ |
| Also show definition location | Adds "defined at line 5" | |
| Minimal type mismatch | Generic error, no variant context | |

**User's choice:** Show expected vs actual
**Notes:** Clear, names the arm and both types.

---

## Control Flow Strictness

| Option | Description | Selected |
|--------|-------------|----------|
| Error: must be bool | "Condition must be bool, got i32". No implicit truthiness. | ✓ |
| Allow integers as truthy/falsy | 0 = false, nonzero = true | |
| Error with hint | Error + suggestion "(x != 0)" | |

**User's choice:** Error: must be bool
**Notes:** Strict, no implicit truthiness. Matches Rust.

---

| Option | Description | Selected |
|--------|-------------|----------|
| Simple reassignment error | "Cannot reassign constant x" | ✓ |
| Include original declaration | Adds "declared const at line 3" | |
| Suggest mutable alternative | Adds "Remove const to make it mutable" | |

**User's choice:** Simple reassignment error
**Notes:** Minimal, const keyword makes intent obvious.

---

| Option | Description | Selected |
|--------|-------------|----------|
| All paths | Every code path must return declared type | ✓ |
| Explicit returns only | Only check explicit return statements | |
| All paths + named return | All paths + named return variable special handling | |

**User's choice:** All paths
**Notes:** Catches missing returns early.

---

## Claude's Discretion

- Implementation approach for literal range checking
- How to implement all-paths return checking
- Schema inheritance chain walking
- Tuple type checking internals
- Implementation order for TYPE requirements
- ICE whitelist expansion

## Deferred Ideas

None — discussion stayed within phase scope.
