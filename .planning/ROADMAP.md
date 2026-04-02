# Roadmap: Runes Compiler Frontend Completion

## Overview

This roadmap completes the Runes bootstrap compiler frontend from its current 70%-complete state to full spec v0.1 compliance. The work proceeds bottom-up through the compiler's semantic layers: fix foundational bugs that corrupt downstream analysis, complete the type system, add interface satisfaction checking, harden the module system for multi-file compilation, implement realm escape analysis, and lock it all down with comprehensive positive and negative test suites. Each phase builds on a stable foundation from the previous one.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation Fixes** - Fix critical bugs that corrupt type inference, module lookups, variant resolution, and binary expression types
- [ ] **Phase 2: Type System Completion** - Correct numeric promotion, literal inference, struct/variant/tuple checking, const correctness, and boolean strictness
- [ ] **Phase 3: Pattern Matching** - Exhaustiveness checking, pattern binding inference, match-as-expression consistency, guard handling, duplicate detection
- [ ] **Phase 4: Interface System** - Interface satisfaction checking, interface-typed parameters, method block resolution
- [ ] **Phase 5: Module System** - Cross-file resolution, pub/private visibility, use-aliasing, cyclic dependency detection
- [ ] **Phase 6: Realm Checker** - Scope-exit escape detection, promote validation, copy-type auto-escape, nesting matrix
- [ ] **Phase 7: Test Suite and Quality** - Positive suite, negative suite, regression tests, enhanced test runner, binary/octal literals

## Phase Details

### Phase 1: Foundation Fixes
**Goal**: The compiler's type inference, symbol resolution, and expression typing produce correct results that all downstream phases can rely on
**Depends on**: Nothing (first phase)
**Requirements**: FOUND-01, FOUND-02, FOUND-03, FOUND-04, FOUND-05, FOUND-06
**Success Criteria** (what must be TRUE):
  1. A type inference failure produces TY_ERROR which suppresses cascading errors; TY_UNKNOWN reaching validation triggers an internal compiler error
  2. Module path resolution works correctly under nested/recursive lookups (no static variable corruption)
  3. Every variant constructor has a concrete payload type after resolution (no TY_UNKNOWN payloads)
  4. Binary expression `i32 + i64` infers i64 (not silently i32); type promotion determines result type
  5. All 32 existing tests pass (the 3 genuine failures fixed, 2 expected-failure tests handled correctly), zero compiler warnings
**Plans:** 3 plans

Plans:
- [x] 01-01-PLAN.md -- TY_INFER_ERROR poison type and typecheck.c guard migration
- [x] 01-02-PLAN.md -- Static Symbol fix, compiler warnings, debug guards, Makefile
- [x] 01-03-PLAN.md -- Variant payloads, binary expr strictness, test infrastructure and fixes

### Phase 2: Type System Completion
**Goal**: All numeric operations, literal assignments, struct/variant/tuple constructions, and control flow expressions type-check correctly per spec v0.1
**Depends on**: Phase 1
**Requirements**: TYPE-01, TYPE-02, TYPE-03, TYPE-04, TYPE-05, TYPE-06, TYPE-07, TYPE-08, TYPE-09, TYPE-10, TYPE-11
**Success Criteria** (what must be TRUE):
  1. Integer and float expressions widen correctly (i8+i16 yields i16, f32+f64 yields f64); mixed-sign integer ops produce a compile error requiring explicit cast
  2. Literal `u8 x = 255` compiles without cast; `u8 x = 256` produces an overflow error; `i8 x = -128` compiles correctly (unary negation context respected)
  3. Struct construction errors name the missing or extra fields; variant constructors reject wrong payload types; tuple access and destructuring are type-checked
  4. Const bindings reject reassignment; if/while conditions reject non-bool expressions; return types are verified on all code paths
  5. Schema inheritance chains are walked for field access (non-JSON aspects)
**Plans**: TBD

### Phase 3: Pattern Matching
**Goal**: Match expressions are fully validated -- exhaustiveness enforced, bindings correctly typed, arms consistent, guards and duplicates handled
**Depends on**: Phase 2
**Requirements**: MATCH-01, MATCH-02, MATCH-03, MATCH-04, MATCH-05, MATCH-06, MATCH-07
**Success Criteria** (what must be TRUE):
  1. A match on a variant missing one arm produces an error naming the missing arm(s); a match on bool missing true or false is caught; a match on integer/string without a wildcard arm is caught
  2. Variant and struct destructuring in match arms creates correctly-typed scoped bindings accessible in the arm body
  3. A match used as an expression rejects arms returning different types
  4. Guarded arms (`if condition`) do not count toward exhaustiveness coverage; duplicate patterns produce a warning
**Plans**: TBD

### Phase 4: Interface System
**Goal**: Interface satisfaction is verified at definition and use sites -- types implementing an interface have all required methods with correct signatures
**Depends on**: Phase 3
**Requirements**: IFACE-01, IFACE-02, IFACE-03
**Success Criteria** (what must be TRUE):
  1. `method Drawable for Vec2` that is missing a required method or has a wrong signature produces a clear error naming the mismatch
  2. A function parameter typed as an interface rejects arguments whose type does not satisfy that interface
  3. Methods declared on types are discoverable for both dispatch and interface satisfaction checking
**Plans**: TBD

### Phase 5: Module System
**Goal**: The compiler resolves symbols across files with correct visibility, aliasing, and cycle detection
**Depends on**: Phase 4
**Requirements**: MOD-01, MOD-02, MOD-03, MOD-04, MOD-05
**Success Criteria** (what must be TRUE):
  1. `use kernel.arch.x86` successfully loads and resolves symbols from another file; symbols are callable/usable in the importing file
  2. Non-pub symbols are inaccessible from outside their module; accessing one produces a visibility error
  3. `use kernel.alloc_page` brings `alloc_page` into scope as a local name; aliased names resolve correctly in type checking
  4. Circular module imports (A uses B, B uses A) produce a clear error message instead of infinite recursion or crash
**Plans**: TBD

### Phase 6: Realm Checker
**Goal**: The compiler enforces memory realm safety -- values cannot escape their realm without explicit promotion or being copy types
**Depends on**: Phase 5
**Requirements**: REALM-01, REALM-02, REALM-03, REALM-04
**Success Criteria** (what must be TRUE):
  1. A stack-allocated pointer assigned to a dynamic-realm variable without promote() produces a realm escape error
  2. `promote(&t) as dynamic` is validated against the spec's full 6x6 nesting matrix; invalid promotions are rejected
  3. Primitive types and value structs (copy types) can be returned from inner scopes without promote -- no false positive errors
  4. Nesting matrix implemented as 6x6 lookup table replacing the current if-chain
**Plans**: TBD

### Phase 7: Test Suite and Quality
**Goal**: Every spec v0.1 construct has test coverage and every category of invalid program is verified to produce the correct error
**Depends on**: Phase 6
**Requirements**: TEST-01, TEST-02, TEST-03, TEST-04, TEST-05
**Success Criteria** (what must be TRUE):
  1. At least one positive test file exists for each major spec v0.1 construct (functions, structs, variants, schemas, interfaces, match, modules, realms, error types) and all pass the full pipeline
  2. Negative test files exist for every error category (type mismatch, realm violation, missing match arm, visibility error, undefined name, cyclic import) and the compiler rejects each with the expected error message
  3. Every bug fixed during this project has a dedicated regression test that would catch reintroduction
  4. `tester.bash` supports expected-failure tests with stderr pattern matching; binary (0b1010) and octal (0o777) integer literals lex correctly
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation Fixes | 0/3 | Planning complete | - |
| 2. Type System Completion | 0/? | Not started | - |
| 3. Pattern Matching | 0/? | Not started | - |
| 4. Interface System | 0/? | Not started | - |
| 5. Module System | 0/? | Not started | - |
| 6. Realm Checker | 0/? | Not started | - |
| 7. Test Suite and Quality | 0/? | Not started | - |
