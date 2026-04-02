# Technology Stack: Compiler Frontend Completion

**Project:** Runes Bootstrap Compiler -- Frontend Completion
**Researched:** 2026-04-02
**Mode:** Ecosystem (stack dimension for hand-written C compiler)

## Constraint Summary

This is a pure C11 bootstrap compiler. No external dependencies. Arena allocation only. The stack is already chosen -- the research here covers **algorithms, data structures, and implementation techniques** for each missing feature area.

## Recommended Techniques by Feature Area

### 1. Type Promotion / Numeric Widening

| Technique | Purpose | Why |
|-----------|---------|-----|
| Static rank table | Determine promotion direction | O(1) lookup, trivially correct, matches C's own integer promotion rules |
| Separate literal typing | Distinguish `i32`-the-literal from `i32`-the-declared-variable | Prevents false widening; lets `u8 x = 5` pass without needing promotion |

**Algorithm: Numeric Type Rank Table**

Define a static integer rank per numeric type. Promotion is legal when source rank <= target rank within the same sign family, or for specific cross-sign cases.

```c
// In types.c
static int numeric_rank(const char *name) {
    // Signed integers
    if (strcmp(name, "i8")  == 0) return 1;
    if (strcmp(name, "i16") == 0) return 2;
    if (strcmp(name, "i32") == 0) return 3;
    if (strcmp(name, "i64") == 0) return 4;
    // Unsigned integers
    if (strcmp(name, "u8")  == 0) return 1;
    if (strcmp(name, "u16") == 0) return 2;
    if (strcmp(name, "u32") == 0) return 3;
    if (strcmp(name, "u64") == 0) return 4;
    if (strcmp(name, "usize") == 0) return 4;
    // Floats
    if (strcmp(name, "f32") == 0) return 5;
    if (strcmp(name, "f64") == 0) return 6;
    return 0; // non-numeric
}

static bool is_signed(const char *name)   { return name[0] == 'i'; }
static bool is_unsigned(const char *name)  { return name[0] == 'u'; }
static bool is_float(const char *name)     { return name[0] == 'f'; }
```

**Promotion rules (implement in `type_is_promotable(target, source)`):**
- Same family (both signed, both unsigned, both float): source rank <= target rank.
- Unsigned to signed: allowed if target rank > source rank (u8 -> i16 is safe, u32 -> i32 is not).
- Integer to float: always allowed (may lose precision, but spec allows it).
- Float to integer: NEVER implicit. Require explicit cast.
- Signed to unsigned: NEVER implicit. Require explicit cast.

**Literal handling:** The current code treats all integer literals as `i32` and all float literals as `f64`. This is the right default. Add a `is_literal` flag to the expression node (or infer from AST_INT_LITERAL/AST_FLOAT_LITERAL context) so that literal `5` can be assigned to `u8` without promotion -- it is a compile-time-known value, not a widening conversion. Implement range checking: if the literal value exceeds the target type's range, emit an error.

**Confidence:** HIGH -- this is the standard technique used in GCC, Clang, tcc, and every C-family compiler. Rank tables are the canonical approach.

**What NOT to do:** Do not use string comparison chains for every promotion check (the current `type_is_assignable` approach). It does not scale, is error-prone, and misses cases. Replace with rank-based logic.

---

### 2. Interface Satisfaction Checking

| Technique | Purpose | Why |
|-----------|---------|-----|
| Structural method-set comparison | Check if a type satisfies an interface | Matches Go's approach; natural fit for Runes which has no `impl` keyword |
| Lazy checking at use-site | Only check satisfaction when a value is used where an interface is expected | Avoids O(N*M) pre-computation of all type-interface pairs |

**Algorithm: Structural Satisfaction**

When a value of type `T` is used where interface `I` is expected (function parameter, assignment), check:

1. For each method `m` in `I.method_names[0..method_count]`:
   a. Walk `T.methods` linked list (for structs/variants) looking for a method with matching name.
   b. If found, check `type_equals(I.method_types[i], found->type)` with special handling: the `self` parameter type should match `T` (or `*T`).
   c. If not found, emit error: "type T does not satisfy interface I: missing method m".

2. Return true only if ALL interface methods are satisfied.

```c
// In types.c or typecheck.c
bool type_satisfies_interface(TypeChecker *tc, Type *concrete, Type *iface) {
    if (iface->kind != TY_INTERFACE) return false;
    InterfaceType *it = &iface->as.interface_t;

    for (int i = 0; i < it->method_count; i++) {
        Method *found = NULL;
        // Search concrete type's method list
        Method *m = NULL;
        if (concrete->kind == TY_STRUCT) m = concrete->as.struct_t.methods;
        else if (concrete->kind == TY_VARIANT) m = concrete->as.variant.methods;

        while (m) {
            if (strcmp(m->name, it->method_names[i]) == 0) {
                found = m;
                break;
            }
            m = m->next;
        }

        if (!found) return false;
        // Check signature compatibility (ignoring self parameter)
        if (!function_signature_compatible(it->method_types[i], found->type))
            return false;
    }
    return true;
}
```

**Where to call it:** In `type_is_assignable()` -- when target is `TY_INTERFACE` and source is `TY_STRUCT` or `TY_VARIANT`, delegate to `type_satisfies_interface`. Also in function call argument checking.

**Confidence:** HIGH -- structural typing / duck typing for interfaces is well-established (Go, TypeScript structural types). The method linked list is already in the codebase.

**What NOT to do:** Do not build a global "implements" table. It is premature complexity for a bootstrap compiler with no generics. Check at use-site only.

---

### 3. Exhaustiveness Checking for Match Expressions

| Technique | Purpose | Why |
|-----------|---------|-----|
| Constructor set coverage | Check all variant arms are covered | Simple, correct, sufficient for v0.1 without nested patterns |
| Wildcard-aware short circuit | Presence of `_` makes match exhaustive | Handles the common case trivially |

**Algorithm: Simple Variant Coverage**

For v0.1 (no generics, no nested pattern analysis needed), exhaustiveness reduces to set coverage on variant arms:

1. Determine the subject type of the match expression.
2. If subject type is `TY_VARIANT`:
   a. Build a bitset (or bool array) of size `arm_count` from the variant definition.
   b. For each match arm pattern:
      - If wildcard (`_`): mark exhaustive, done.
      - If identifier (catch-all binding): mark exhaustive, done.
      - If variant constructor name: find its index in the variant definition, mark that index as covered.
   c. After all arms: check all indices are covered. If not, emit error listing uncovered arms.
3. If subject type is `TY_PRIMITIVE` (bool):
   a. Track `true` and `false` coverage. Require both or a wildcard.
4. If subject type is anything else (integers, structs):
   a. Require a wildcard arm. If none, emit error.

```c
static void check_match_exhaustiveness(TypeChecker *tc, AstNode *match_node,
                                        Type *subject_type) {
    bool has_wildcard = false;
    // For variants: track which arms are covered
    bool *covered = NULL;
    int variant_count = 0;

    if (subject_type->kind == TY_VARIANT) {
        variant_count = subject_type->as.variant.arm_count;
        covered = arena_alloc(tc->arena, sizeof(bool) * variant_count);
        memset(covered, 0, sizeof(bool) * variant_count);
    }

    AstNode *arm = match_node->as.match_expr.arms;
    while (arm) {
        AstNode *pattern = arm->as.match_arm.pattern;
        if (pattern_is_wildcard(pattern)) {
            has_wildcard = true;
            break;
        }
        if (subject_type->kind == TY_VARIANT) {
            const char *arm_name = extract_variant_name(pattern);
            if (arm_name) {
                for (int i = 0; i < variant_count; i++) {
                    if (strcmp(subject_type->as.variant.arm_names[i], arm_name) == 0) {
                        covered[i] = true;
                        break;
                    }
                }
            }
        }
        arm = arm->next; // or however arms are linked
    }

    if (!has_wildcard && subject_type->kind == TY_VARIANT) {
        for (int i = 0; i < variant_count; i++) {
            if (!covered[i]) {
                typechecker_error(tc, match_node->line, match_node->col,
                    "Non-exhaustive match: missing arm '%s'",
                    subject_type->as.variant.arm_names[i]);
            }
        }
    }
}
```

**Why not the full Maranget algorithm:** The Maranget usefulness/exhaustiveness algorithm (from "Warnings for pattern matching", 2007) handles nested tuple destructuring, constructor nesting, and guard interactions. Runes v0.1 does not need this -- variants are flat (no nested variant-in-variant patterns in the spec). Implementing Maranget would be premature complexity. If v0.2 introduces nested patterns or generics, upgrade then.

**Confidence:** HIGH -- set coverage for flat variants is textbook. Every ML-family compiler starts here.

**What NOT to do:** Do not attempt to analyze guards for exhaustiveness. A guarded arm is never considered to cover its constructor (standard approach). Only unguarded arms contribute to coverage.

---

### 4. Cross-File Module Resolution

| Technique | Purpose | Why |
|-----------|---------|-----|
| Module registry (hash map of module name -> symbol table) | Track loaded modules | O(1) lookup, natural fit with existing symbol table infrastructure |
| File path mapping convention | Map `use kernel.arch.x86` to file path | Consistent, predictable, no configuration needed |
| Two-phase module loading | Register declarations first, then type-check | Handles forward references across modules |

**Algorithm: Module Loading Pipeline**

```
1. Parse the root file. Collect all `use` declarations.
2. For each `use kernel.arch.x86`:
   a. Map to file path: kernel/arch/x86.runes (relative to project root)
   b. If already loaded (check module registry): skip.
   c. Lex + parse the file. Store its AST.
   d. Recursively process its `use` declarations (detect cycles).
3. Two-pass resolution:
   a. Pass 1: Walk all module ASTs. Register top-level declarations
      (functions, types, interfaces) into per-module scopes in the symbol table.
   b. Pass 2: Resolve all references, now that all modules' symbols are available.
4. Type checking proceeds on the combined, resolved AST forest.
```

**Data structure additions:**

```c
// Module registry -- add to a new module.h or extend symbol_table.h
typedef struct Module {
    const char *path;          // interned file path
    const char *name;          // "kernel.arch.x86"
    AstNode *ast;              // parsed AST root
    Scope *scope;              // module-level symbol scope
    bool resolved;             // has pass-1 resolution completed?
    struct Module *next;       // hash chain
} Module;

typedef struct {
    Module **buckets;
    uint32_t capacity;
    Arena *arena;
} ModuleRegistry;
```

**Cycle detection:** Maintain a "currently loading" set (a simple linked list or bitset of module IDs). If a module is encountered that is already in the loading set, emit a cyclic dependency error. This is the standard approach used by Go, Rust, and Python compilers.

**Confidence:** MEDIUM -- the algorithm is well-known, but the implementation requires significant new infrastructure (file I/O, module registry, multi-file AST management). The current codebase processes a single file. This is the largest new feature.

**What NOT to do:** Do not implement incremental/lazy module loading for the bootstrap compiler. Load everything eagerly. Do not implement a build system or dependency graph solver -- linear topological order from `use` statements is sufficient.

---

### 5. Visibility Enforcement (pub/private)

| Technique | Purpose | Why |
|-----------|---------|-----|
| Check `is_pub` at reference site | Enforce access control | Simple, O(1), already have the `is_pub` field on Symbol |
| Module-scoped visibility | Private means module-private, not file-private | Matches the spec's `pub` semantics |

**Algorithm:**

The `Symbol` struct already has `is_pub`. The enforcement point is in the resolver, during name lookup across module boundaries:

```c
// In resolver.c, when resolving a qualified name like module.symbol
Symbol *sym = module_scope_lookup(target_module, symbol_name);
if (sym && !sym->is_pub && current_module != target_module) {
    error(r, node->line, node->col,
          "Cannot access private symbol '%s' from module '%s'",
          symbol_name, target_module->name);
}
```

**Key rule:** Within the same module, all symbols are visible regardless of `pub`. The `pub` modifier only affects cross-module access.

**Confidence:** HIGH -- this is trivial once module resolution exists. The data is already in the symbol table.

**What NOT to do:** Do not add visibility levels beyond pub/private for v0.1. No `pub(crate)`, no `protected`, no `internal`.

---

### 6. Realm / Lifetime Checking

| Technique | Purpose | Why |
|-----------|---------|-----|
| Realm nesting matrix (lookup table) | Validate function nesting | Already partially implemented as conditionals; convert to table for maintainability |
| Scope-exit escape analysis | Detect values escaping their realm | Prevents dangling pointers from regional/stack scopes |
| Copy-type whitelist | Auto-allow primitive escapes | Primitives and small Copy types can escape any scope safely |

**Algorithm: Realm Nesting Validation**

Replace the current `is_realm_nesting_legal()` chain of if-statements with a lookup table:

```c
// 6x6 matrix: [outer][inner] = allowed
static const bool REALM_NESTING[6][6] = {
    //              STACK  ARENA  HEAP   GC     FLEX   MAIN
    /* STACK */   { true,  false, false, false, false, false },
    /* ARENA */   { true,  false, false, false, false, false },
    /* HEAP  */   { true,  true,  true,  true,  true,  false },
    /* GC    */   { true,  false, false, true,  false, false },
    /* FLEX  */   { true,  true,  true,  true,  true,  false },
    /* MAIN  */   { true,  true,  true,  true,  true,  false },
};
```

**Algorithm: Scope-Exit Escape Analysis**

This is the realm checker's core job. For each assignment or return:

1. Determine the **realm** of the target (where the value is going).
2. Determine the **realm** of the source (where the value lives).
3. If source realm is more restrictive than target realm (e.g., STACK value assigned to HEAP pointer), and the source type is NOT a Copy type:
   a. Check if `promote() as X` is being used. If yes, allow.
   b. Otherwise, emit error: "value from [source_realm] cannot escape to [target_realm]".

**Copy type definition:** Primitives (i8..i64, u8..u64, f32, f64, bool, char, usize) and small structs marked `#[copy]` (future). For v0.1, just primitives.

**Where to implement:** The PROJECT.md notes a pending decision about whether realm checking stays in `typecheck.c` or moves to `realm_check.c`. Recommendation: **implement in `realm_check.c` as a separate pass** that runs after type checking. Rationale:
- Type checking is already 1445 lines and growing.
- Realm checking needs fully resolved types to work correctly.
- Separation makes both passes easier to test independently.
- The `realm_check.h` / `realm_check.c` files already exist (empty stubs).

**Confidence:** MEDIUM -- the nesting matrix is straightforward (HIGH confidence). The escape analysis is conceptually clear but implementation-specific details (tracking which realm each value belongs to, handling pointer chains, handling promote) require careful design. No external reference implementation exists for this exact memory model.

**What NOT to do:** Do not attempt full Rust-style borrow checking. Runes realms are simpler -- they are about allocation strategy scope, not reference lifetimes. Do not track individual reference lifetimes. Track realm membership only.

---

### 7. Comprehensive Test Coverage

| Technique | Purpose | Why |
|-----------|---------|-----|
| Positive + negative test suite | Verify correct acceptance AND rejection | Both are essential; a compiler that accepts everything is not useful |
| Snapshot testing via bash runner | Compare actual output to expected output | Already partially in place with `tester.bash` |
| Per-feature test files | One `.runes` file per language feature | Easy to identify what broke when a test fails |

**Test organization:**

```
src/tests/
  tester.bash              # existing test runner
  positive/                # programs that SHOULD compile
    01_primitives.runes
    02_functions.runes
    03_structs.runes
    04_variants.runes
    05_match_basic.runes
    06_match_exhaustive.runes
    07_interfaces.runes
    08_modules.runes
    09_realms.runes
    10_error_sets.runes
    11_type_promotion.runes
  negative/                # programs that SHOULD fail with specific errors
    err_type_mismatch.runes
    err_nonexhaustive_match.runes
    err_private_access.runes
    err_realm_nesting.runes
    err_realm_escape.runes
    err_undefined_name.runes
    err_duplicate_decl.runes
  expected/                # expected stderr output for negative tests
    err_type_mismatch.expected
    ...
```

**Test runner enhancement:** Extend `tester.bash` to:
1. Run positive tests: exit code 0 expected.
2. Run negative tests: exit code 1 expected, AND stderr matches `.expected` file.

**Confidence:** HIGH -- this is standard practice for every compiler project. The infrastructure (`tester.bash`) already exists.

---

## Supporting Libraries and Utilities

### Already In Place (No Changes Needed)

| Component | Location | Status |
|-----------|----------|--------|
| Arena allocator | `src/utils/arena.c` | Working, sufficient for all new features |
| String interning | `src/utils/strtab.c` | Working, used for all identifier comparisons |
| Symbol table (hash + scope stack) | `src/symbol_table.c` | Working, needs module-scope extension |
| Type representation (tagged union) | `src/types.c` | Working, needs new comparison functions |

### Needs Extension

| Component | What to Add | Where |
|-----------|-------------|-------|
| `types.c` | `type_is_promotable()`, `type_satisfies_interface()`, `numeric_rank()` | New functions in existing file |
| `symbol_table.h` | Module-aware scope (optional: module pointer on Scope) | Add `Module *module` field to Scope |
| `typecheck.c` | Match exhaustiveness, interface checks at call sites | New static functions |
| `realm_check.c` | Full realm validation pass | Implement from scratch |
| New: `module.c/h` | Module registry, file loading, cross-file resolution | New file pair |

### No New External Dependencies

The project constraint is pure C with standard library only. All techniques above use only:
- `<string.h>` for strcmp (already used everywhere)
- `<stdbool.h>` for bool arrays
- `<stdio.h>` for file I/O (module loading)
- Arena allocator for all dynamic allocation

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Exhaustiveness | Set coverage (flat) | Maranget algorithm | Over-engineered for v0.1; no nested patterns needed |
| Interface checking | Structural at use-site | Global implements table | Premature; adds complexity with no benefit until generics |
| Type promotion | Rank table | Per-pair matrix | Rank table is more compact, easier to extend, less error-prone |
| Module loading | Eager, linear | Lazy / on-demand | Bootstrap compiler; simplicity wins |
| Realm checking | Separate pass | Inline in type checker | typecheck.c already too large; separation aids testing |
| Test framework | Bash + `.expected` files | C unit test framework (Unity, Check) | No new dependencies constraint; bash is sufficient |

## Build System

No changes needed. The existing `gcc` compilation with manual file list works. When adding `module.c` and `realm_check.c`, add them to the compilation command.

```bash
# Current (add new .c files as they are created)
gcc -std=c11 -Wall -Wextra -o runes \
    src/main.c src/lexer.c src/parser.c src/ast.c \
    src/resolver.c src/symbol_table.c src/types.c src/typecheck.c \
    src/realm_check.c src/module.c \
    src/utils/arena.c src/utils/strtab.c
```

## Sources

All recommendations are based on:
- Direct analysis of the Runes codebase (types.h, typecheck.c, resolver.c, symbol_table.h, ast.h, types.c)
- The Runes spec v0.1 (`docs/specv0_1.md`)
- The type checker implementation guide (`type_checker_guide1.md`)
- Standard compiler construction techniques (Dragon Book, Engineering a Compiler by Cooper & Torczon, "Warnings for pattern matching" by Maranget 2007)
- Go compiler's structural interface satisfaction model
- C language integer promotion rules (ISO C11 section 6.3.1)

**Note:** Web search was unavailable during this research. All algorithm recommendations come from training data on well-established compiler techniques. Confidence levels reflect this -- the algorithms themselves are well-known and stable, but specific library version numbers or cutting-edge alternatives may exist that were not surveyed.
