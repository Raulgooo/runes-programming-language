# Project Research Summary

**Project:** Runes Bootstrap Compiler — Frontend Completion
**Domain:** Compiler frontend (type checker, resolver, realm checker) for a systems programming language written in pure C11
**Researched:** 2026-04-02
**Confidence:** HIGH (architecture and pitfalls from direct codebase analysis; stack and features from spec analysis plus established compiler engineering)

## Executive Summary

The Runes compiler frontend is a hand-written C11 compiler with no external dependencies that is approximately 70% complete. The lexer, parser, and resolver are largely working; the type checker handles basic cases (27/32 tests passing) but is missing critical correctness features: numeric type promotion, match exhaustiveness, interface satisfaction checking, and scope-exit realm validation. The recommended approach is a layered completion strategy — fix foundational bugs first, then layer in type system features, then the interface system, then multi-file module resolution, and finally harden the realm checker. This ordering respects the feature dependency graph and avoids building on broken foundations.

The single most important architectural decision already made is to keep realm checking integrated inside `typecheck.c` rather than extracting it to a separate pass. This is the correct decision because realm validation requires fully resolved types and is interleaved with type inference — a separate pass would require duplicating the entire AST traversal. The stack adds only two new source files: `module.c/h` for the module registry, with all other changes being additions to existing files. No new external dependencies are introduced.

The key risks are (1) the `TY_UNKNOWN` swallowing behavior where failed type inference silently passes all checks, (2) the static `Symbol tmp` bug in the resolver that will corrupt module lookups, and (3) exhaustiveness checking being built on top of incomplete variant payload types. All three must be fixed before implementing the features that depend on them. The realm system is Runes' most novel feature and has no external reference implementation — implementation-specific details around escape analysis and the promote matrix will require careful testing rather than mechanical translation from known sources.

## Key Findings

### Recommended Stack

The project is pure C11 with standard library only. No external dependencies will be added. The "stack" is entirely algorithmic: the recommended techniques for each missing feature are well-established compiler patterns adapted to this codebase's constraints (arena allocation, string interning, no generics in v0.1).

**Core techniques:**
- **Numeric rank table** (`types.c`): static integer rank per type for promotion rules — O(1) lookup, canonical approach from GCC/Clang/tcc. Replaces the current brittle string-comparison chain in `type_is_assignable`.
- **Structural interface satisfaction** (`typecheck.c` TC pass 3): check method set at use-site, not eagerly for all type pairs — matches Go's structural typing model, natural fit for Runes' no-`impl` design.
- **Set coverage exhaustiveness** (`typecheck.c`): bitset of variant arm coverage, wildcard as short-circuit — sufficient for v0.1 flat patterns; Maranget algorithm is deferred until nested patterns are needed.
- **Module registry** (new `module.c/h`): hash map of module name to symbol scope, two-pass loading (declarations then resolution), cycle detection via "currently loading" set — standard approach from Go, Rust, Python compilers.
- **Realm nesting matrix**: convert current if-chain to `[6][6]` lookup table for maintainability; scope-exit escape analysis tracks realm membership (not reference lifetimes — this is explicitly not Rust borrow checking).
- **Test infrastructure**: bash runner with positive (exit 0) and negative (exit 1 + stderr match) test suites. Infrastructure already partially exists in `tester.bash`.

### Expected Features

**Must have (table stakes) — compiler is incomplete without these:**
- Integer and float type promotion (widening rules; signed-to-unsigned and float-to-int require explicit cast)
- Literal type inference with contextual typing (`u8 x = 255` works; overflow detection)
- Match exhaustiveness for variants, booleans, and integer subjects
- Pattern binding type inference (destructuring variant and struct payloads into scoped bindings)
- Interface satisfaction checking (`method Drawable for Vec2` verifies all required methods with correct signatures)
- Cross-file module resolution with `use` aliasing and pub/private visibility enforcement
- Scope-exit realm validation (values cannot escape their realm without `promote()` or being Copy types)
- Negative test suite (compiler must reject invalid programs)

**Should have (differentiators worth building in v0.1):**
- Realm-aware error messages that name the missing `promote()` call
- Match exhaustiveness diagnostics that name the missing variant arm
- Struct field suggestions in error messages ("available fields: x, y")
- Unused variable warnings (low effort, high DX value)

**Defer to v2+:**
- Generics (`<T>`) — massive complexity, explicitly deferred; parser can accept syntax but type checker should error clearly
- JSON schemas / `as J` / schema inheritance — deprecated per PROJECT.md
- Pipes — deferred; type checker should error clearly
- List types (`sl`/`dl`) — depends on generics
- Flex function full re-checking — for v0.1, treat flex bodies as realm-neutral (no direct allocation)
- Incremental compilation, LSP, async/await

### Architecture Approach

The pipeline is four sequential passes (Lexer -> Parser -> Resolver -> TypeChecker) sharing Arena, SymbolTable, and TypeContext. The pipeline structure does not change for frontend completion. All additions are within existing component boundaries, with the exception of a new `module.c/h` for the module registry. The resolver gains module-scope persistence (via a `module_scope` field on `Symbol`), visibility enforcement, fixed use-aliasing, and cycle detection. The type checker gains type promotion, interface satisfaction (as a dedicated TC pass 3 after method bodies are checked), exhaustiveness checking, and scope-exit validation. `realm_check.c` stays empty — realm logic stays in `typecheck.c`.

**Major components and their new responsibilities:**

1. **`types.c`** — Add `type_is_promotable_to()`, `type_promote()`, `type_is_copy()`, `type_satisfies_interface()`. Type algebra stays in types.c, not typecheck.c.
2. **`resolver.c`** — Fix static Symbol bug, add module scope persistence, visibility enforcement at cross-module boundaries, proper use-aliasing, cyclic dependency detection.
3. **`typecheck.c`** — Type promotion in binary expressions and assignments, exhaustiveness checking in match, interface satisfaction as pass 3, scope-exit escape analysis as final validation.
4. **`module.c/h`** (new) — Module registry (hash map), file I/O for cross-file loading, two-pass loading pipeline if multi-file support is needed beyond inline `mod` blocks.
5. **`symbol_table.h`** — Add `module_scope` field to Symbol struct so module scopes persist after the scope stack is popped.

### Critical Pitfalls

1. **TY_UNKNOWN swallows errors** — The `if (kind != TY_UNKNOWN)` guard pattern in 15+ places means failed type inference silently passes all checks. Must split into `TY_UNKNOWN` (not yet inferred) and `TY_ERROR` (failed, already reported). `TY_ERROR` propagates silently to suppress cascades; `TY_UNKNOWN` reaching validation is an ICE. Fix this before any other type system work.

2. **Static Symbol bug in resolver** — `resolver.c:159` uses `static Symbol tmp` for module path resolution. Nested or recursive module lookups corrupt results because the static is shared across all call frames. Replace with arena allocation. Fix before module system work.

3. **Exhaustiveness depends on incomplete variant types** — Variant arm payloads are currently `TY_UNKNOWN`. Building exhaustiveness checking before completing variant type resolution produces wrong results. Complete variant payload resolution first.

4. **Binary expressions always return left type** — `typecheck.c:540` always returns `lty` for binary expressions. `i32 + i64` silently infers `i32`. Fix by implementing `type_promote()` and using it in binary expression inference.

5. **Negative literal overflow false positive** — The parser creates `-128` as unary minus applied to `128`. Range checking on the positive literal first will incorrectly reject `i8 x = -128`. Literal overflow checking must account for unary negation context.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Foundation Fixes
**Rationale:** Four critical bugs (TY_UNKNOWN/TY_ERROR split, static Symbol, variant type incompleteness, binary expression result type) corrupt the correctness of every feature built on top of them. Building on these bugs guarantees wasted rework. This is pre-work with no user-visible feature output, but it is the highest-leverage work in the project.
**Delivers:** A foundation where type inference failures are correctly flagged, module lookups are safe, variant payloads are resolved, and binary arithmetic infers the correct type.
**Addresses:** TY_UNKNOWN swallowing (PITFALL 1), static Symbol bug (PITFALL 2), variant type incompleteness (PITFALL 3), binary expression result type (PITFALL 4).
**Key tasks:** Split TY_UNKNOWN/TY_ERROR in types.h and all guards in typecheck.c; replace static Symbol in resolver.c; complete variant arm payload type resolution; implement `type_promote()` in types.c and apply it in binary expressions.

### Phase 2: Type System Completion
**Rationale:** Type promotion and literal type inference affect the majority of expressions. Exhaustiveness checking depends on complete variant types (fixed in Phase 1). These are self-contained additions to typecheck.c with high test coverage value.
**Delivers:** Correct type inference for all numeric operations, correct literal assignment without spurious casts, non-exhaustive match detection with named missing arms.
**Addresses:** Integer/float promotion (FEATURES table stakes), literal type inference, match exhaustiveness, pattern binding type inference, negative literal overflow (PITFALL 8), match guard coverage (PITFALL 11).
**Key tasks:** Implement `type_is_promotable_to()` with numeric rank table; implement contextual literal typing with range checking; implement exhaustiveness checker with wildcard short-circuit and per-subject-type rules; implement pattern binding destructuring for struct and variant payloads.
**Research flag:** Standard patterns — skip research-phase. Rank tables and set coverage exhaustiveness are well-documented.

### Phase 3: Interface System
**Rationale:** Interface satisfaction requires complete method signatures (available after Phase 2 type checking) and must happen as a distinct pass after all method bodies are checked, to avoid O(calls * methods) redundancy. This is architecturally self-contained.
**Delivers:** Verified `method Interface for Type` blocks — all interface methods present with correct signatures. Interface-typed parameters correctly validated at call sites.
**Addresses:** Interface satisfaction checking (FEATURES table stakes), interface-typed parameters, method block resolution.
**Avoids:** Checking satisfaction per-call-site (ARCHITECTURE anti-pattern 3); global implements table (STACK "what not to do").
**Research flag:** Standard patterns — structural typing is well-documented (Go model).

### Phase 4: Module System
**Rationale:** Module system is the most infrastructure-intensive feature (new module.c/h, SymbolTable changes, resolver overhaul). It must come after the type system is stable because visibility enforcement changes how symbols are resolved, and introducing it earlier would cause mass test failures. Cross-file loading requires correct type checking to be in place.
**Delivers:** Multi-file compilation with `use` aliasing, pub/private visibility enforcement at module boundaries, cyclic dependency detection with clear error messages.
**Addresses:** Cross-file module resolution (FEATURES high complexity), pub/private visibility (FEATURES), cyclic dependency detection (FEATURES), use-statement aliasing.
**Avoids:** Static Symbol bug must be fixed in Phase 1 before this can work; arena lifetime issue (single arena for full compilation, PITFALL 13); visibility leaking (PITFALL 7 — add incrementally, mark all pub first then enforce).
**Research flag:** Needs careful implementation — module scope persistence in SymbolTable is novel to this codebase. No external reference needed for the algorithm, but codebase-specific integration requires attention.

### Phase 5: Realm Checker Hardening
**Rationale:** Realm/lifetime analysis is the final semantic pass and depends on complete type information from all prior phases (requires `type_is_copy()`, correct pointer types, fully resolved types). It is Runes' most novel feature with no external reference implementation for the full escape analysis.
**Delivers:** Scope-exit validation (values cannot escape realm without promote or being Copy), promote() validation against the full spec matrix, Copy-type auto-escape, realm nesting matrix as lookup table.
**Addresses:** Scope escape detection (FEATURES high complexity), promote validation, Copy type auto-escape, flex function realm inheritance (conservatively: realm-neutral bodies only for v0.1).
**Avoids:** Separate realm_check.c pass (ARCHITECTURE anti-pattern 2); full Rust-style borrow checking (STACK "what not to do"); realm analysis running before types are resolved (PITFALL 5).
**Research flag:** Needs deeper design work during planning — escape analysis implementation details are codebase-specific. No external reference for Runes' exact realm model. Conservative approach for flex functions needs explicit spec validation.

### Phase 6: Test Suite and Quality
**Rationale:** Negative test suite validation and comprehensive positive coverage should be built alongside each phase (incrementally), but a dedicated hardening phase ensures full coverage before the frontend is declared complete. The existing 27/32 passing tests are the baseline; this phase drives to full coverage of all spec constructs.
**Delivers:** Complete positive test suite (all v0.1 spec constructs), negative test suite (all invalid program categories), regression tests for all fixed bugs, enhanced tester.bash with stderr matching.
**Addresses:** Negative test validation (FEATURES high priority), test suite regression prevention (PITFALL 16).
**Research flag:** Standard patterns — bash snapshot testing is well-documented. Skip research-phase.

### Phase Ordering Rationale

- **Phases 1 before everything** because correctness bugs in the foundation invalidate all downstream work.
- **Phase 2 before Phase 3** because interface satisfaction requires complete method type signatures, which depend on type promotion being correct.
- **Phase 3 before Phase 4** because interface checking only needs the existing SymbolTable; module system changes restructure the SymbolTable in ways that could disrupt a partially-built interface system.
- **Phase 4 before Phase 5** because realm escape analysis must respect module boundaries (a value escaping to a different module is an escape), and visibility enforcement must be in place before realm checking is authoritative.
- **Phase 5 last** because it is the most complex semantic validation and depends on all preceding type, interface, and module work being stable.
- **Tests built incrementally** across all phases but Phase 6 is a dedicated quality gate before frontend completion.

### Research Flags

Phases likely needing deeper design work during planning:
- **Phase 5 (Realm Checker):** Escape analysis implementation is codebase-specific with no external reference. The promote() validation matrix from the spec needs careful translation. Flex function conservative handling needs spec sign-off.

Phases with standard patterns (skip research-phase):
- **Phase 1 (Foundation Fixes):** All fixes are surgical corrections to known bugs with clear solutions.
- **Phase 2 (Type System):** Rank tables and set coverage exhaustiveness are textbook compiler patterns.
- **Phase 3 (Interface System):** Structural typing at use-site is well-documented (Go model).
- **Phase 6 (Test Suite):** Bash snapshot testing is standard.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All algorithmic techniques (rank tables, structural typing, set coverage) are canonical compiler patterns with multiple reference implementations. No web verification available but training data is authoritative for these topics. |
| Features | MEDIUM | Based on spec analysis and codebase audit. Feature list is accurate but complexity estimates may shift during implementation, especially for realm checker edge cases. |
| Architecture | HIGH | Based on direct codebase analysis of all .c and .h files. Component boundaries and anti-patterns identified from actual code, not inference. |
| Pitfalls | HIGH | Critical pitfalls identified from direct source code reading with line numbers. These are real bugs, not hypothetical risks. |

**Overall confidence:** HIGH

### Gaps to Address

- **Flex function realm semantics:** The conservative approach (flex bodies are realm-neutral) is simpler but may be too restrictive for real Runes programs. Needs validation against spec section 5 and any existing flex function tests during Phase 5 planning.
- **Schema inheritance chain:** PITFALL 14 notes that schemas can inherit from other schemas and the type checker must walk the chain. The spec's current deprecation of JSON schemas (per PROJECT.md) makes this lower priority, but needs confirmation on whether schema inheritance is fully deprecated or partially retained.
- **`!T` error type propagation completeness:** PITFALL 15 notes fallible types must propagate correctly. The current implementation handles basic cases but edge cases (nested fallibles, `catch` with default vs handler) need explicit test coverage.
- **Module search strategy for cross-file resolution:** The spec uses dot-notation paths (`use kernel.arch.x86`). The mapping to filesystem paths (e.g., `kernel/arch/x86.runes`) needs to be validated against any existing multi-file test programs.

## Sources

### Primary (HIGH confidence — direct codebase analysis)
- `/home/raul/Desktop/runes/src/typecheck.c` — Type checker implementation, identified bugs at specific line numbers
- `/home/raul/Desktop/runes/src/resolver.c` — Resolver implementation, static Symbol bug at line 159
- `/home/raul/Desktop/runes/src/types.c`, `types.h`, `symbol_table.h`, `ast.h` — Type representations and data structures
- `/home/raul/Desktop/runes/docs/specv0_1.md` — Language specification v0.1 (authoritative for feature requirements)
- `/home/raul/Desktop/runes/.planning/PROJECT.md` — Project context, deferred features, pending decisions
- `/home/raul/Desktop/runes/.planning/codebase/ARCHITECTURE.md` — Existing architecture documentation
- `/home/raul/Desktop/runes/type_checker_guide1.md` — Type checker implementation roadmap

### Secondary (MEDIUM confidence — established patterns, training data)
- Standard compiler construction (Dragon Book, Engineering a Compiler by Cooper & Torczon)
- Go compiler structural interface satisfaction model
- C11 integer promotion rules (ISO C11 section 6.3.1)
- "Warnings for pattern matching" (Maranget 2007) — referenced to justify NOT using full algorithm for v0.1
- Python, Rust, Go module cycle detection patterns

### Notes on Source Limitations
- Web search was unavailable during research. All algorithm recommendations are from training data on well-established compiler techniques. The algorithms themselves are stable and canonical; cutting-edge alternatives or 2026-specific library versions were not surveyed.

---
*Research completed: 2026-04-02*
*Ready for roadmap: yes*
