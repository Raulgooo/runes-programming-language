# Feature Landscape

**Domain:** Compiler frontend (type checker, resolver, realm checker) for the Runes systems language
**Researched:** 2026-04-02
**Confidence:** MEDIUM (based on spec analysis, codebase audit, and compiler engineering knowledge from training data -- no web verification available)

## Table Stakes

Features that must work correctly for the compiler frontend to be considered complete. Missing any of these means the compiler will accept invalid programs or reject valid ones.

### Type Checker -- Core

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Integer type promotion (i8->i16->i32->i64, u8->u16->u32->u64) | Binary ops on mixed integer sizes must have defined behavior; spec uses i32/i64/u64 freely | Med | Widening only. Define promotion matrix: smaller signed widens to larger signed, smaller unsigned widens to larger unsigned. Mixed sign (i32 + u32) needs a policy -- recommend error, force explicit cast |
| Float type promotion (f32->f64) | `3.14` is f64, operations with f32 need rules | Low | Straightforward: f32 widens to f64 in mixed expressions |
| Integer-to-float coercion | Expressions like `i32 * f64` need defined behavior | Low | Recommend: no implicit int-to-float. Require explicit cast. Prevents subtle precision loss |
| Literal type inference | `z = 3.14` infers f64, `x = 5` infers i32, `0xFF` infers contextual | Med | Integer literals need contextual typing: `u8 x = 255` should work without cast. Uncontextualized integer literals default to i32 |
| Boolean strictness | `if x` where x is i32 must be rejected -- condition must be bool | Low | Already partially implemented. Verify no implicit truthiness |
| Assignment type compatibility | `i32 x = expr` must verify expr is i32 or promotable to i32 | Low | Already implemented. Extend with promotion rules |
| Function signature checking | Argument count, argument types, return type all verified | Med | Partially implemented. Needs: named args, default values, variadic extern |
| Method resolution (`self` dispatch) | `v.length()` must find `method Vec2 { f length(self) }` | Med | Must look up method blocks for the receiver type, bind `self`, check args. Currently partially working |
| Interface satisfaction checking | `method Drawable for Vec2` must verify all required methods exist with correct signatures | High | Not implemented. Must: (1) collect interface method signatures, (2) collect impl method signatures, (3) verify 1:1 correspondence with compatible types |
| Interface-typed parameters | `f render(d: Drawable)` must accept any type that satisfies Drawable | High | Requires interface dispatch table awareness. For frontend: verify argument type has a `method Drawable for T` block |
| Struct field type checking | `Vec2(x: 1.0, y: 2.0)` must verify field names and types | Med | Partially working. Needs: default values, partial construction, missing field errors |
| Variant constructor checking | `RGB(255, 0, 0)` must verify payload types match variant arm definition | Med | Marked TODO in codebase. Must resolve variant type, find arm, check payload |
| Pointer type checking | `*T` operations: dereference yields T, address-of T yields *T | Med | Partially implemented. Needs: null checking policy, pointer arithmetic in unsafe only |
| Array type checking | `[N]T` -- index must be integer, result is T, bounds known at compile time | Med | Index type verified. Needs: bounds checking for constant indices, length access |
| Fallible type unwrapping | `try expr` requires expr to be `!T`, yields T. `catch` requires `!T` on left | Med | Implemented. Verify edge cases: nested fallibles, catch with default value vs catch with handler |
| Tuple type checking | `(T1, T2)` construction and destructuring | Med | Destructuring partially implemented. Verify: tuple field access, tuple in return position |
| Const correctness | `const` bindings cannot be reassigned | Low | Verify this is enforced in type checker or resolver |
| Return type verification | Named return variable must be assigned a compatible type; void functions must not return values | Med | Partially implemented. Named returns are Runes-specific -- ensure the return variable is properly typed and always assigned on all paths |

### Type Checker -- Pattern Matching

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Match exhaustiveness (variants) | `match color { Red -> ..., Green -> ... }` must error if Blue is missing and no wildcard | High | Core correctness requirement. For v0.1: enumerate variant arms, verify all covered OR wildcard present. Guards make exhaustiveness undecidable -- treat guarded arms as non-exhaustive for their pattern |
| Match exhaustiveness (booleans) | `match b { true -> ..., false -> ... }` is exhaustive | Low | Special case of variant exhaustiveness |
| Match exhaustiveness (integers) | Integer match is never exhaustive without wildcard | Low | Just require wildcard for integer/string subjects |
| Pattern binding type inference | `match point { Vec2(x, y) -> ... }` binds x:f32, y:f32 in arm scope | High | Must destructure struct/variant, create scoped bindings with correct types. Marked TODO: "full struct field lookup" |
| Guard type checking | `n if n < 0 -> ...` guard must be bool | Low | Already implemented |
| Nested pattern matching | `match p { Vec2(x: 0.0, y) -> ... }` with literal sub-patterns | Med | Must check literal equality type matches field type |
| Duplicate arm detection | Two arms with identical patterns should warn | Low | Nice-to-have for v0.1, but standard in mature compilers |
| Match expression type consistency | All arms of a match-as-expression must return the same type | Med | When match is used as expression (`str label = match ...`), all arm body types must unify |

### Resolver -- Name Resolution

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Cross-file module resolution | `use kernel.arch.x86` must find the module across files | High | Currently single-file only. Must: define module search strategy (directory-based? single-file modules?), parse imported files, merge symbol tables |
| Pub/private visibility enforcement | Non-pub symbols in a module are inaccessible from outside | Med | `pub` keyword exists in parser. Resolver must check visibility on cross-scope lookups |
| Use-statement aliasing | `use kernel.alloc_page` brings `alloc_page` into scope | Med | Must handle: whole-module import (`use kernel`), specific symbol import (`use kernel.alloc_page`), nested paths |
| Cyclic dependency detection | `mod A { use B }` and `mod B { use A }` must error or be handled | Med | Detect during module graph construction. Recommend: error on circular module imports (simplest correct behavior for bootstrap compiler) |
| Forward reference resolution | Types/functions used before declaration in same scope | Med | Two-pass resolution already exists. Verify it handles: mutual recursion between functions, type references in struct fields |
| Shadowing rules | Inner scope can shadow outer scope names | Low | Already implemented. Verify correct behavior: shadow warns or silently allows per spec intent |
| Method block resolution | `method Vec2 { ... }` methods must be findable when calling `v.method_name()` | Med | Must associate method blocks with their target type during resolution |
| Error set resolution | `error.MathError.DivByZero` qualified path must resolve | Med | Three-level qualified name: error keyword, set name, variant name |

### Realm Checker -- Memory Safety

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Nesting matrix enforcement | `regional f` inside `f` (non-main) is illegal per spec | Med | Already implemented in typecheck.c via `is_realm_nesting_legal()`. Verify completeness against spec table |
| Scope escape detection | Values cannot leave their memory scope unless Copy or promoted | High | Core memory safety feature. Must track: which values are allocated in which realm, whether they escape via return/assignment to outer scope |
| Promote validation | `promote(&t) as dynamic` only valid in certain contexts; `promote() as f` always invalid | Med | Partially implemented. Verify full matrix from spec section 5 |
| Copy type auto-escape | Primitives and small structs can escape scope without promote | Med | Must define which types are Copy (all primitives, small value structs). Check at scope boundaries |
| Flex function realm inheritance | `flex f` adopts caller's realm rules at each call site | High | Complex: must re-check flex function body under caller's realm constraints. For bootstrap compiler, may be acceptable to check flex bodies are realm-agnostic (no realm-specific operations) |
| Region cleanup guarantees | Regional function's arena is freed at scope exit -- no dangling references | Med | Verify no pointer to regional data escapes without promote |
| Stack-only enforcement | `stack f` cannot contain any non-stack strategy | Low | Subset of nesting matrix. Verify strictest case works |

### Error Reporting

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Accurate line/column in all errors | Every diagnostic must point to the right source location | Med | Infrastructure exists. Verify all new error paths carry correct location |
| Descriptive error messages | "cannot assign f64 to i32" not "type mismatch" | Med | Ongoing effort. Each new check needs a clear message |
| Error recovery (don't stop at first error) | Report multiple errors per compilation | Med | Already implemented (error accumulation). Verify new checks don't cascade into false positives |
| Negative test validation | Programs that SHOULD fail must fail with expected error | High | Test infrastructure needed: expected-error annotations in test files, test runner that verifies error output |

### Testing

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Positive test suite (all spec constructs) | Every v0.1 feature needs at least one test that exercises the full pipeline | High | Currently 27/32 pass. Need comprehensive coverage of: every type, every operator, every control flow, every memory strategy combination |
| Negative test suite (expected failures) | Compiler must reject invalid programs | High | Need tests for: type mismatches, realm violations, missing match arms, visibility violations, undefined names, cyclic imports |
| Regression tests for fixed bugs | Every bug fix gets a test that would have caught it | Low | Standard practice. Add test for each fix |

## Differentiators

Features that would make this compiler frontend stand out. Not expected for a bootstrap compiler but add significant value.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Realm-aware type error messages | "cannot return *PageTable from regional scope -- use promote() as dynamic" -- suggests the fix | Med | Dramatically improves DX for Runes' unique memory model. Worth doing |
| Match exhaustiveness with suggestions | "non-exhaustive match: missing arm for Blue" -- names the missing variants | Med | Standard in Rust/OCaml. High value, moderate effort on top of exhaustiveness checking |
| Unused variable/import warnings | Warn on declared-but-unused symbols | Low | Easy to add during type checking. Useful for catching dead code early |
| Type inference for complex expressions | `result = if cond { 1 } else { 2 }` infers i32 without annotation | Med | If-as-expression and match-as-expression type inference. Partially needed for correctness, but full inference is a differentiator |
| Promote suggestion on scope escape | Auto-suggest `promote() as X` when a value illegally escapes scope | Med | Pairs with realm checking. Very Runes-specific, very helpful |
| Dead code detection after match | Warn if code after a fully-returning match is unreachable | Low | Nice-to-have. Low effort once control flow analysis exists |
| Struct field completion in errors | "type Vec2 has no field 'z' -- available fields: x, y" | Low | Helpful diagnostic. Easy to implement during field lookup |

## Anti-Features

Features to explicitly NOT build in this phase. Each would waste time or introduce scope creep.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Generics / type parameters (`<T>`) | Explicitly deferred to future version per PROJECT.md. Massive complexity (monomorphization or type erasure, constraint solving) | Accept `<T>` in parser but reject in type checker with clear "generics not yet supported" error |
| JSON serialization / `as J` / schema inheritance | Deprecated per PROJECT.md. spec section 13 will not be implemented | Parser can parse schemas but type checker should skip JSON-specific validation. Error on `as J` usage |
| Pipes (`pipe ... = expr \| expr`) | Deferred to future version. Requires pipeline type threading | Parser may already handle syntax. Type checker should error with "pipes not yet supported" |
| List types (sl/dl) | Deferred. Depends on generics and runtime support | Error on usage with clear message |
| Incremental compilation | Recompiling only changed files. Significant infrastructure | Full recompilation is fine for bootstrap compiler |
| Trait/interface default method implementations | Over-engineering for v0.1. Adds complexity to interface satisfaction checking | Require all interface methods to be explicitly implemented |
| Implicit type conversions beyond numeric promotion | Automatic `str` to `i32`, custom conversion operators, etc. | Explicit conversions only. Prevents surprise behavior |
| Fancy type inference (bidirectional, HM-style) | Overkill for Runes v0.1. Local type inference + explicit annotations is sufficient | Infer from initializers and literals. Require annotations on function parameters and struct fields |
| Borrow checker (Rust-style lifetime analysis) | Runes uses realms, not borrows. Completely different memory safety model | Realm checker covers the safety guarantees Runes needs |
| Code generation | Separate project per PROJECT.md | Frontend produces typed, validated AST. Codegen consumes it later |
| LSP / IDE integration | Future work | Focus on good error messages to stderr. LSP wraps that later |
| Async/await | v0.2 feature per spec | Not even parser support needed now |

## Feature Dependencies

```
Integer type promotion -> Assignment type compatibility (uses promotion rules)
Integer type promotion -> Binary expression type inference (uses promotion rules)
Literal type inference -> Integer type promotion (contextual literal typing depends on target type)

Struct field type checking -> Pattern binding type inference (destructuring needs field knowledge)
Variant constructor checking -> Match exhaustiveness (need variant arm enumeration)
Variant constructor checking -> Pattern binding type inference (variant payload types)

Cross-file module resolution -> Pub/private visibility enforcement (visibility only matters across modules)
Cross-file module resolution -> Use-statement aliasing (imports resolve to cross-file symbols)
Cross-file module resolution -> Cyclic dependency detection (cycles only possible with multiple files)
Use-statement aliasing -> Cross-file module resolution (aliasing resolves cross-file names)

Interface satisfaction checking -> Interface-typed parameters (must verify satisfaction before accepting)
Method block resolution -> Interface satisfaction checking (must find impl methods)
Method block resolution -> Method resolution (must find methods for dispatch)

Scope escape detection -> Copy type auto-escape (escape rules depend on Copy trait)
Scope escape detection -> Promote validation (promote is the escape mechanism)
Nesting matrix enforcement -> Flex function realm inheritance (flex inherits nesting rules)

Positive test suite -> (all other features) -- tests validate everything
Negative test suite -> Error reporting (tests verify error messages)
Match exhaustiveness -> Variant constructor checking (must enumerate arms from type definition)
```

## MVP Recommendation

Prioritize in this order (dependency-driven):

1. **Fix existing test failures** -- Baseline: all 32 tests should pass (or be properly categorized as expected-negative). Cannot build on a broken foundation.

2. **Integer/float type promotion rules** -- Table stakes for any expression-heavy language. Define the promotion matrix once, every binary operation and assignment uses it. Unblocks correct type checking for the majority of expressions.

3. **Struct field lookup and variant arm resolution** -- Currently marked TODO. Unblocks pattern matching, method calls, and constructor checking. High dependency count downstream.

4. **Match exhaustiveness checking** -- Core correctness. Without this, the compiler accepts programs that will crash at runtime on unhandled variant arms. Depends on variant resolution (item 3).

5. **Interface satisfaction checking** -- Required for `method Interface for Type` blocks to be verified. Without this, interface contracts are unenforced promises.

6. **Cross-file module resolution + visibility** -- Required for any multi-file Runes program. High complexity but essential for real-world usage.

7. **Realm checker hardening (scope escape, promote validation)** -- The realm system is Runes' core differentiator. Must be correct. Partially implemented; needs completion and thorough testing.

8. **Negative test suite** -- Validates all of the above. Build incrementally alongside each feature.

Defer to post-MVP:
- **Flex function full re-checking** -- Complex, can be simplified for bootstrap (treat flex as "accepts any realm, no realm-specific ops allowed inside")
- **Unused variable warnings** -- Nice but not correctness-critical
- **Fancy error suggestions** -- Build after core checking works

## Sources

- `/home/raul/Desktop/runes/docs/specv0_1.md` -- Language specification v0.1 (authoritative)
- `/home/raul/Desktop/runes/.planning/PROJECT.md` -- Project context and scope
- `/home/raul/Desktop/runes/.planning/codebase/ARCHITECTURE.md` -- Current architecture
- `/home/raul/Desktop/runes/type_checker_guide1.md` -- Type checker implementation roadmap
- `/home/raul/Desktop/runes/src/typecheck.c` -- Current type checker implementation (1445 lines, 2 TODOs)
- `/home/raul/Desktop/runes/src/resolver.c` -- Current resolver implementation (493 lines)
- Compiler engineering knowledge from training data (confidence: MEDIUM -- established patterns but not verified against 2026 sources)
