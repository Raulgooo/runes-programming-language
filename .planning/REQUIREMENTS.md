# Requirements: Runes Compiler Frontend Completion

**Defined:** 2026-04-02
**Core Value:** Every valid Runes program (per spec v0.1, excluding deprecated features) passes through the full frontend pipeline without false positives or missed errors, with comprehensive test coverage proving it.

## v1 Requirements

### Foundation

- [x] **FOUND-01**: TY_UNKNOWN/TY_ERROR split — failed type inference produces TY_ERROR (suppresses cascading), TY_UNKNOWN reaching validation is an internal compiler error
- [x] **FOUND-02**: Static Symbol bug in resolver fixed — module path resolution uses arena-allocated symbols instead of static variable
- [x] **FOUND-03**: Variant arm payload types fully resolved — every variant constructor has a concrete payload type, not TY_UNKNOWN
- [x] **FOUND-04**: Binary expressions infer correct result type via type promotion, not always left operand
- [x] **FOUND-05**: All existing compiler warnings fixed (sign comparison, unused variables)
- [x] **FOUND-06**: All 3 genuine test failures fixed (float_range_tests, test_type_safety false positives, 08_schema_json deprecated gracefully)

### Type System

- [ ] **TYPE-01**: Integer type promotion — i8->i16->i32->i64, u8->u16->u32->u64 widening in binary ops and assignments; mixed-sign requires explicit cast
- [ ] **TYPE-02**: Float type promotion — f32->f64 in mixed float expressions
- [ ] **TYPE-03**: Literal type inference — integer literals contextually typed (u8 x = 255 works), uncontextualized default to i32, float to f64
- [ ] **TYPE-04**: Negative literal overflow handled correctly — -128 accepted as i8, unary negation context respected
- [ ] **TYPE-05**: Struct field type checking complete — default values, missing field errors, field suggestions in diagnostics
- [ ] **TYPE-06**: Variant constructor type checking — payload types verified against variant arm definition
- [ ] **TYPE-07**: Return type verified on all code paths — named return variable properly typed and assigned
- [ ] **TYPE-08**: Const correctness enforced — const bindings cannot be reassigned
- [ ] **TYPE-09**: Boolean strictness — conditions in if/while must be bool, no implicit truthiness
- [ ] **TYPE-10**: Tuple type checking complete — construction, destructuring, field access, return position
- [ ] **TYPE-11**: Schema inheritance chain walked — derived schema fields accessible, inheritance validated (non-JSON aspects only)

### Pattern Matching

- [ ] **MATCH-01**: Match exhaustiveness for variants — missing arms detected and named in error message
- [ ] **MATCH-02**: Match exhaustiveness for booleans — true/false coverage checked
- [ ] **MATCH-03**: Integer/string match requires wildcard arm — compiler errors without it
- [ ] **MATCH-04**: Pattern binding type inference — variant and struct destructuring creates correctly-typed scoped bindings
- [ ] **MATCH-05**: Match-as-expression type consistency — all arms must return same type
- [ ] **MATCH-06**: Guarded arms do not count toward exhaustiveness coverage
- [ ] **MATCH-07**: Duplicate arm detection — warn on identical patterns

### Interface System

- [ ] **IFACE-01**: Interface satisfaction checking — method Drawable for Vec2 verifies all required methods with correct signatures
- [ ] **IFACE-02**: Interface-typed parameters — function accepting interface type validates argument satisfies it
- [ ] **IFACE-03**: Method block resolution — methods on types are discoverable for dispatch and interface checking

### Module System

- [ ] **MOD-01**: Cross-file module resolution — use kernel.arch.x86 finds and loads symbols from other files
- [ ] **MOD-02**: Pub/private visibility enforcement — non-pub symbols inaccessible from outside their module
- [ ] **MOD-03**: Use-statement aliasing — use kernel.alloc_page brings alloc_page into scope correctly
- [ ] **MOD-04**: Cyclic dependency detection — circular module imports produce clear error message
- [ ] **MOD-05**: Module scope persistence — module symbol scopes survive scope stack pop for cross-module lookups

### Realm Checker

- [ ] **REALM-01**: Scope-exit escape detection — pointer values cannot leave their realm without promote() or being Copy type
- [ ] **REALM-02**: Promote validation — promote(&t) as dynamic/gc checked against full spec matrix
- [ ] **REALM-03**: Copy-type auto-escape — primitive types and value structs can escape scope without promote
- [ ] **REALM-04**: Nesting matrix as 6x6 lookup table — replaces current if-chain for maintainability

### Testing

- [ ] **TEST-01**: Positive test suite — at least one test file per major spec v0.1 construct through full pipeline
- [ ] **TEST-02**: Negative test suite — test files for every category of invalid program (type mismatch, realm violation, missing match arm, visibility error, undefined name, cyclic import)
- [ ] **TEST-03**: Regression tests — every bug fixed during this project gets a dedicated test
- [ ] **TEST-04**: Enhanced tester.bash — supports expected-failure tests with stderr matching
- [ ] **TEST-05**: Binary/octal integer literal support in lexer — 0b1010, 0o777

## v2 Requirements

### Advanced Type Features

- **ADV-01**: Generics / type parameters (<T>) — parametric polymorphism
- **ADV-02**: Pipes (pipe ... = expr | expr) — pipeline type threading
- **ADV-03**: List types (sl/dl) — singly/doubly linked list types
- **ADV-04**: Flex function full re-checking — re-check body under each caller's realm context

### Developer Experience

- **DX-01**: Unused variable/import warnings
- **DX-02**: Realm-aware error suggestions (suggests promote() call)
- **DX-03**: Dead code detection after fully-returning match

## Out of Scope

| Feature | Reason |
|---------|--------|
| Generics (<T>) | Massive complexity, deferred to v2+ |
| JSON serialization / as J / schema JSON features | Deprecated -- spec section 13 will not be implemented |
| Pipes (pipe syntax) | Deferred to v2+ |
| List types (sl/dl) | Depends on generics |
| Code generation backend | Separate project after frontend |
| Runtime / standard library | Depends on codegen |
| Language server / IDE tooling | Future work |
| Incremental compilation | Not needed for bootstrap compiler |
| Implicit type conversions beyond numeric promotion | Explicit casts only, prevents surprise behavior |
| Async/await | v0.2 spec feature |
| Flex function realm checking | Depends on generics, deferred to v2+ |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FOUND-01 | Phase 1 | Complete |
| FOUND-02 | Phase 1 | Complete |
| FOUND-03 | Phase 1 | Complete |
| FOUND-04 | Phase 1 | Complete |
| FOUND-05 | Phase 1 | Complete |
| FOUND-06 | Phase 1 | Complete |
| TYPE-01 | Phase 2 | Pending |
| TYPE-02 | Phase 2 | Pending |
| TYPE-03 | Phase 2 | Pending |
| TYPE-04 | Phase 2 | Pending |
| TYPE-05 | Phase 2 | Pending |
| TYPE-06 | Phase 2 | Pending |
| TYPE-07 | Phase 2 | Pending |
| TYPE-08 | Phase 2 | Pending |
| TYPE-09 | Phase 2 | Pending |
| TYPE-10 | Phase 2 | Pending |
| TYPE-11 | Phase 2 | Pending |
| MATCH-01 | Phase 3 | Pending |
| MATCH-02 | Phase 3 | Pending |
| MATCH-03 | Phase 3 | Pending |
| MATCH-04 | Phase 3 | Pending |
| MATCH-05 | Phase 3 | Pending |
| MATCH-06 | Phase 3 | Pending |
| MATCH-07 | Phase 3 | Pending |
| IFACE-01 | Phase 4 | Pending |
| IFACE-02 | Phase 4 | Pending |
| IFACE-03 | Phase 4 | Pending |
| MOD-01 | Phase 5 | Pending |
| MOD-02 | Phase 5 | Pending |
| MOD-03 | Phase 5 | Pending |
| MOD-04 | Phase 5 | Pending |
| MOD-05 | Phase 5 | Pending |
| REALM-01 | Phase 6 | Pending |
| REALM-02 | Phase 6 | Pending |
| REALM-03 | Phase 6 | Pending |
| REALM-04 | Phase 6 | Pending |
| TEST-01 | Phase 7 | Pending |
| TEST-02 | Phase 7 | Pending |
| TEST-03 | Phase 7 | Pending |
| TEST-04 | Phase 7 | Pending |
| TEST-05 | Phase 7 | Pending |

**Coverage:**
- v1 requirements: 37 total
- Mapped to phases: 37
- Unmapped: 0

---
*Requirements defined: 2026-04-02*
*Last updated: 2026-04-02 after roadmap creation*
