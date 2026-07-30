# Associated (Static) Methods Plan

Status: implemented on `v0-1-exp`, including type-qualified parsing, generic
and alias-aware lookup, compile-time realm specialization, `Vec<T>`
constructors, compatibility wrappers, diagnostics, generated-C assertions,
sanitizer coverage, fuzz input, and editor parsing.

## Goal

Runes should support functions that belong to a type but do not require an
existing value:

```runes
pub method Vec<T> {
    flex f new() = outcome: Vec<T> {
        ...
    }

    flex f tnew() = outcome: Result<Vec<T>, AllocationError> {
        ...
    }

    flex f with_capacity(capacity: usize) = outcome: Vec<T> {
        ...
    }
}

dynamic f example() {
    values := Vec<i32>.with_capacity(16)
    values.push(10)
    values.deinit()
}
```

`new` and `with_capacity` are associated methods. `push` is an instance
method. Constructors are only a naming convention: associated methods may
perform parsing, conversion, validation, or any other type-level operation.

The feature must work with:

- ordinary and generic owner types;
- generic associated methods;
- imported and aliased types;
- realm-family types;
- `flex`, concrete-realm, overloaded, and blacklisted declarations;
- `when realm`;
- normal reachability pruning and C name mangling;
- LSP outlines, diagnostics, and editor parsing.

## Foundation used

The parser already accepts functions without `self` inside a `method` block:

```runes
method Vec2 {
    regional f make() = result: Vec2 {
        result = Vec2(x: 0.0, y: 0.0)
    }
}
```

Such declarations were already accepted in method blocks. The implementation
adds the missing type-qualified call path, lookup distinction,
specialization, and lowering.

## Surface design

### Declaration rule

No new `static` keyword is required.

- A function whose first parameter is `self` is an instance method.
- A function with no `self` parameter is an associated method.
- `self` anywhere except the first parameter is a declaration error.

This follows the receiver already present in Runes syntax and keeps realm
modifiers unchanged:

```runes
method Buffer<T> {
    flex f new() = result: Buffer<T> { ... }          -- associated
    gc f managed() = result: Buffer<T> { ... }        -- associated
    f len(self) = result: usize { ... }                -- instance
    flex f push(self: *Buffer<T>, value: T) { ... }    -- instance
}
```

An explicit `static` keyword would repeat information already expressed by
the absence of `self`, add a lexer/editor migration, and create modifier-order
questions without adding semantic power. It may be reconsidered only if
real-world code shows that receiver omission is too easy to miss.

### Call rule

```runes
Clock.now()
Vec<i32>.new()
Vec<i32>.with_capacity(32)
Matrix<f32>.identity<4>()
```

Grammar:

```text
associated-call =
    type-reference "." identifier method-type-arguments? arguments
```

Owner type arguments and method type arguments are separate:

```runes
Container<Key>.convert<Value>(input)
```

For the first implementation:

- generic owner arguments may be explicit (`Box<i32>.new(1)`) or inferred
  from associated-method arguments when unambiguous (`Box.new(1)`);
- `Vec.new<i32>()` is not an alternate spelling;
- inference solely from an assignment or return expectation is deferred, so
  a zero-argument generic constructor normally needs explicit owner arguments;
- imported aliases work: `Vector<i32>.new()`;
- module qualification must preserve canonical declaration identity;
- associated methods cannot be extracted as function values initially;
- importing an associated method independently from its owner remains
  unsupported.

### Lookup and namespace

Type-qualified syntax searches only associated methods on that owner.
Value-qualified syntax searches only instance methods.

Therefore:

```runes
Vec<i32>.push(10)  -- error: push requires a receiver
values.new()       -- error: new is associated with Vec, not an instance
```

In the initial design, an owner cannot declare an associated and an instance
method with the same name. Rejecting the collision gives deterministic
diagnostics and leaves room for future overload rules.

Variant constructors remain constructors:

```runes
Option.Some<i32>(10)
Result.Ok<i32, IoError>(10)
```

They are not reclassified as associated methods. Resolution distinguishes a
variant arm from a type-associated declaration before generic
specialization.

### Visibility and interfaces

- A public `method` block exposes its associated methods under the same
  visibility rules as its instance methods.
- A private owner cannot be used to expose a public associated method.
- Associated methods do not satisfy interface requirements in the first
  implementation. Current interfaces describe operations on values.
- Associated requirements in interfaces need a separate design covering
  interface-qualified calls, witness layout, and generic constraints.

## Realm semantics

An associated method has no receiver owner realm. Its dispatch realm is
therefore the effective execution realm of the call.

```runes
pub method Vec<T> {
    flex f new() = outcome: Vec<T> {
        ...
    }

    flex f tnew() = outcome: Result<Vec<T>, AllocationError> { ... }
}

dynamic f a()  { Vec<i32>.new() }  -- dynamic specialization
regional f b() { Vec<i32>.new() }  -- regional specialization
gc f c()       { Vec<i32>.new() }  -- GC specialization
```

Required rules:

- a `flex` associated method specializes from the caller's effective realm;
- an associated method in a realm overload family selects from the caller's
  effective realm;
- `except` blacklists are checked at the call site;
- `when realm` is pruned at compile time inside the selected specialization;
- a realm-family return type is instantiated in that same effective realm;
- stack calls to owning constructors are rejected through the existing
  storage rules;
- there is no runtime realm tag or realm branch.

After construction, instance methods continue dispatching from the value's
persistent owner realm. Passing the returned value elsewhere must not change
its owner.

## Compiler representation

The implementation reuses the existing field-and-call expression shape, but
the qualifier is an `AST_TYPE_EXPR` rather than a runtime value. This retains
the owner type, owner generic arguments, method name, method generic
arguments, value arguments, source span, and resolved declaration without
adding a nearly identical call node.

Associated status is derived consistently from the declaration: a leading
`self` means instance method; its absence means associated method. Resolution
marks the selected declaration on the field expression, and later phases use
that declaration rather than guessing from the spelling. For bare
non-generic owners, type checking consults the current symbol scope so a local
value shadowing a type name remains a value.

Generated names must include:

- canonical owner identity;
- concrete owner type arguments;
- associated method name;
- method type arguments;
- selected effective realm when specialized.

This prevents collisions with free functions, instance methods, variant
constructors, and identically named types from other modules.

## Implementation milestones

### Milestone 1: freeze syntax and AST

1. Add parser lookahead for `Type<Args>.member(...)` without confusing `<`
   with comparison.
2. Represent the owner as `AST_TYPE_EXPR` in the existing field/call AST.
3. Classify no-`self` method-block functions as associated.
4. Reject `self` in a non-leading parameter position.
5. Preserve existing parsing of value methods, module-qualified calls,
   generic free calls, and variant constructors.

Exit: valid calls have an unambiguous AST and malformed calls produce one
focused diagnostic.

### Milestone 2: declaration indexing and resolution

1. Index both forms under canonical owner identity and filter lookup by
   receiver presence.
2. Resolve imported and aliased owner types before member lookup.
3. Enforce visibility.
4. Diagnose unknown associated methods and static/instance misuse.
5. Reject duplicate associated/instance names on one owner.

Exit: resolution identifies one declaration without using textual module leaf
names as identity.

### Milestone 3: type checking and generic inference

1. Bind generic owner arguments from the type qualifier.
2. Infer method-level generic arguments from value arguments.
3. Substitute both binding sets into parameters and return types.
4. Check argument count and assignability without injecting `self`.
5. Keep ordinary instance-call receiver injection unchanged.
6. Reject omitted generic owner arguments with an actionable fix:
   `write Vec<i32>.new()`.

Exit: associated methods type-check like functions while retaining their
owner's generic context.

### Milestone 4: realm specialization

1. Add associated-method specialization plans to monomorphization.
2. Select `flex` and overload-family behavior from effective execution realm.
3. Instantiate realm-family owner and return types in that realm.
4. Apply blacklists and `when realm` pruning.
5. Reuse specializations only when owner arguments, method arguments, realm,
   and canonical declaration identity all match.

Exit: one source declaration emits distinct dynamic, regional, and GC
specializations with no runtime dispatch.

### Milestone 5: lowering and reachability

1. Emit associated calls without receiver injection or evaluation.
2. Mangle declarations using owner, generics, and realm.
3. Teach reachability to retain only called associated specializations.
4. Preserve C prototypes, declaration ordering, and tuple/type dependencies.
5. Ensure unused associated methods generate no reachable C body.

Exit: generated C compiles under `-Wall -Wextra -Werror` and contains only the
expected specializations.

### Milestone 6: standard-library migration

1. Move `std.vec.new<T>` and `with_capacity<T>` into `method Vec<T>`.
2. Make `Vec<i32>.new()` and `Vec<i32>.with_capacity(16)` canonical.
3. Temporarily keep the module-level functions as compatibility wrappers so
   existing applications do not break in the same compiler change.
4. Update all stdlib examples and tests to the associated spelling.
5. Remove wrappers only in an explicitly announced compatibility milestone.

Exit: application code constructs vectors through the type while old source
continues to compile during migration.

### Milestone 7: tooling and documentation

1. Update the syntax reference and standard-library reference.
2. Update implementation status and the feature matrix.
3. Validate the token/delimiter tree-sitter grammar, highlights, outline
   queries, and indentation fixtures against type-qualified calls. No grammar
   production change is required by the current concrete-syntax grammar.
4. Make LSP document symbols distinguish associated and instance methods where
   the protocol allows it.
5. Keep semantic member hover/completion as a tooling follow-up; the current
   LSP does not yet provide semantic member completion.
6. Add associated calls to the fuzz corpus.

Exit: compiler, docs, and editor tooling agree on the same syntax.

## Testing methodology

### Lexer and parser

Positive cases:

- non-generic `Clock.now()`;
- generic owner `Vec<i32>.new()`;
- generic method `Factory<i32>.convert<u64>(value)`;
- aliased owner `Vector<i32>.new()`;
- module-qualified owner;
- multiline owner arguments and call arguments;
- nested associated calls.

Negative cases:

- `Vec.new()` when no argument can determine the owner type;
- `Vec<i32>.push(value)` for an instance method;
- `values.new()` for an associated method;
- `self` appearing after another parameter;
- unknown/private associated method;
- duplicate associated/instance name;
- malformed `>` or missing `.`/parentheses;
- attempting to use an associated method as a function value.

Parser regression cases must include comparisons such as `a < b`, shifts,
generic free calls, field access, and variant constructors.

### Semantic and generic tests

Test:

- owner generics only;
- method generics only;
- both generic layers together;
- inference from normal, pointer, slice, tuple, and variant arguments;
- return types containing owner parameters;
- two modules with the same owner and method names;
- type aliases preserving nominal and specialization identity;
- correct privacy errors across modules.

Every negative sample must assert a stable diagnostic substring and source
location class rather than merely asserting compilation failure.

### Realm matrix

The same `Vec<i32>.new()` source must be called from:

| Caller | Expected constructor specialization | Expected storage |
|---|---|---|
| stack | compile-time rejection | none |
| dynamic | dynamic | individually released |
| regional | regional | current arena |
| GC | GC | traced storage |
| root `main` | dynamic | individually released |
| specialized `flex` caller | inherited concrete realm | matching backend |

Additional tests:

- associated overload exact variants and fallback;
- blacklisted realm rejection;
- `when realm` branch pruning;
- ordinary constructor result and `tnew` payload passed through a shared
  generic helper;
- returned container retains owner when used from another execution realm;
- nested regional owner rejection remains intact;
- pointer-bearing GC vector survives forced collection.

Generated-C assertions must prove all expected realm symbols exist, forbidden
symbols do not exist, and no runtime realm branch/tag was emitted.

### Runtime and safety

Run constructor and destructor paths under:

- successful allocation;
- forced allocation failure;
- capacity overflow;
- repeated growth;
- empty and non-empty destruction;
- ASan and UBSan;
- leak detection for dynamic allocations;
- forced GC before and after initialized pointer publication;
- regional parent/child arena boundaries.

### Whole-project gates

Before the feature is marked complete:

```text
make test
make test-sanitize
make fuzz-smoke
make test-docs
editors/zed/test.bash
git diff --check
```

The generated compiler binary must be rebuilt if tracked binaries remain part
of branch policy.

## Completion criteria

The feature is complete only when:

- `Vec<i32>.new()` is the documented canonical constructor;
- `Vec<i32>.tnew()` is the documented recoverable constructor;
- associated and instance calls cannot be confused;
- generic owner and method arguments specialize correctly;
- aliases and module identity remain correct;
- dynamic, regional, and GC construction is inferred at compile time;
- stack and blacklist failures are compile-time diagnostics;
- generated C has no runtime realm dispatch;
- old vector constructor calls remain compatible for the migration window;
- compiler, sanitizer, fuzz, documentation, LSP, and Zed tests pass.
