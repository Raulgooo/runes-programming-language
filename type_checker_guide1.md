# Guide: Implementing the Runes Type Checker

The type checker is the heart of the Runes compiler. It transforms the "name-resolved" AST into a "type-annotated" AST (or simply validates that all operations are type-safe before codegen).

## 1. Internal Type Representation ([types.h](file:///home/raul/Desktop/projects/runes/src/types.h))

First, define what a "Type" is in memory. Since Runes is statically typed, every expression must resolve to one of these.

```c
typedef enum {
    TY_PRIMITIVE,
    TY_PTR,
    TY_ARRAY,
    TY_TUPLE,
    TY_FALLIBLE,
    TY_STRUCT,
    TY_VARIANT,
    TY_FUNC,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    union {
        PrimitiveKind primitive; // i32, u64, str, etc.
        struct Type* ptr_to;
        struct { struct Type* elem; uint64_t size; } array;
        struct { struct Type** elems; int count; } tuple;
        struct Type* fallible_inner;
        struct { const char* name; Field* fields; int count; } structure;
        // ...
    } as;
} Type;
```

> [!TIP]
> Use an **Arena** to allocate `Type` objects so you never have to manually [free](file:///home/raul/Desktop/projects/runes/src/parser.c#184-185) them during the compiler pass.

## 2. The Type Checking Pass

The type checker is a recursive traversal of the AST. Its main entry point usually looks like:

```c
Type* check_node(TypeContext* ctx, AstNode* node);
```

### Expression Checking (`check_expr`)
For expressions, the goal is to **infer** the type and return it.
- **Literals**: Easy. `1.0` -> `f64`, `"hi"` -> [str](file:///home/raul/Desktop/projects/runes/src/lexer.c#498-525).
- **Binary Ops**: Check `left` and `right`. If `left` is `i32` and `right` is `i32`, and op is `+`, return `i32`.
- **Calls**: Look up the function signature. Verify each argument type matches the parameter type.

### Statement Checking
Statements don't usually have a type (or return `void`).
- **[var_decl](file:///home/raul/Desktop/projects/runes/src/parser.c#376-471)**: Resolve the [init](file:///home/raul/Desktop/projects/runes/src/lexer.c#7-18) type. If an explicit type was provided, verify it matches the [init](file:///home/raul/Desktop/projects/runes/src/lexer.c#7-18) type.
- **[if_stmt](file:///home/raul/Desktop/projects/runes/src/parser.c#1092-1115)**: Verify `condition` is `bool`. Check both branches.

## 3. Runes-Specific Logic

### A. Memory Strategy (Nesting Rules)
This is unique to Runes. Your `TypeContext` must keep track of the **current function's strategy**.

```c
typedef struct {
    MemoryRealm current_realm;
    // ...
} TypeContext;
```

When you hit an `AST_FUNC_DECL`:
1. Save the previous `current_realm`.
2. Set `ctx->current_realm = node->as.func_decl.realm`.
3. Check if `current_realm` is allowed to be inside the previous realm (refer to the nesting matrix in [specv0_1.md](file:///home/raul/Desktop/projects/runes/docs/specv0_1.md)).
4. Resolve the function body.
5. Restore the previous realm.

### B. Fallible Types (`!T`)
- **`try expr`**:
  1. `inner = check_node(expr)`.
  2. Verify `inner` is `TY_FALLIBLE`.
  3. Return `inner->as.fallible_inner`.
- **[catch](file:///home/raul/Desktop/projects/runes/src/parser.c#1554-1590)**:
  1. Verify the expression returns a `!T`.
  2. The handler must return `T` (or diverge via `return`/`break`).

### C. Pattern Matching
- **Exhaustiveness**: For v0.1, you can just verify that if you match a `Variant`, all arms are covered or there is a `_` wildcard.
- **Bindings**: Ensure `match x { x_val -> ... }` defines `x_val` in the arm's scope with the type of `x`.

## 4. Unification and Substitutions

When comparing two types (e.g., assignment `x = y`), don't just use `==`. Use a `type_equals` or `unify` function.

```c
bool type_equals(Type* a, Type* b) {
    if (a->kind != b->kind) return false;
    // ... recursive check for arrays/pointers ...
}
```

## 5. Implementation Roadmap

1. **Phase 1: Primitives**: Get `i32`, `f32`, `bool`, and [str](file:///home/raul/Desktop/projects/runes/src/lexer.c#498-525) working with basic binary expressions.
2. **Phase 2: Variables & Scopes**: Integrate with the `SymbolTable`. Store `Type*` in the `Symbol` struct.
3. **Phase 3: Functions**: Check parameter signatures and return values.
4. **Phase 4: Compound Types**: Pointers and Arrays.
5. **Phase 5: User Types**: Structs and Variants.
6. **Phase 6: The "Gold": Memory strategies and Fallibles.**
