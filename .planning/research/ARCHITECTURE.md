# Architecture Patterns

**Domain:** C compiler frontend completion (Runes language)
**Researched:** 2026-04-02
**Confidence:** HIGH (based on direct codebase analysis, not external sources)

## Current Pipeline

```
Source files
    |
    v
[Lexer] -- per-file tokenization
    |
    v
[Parser] -- per-file AST construction, merged into single program node
    |
    v
[Resolver] -- two-pass name resolution (collect decls, then resolve refs)
    |       -- module path resolution via find_symbol_in_path()
    |       -- scope push/pop for blocks, functions, match arms
    |
    v
[TypeChecker] -- type inference, assignment checking, realm nesting
    |         -- method body checking, pattern matching
    |         -- partially checks promote() and realm nesting
    |
    v
(codegen -- future, out of scope)
```

All phases share: Arena (allocation), SymbolTable (scopes + symbols), TypeContext (type singletons).

## Recommended Architecture After Completion

The pipeline should remain **4 passes** with the realm/lifetime checking staying inside the type checker, not split into a separate `realm_check.c` pass. The existing `realm_check.c` is empty (1 line) and should remain unused.

### Why NOT a Separate Realm Check Pass

1. **Realm nesting is already woven into typecheck.c** -- `current_realm` is saved/restored around every `AST_FUNC_DECL` and `AST_METHOD_DECL`. The nesting matrix check (`is_realm_nesting_legal`) runs during type checking. Extracting it would require duplicating the entire function/method traversal.

2. **Promote validation requires type information** -- `promote(&t) as dynamic` needs to know the type of `t` (is it a copy type? a pointer?) to validate the operation. This information is only available during or after type inference. Running it as a separate pass would mean re-traversing the AST with the same type context.

3. **Scope-exit validation is a type+realm hybrid** -- checking whether a value escapes its memory scope requires knowing both the type (copy types auto-escape) and the realm context. These are interleaved decisions, not separable passes.

4. **The PROJECT.md decision is correct** -- "Realm checking stays in typecheck.c" is the right call.

### Recommended Final Pipeline

```
[Lexer] --> [Parser] --> [Resolver] --> [TypeChecker]
                                            |
                                            +-- type inference
                                            +-- type promotion rules
                                            +-- interface satisfaction
                                            +-- exhaustiveness checking
                                            +-- realm nesting enforcement
                                            +-- lifetime/scope-exit validation
                                            +-- promote() validation
```

## Component Boundaries

| Component | Responsibility | Reads From | Writes To |
|-----------|---------------|------------|-----------|
| **lexer.c** | Tokenization, string interning | Source text, StrTab | Token stream |
| **parser.c** | AST construction, syntax validation | Token stream, Arena | AST (AstNode tree) |
| **resolver.c** | Name binding, scope management, module path resolution, use-aliasing, visibility enforcement, cyclic dependency detection | AST, SymbolTable | SymbolTable (populated with Symbol entries) |
| **typecheck.c** | Type inference, type checking, realm enforcement, interface satisfaction, exhaustiveness, promote validation | AST, SymbolTable, TypeContext | `resolved_type` field on AstNode, error reports |
| **types.c** | Type representation, type comparison, type construction | TypeContext, Arena | Type objects |
| **symbol_table.c** | Scope stack, symbol storage, lookup | Arena | Scope chain |

### New Responsibilities by Component

**resolver.c additions:**
- Cross-file module resolution (currently single-file only; multi-file merges ASTs in main.c but resolver sees one flat program)
- Visibility enforcement (`pub` vs private across module boundaries)
- Cyclic dependency detection (detect `mod A { use B }` / `mod B { use A }` cycles)
- Proper `use` aliasing (current implementation uses a static `Symbol tmp` which is a bug -- it returns a pointer to a static local that gets overwritten)

**typecheck.c additions:**
- Type promotion rules (i8->i16->i32->i64, u8->u16->u32->u64, i32->f64, etc.)
- Interface satisfaction checking (`method Drawable for Vec2 { ... }` verifies all interface methods are implemented with correct signatures)
- Exhaustiveness checking for match expressions (all variant arms covered, or wildcard present)
- Scope-exit/lifetime enforcement (values cannot escape their realm without promote or being copy types)
- Copy-type auto-escape validation

**types.c additions:**
- `type_is_promotable(Type *from, Type *to)` -- numeric widening rules
- `type_satisfies_interface(Type *concrete, Type *iface)` -- method signature matching
- `type_is_copy(Type *t)` -- determines if a type auto-escapes scope boundaries

## Data Flow

### Phase 1: Lexing (unchanged)
```
Source text --> Lexer --> Token stream
                |
                v
              StrTab (interned strings)
```

### Phase 2: Parsing (unchanged)
```
Token stream --> Parser --> AST (AstNode tree)
                  |
                  v
                Arena (all nodes allocated here)
```

### Phase 3: Name Resolution (enhanced)

Current flow:
```
AST --> Resolver pass 1: collect_decls (top-level names)
    --> Resolver pass 2: resolve references + handle use-decls
    --> SymbolTable populated
```

Enhanced flow:
```
AST --> Resolver pass 1: collect_decls (all top-level + module-scoped names)
    --> Resolver pass 1.5: resolve module paths + build module symbol tables
    --> Resolver pass 2: resolve all references
        - For each identifier: lookup in local scope, then parent scopes
        - For qualified names (mod.Type): lookup via module symbol table
        - For use-decls: resolve path, create alias in current scope
        - VISIBILITY CHECK: if symbol crosses module boundary, require is_pub
    --> Resolver pass 3: cyclic dependency check (optional, can be done in pass 1.5)
    --> SymbolTable fully populated, all names resolved
```

Key data structures needed in resolver:
```c
// Add to Resolver struct:
typedef struct {
    SymbolTable *st;
    int error_count;
    bool had_error;
    MemoryRealm current_realm;

    // NEW: module context for visibility checking
    const char *current_module;  // NULL at top level
    int module_depth;            // for nested modules
} Resolver;
```

### Phase 4: Type Checking (enhanced)

Current flow:
```
AST + SymbolTable --> TypeChecker collect_decls (register type declarations)
                  --> TypeChecker check_node (recursive, per-declaration)
                      - infer expression types
                      - check assignments
                      - validate realm nesting
                  --> AstNode.resolved_type populated
```

Enhanced flow:
```
AST + SymbolTable --> TC pass 1: collect type declarations (existing)
                  --> TC pass 1.5: register interface declarations + method implementations
                  --> TC pass 2: check_node (recursive, per-declaration)
                      - infer expression types (existing)
                      - apply type promotion where needed (NEW)
                      - check assignments with promotion (enhanced)
                      - validate realm nesting (existing)
                      - validate promote() operations (enhanced)
                      - check scope-exit safety (NEW)
                      - check match exhaustiveness (NEW)
                  --> TC pass 3: interface satisfaction validation
                      - for each "method <Iface> for <Type>" block
                      - verify all interface methods implemented
                      - verify signature compatibility
                  --> AstNode.resolved_type fully populated
```

### Cross-Component Data Dependencies

```
                    SymbolTable
                   /     |      \
                  /      |       \
          Resolver --> TypeChecker
             |              |
             v              v
       (names bound)  (types resolved)
             |              |
             +------+-------+
                    |
                    v
              AstNode.resolved_type
```

The SymbolTable is the shared state between resolver and type checker. The resolver populates `Symbol.name`, `Symbol.kind`, `Symbol.node`, `Symbol.is_pub`. The type checker later fills in `Symbol.type` during its collect_decls pass.

## Patterns to Follow

### Pattern 1: Two-Pass Declaration Collection
**What:** Collect all names before resolving references. Already used in both resolver and type checker.
**When:** Always, for both resolver and type checker.
**Why:** Allows forward references (function A calls function B defined later).

This pattern must be extended for modules: collect module names first, then collect names within each module, then resolve cross-module references.

### Pattern 2: Save/Restore Context
**What:** Save current state (realm, expected_ret, module) before entering a scope, restore after.
**When:** Every function declaration, method block, module block.
**Example (existing):**
```c
MemoryRealm saved_realm = tc->current_realm;
tc->current_realm = func_realm;
// ... check body ...
tc->current_realm = saved_realm;
```

This pattern extends to module context in the resolver:
```c
const char *saved_module = r->current_module;
r->current_module = node->as.mod_decl.name;
// ... resolve module contents ...
r->current_module = saved_module;
```

### Pattern 3: Type Helpers in types.c
**What:** Type comparison logic lives in types.c, not typecheck.c.
**When:** Any new type relationship (promotion, interface satisfaction, copy semantics).
**Why:** Keeps typecheck.c focused on traversal and error reporting; types.c owns the type algebra.

```c
// types.c additions:
bool type_is_numeric(Type *t);
bool type_is_promotable_to(Type *from, Type *to);
Type *type_promote(TypeContext *ctx, Type *a, Type *b); // returns common type
bool type_is_copy(Type *t);                             // primitives, small structs
bool type_satisfies_interface(Type *concrete, InterfaceType *iface);
```

### Pattern 4: Error Accumulation Without Halting
**What:** Report every error found, do not stop at first error.
**When:** All validation passes.
**Why:** Better developer experience -- show all problems at once.

Already implemented correctly in both resolver and type checker.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Static Local for Symbol Return
**What:** `find_symbol_in_path()` in resolver.c uses `static Symbol tmp` to return a symbol found inside a module. This is a data race waiting to happen and a correctness bug if the caller doesn't immediately copy.
**Why bad:** The returned pointer becomes invalid on the next call to `find_symbol_in_path()`.
**Instead:** Allocate the symbol in the arena, or better yet, define the symbol directly into the target scope during use-resolution rather than returning a temporary.

### Anti-Pattern 2: Duplicating Traversal Logic
**What:** Creating a separate `realm_check.c` that re-walks the AST to check realm rules.
**Why bad:** The type checker already walks every node. A second pass adds complexity and doubles realm-related state management.
**Instead:** Keep realm checking integrated in typecheck.c where it already lives.

### Anti-Pattern 3: Checking Interface Satisfaction Per-Call-Site
**What:** Verifying that a type satisfies an interface every time a method is called.
**Why bad:** O(calls * methods) instead of O(types * interfaces). Also produces duplicate errors.
**Instead:** Check interface satisfaction once, during the dedicated TC pass 3, after all method blocks have been processed.

### Anti-Pattern 4: Inline Promotion Rules in Expression Inference
**What:** Hardcoding promotion chains (i8->i16->i32->i64) inside `typechecker_infer_expr`.
**Why bad:** Promotion rules will be referenced from multiple places (binary ops, assignments, function args).
**Instead:** Put promotion logic in `types.c` as `type_promote()` and call it from typecheck.c.

## Module System Integration with Resolver

### How Modules Should Work

The current resolver handles `mod` and `use` but has gaps:

1. **Module scope is flat** -- `collect_decls` is called for module contents but they share the same scope push/pop as blocks. This works for single-module programs but breaks for visibility enforcement.

2. **Cross-module visibility is not checked** -- `find_symbol_in_path` hardcodes `tmp.is_pub = true` for all resolved symbols, bypassing visibility entirely.

3. **Use-decl aliasing has the static Symbol bug** described above.

### Recommended Module Architecture

```
Module resolution in resolver:

1. collect_decls at program level
   - For each AST_MOD_DECL: register as SYM_MOD
   - Push scope for module contents
   - collect_decls recursively inside module
   - Pop scope (but keep module's scope accessible via symbol)

2. resolve use-decls
   - Walk module path: kernel.arch.x86.read_cr3
   - At each segment, verify SYM_MOD, enter its scope
   - At final segment, verify is_pub (unless same module)
   - Define alias in current scope

3. resolve references
   - Qualified references (mod.symbol): verify visibility
   - Unqualified references: normal scope lookup
```

### Symbol Table Enhancement Needed

The current `SymbolTable` uses a simple scope stack. For module support, each module needs its own scope that persists after the module's scope is popped:

```c
// Option A (simpler): Store module scope pointer in Symbol
typedef struct Symbol {
    const char *name;
    SymbolKind kind;
    AstNode *node;
    struct Type *type;
    bool is_pub;
    struct Scope *module_scope;  // NEW: non-NULL only for SYM_MOD
} Symbol;
```

This allows `find_symbol_in_path` to enter a module's scope without pushing/popping the global scope stack.

## Exhaustiveness Checking Architecture

Exhaustiveness checking belongs in the type checker because it requires knowing the variant's arm count and types.

### Algorithm

```
For each match statement:
1. Infer subject type
2. If subject is TY_VARIANT:
   a. Collect all arm patterns
   b. If any pattern is wildcard (_): exhaustive
   c. Otherwise: build set of covered arm names
   d. Compare against variant's arm_names
   e. Report missing arms
3. If subject is TY_PRIMITIVE (bool):
   a. Check for true + false coverage, or wildcard
4. If subject is anything else:
   a. Require wildcard arm (warn if missing)
```

This is a leaf operation -- it does not affect data flow to other components.

## Suggested Build Order (Feature Dependencies)

Features have dependency chains. Build in this order:

```
Layer 0 (foundations, no new deps):
  - Fix static Symbol bug in resolver.c
  - Add type_is_copy(), type_is_numeric() to types.c
  - Add type_is_promotable_to(), type_promote() to types.c

Layer 1 (type system completions):
  - Type promotion in binary expressions and assignments
    Depends on: Layer 0 (type_promote)
  - Exhaustiveness checking for match
    Depends on: nothing new (variant types already resolved)

Layer 2 (interface system):
  - Interface satisfaction checking
    Depends on: Layer 0 (type helpers), method bodies already checked
  - Register interface implementations in type checker collect_decls
    Depends on: nothing new

Layer 3 (module system):
  - Module scope persistence (Symbol.module_scope)
    Depends on: nothing (symbol_table.h change)
  - Visibility enforcement in resolver
    Depends on: Layer 3 module scope
  - Cross-module use-aliasing fix
    Depends on: Layer 3 module scope
  - Cyclic dependency detection
    Depends on: Layer 3 module scope

Layer 4 (realm/lifetime):
  - Scope-exit validation (values escaping realm)
    Depends on: Layer 0 (type_is_copy), Layer 1 (promotion rules)
  - Promote() validation hardening
    Depends on: Layer 0 (type_is_copy)
  - Copy-type auto-escape validation
    Depends on: Layer 0 (type_is_copy)
```

### Rationale for This Order

- **Layer 0 first** because it is pure additions with zero risk of regression. Helper functions in types.c and the static Symbol fix are surgical.
- **Layer 1 before Layer 2** because type promotion is simpler and more testable in isolation. Exhaustiveness is self-contained.
- **Layer 2 before Layer 3** because interface checking only needs the existing symbol table, while module system changes touch the shared SymbolTable structure.
- **Layer 3 before Layer 4** because visibility enforcement changes how symbols are resolved, and realm/lifetime checking must respect module boundaries.
- **Layer 4 last** because it is the most complex validation and depends on all preceding type system work being correct.

## Files to Create or Modify

| File | Action | What Changes |
|------|--------|-------------|
| `src/types.h` | Modify | Add `type_is_copy`, `type_is_numeric`, `type_is_promotable_to`, `type_promote`, `type_satisfies_interface` declarations |
| `src/types.c` | Modify | Implement new type helper functions |
| `src/symbol_table.h` | Modify | Add `module_scope` field to Symbol struct |
| `src/symbol_table.c` | Modify | Handle module_scope in define/lookup |
| `src/resolver.h` | Modify | Add `current_module` and `module_depth` to Resolver struct |
| `src/resolver.c` | Modify | Visibility enforcement, fix static Symbol bug, cyclic detection, proper use-aliasing |
| `src/typecheck.c` | Modify | Type promotion, interface satisfaction, exhaustiveness, scope-exit validation |
| `src/realm_check.c` | No change | Keep empty -- realm checking stays in typecheck.c |

No new source files needed. The architecture stays clean with the existing file boundaries.

## Sources

- Direct codebase analysis of all `.c` and `.h` files in `src/`
- `docs/specv0_1.md` -- language specification for modules, realms, promote, visibility
- `type_checker_guide1.md` -- implementation roadmap
- `.planning/codebase/ARCHITECTURE.md` -- existing architecture documentation
- `.planning/PROJECT.md` -- project context and decisions
