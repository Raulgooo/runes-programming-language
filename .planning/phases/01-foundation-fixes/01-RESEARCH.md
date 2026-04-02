# Phase 1: Foundation Fixes - Research

**Researched:** 2026-04-02
**Domain:** C compiler internals -- type system correctness, symbol resolution, test infrastructure
**Confidence:** HIGH

## Summary

Phase 1 fixes four critical correctness bugs in the Runes compiler that corrupt all downstream type checking, plus cleans up compiler warnings and test failures. The bugs are: (1) TY_UNKNOWN silently passes all type checks, hiding real errors; (2) a static Symbol variable in resolver.c corrupts module path lookups; (3) variant arm payload types are left as TY_UNKNOWN instead of being resolved; (4) binary expressions always return the left operand's type, ignoring type promotion. All four have been verified by direct source code reading with exact line numbers.

The fixes are surgical -- each targets a specific function or code pattern. No new files are needed. The changes affect `src/types.h`, `src/types.c`, `src/typecheck.c`, `src/resolver.c`, `src/tests/tester.bash`, and `Makefile`. The user has made explicit decisions (D-01 through D-11) that lock the implementation approach: TY_ERROR as a poison type, maximally strict type promotion (no implicit widening at all), specific handling for each failing test.

**Primary recommendation:** Fix in dependency order -- TY_ERROR split first (all other fixes benefit from it), then static Symbol, then variant payloads, then binary expression types, then warnings/tests. Each fix is independently testable.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Split TY_UNKNOWN into TY_UNKNOWN (not yet inferred) and TY_ERROR (inference failed, already reported)
- **D-02:** TY_ERROR is a poison type -- any check involving TY_ERROR silently passes (suppress ALL downstream checks, not just type mismatches). This prevents cascading error noise.
- **D-03:** If TY_UNKNOWN reaches validation (compiler bug), emit a minimal ICE: "internal error: unresolved type at line X -- please report this bug". Do not crash with assert().
- **D-04:** ALL implicit numeric promotion is rejected. Mixed-width operations (e.g., i8 + i32) require an explicit `as` cast. This applies to same-family widening too -- Runes enforces maximum strictness.
- **D-05:** Mixed-sign operations (e.g., i32 + u32) are always an error requiring explicit cast.
- **D-06:** Type casting (`as` for numeric conversion) and realm promotion (`promote(&t) as dynamic` for memory realm transfer) are completely separate concepts that happen to share the `as` keyword.
- **D-07:** Delete 08_schema_json.runes entirely -- it tests deprecated JSON/schema features that will not be implemented.
- **D-08:** test_type_safety.runes errors (duplicate variant arm, method/field name conflict) are expected rejections -- convert to expected-failure test.
- **D-09:** float_range_tests.runes: keep the overflow line (3.5e38 > f32 max), convert to expected-failure test that validates overflow detection.
- **D-10:** Wrap existing debug printf statements in `#ifdef DEBUG` guards (compile out by default, re-enable with `-DDEBUG` flag). Do not delete them.
- **D-11:** Fix all compiler warnings (sign comparison, unused variables) -- zero warnings on clean build.

### Claude's Discretion
- Implementation details of the TY_UNKNOWN -> TY_ERROR migration (which guards to update, ordering)
- Exact approach for fixing the static Symbol bug in resolver.c (arena allocation specifics)
- How to structure the type_promote() function in types.c (rank table design)
- How to complete variant arm payload type resolution

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| FOUND-01 | TY_UNKNOWN/TY_ERROR split -- failed inference produces TY_ERROR (suppresses cascading), TY_UNKNOWN reaching validation is ICE | 30 TY_UNKNOWN guard locations identified in typecheck.c; SemTypeKind enum in types.h at line 27; TypeContext singleton at line 138; type_is_assignable and type_is_comparable both have TY_UNKNOWN passthrough at lines 173 and 222 |
| FOUND-02 | Static Symbol bug in resolver fixed -- arena-allocated symbols instead of static variable | Static Symbol at resolver.c:158; arena_alloc pattern used everywhere else in codebase |
| FOUND-03 | Variant arm payload types fully resolved -- concrete types, not TY_UNKNOWN | typecheck.c:386 sets arm_types[i] = tc->tctx->type_unknown; variant_arm.fields is a linked list of AST_TYPE_EXPR; typechecker_resolve_type_expr() already handles all type expression kinds |
| FOUND-04 | Binary expressions infer correct result type via type promotion | typecheck.c:540 always returns lty; D-04 says no implicit promotion, so binary ops on mismatched types should ERROR, not promote |
| FOUND-05 | All compiler warnings fixed | 9 warnings on clean build: 7 sign-comparison in parser.c, 1 unused-parameter in parser.c:546, 1 unused-variable in typecheck.c:599 |
| FOUND-06 | All test failures fixed | 5 failing tests: 08_schema_json (delete per D-07), test_type_safety (expected-failure per D-08), float_range_tests (expected-failure per D-09), 03_invalid_names (already correct rejection -- tester needs expected-failure support), match_incorrect (already correct rejection -- tester needs expected-failure support) |
</phase_requirements>

## Standard Stack

No new libraries or dependencies. This phase is entirely modifications to existing C source files.

### Files to Modify

| File | Changes | Purpose |
|------|---------|---------|
| `src/types.h` | Add `TY_INFER_ERROR` enum value (or rename approach) | FOUND-01: Error type split |
| `src/types.c` | Add type_error singleton, update type_is_assignable/type_is_comparable to passthrough on error, add type_promote() stub | FOUND-01, FOUND-04 |
| `src/typecheck.c` | Update 30 TY_UNKNOWN guards, fix binary expr at line 540, resolve variant payloads at line 386, wrap debug prints, fix unused variable | FOUND-01 through FOUND-05 |
| `src/resolver.c` | Replace static Symbol at line 158 with arena-allocated symbol | FOUND-02 |
| `src/parser.c` | Fix sign-comparison warnings (cast or change types), remove unused parameter | FOUND-05 |
| `src/tests/tester.bash` | Add expected-failure support with stderr pattern matching | FOUND-06 |
| `Makefile` | Add `debug` target with `-DDEBUG` flag | D-10 |

### No New Files Needed

All changes are to existing files. No new `.c` or `.h` files.

## Architecture Patterns

### Pattern 1: TY_ERROR Poison Type

**What:** Add a new SemTypeKind value that silently passes all checks, distinct from TY_UNKNOWN (genuinely unresolved).

**Implementation approach:**

The SemTypeKind enum in types.h currently has `TY_ERROR` used for error set types (e.g., `error MathError = ...`). The new "inference failed" type needs a different name to avoid collision.

**Naming options (Claude's discretion):**
- Rename existing `TY_ERROR` to `TY_ERROR_SET` and use `TY_ERROR` for the poison type
- Add `TY_INFER_ERROR` for the poison type, keep `TY_ERROR` for error sets

Recommendation: Use `TY_INFER_ERROR` -- it is more descriptive and avoids renaming an existing enum value that may be referenced in many places.

**Guard update pattern:**

Current pattern (30 locations in typecheck.c):
```c
if (lty->kind != TY_UNKNOWN && rty->kind != TY_UNKNOWN) {
    // perform check
}
```

New pattern:
```c
if (lty->kind == TY_INFER_ERROR || rty->kind == TY_INFER_ERROR) {
    // silently pass -- error already reported
} else if (lty->kind == TY_UNKNOWN || rty->kind == TY_UNKNOWN) {
    // ICE: should not reach here in production
} else {
    // perform check
}
```

Or more concisely with a helper:
```c
static bool is_error_or_unknown(Type *t) {
    return t->kind == TY_INFER_ERROR || t->kind == TY_UNKNOWN;
}

// In guards:
if (!is_error_or_unknown(lty) && !is_error_or_unknown(rty)) {
    // perform check
}
```

The helper approach is cleaner for the 30 guard locations. TY_INFER_ERROR should suppress checks. TY_UNKNOWN should also suppress for now, but the ICE check (D-03) goes at a final validation point, not at every guard.

**Where to emit TY_INFER_ERROR:** When typechecker_infer_expr fails to determine a type and has already emitted an error message, return the type_error singleton instead of type_unknown.

**TypeContext addition:**
```c
// In TypeContext struct (types.h)
Type *type_error;  // inference-failed poison type

// In type_context_init (types.c)
ctx->type_error = arena_alloc(arena, sizeof(Type));
ctx->type_error->kind = TY_INFER_ERROR;
```

**type_is_assignable / type_is_comparable update:**
```c
// At top of both functions, before any other checks:
if (target->kind == TY_INFER_ERROR || source->kind == TY_INFER_ERROR)
    return true;  // suppress cascading
```

### Pattern 2: Arena-Allocated Symbol Replacement

**What:** Replace `static Symbol tmp` at resolver.c:158 with arena-allocated memory.

**Current code:**
```c
static Symbol tmp;
tmp.name = name;
tmp.kind = get_node_sym_kind(decl);
tmp.node = decl;
tmp.is_pub = true;
current = &tmp;
```

**Fix:**
```c
Symbol *tmp = arena_alloc(r->arena, sizeof(Symbol));
tmp->name = name;
tmp->kind = get_node_sym_kind(decl);
tmp->node = decl;
tmp->is_pub = true;
current = tmp;
```

The resolver struct has an `arena` field (verified). The change is straightforward -- allocate from arena instead of using static storage. Each call frame gets its own Symbol.

### Pattern 3: Variant Payload Type Resolution

**What:** Replace `arm_types[i] = tc->tctx->type_unknown` with actual resolution of the field type expressions.

**Current code (typecheck.c:384-386):**
```c
if (a->as.variant_arm.fields) {
    // ... (Simplified for v0.1)
    arm_types[i] = tc->tctx->type_unknown;
}
```

**Fix approach:** The variant arm's `fields` is a linked list of AST_TYPE_EXPR nodes. For single-payload variants (e.g., `Hex(str)`), resolve the single type expression. For multi-payload variants (e.g., `RGB(u8, u8, u8)`), create a tuple type from all field type expressions.

```c
if (a->as.variant_arm.fields) {
    // Count fields
    int field_count = 0;
    AstNode *f = a->as.variant_arm.fields;
    while (f) { field_count++; f = f->next; }
    
    if (field_count == 1) {
        arm_types[i] = typechecker_resolve_type_expr(tc, a->as.variant_arm.fields);
    } else {
        // Multi-field: create tuple type
        Type **field_ts = arena_alloc(tc->arena, sizeof(Type*) * field_count);
        f = a->as.variant_arm.fields;
        for (int j = 0; j < field_count; j++) {
            field_ts[j] = typechecker_resolve_type_expr(tc, f);
            f = f->next;
        }
        arm_types[i] = type_new_tuple(tc->tctx, field_ts, field_count);
    }
} else {
    arm_types[i] = NULL;  // unit variant
}
```

### Pattern 4: Binary Expression Type Strictness

**What:** Per D-04, Runes rejects ALL implicit numeric promotion. Binary ops on mismatched types are errors.

**Current code (typecheck.c:540):**
```c
inferred = lty;  // always returns left type
```

**Fix approach:** For arithmetic binary ops (+, -, *, /, %), check that left and right types are exactly equal (both primitives, same name). If not, emit a type mismatch error. Return the common type (which must be equal).

```c
// After verifying both sides are numeric:
if (lty->kind == TY_PRIMITIVE && rty->kind == TY_PRIMITIVE) {
    if (type_equals(lty, rty)) {
        inferred = lty;
    } else {
        typechecker_error(tc, expr->line, expr->col,
            "Type mismatch in arithmetic: '%s' and '%s' (use 'as' to cast)",
            lty->as.primitive.name, rty->as.primitive.name);
        inferred = tc->tctx->type_error;
    }
}
```

Note: This is simpler than implementing a full type_promote() function. Since D-04 says NO implicit promotion, the "promotion" for Phase 1 is really just strict equality checking with a good error message. A `type_promote()` function that returns the common type or NULL (meaning "incompatible, needs cast") can be added for future use, but for Phase 1 the behavior is: mismatched types = error.

**Important:** Pointer arithmetic (`ptr + int`) must still be allowed -- the existing check at lines 528-533 handles this. The fix should preserve that logic.

### Pattern 5: Debug Print Guards

**Current code (typecheck.c:580-584):**
```c
printf("DEBUG ASSERT: Cannot assign %d to %d\n", rty->kind, lty->kind);
if (lty->kind == TY_PRIMITIVE)
    printf("DEBUG: lty name: %s\n", lty->as.primitive.name);
if (rty->kind == TY_PRIMITIVE)
    printf("DEBUG: rty name: %s\n", rty->as.primitive.name);
```

**Fix:**
```c
#ifdef DEBUG
printf("DEBUG ASSERT: Cannot assign %d to %d\n", rty->kind, lty->kind);
if (lty->kind == TY_PRIMITIVE)
    printf("DEBUG: lty name: %s\n", lty->as.primitive.name);
if (rty->kind == TY_PRIMITIVE)
    printf("DEBUG: rty name: %s\n", rty->as.primitive.name);
#endif
```

**Makefile addition:**
```makefile
debug: CFLAGS += -DDEBUG
debug: $(TARGET)
```

### Anti-Patterns to Avoid
- **Do not add TY_INFER_ERROR checks at every single guard individually** -- use a helper function to keep the 30 guard updates mechanical and consistent.
- **Do not implement full type promotion in Phase 1** -- D-04 says no implicit widening. Just enforce strict equality for binary ops. Promotion logic belongs in Phase 2 (TYPE-01).
- **Do not modify test file content to make tests pass** -- the decision is to fix the test infrastructure (expected-failure support), not change what the tests test.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Expected-failure test framework | C test harness | Extend tester.bash with exit code + stderr matching | Already have bash runner; no-new-dependencies constraint |
| Type error suppression | Per-check error counting | TY_INFER_ERROR poison type pattern | Poison types are the standard compiler approach; avoids N*M error/check combinations |

## Common Pitfalls

### Pitfall 1: TY_INFER_ERROR in type_equals

**What goes wrong:** If TY_INFER_ERROR is added but type_equals() doesn't handle it, two error types might compare as "not equal" causing secondary errors.
**Why it happens:** type_equals has a switch on kind; a new kind without a case hits the default.
**How to avoid:** Add a case for TY_INFER_ERROR in type_equals that returns true (any error type equals any other error type, since errors are poison).
**Warning signs:** "Type mismatch" errors appearing after an inference failure was already reported.

### Pitfall 2: Forgetting TY_UNKNOWN in type_is_assignable

**What goes wrong:** After adding TY_INFER_ERROR passthrough, the existing TY_UNKNOWN passthrough at types.c:173 is still needed during the transition. Removing it prematurely will cause regressions in code paths that haven't been updated to emit TY_INFER_ERROR yet.
**Why it happens:** TY_UNKNOWN is still returned by some code paths that haven't been migrated.
**How to avoid:** Keep the TY_UNKNOWN passthrough during Phase 1. The ICE check (D-03) should be added as a final validation pass that runs after all inference, not as an inline check that fires during inference.
**Warning signs:** Tests that previously passed now report type mismatches on expressions with unresolved types.

### Pitfall 3: Sign-Comparison Fix Breaking Line Tracking

**What goes wrong:** The 7 sign-comparison warnings in parser.c compare `int` (line number) with `uint32_t` (prev_line). Changing the type of one side could break other comparisons elsewhere.
**Why it happens:** `p->current.line` is int but `p->prev_line` is uint32_t.
**How to avoid:** Cast at the comparison site: `(uint32_t)p->current.line == p->prev_line`. Do not change the type of `line` in the Token struct (that would cascade through the entire codebase).
**Warning signs:** Compilation succeeds but line numbers in error messages are wrong.

### Pitfall 4: Expected-Failure Test with Wrong Exit Code

**What goes wrong:** tester.bash currently treats exit code != 0 as failure. Some "expected failure" tests might exit with code 2 (resolver error) instead of 1 (type error). The expected-failure pattern must match on exit code AND error text.
**Why it happens:** Different compiler phases use different exit codes.
**How to avoid:** Expected-failure tests should check that exit code is non-zero (not specifically 1) AND that stderr contains expected error patterns.
**Warning signs:** Tests marked as expected-failure still show as failures because exit code doesn't match.

### Pitfall 5: Variant Payload Resolution Ordering

**What goes wrong:** If variant payload types reference types that haven't been registered yet (forward references), typechecker_resolve_type_expr returns type_unknown.
**Why it happens:** Variants are processed in declaration order; a variant arm might reference a struct declared later.
**How to avoid:** The resolver's first pass (collect_decls) should register all type names before the type checker resolves variant payloads. Check that the current two-pass approach handles this correctly.
**Warning signs:** Variant payloads resolve to TY_UNKNOWN even though the referenced type exists in the file.

## Code Examples

### Example 1: Complete TY_INFER_ERROR Addition to types.h

```c
// Source: direct analysis of src/types.h
typedef enum {
  TY_PRIMITIVE,
  TY_POINTER,
  TY_ARRAY,
  TY_TUPLE,
  TY_FUNCTION,
  TY_FALLIBLE,
  TY_STRUCT,
  TY_VARIANT,
  TY_INTERFACE,
  TY_ERROR,       // error MathError = { | DivByZero | Overflow }
  TY_UNKNOWN,     // pending inference
  TY_INFER_ERROR, // inference failed, error already reported (poison)
} SemTypeKind;

// In TypeContext:
Type *type_error;    // singleton for TY_INFER_ERROR
```

### Example 2: Helper Function for Guards

```c
// Source: pattern for updating 30 guard locations in typecheck.c
static inline bool type_is_resolved(Type *t) {
  return t && t->kind != TY_UNKNOWN && t->kind != TY_INFER_ERROR;
}

// Usage in guards (replaces `lty->kind != TY_UNKNOWN && rty->kind != TY_UNKNOWN`):
if (type_is_resolved(lty) && type_is_resolved(rty)) {
    // perform check
}
```

### Example 3: Expected-Failure Test Convention

```bash
# In tester.bash, expected-failure test detection
# Convention: files in samples/ starting with "expect_fail_" or having
# a first-line comment "-- EXPECT FAIL: <pattern>"

for f in src/tests/samples/*.runes; do
  expected_pattern=$(head -1 "$f" | grep -oP '(?<=-- EXPECT FAIL: ).*')
  output=$(./runes src/std/prelude.runes "$f" 2>&1)
  status=$?
  name=$(basename "$f")
  
  if [ -n "$expected_pattern" ]; then
    # Expected-failure test
    if [ $status -ne 0 ] && echo "$output" | grep -q "$expected_pattern"; then
      echo "PASS (expected failure) $name"
    else
      echo "FAIL $name (expected failure with '$expected_pattern')"
    fi
  else
    # Normal test
    if [ $status -eq 0 ]; then
      echo "PASS $name"
    else
      echo "FAIL $name ($status)"
    fi
  fi
done
```

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Custom bash runner (tester.bash) |
| Config file | `src/tests/tester.bash` |
| Quick run command | `bash src/tests/tester.bash` |
| Full suite command | `bash src/tests/tester.bash` (same -- all tests run in ~2 seconds) |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FOUND-01 | TY_INFER_ERROR suppresses cascading errors | integration (compile .runes with type error, verify single error) | `bash src/tests/tester.bash` | Needs new test file |
| FOUND-01 | TY_UNKNOWN reaching validation emits ICE | integration (compile .runes with deliberately unresolved type) | `bash src/tests/tester.bash` | Needs new test file |
| FOUND-02 | Module path resolution under nested lookups | integration (compile .runes with nested module access) | `bash src/tests/tester.bash` | `09_modules.runes` exists (passes) |
| FOUND-03 | Variant constructors have concrete payload types | integration (compile .runes with variant constructor + payload) | `bash src/tests/tester.bash` | `04_types_interfaces.runes` partially covers |
| FOUND-04 | Binary `i32 + i64` is an error (strict mode per D-04) | integration (expected-failure test) | `bash src/tests/tester.bash` | Needs new expected-failure test |
| FOUND-05 | Zero compiler warnings on clean build | build check | `make clean && make 2>&1 \| grep warning \| wc -l` | N/A (build system check) |
| FOUND-06 | All tests pass (expected failures handled correctly) | integration | `bash src/tests/tester.bash` | Needs tester.bash enhancement |

### Sampling Rate
- **Per task commit:** `make clean && make 2>&1 | grep -c warning` then `bash src/tests/tester.bash`
- **Per wave merge:** Same (full suite is fast)
- **Phase gate:** Zero warnings + all tests pass (including expected-failure tests showing correct behavior)

### Wave 0 Gaps
- [ ] New test file for TY_INFER_ERROR cascading suppression (e.g., `expect_fail_cascade.runes`)
- [ ] New test file for mixed-type binary expression rejection (e.g., `expect_fail_mixed_arithmetic.runes`)
- [ ] Enhanced tester.bash with expected-failure support
- [ ] Convention for marking expected-failure tests (first-line comment or filename pattern)

## Current Codebase State (Verified)

### Build Warnings (9 total)

| File | Line | Warning | Fix |
|------|------|---------|-----|
| parser.c | 356 | sign-compare: int vs uint32_t | Cast: `(uint32_t)p->current.line` |
| parser.c | 546 | unused parameter `is_pub` | Add `(void)is_pub;` or use attribute |
| parser.c | 1954 | sign-compare | Cast at comparison site |
| parser.c | 1969 | sign-compare | Cast at comparison site |
| parser.c | 2063 | sign-compare | Cast at comparison site |
| parser.c | 2083 | sign-compare | Cast at comparison site |
| parser.c | 2104 | sign-compare | Cast at comparison site |
| parser.c | 2125 | sign-compare | Cast at comparison site |
| typecheck.c | 599 | unused variable `arg_count` | Remove or use the variable |

### Test Results (5 failures, all accounted for)

| Test | Status | Category | Action per CONTEXT.md |
|------|--------|----------|----------------------|
| 08_schema_json.runes | FAIL (25 errors) | Deprecated feature | D-07: Delete entirely |
| test_type_safety.runes | FAIL (5 errors) | Expected rejections | D-08: Convert to expected-failure |
| float_range_tests.runes | FAIL (1 error) | Overflow detection working correctly | D-09: Convert to expected-failure |
| 03_invalid_names.runes | FAIL (1 error) | Expected rejection (undefined var + duplicate decl) | Convert to expected-failure |
| match_incorrect.runes | FAIL (3 errors) | Expected rejection (type mismatches in match) | Convert to expected-failure |

### TY_UNKNOWN Guard Locations (30 in typecheck.c)

Lines: 92, 99, 106, 114, 136, 526, 548, 558, 578, 650, 697, 706, 745, 750, 770, 866, 882, 913, 914, 927, 967, 982, 984, 1070, 1169, 1170, 1195, 1222, 1250 (plus type_is_assignable:173 and type_is_comparable:222 in types.c).

All 30+ guards follow the same pattern: `if (X->kind != TY_UNKNOWN)`. With the `type_is_resolved()` helper, all can be updated mechanically.

## Open Questions

1. **Naming: TY_INFER_ERROR vs renaming TY_ERROR**
   - What we know: TY_ERROR currently means "error set type" (e.g., `error MathError`). The new "inference failed" type needs a distinct name.
   - What's unclear: Whether downstream phases (Phase 2+) would benefit from TY_ERROR being the poison type name.
   - Recommendation: Use TY_INFER_ERROR for now. If later phases find it awkward, rename is trivial (enum values are compile-time constants).

2. **ICE check placement for TY_UNKNOWN (D-03)**
   - What we know: D-03 says TY_UNKNOWN reaching validation should emit an ICE, not crash.
   - What's unclear: Where exactly "validation" is -- after the full typechecker_check_program() run? At each statement? As a separate final pass?
   - Recommendation: Add a single check after typechecker_check_program() returns, walking the AST for any nodes whose resolved_type is still TY_UNKNOWN. This is cleanest and catches all cases without adding checks to every guard.

3. **Resolver arena access**
   - What we know: The static Symbol fix needs arena_alloc. The Resolver struct should have an arena field.
   - What's unclear: Need to verify the Resolver struct has an arena pointer.
   - Recommendation: Check at implementation time. If not present, the Resolver uses the same arena as the TypeChecker -- just add the field.

## Sources

### Primary (HIGH confidence)
- Direct reading of `src/types.h` (enum at line 7-28, TypeContext at line 117-139)
- Direct reading of `src/types.c` (type_is_assignable at line 167, type_is_comparable at line 216)
- Direct reading of `src/typecheck.c` (30 TY_UNKNOWN guards, binary expr at line 540, variant payload at line 386, debug prints at lines 580-584)
- Direct reading of `src/resolver.c` (static Symbol at line 158)
- Direct reading of `src/parser.c` (warning locations from gcc output)
- Compiler build output (9 warnings enumerated)
- Test runner output (5 failures enumerated with exact error messages)

### Secondary (MEDIUM confidence)
- PITFALLS.md research document (bug descriptions verified against source)
- STACK.md research document (rank table algorithm -- standard compiler technique)
- CONTEXT.md decisions (D-01 through D-11 -- user-locked)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, all changes to existing files verified by reading
- Architecture: HIGH -- all patterns verified against actual source code with line numbers
- Pitfalls: HIGH -- enumerated from direct code reading and build/test output

**Research date:** 2026-04-02
**Valid until:** 2026-05-02 (stable -- C codebase, no external dependency drift)
