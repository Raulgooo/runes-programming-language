# Runes Compiler — Frontend Completion

## What This Is

The Runes bootstrap compiler, written in C, implementing a systems-level language with explicit memory realm strategies (stack, regional, dynamic, gc, flex). The compiler currently handles lexing, parsing, name resolution, and partial type checking. This project completes the entire frontend pipeline to full spec v0.1 compliance.

## Core Value

Every valid Runes program (per spec v0.1, excluding deprecated features) passes through lex → parse → resolve → typecheck → realm-check without false positives or missed errors, with comprehensive test coverage proving it.

## Requirements

### Validated

- ✓ Lexer tokenizes all v0.1 keywords, operators, and literal forms — existing
- ✓ Parser handles variable declarations, functions with memory strategies, structs, variants, schemas, interfaces, error sets, match, for-with-captures, try/catch, attributes, modules, use, extern, unsafe, asm — existing
- ✓ Resolver performs two-pass name resolution with scoped symbol table — existing
- ✓ Type checker infers expressions, checks assignments, validates fallible types, and enforces memory realm nesting matrix — existing
- ✓ Arena allocator and string interning infrastructure — existing
- ✓ AST with 53+ node kinds and intrusive linked lists — existing
- ✓ Standard library prelude parsed for builtin definitions — existing

### Active

- [ ] Fix all existing test failures (08_schema_json parse errors, float_range_tests, test_type_safety false positives)
- [ ] Complete resolver: cross-file module resolution, visibility enforcement (pub/private), cyclic dependency detection, proper use-aliasing
- [ ] Complete type checker: type promotion rules (i32→i64, etc.), interface satisfaction checking, full struct field lookup in patterns, variant arm resolution
- [ ] Complete type checker: exhaustiveness checking for match expressions
- [ ] Implement realm checker as proper validation pass — lifetime/scope-exit enforcement, copy-type auto-escape validation, region cleanup guarantees
- [ ] Add missing parser support: binary/octal integer literals
- [ ] Fix compiler warnings (sign comparison, unused variables)
- [ ] New test files exercising every spec v0.1 construct through the full pipeline
- [ ] Negative test suite — programs that SHOULD fail, verifying correct error messages

### Out of Scope

- Generics / type parameters (`<T>`) — deferred to future version
- Pipes (`pipe ... = expr | expr`) — deferred to future version
- List types (sl/dl) — deferred to future version
- JSON serialization / `as J` / schema JSON features (spec §13) — deprecated, will not be implemented
- Code generation backend — separate project after frontend completion
- Flex function realm checking — depends on generics, deferred to v2+
- Runtime / standard library implementation — depends on codegen
- Language server / IDE tooling — future work

## Context

- **Language**: C (ISO C11), compiled with gcc
- **Spec**: `docs/specv0_1.md` — the authoritative language reference
- **Type checker guide**: `type_checker_guide1.md` — implementation roadmap for type checking phases
- **Codebase map**: `.planning/codebase/` — 7 documents covering architecture, stack, conventions, etc.
- **Test suite**: `src/tests/tester.bash` runs all `.runes` samples through the compiler with prelude; currently 27/32 pass (3 genuine failures, 2 expected negative tests)
- **Memory model**: Arena allocator exclusively — no malloc in compiler logic, no manual freeing
- **Naming convention**: Module-prefixed functions (`lexer_*`, `parser_*`, `typechecker_*`, `resolver_*`), `ast_new_*` constructors, UPPERCASE constants

## Constraints

- **Language**: Must remain C — this is a bootstrap compiler
- **No new dependencies**: Pure C with standard library only, no LLVM or external libs
- **Spec compliance**: `docs/specv0_1.md` is the source of truth for language behavior
- **Memory**: Continue using arena allocator pattern — no heap allocation in compiler logic
- **Backwards compatible**: Existing passing tests must continue to pass

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Exclude generics from v1 | Narrow scope to ship a complete frontend faster | — Pending |
| Deprecate JSON/schema features | Simplify language, remove spec §13 entirely | — Pending |
| Realm checking stays in typecheck.c | Already partially implemented there; separate pass (realm_check.c) would duplicate work | — Pending |
| Exclude pipes and list types from v1 | Not essential for bootstrap compiler | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-02 after initialization*
