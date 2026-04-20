# Phase 2: Type System Completion - Research

**Researched:** 2026-04-02
**Domain:** C compiler type checker implementation -- numeric types, composite types, control flow validation
**Confidence:** HIGH

## Summary

Phase 2 completes the Runes type checker so that numeric operations, literal assignments, struct/variant/tuple constructions, and control flow expressions are fully validated per spec v0.1. The existing codebase (1632 lines in `typecheck.c`) already has substantial infrastructure from Phase 1: binary expression strictness with literal coercion, variant arm payload resolution, poison type passthrough, and boolean condition checking for if/while. However, several critical features are missing or incomplete.

The primary gaps are: (1) integer literal contextual typing with range checking exists only partially -- the `type_is_assignable` function permissively allows i32 to any integer type without range checks, and the VAR_DECL handler has basic range checks but misses negative literal fusion; (2) struct construction does not validate missing fields; (3) variant constructor payload types are not verified (the `TODO: Full variant arm resolution` at line 736); (4) tuple expressions have no handler in `typechecker_infer_expr`; (5) const reassignment is not checked at all; (6) return type verification is per-statement only, not all-paths; (7) schemas have no type checker support whatsoever; (8) cast expressions (`as`) have no handler in `typechecker_infer_expr`.

**Primary recommendation:** Implement in dependency order -- literals/numerics first (foundation for everything), then struct/variant/tuple validation (composite types), then control flow enforcement (const, return paths), then schema inheritance. Each requirement maps to a focused modification area in `typecheck.c` and `types.c`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** ALL implicit numeric promotion is rejected -- same-sign widening (i8 + i32) requires explicit `as` cast. Carries forward from Phase 1 D-04.
- **D-02:** Mixed-sign operations (i32 + u32) always error requiring explicit cast. Carries forward from Phase 1 D-05.
- **D-03:** Float + int operations (f64 + i32) always error -- no implicit cross-family conversion.
- **D-04:** Type mismatch errors on mixed-width ops suggest the widening cast direction. E.g., "Type mismatch: i8 and i32. Use explicit cast: (x as i32) + y".
- **D-05:** TYPE-01/TYPE-02 requirements are reinterpreted as: the compiler correctly rejects mixed-width operations with clear, helpful error messages suggesting the needed cast. Not auto-widening.
- **D-06:** Contextual typing for integer literals -- `u8 x = 255` works (literal adopts target type if value fits). `u8 x = 256` is an overflow error.
- **D-07:** Uncontextualized integer literals default to i32. E.g., `x := 42` infers i32.
- **D-08:** Uncontextualized float literals default to f64. E.g., `x := 3.14` infers f64.
- **D-09:** Unary negation fused in typed context -- `i8 x = -128` recognizes -128 as valid i8. In uncontextualized context, `-128` defaults to i32.
- **D-10:** Struct construction missing fields: error names each missing field. E.g., "Struct Point missing required fields: y, z".
- **D-11:** Extra fields on struct construction: error on unknown fields. E.g., "Unknown field z on struct Point".
- **D-12:** Variant payload mismatch: show expected vs actual. E.g., "Variant arm Some expects payload type i32, got str".
- **D-13:** Boolean strictness enforced -- if/while conditions must be bool. No implicit truthiness.
- **D-14:** Const reassignment: simple error. "Cannot reassign constant x". No suggestions.
- **D-15:** Return type verification on ALL code paths. Missing return at end of non-void function is an error.

### Claude's Discretion
- Implementation approach for literal range checking (compile-time evaluation vs lookup tables)
- How to implement all-paths return checking (control flow graph vs recursive descent)
- Schema inheritance chain walking implementation details
- Tuple type checking internals (construction, destructuring, field access)
- Exact order of implementation (which TYPE requirements to tackle first)
- How to extend the existing ICE whitelist as new expression handlers are added

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TYPE-01 | Integer type promotion -- i8->i16->i32->i64, u8->u16->u32->u64 widening in binary ops and assignments; mixed-sign requires explicit cast | Binary expr handler at lines 535-610 already rejects mismatched types with literal coercion; needs enhanced error message suggesting cast direction per D-04 |
| TYPE-02 | Float type promotion -- f32->f64 in mixed float expressions | Same binary expr handler; already errors on type mismatch; needs cast suggestion |
| TYPE-03 | Literal type inference -- integer literals contextually typed, uncontextualized default to i32/f64 | VAR_DECL handler at line 1110-1176 has partial range check; `type_is_assignable` at line 175 permissively allows i32 to any int without range check; needs overhaul |
| TYPE-04 | Negative literal overflow -- -128 accepted as i8, unary negation context respected | AST_UNARY_EXPR handler at line 744 returns inner type; VAR_DECL range check at line 1129 doesn't account for wrapping AST_UNARY_EXPR around AST_INT_LITERAL |
| TYPE-05 | Struct field type checking -- default values, missing field errors, field suggestions | Struct constructor at line 698-731 checks named args and unknown fields but does NOT check for missing required fields |
| TYPE-06 | Variant constructor type checking -- payload types verified | Line 732-740 has `TODO: Full variant arm resolution`; variant callee returns variant type but does not validate payload args |
| TYPE-07 | Return type verified on all code paths | Return handler at line 1214-1225 checks individual returns but no all-paths analysis exists |
| TYPE-08 | Const correctness -- const bindings cannot be reassigned | `is_const` set by parser on AST var_decl; type checker assign handler at line 613-631 does not check it |
| TYPE-09 | Boolean strictness -- if/while conditions must be bool | Already implemented at lines 1240-1256 (if) and 1296-1303 (while); needs verification with tests |
| TYPE-10 | Tuple type checking -- construction, destructuring, field access, return position | AST_TUPLE_EXPR has no handler in typechecker_infer_expr; tuple destructuring at line 1179-1211 partially works |
| TYPE-11 | Schema inheritance chain -- derived schema fields accessible | No schema support in typecheck.c at all; resolver defines schemas as SYM_TYPE; schema_decl has parent field |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **Language**: Must remain C (C11) -- bootstrap compiler
- **No new dependencies**: Pure C with standard library only
- **Spec compliance**: `docs/specv0_1.md` is source of truth
- **Memory**: Arena allocator pattern -- no heap allocation in compiler logic
- **Backwards compatible**: Existing 32 passing tests must continue to pass
- **Build system**: `make` with `gcc -Isrc -Wall -Wextra -g`
- **Code style**: 2-space indentation, K&R braces, snake_case functions, PascalCase types
- **Error reporting**: `typechecker_error(tc, line, col, "format", ...)`
- **Type comparison**: `type_equals()`, `type_is_assignable()`, `type_is_comparable()`
- **Poison passthrough**: `type_is_resolved()` guard before any validation
- **GSD workflow**: All changes through GSD commands

## Architecture Patterns

### Current typecheck.c Structure
```
typechecker_init()                    -- initialize
typechecker_error()                   -- error reporting
type_is_resolved()                    -- poison guard
is_realm_nesting_legal()              -- realm matrix
typechecker_check_pattern()           -- match pattern checking
typechecker_resolve_type_expr()       -- AST type expr -> Type*
typechecker_collect_decls()           -- forward declaration collection
typechecker_infer_expr()              -- expression type inference (main switch)
typechecker_check_node()              -- statement/declaration checking (main switch)
should_have_resolved_type()           -- ICE whitelist
check_unresolved_types()              -- post-check ICE walk
typechecker_check()                   -- entry point
```

### Pattern: Contextual Typing for Literals
The current approach in `typechecker_infer_expr` infers literals as their default type (i32/f64). The VAR_DECL handler in `typechecker_check_node` then validates the inferred type against the declared type. This needs refinement:

**Recommended approach:** Keep literal default inference (AST_INT_LITERAL -> i32, AST_FLOAT_LITERAL -> f64) in `typechecker_infer_expr`. In `typechecker_check_node` for AST_VAR_DECL, when the init is a literal and declared type is a compatible numeric, check the actual value against the target type's range. If within range, accept (contextual adoption). If out of range, error.

For unary negation fusion (D-09): when the init is AST_UNARY_EXPR with TOKEN_MINUS wrapping AST_INT_LITERAL, extract the negated value and check against the signed target type's range (e.g., -128 to 127 for i8).

### Pattern: Missing Field Detection for Structs
When handling struct construction in AST_CALL_EXPR where callee type is TY_STRUCT:
1. Track which fields were provided (bitmap or bool array, arena-allocated)
2. After processing all named args, iterate struct field_names to find unprovided fields
3. Check if unprovided fields have defaults (currently AST_FIELD_DECL has `default_val` field)
4. Error on missing fields without defaults, naming each one

### Pattern: All-Paths Return Analysis
**Recommended: recursive descent approach** (simpler than building a CFG for this compiler stage).

Add a helper `bool all_paths_return(AstNode *node)` that returns true if every execution path through the node ends in a return:
- AST_RETURN_STMT: true
- AST_BLOCK: true if last statement returns, or any statement is a return (simplified) -- actually, true if the block's statement list has all_paths_return on the last statement
- AST_IF_STMT: true only if both then_branch AND else_branch exist AND both all_paths_return
- AST_MATCH_STMT: true if every arm all_paths_returns (and match is exhaustive -- but that is Phase 3)
- Everything else: false

Call this after checking a function body for non-void functions.

### Pattern: Const Reassignment Check
In the AST_ASSIGN handler of `typechecker_infer_expr`:
1. Check if the target is AST_IDENTIFIER
2. Look up the symbol
3. If symbol's node is AST_VAR_DECL with `is_const == true`, emit error per D-14

### Anti-Patterns to Avoid
- **Over-permissive type_is_assignable:** The current function allows i32 (literal default type) to be assigned to ANY integer type without range checking. This must be tightened -- but carefully, since Phase 1's literal coercion in binary expressions depends on this. Solution: add an `is_literal` parameter or check the AST node kind at the call site.
- **Modifying type_is_assignable globally without testing:** The permissive behavior is load-bearing for 27 passing tests. Changes must be scoped to assignment/initialization contexts where the literal's actual value is accessible.
- **Ignoring poison type:** Every new check must have `type_is_resolved()` guard. Missing this causes cascading errors.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Integer range bounds | Custom calculation per type | Lookup table with min/max constants | Error-prone to compute signed min values correctly; table is maintainable |
| Type classification (is_signed, is_integer, is_float) | Repeated strcmp chains | Helper functions like `type_is_integer()`, `type_is_signed()` | Currently scattered strcmp calls for type classification; centralize in types.c |
| Widening direction suggestion | Custom logic per pair | Ordered rank table for numeric types | D-04 requires suggesting correct cast direction; a rank table makes this mechanical |

## Common Pitfalls

### Pitfall 1: Breaking Existing Literal Coercion
**What goes wrong:** Tightening `type_is_assignable` to reject i32-to-other-int causes 10+ test failures because existing test files use untyped integer literals with typed variables.
**Why it happens:** The current `type_is_assignable` in types.c (line 188-209) is intentionally permissive for literals, but it cannot distinguish "this i32 is actually a literal" from "this i32 is a typed variable."
**How to avoid:** Do NOT tighten `type_is_assignable` for the general case. Instead, perform range checking at the assignment/declaration site in `typechecker_check_node` where the AST node is accessible. Keep `type_is_assignable` as-is for now, or add a separate `type_is_literal_assignable(Type *target, AstNode *literal_node)` helper.
**Warning signs:** Multiple existing tests suddenly failing after changes to types.c.

### Pitfall 2: Negative Literal Range Check Off-By-One
**What goes wrong:** `i8 x = -128` is rejected because the range check sees `val = 128` (unsigned) which is > 127.
**Why it happens:** AST stores int_literal.value as `unsigned long long`. The negation is a separate AST_UNARY_EXPR node. Range checking only sees the unsigned value 128 without considering the wrapping negation.
**How to avoid:** When checking VAR_DECL init, detect the pattern: init is AST_UNARY_EXPR with op==TOKEN_MINUS and inner is AST_INT_LITERAL. In that case, compute the negated value as a signed check: for signed types, max positive range is `(1 << (bits-1)) - 1` and max negative magnitude is `(1 << (bits-1))`. So for i8: positive max 127, negative max magnitude 128.
**Warning signs:** Test `i8 x = -128` fails but `i8 x = -127` passes.

### Pitfall 3: Struct Missing-Field Check Ignoring Defaults
**What goes wrong:** `Vec2 v = Vec2(x: 1.0)` errors about missing `y` even though `y` has a default value `0.0`.
**Why it happens:** Not consulting `field_decl.default_val` when determining whether a field is required.
**How to avoid:** During missing-field analysis, a field is required only if its corresponding AST_FIELD_DECL has `default_val == NULL`. The field_decl nodes are reachable from the type's declaration node (via symbol lookup).
**Warning signs:** Constructor calls with partial fields all error instead of using defaults.

### Pitfall 4: Schema Type Not Registered in TypeContext
**What goes wrong:** Schema types resolve to TY_UNKNOWN because `typechecker_collect_decls` does not handle AST_SCHEMA_DECL.
**Why it happens:** The resolver registers schemas as SYM_TYPE with SYM_SCHEMA kind, but the type checker's `typechecker_collect_decls` only handles AST_FUNC_DECL, AST_VAR_DECL, AST_TYPE_DECL, AST_VARIANT_DECL, and AST_METHOD_DECL.
**How to avoid:** Add AST_SCHEMA_DECL handling in `typechecker_collect_decls` similar to AST_TYPE_DECL, but also walk the parent chain to merge inherited fields.
**Warning signs:** Any access to schema fields produces "Field not found" errors.

### Pitfall 5: Return Path Analysis False Positives
**What goes wrong:** Functions using named return variables (common Runes pattern: `f foo() = r: i32 { r = 42 }`) are flagged as missing a return.
**Why it happens:** The Runes spec uses named return variables that are implicitly returned at function end. This is NOT the same as requiring an explicit `return` statement.
**How to avoid:** A function with a named return variable (`ret_name != NULL`) implicitly returns -- all-paths analysis should only apply to functions that use explicit `return` statements without named returns, OR simply check that the named return variable is assigned on all paths (simpler: just skip the all-paths check when `ret_name` is set).
**Warning signs:** Most existing test functions use named returns and start failing.

## Code Examples

### Numeric Range Lookup Table
```c
// In types.c or typecheck.c
typedef struct {
  const char *name;
  bool is_signed;
  bool is_float;
  int bit_width;
  long long min_val;         // signed min (e.g., -128 for i8)
  unsigned long long max_val; // unsigned max (e.g., 255 for u8)
  int rank;                  // for widening direction suggestion
} NumericTypeInfo;

static const NumericTypeInfo numeric_types[] = {
  {"i8",    true,  false, 8,  -128LL,                127ULL,                1},
  {"i16",   true,  false, 16, -32768LL,              32767ULL,              2},
  {"i32",   true,  false, 32, -2147483648LL,         2147483647ULL,         3},
  {"i64",   true,  false, 64, (-9223372036854775807LL - 1), 9223372036854775807ULL, 4},
  {"u8",    false, false, 8,  0,                     255ULL,                1},
  {"u16",   false, false, 16, 0,                     65535ULL,              2},
  {"u32",   false, false, 32, 0,                     4294967295ULL,         3},
  {"u64",   false, false, 64, 0,                     18446744073709551615ULL, 4},
  {"f32",   true,  true,  32, 0,                     0,                     1},
  {"f64",   true,  true,  64, 0,                     0,                     2},
  {"usize", false, false, 64, 0,                     18446744073709551615ULL, 4},
};

static const NumericTypeInfo *get_numeric_info(const char *name) {
  for (int i = 0; i < (int)(sizeof(numeric_types)/sizeof(numeric_types[0])); i++) {
    if (strcmp(numeric_types[i].name, name) == 0)
      return &numeric_types[i];
  }
  return NULL;
}
```

### Negative Literal Fusion in VAR_DECL
```c
// Inside typechecker_check_node, AST_VAR_DECL case, after type_is_assignable check:
// Check for negative literal: AST_UNARY_EXPR(MINUS, AST_INT_LITERAL)
AstNode *init = node->as.var_decl.init;
bool is_negated_literal = false;
unsigned long long lit_val = 0;

if (init->kind == AST_INT_LITERAL) {
  lit_val = init->as.int_literal.value;
} else if (init->kind == AST_UNARY_EXPR &&
           init->as.unary.op == TOKEN_MINUS &&
           init->as.unary.expr->kind == AST_INT_LITERAL) {
  lit_val = init->as.unary.expr->as.int_literal.value;
  is_negated_literal = true;
}

if (lit_val > 0 || init->kind == AST_INT_LITERAL) {
  const NumericTypeInfo *info = get_numeric_info(decl_t->as.primitive.name);
  if (info && !info->is_float) {
    bool overflow = false;
    if (is_negated_literal) {
      // For signed types: magnitude must be <= 2^(bits-1)
      if (info->is_signed) {
        unsigned long long max_neg = 1ULL << (info->bit_width - 1);
        overflow = (lit_val > max_neg);
      } else {
        overflow = true; // negative value in unsigned type
      }
    } else {
      overflow = (lit_val > info->max_val);
    }
    if (overflow) {
      typechecker_error(tc, init->line, init->col,
                        "Integer literal overflow for type '%s'",
                        info->name);
    }
  }
}
```

### Struct Missing Field Check
```c
// Inside typechecker_infer_expr, AST_CALL_EXPR, TY_STRUCT branch:
// After processing all named args...
bool *field_provided = arena_alloc(tc->arena,
    sizeof(bool) * callee_t->as.struct_t.field_count);
memset(field_provided, 0, sizeof(bool) * callee_t->as.struct_t.field_count);

// Mark provided fields during named arg loop
for (int i = 0; i < callee_t->as.struct_t.field_count; i++) {
  if (strcmp(callee_t->as.struct_t.field_names[i], name) == 0) {
    field_provided[i] = true;
    // ... existing type check ...
  }
}

// After all args processed, check for missing required fields
// Need to look up the original AST_TYPE_DECL to check for defaults
Symbol *type_sym = symbol_table_lookup(tc->st, callee_t->as.struct_t.name);
AstNode *field_decl = type_sym ? type_sym->node->as.type_decl.fields : NULL;
for (int i = 0; i < callee_t->as.struct_t.field_count; i++) {
  if (!field_provided[i]) {
    // Check if field has a default value
    bool has_default = false;
    if (field_decl && field_decl->as.field_decl.default_val) {
      has_default = true;
    }
    if (!has_default) {
      typechecker_error(tc, expr->line, expr->col,
                        "Struct '%s' missing required field: '%s'",
                        callee_t->as.struct_t.name,
                        callee_t->as.struct_t.field_names[i]);
    }
  }
  if (field_decl) field_decl = field_decl->next;
}
```

### Const Reassignment Check
```c
// Inside typechecker_infer_expr, AST_ASSIGN case:
if (expr->as.assign.target->kind == AST_IDENTIFIER) {
  Symbol *sym = symbol_table_lookup(tc->st, expr->as.assign.target->as.identifier.name);
  if (sym && sym->node && sym->node->kind == AST_VAR_DECL &&
      sym->node->as.var_decl.is_const) {
    typechecker_error(tc, expr->line, expr->col,
                      "Cannot reassign constant '%s'",
                      expr->as.assign.target->as.identifier.name);
  }
}
```

### All-Paths Return Analysis
```c
static bool all_paths_return(AstNode *node) {
  if (!node) return false;
  switch (node->kind) {
  case AST_RETURN_STMT:
    return true;
  case AST_BLOCK: {
    // Check if the last statement in the block returns on all paths
    AstNode *stmt = node->as.block.statements;
    AstNode *last = NULL;
    while (stmt) { last = stmt; stmt = stmt->next; }
    return all_paths_return(last);
  }
  case AST_IF_STMT:
    return node->as.if_stmt.else_branch &&
           all_paths_return(node->as.if_stmt.then_branch) &&
           all_paths_return(node->as.if_stmt.else_branch);
  default:
    return false;
  }
}
```

### Schema Inheritance Walking
```c
// In typechecker_collect_decls, add AST_SCHEMA_DECL handler:
// 1. Collect own fields
// 2. If parent exists, look up parent schema and prepend its fields
// 3. Register as TY_STRUCT (schemas are struct-like for type checking)

// Field access on schema: when looking up a field in a struct type,
// if the struct was built from a schema with inheritance, all parent
// fields are already merged into the field list.
```

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bash + expected-failure convention |
| Config file | `src/tests/tester.bash` |
| Quick run command | `bash src/tests/tester.bash` |
| Full suite command | `bash src/tests/tester.bash` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TYPE-01 | Mixed-width integer ops error with cast suggestion | expected-failure | `bash src/tests/tester.bash` | Wave 0: create `type_int_promotion.runes` |
| TYPE-02 | Mixed-float ops error with cast suggestion | expected-failure | `bash src/tests/tester.bash` | Wave 0: create `type_float_promotion.runes` |
| TYPE-03 | Literal contextual typing (u8 x = 255 OK, u8 x = 256 error) | expected-failure + positive | `bash src/tests/tester.bash` | Wave 0: create `type_literal_inference.runes` |
| TYPE-04 | Negative literal (i8 x = -128 OK) | positive | `bash src/tests/tester.bash` | Wave 0: create `type_negative_literals.runes` |
| TYPE-05 | Struct missing/extra field errors | expected-failure | `bash src/tests/tester.bash` | Wave 0: create `type_struct_fields.runes` |
| TYPE-06 | Variant payload type mismatch error | expected-failure | `bash src/tests/tester.bash` | Wave 0: create `type_variant_payload.runes` |
| TYPE-07 | All-paths return type verification | expected-failure | `bash src/tests/tester.bash` | Wave 0: create `type_return_paths.runes` |
| TYPE-08 | Const reassignment error | expected-failure | `bash src/tests/tester.bash` | Wave 0: create `type_const_assign.runes` |
| TYPE-09 | Boolean strictness in if/while | expected-failure | `bash src/tests/tester.bash` | Already partially tested via existing tests |
| TYPE-10 | Tuple construction/destructuring | positive | `bash src/tests/tester.bash` | Wave 0: create `type_tuples.runes` |
| TYPE-11 | Schema inheritance field access | positive | `bash src/tests/tester.bash` | Wave 0: create `type_schema_inherit.runes` |

### Sampling Rate
- **Per task commit:** `make && bash src/tests/tester.bash`
- **Per wave merge:** `make && bash src/tests/tester.bash` (full suite)
- **Phase gate:** Full suite green + all new expected-failure tests passing

### Wave 0 Gaps
- [ ] `src/tests/samples/type_int_promotion.runes` -- covers TYPE-01, TYPE-02
- [ ] `src/tests/samples/type_literal_inference.runes` -- covers TYPE-03, TYPE-04
- [ ] `src/tests/samples/type_struct_fields.runes` -- covers TYPE-05
- [ ] `src/tests/samples/type_variant_payload.runes` -- covers TYPE-06
- [ ] `src/tests/samples/type_return_paths.runes` -- covers TYPE-07
- [ ] `src/tests/samples/type_const_assign.runes` -- covers TYPE-08
- [ ] `src/tests/samples/type_bool_strict.runes` -- covers TYPE-09
- [ ] `src/tests/samples/type_tuples.runes` -- covers TYPE-10
- [ ] `src/tests/samples/type_schema_inherit.runes` -- covers TYPE-11

## Implementation Order Recommendation

Based on dependency analysis and risk assessment:

**Wave 1: Numeric Foundation (TYPE-01, TYPE-02, TYPE-03, TYPE-04)**
- Add numeric type info lookup table in types.c
- Add type classification helpers (`type_is_integer`, `type_is_signed`, `type_is_float`)
- Enhance binary expression error messages with cast direction suggestion (D-04)
- Implement proper literal range checking in VAR_DECL with negative literal fusion (D-06, D-09)
- Tighten `type_is_assignable` to be range-aware for literal contexts
- These form the foundation; other requirements don't depend on them but they are the highest-risk changes to existing code

**Wave 2: Composite Types and Control Flow (TYPE-05, TYPE-06, TYPE-08, TYPE-09, TYPE-10)**
- Struct missing field detection and validation (D-10, D-11)
- Variant constructor payload validation (D-12)
- Const reassignment check in AST_ASSIGN (D-14)
- Boolean strictness verification tests (already implemented, needs test coverage)
- Tuple expression type inference in typechecker_infer_expr
- Cast expression (AST_CAST_EXPR) handler needed for `as` operator referenced in error messages

**Wave 3: Return Paths and Schema (TYPE-07, TYPE-11)**
- All-paths return analysis (D-15) -- higher risk of false positives
- Schema inheritance in typechecker_collect_decls
- ICE whitelist expansion for newly handled expression kinds

## Key Files to Modify

| File | Changes | Risk |
|------|---------|------|
| `src/typecheck.c` | Main implementation: literal checking, struct/variant validation, const check, return paths, tuple inference, schema support | HIGH -- 1632 lines, central to all tests |
| `src/types.c` | Numeric type info table, type classification helpers, potential `type_is_assignable` refinement | MEDIUM -- load-bearing for existing tests |
| `src/types.h` | Function declarations for new helpers | LOW |
| `src/tests/samples/*.runes` | New test files for each requirement | LOW |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Permissive i32-to-any assignment | Contextual literal typing with range check | This phase | Literals fit target or error; typed vars stay strict |
| No struct field validation | Missing/extra field errors | This phase | Constructor correctness enforced |
| No const enforcement | Const reassignment rejected | This phase | Language semantics match spec |

## Open Questions

1. **Cast expression handler scope**
   - What we know: D-04 tells users to use `as` cast, but AST_CAST_EXPR has no handler in typechecker_infer_expr
   - What's unclear: How much cast validation is needed this phase vs Phase 4+
   - Recommendation: Add basic cast handler that resolves the target type and returns it; full validation (e.g., checking cast legality) can be deferred

2. **type_is_assignable backward compatibility**
   - What we know: 27+ tests rely on current permissive behavior
   - What's unclear: Whether it's safe to add range checking inside type_is_assignable or if it must stay at the AST level
   - Recommendation: Keep type_is_assignable permissive for now; do range checking at the VAR_DECL/ASSIGN node level where the AST literal node is accessible. This preserves backward compatibility.

3. **Named return variable semantics for all-paths check**
   - What we know: Runes functions commonly use `f foo() = r: i32 { r = 42 }` pattern where `r` is implicitly returned
   - What's unclear: Whether all-paths check should verify that `r` is assigned on all paths, or just skip the check for named-return functions
   - Recommendation: Skip all-paths return check for functions with named return variables. Only enforce on functions that use explicit `return` statements. This matches spec behavior and avoids false positives.

## Sources

### Primary (HIGH confidence)
- `src/typecheck.c` -- full 1632-line read, all handlers analyzed
- `src/types.c` -- full 271-line read, type_is_assignable behavior confirmed
- `src/types.h` -- TypeContext with all primitive singletons confirmed
- `src/ast.h` -- node structures, is_const field, schema_decl.parent field confirmed
- `src/symbol_table.h` -- Symbol structure, no is_const field (must use node)
- `docs/specv0_1.md` -- sections 3, 6, 7, 13 on types, control flow, structs/variants, schemas

### Secondary (MEDIUM confidence)
- `type_checker_guide1.md` -- implementation roadmap (partially outdated, written before Phase 1)
- `.planning/phases/01-foundation-fixes/01-03-SUMMARY.md` -- Phase 1 literal coercion details
- `.planning/codebase/CONCERNS.md` -- known gaps documentation

### Tertiary (LOW confidence)
- None -- all findings verified against source code

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- pure C, no external libraries, all code inspected
- Architecture: HIGH -- typecheck.c fully read, modification points identified precisely with line numbers
- Pitfalls: HIGH -- based on actual code analysis and Phase 1 experience with literal coercion regressions

**Research date:** 2026-04-02
**Valid until:** 2026-05-02 (stable -- C compiler with no external dependencies)
