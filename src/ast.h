#ifndef RUNES_AST_H
#define RUNES_AST_H

#include "lexer.h"
#include "utils/arena.h"
#include <stdbool.h>
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
// Memory Realms
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
  REALM_STACK, // f / stack f
  REALM_ARENA, // regional f
  REALM_HEAP,  // dynamic f
  REALM_GC,    // gc f
  REALM_FLEX,  // flex f — inherits caller's realm
  REALM_MAIN,  // f main() — unrestricted orchestrator
} MemoryRealm;

// Nesting legality matrix (enforced by the type checker, not the parser).
//
//  outer\inner  STACK  ARENA  HEAP  GC   FLEX
//  MAIN          ✅     ✅     ✅    ✅    ✅
//  STACK         ✅     ❌     ❌    ❌    ❌
//  ARENA         ✅     ❌     ❌    ❌    ❌
//  HEAP          ✅     ✅     ✅    ✅    ✅
//  GC            ✅     ❌     ❌    ✅    ❌
//  FLEX          inherits caller — resolved at call site

// ─────────────────────────────────────────────────────────────────────────────
// Attributes  (#[packed], #[align(N)], #[section("...")], etc.)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Attr {
  const char *name;    // "packed" | "align" | "repr" | "section"
                       // "link_name" | "callconv" | "interrupt"
                       // "json" | "json_skip"
  struct AstNode *arg; // NULL if attribute takes no argument
  struct Attr *next;
} Attr;

// ─────────────────────────────────────────────────────────────────────────────
// Supporting enums
// ─────────────────────────────────────────────────────────────────────────────

// Type expression kinds
typedef enum {
  TYPE_NAMED, // i32, u64, str, bool, char, usize — primitive or user-defined
  TYPE_QUALIFIED, // module.Type — qualified type name
  TYPE_PTR,       // *T
  TYPE_ARRAY,     // [N]T
  TYPE_SL,        // sl — singly linked list (element type erased in v0.1)
  TYPE_DL,        // dl — doubly linked list (element type erased in v0.1)
  TYPE_FALLIBLE,  // !T — can fail; inner == NULL means !void
  TYPE_TUPLE,     // (T, U, ...) — multiple returns
  TYPE_J,         // J — built-in JSON type
} TypeKind;

// for-loop capture forms
typedef enum {
  CAPTURE_VALUE,   // |n|
  CAPTURE_PTR,     // |*n|   — mutate in place
  CAPTURE_INDEXED, // |n, i| — value + index
} CaptureKind;

// as-expression disambiguation — resolved at parse time
typedef enum {
  CAST_RAW,    // 0xFF as *u32     — raw bitcast / pointer cast
  CAST_TO_J,   // val as J         — serialize to JSON
  CAST_FROM_J, // j as Point       — deserialize from JSON
} CastKind;

// ─────────────────────────────────────────────────────────────────────────────
// Node kinds
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
  // root
  AST_PROGRAM,

  // declarations
  AST_FUNC_DECL,
  AST_VAR_DECL,
  AST_TYPE_DECL,
  AST_VARIANT_DECL,
  AST_VARIANT_ARM,
  AST_SCHEMA_DECL,
  AST_FIELD_DECL,
  AST_METHOD_DECL,
  AST_INTERFACE_DECL,
  AST_ERROR_DECL,
  AST_MOD_DECL,
  AST_USE_DECL,
  AST_EXTERN_DECL,
  AST_PARAM,

  // statements
  AST_BLOCK,
  AST_RETURN_STMT,
  AST_IF_STMT,
  AST_WHILE_STMT,
  AST_FOR_STMT,
  AST_LOOP_STMT,
  AST_BREAK_STMT,    // no payload
  AST_CONTINUE_STMT, // no payload
  AST_MATCH_STMT,
  AST_MATCH_ARM,
  AST_UNSAFE_BLOCK,

  // expressions — literals
  AST_INT_LITERAL,
  AST_FLOAT_LITERAL,
  AST_STRING_LITERAL,
  AST_BOOL_LITERAL,
  AST_CHAR_LITERAL,
  AST_ARRAY_LITERAL,
  AST_TUPLE_EXPR,

  // expressions — operations
  AST_IDENTIFIER,
  AST_BINARY_EXPR,
  AST_UNARY_EXPR,
  AST_ASSIGN,
  AST_CALL_EXPR,
  AST_INDEX_EXPR,
  AST_FIELD_EXPR,
  AST_RANGE_EXPR,

  // expressions — memory / type system
  AST_CAST_EXPR,
  AST_PROMOTE_EXPR,
  AST_SIZEOF_EXPR,
  AST_ALIGNOF_EXPR,
  AST_TYPE_EXPR,

  // expressions — error handling
  AST_TRY_EXPR,
  AST_CATCH_EXPR,
  AST_ERROR_EXPR,

  // expressions — low level
  AST_ASM_EXPR,
  AST_VOLATILE_EXPR,

  // expressions — JSON (v0.1 placeholder; methods resolved as AST_CALL_EXPR)
  AST_JSON_EXPR,

  AST_NAMED_ARG,
  AST_TUPLE_DESTRUCTURE, // x, y, z = tuple_expr
  AST_STRUCT_PATTERN,    // Vec2(x: 0.0, y) - struct destructuring pattern
  AST_FIELD_PATTERN,     // x: 0.0 or x: binding - field in struct pattern
} AstKind;

// ─────────────────────────────────────────────────────────────────────────────
// Universal node
// ─────────────────────────────────────────────────────────────────────────────
//
// AstNode *next enables intrusive linked lists for:
//   - statements inside a block
//   - parameters in a function signature
//   - arms in match / variant / error
//   - fields in a type / schema
//   - arguments in a call
//   - segments in a use path
//
// All string fields point into the arena — no separate heap allocation.

typedef struct AstNode {
  AstKind kind;
  uint32_t line;
  uint32_t col;
  struct AstNode
      *next; // intrusive list — next sibling in whatever list owns this node

  union {

    // ── Root ─────────────────────────────────────────────────────────────

    struct {
      struct AstNode *declarations; // linked list of top-level decls
    } program;

    // ── Declarations ─────────────────────────────────────────────────────

    // regional f alloc_buf(size: u64) = ptr: *u8 { ... }
    // pub f kernel_main() { ... }
    struct {
      MemoryRealm realm;
      bool is_pub;
      bool is_main; // f main() — exempt from nesting rules
      const char *name;
      struct AstNode *params;   // linked list of AST_PARAM; NULL if ()
      const char *ret_name;     // NULL if void
      struct AstNode *ret_type; // NULL if void (AST_TYPE_EXPR)
      struct AstNode *body;     // AST_BLOCK; NULL for interface signatures
      Attr *attrs;              // #[section], #[interrupt], #[callconv], etc.
    } func_decl;

    // i32 x = 5   /   const i32 MAX = 512   /   volatile *u32 uart = ...
    struct {
      bool is_const;
      bool is_volatile;
      const char *name;
      struct AstNode *type; // AST_TYPE_EXPR; NULL if inferred
      struct AstNode *init; // initializer expression; NULL if not present
      Attr *attrs;          // #[section], #[align], etc.
    } var_decl;

    // type Vec2 = { x: f32, y: f32 }
    struct {
      bool is_pub;
      const char *name;
      struct AstNode *fields; // linked list of AST_FIELD_DECL
      Attr *attrs;            // #[packed], #[align], #[repr]
    } type_decl;

    // type Color = | Red | Green | RGB(u8, u8, u8)
    struct {
      bool is_pub;
      const char *name;
      struct AstNode *arms; // linked list of AST_VARIANT_ARM
    } variant_decl;

    // Red | RGB(u8, u8, u8) | Hex(str)
    struct {
      const char *name;
      struct AstNode
          *fields; // linked list of AST_TYPE_EXPR; NULL if unit variant
    } variant_arm;

    // schema RedShoe : Shoe = { color: str = "red", ... }
    struct {
      bool is_pub;
      const char *name;
      const char *parent;     // NULL if no inheritance
      struct AstNode *fields; // linked list of AST_FIELD_DECL
    } schema_decl;

    // name: T = default  (inside type / schema)
    // volatile data: u8  (inside type with volatile qualifier)
    struct {
      bool is_volatile;
      const char *name;
      struct AstNode *type;        // AST_TYPE_EXPR
      struct AstNode *default_val; // NULL if no default
      Attr *attrs;                 // #[json("key")], #[json_skip]
    } field_decl;

    // method Vec2 { }   /   method Drawable for Vec2 { }
    struct {
      bool is_pub;
      const char *type_name;   // the type being extended
      const char *iface_name;  // NULL unless "for <iface>" form
      struct AstNode *methods; // linked list of AST_FUNC_DECL
    } method_decl;

    // interface Drawable { f draw(self) ... }
    struct {
      bool is_pub;
      const char *name;
      struct AstNode
          *methods; // linked list of AST_FUNC_DECL (signatures, body == NULL)
    } interface_decl;

    // error MathError = { | DivByZero | Overflow }
    struct {
      bool is_pub;
      const char *name;
      struct AstNode *variants; // linked list of AST_IDENTIFIER (variant names)
    } error_decl;

    // mod kernel { ... }
    struct {
      bool is_pub;
      const char *name;
      struct AstNode *declarations; // linked list
    } mod_decl;

    // use kernel.arch.x86.read_cr3
    struct {
      struct AstNode *path; // linked list of AST_IDENTIFIER segments
    } use_decl;

    // extern f memset(ptr: *u8, val: i32, len: usize)
    // extern f memcmp(a: *u8, b: *u8, len: usize) = r: i32
    // extern u64 KERNEL_START
    struct {
      bool is_func;
      const char *name;
      struct AstNode *params;   // linked list of AST_PARAM; NULL if variable
      const char *ret_name;     // NULL if void or variable
      struct AstNode *ret_type; // NULL if void or variable (AST_TYPE_EXPR)
      struct AstNode *var_type; // NULL if func (AST_TYPE_EXPR)
    } extern_decl;

    // x: i32
    struct {
      const char *name;
      struct AstNode *type; // AST_TYPE_EXPR
    } param;

    // ── Statements ───────────────────────────────────────────────────────

    struct {
      struct AstNode *statements; // linked list
    } block;

    // explicit early return — used inside catch handlers
    struct {
      struct AstNode *value; // NULL for bare return
    } return_stmt;

    // if x > 0 { ... } else if ... { ... } else { ... }
    // also used as expression: str s = if cond { "a" } else { "b" }
    struct {
      struct AstNode *condition;
      struct AstNode *then_branch; // AST_BLOCK or single expression
      struct AstNode *else_branch; // AST_BLOCK | AST_IF_STMT | NULL
    } if_stmt;

    struct {
      struct AstNode *condition;
      struct AstNode *body; // AST_BLOCK
    } while_stmt;

    // for (iter) |capture| { body }
    struct {
      struct AstNode *iter; // AST_RANGE_EXPR | AST_IDENTIFIER | AST_FIELD_EXPR
      CaptureKind cap_kind;
      const char *cap_value; // main capture name (always present)
      const char *cap_index; // only for CAPTURE_INDEXED; NULL otherwise
      struct AstNode *body;  // AST_BLOCK
    } for_stmt;

    struct {
      struct AstNode *body; // AST_BLOCK
    } loop_stmt;

    // match color { Red -> ..., _ -> ... }
    // also used as expression: str s = match color { Red -> "red", _ -> "?" }
    struct {
      struct AstNode *subject;
      struct AstNode *arms; // linked list of AST_MATCH_ARM
    } match_stmt;

    // Red -> print("red")   /   n if n < 0 -> print("neg")
    struct {
      struct AstNode *pattern; // reuses existing node types:
                               //   AST_IDENTIFIER        → binding or wildcard
                               //   (_) AST_CALL_EXPR         → Variant(a, b, c)
                               //   AST_INT/FLOAT/STR_LIT → literal pattern
                               //   AST_FIELD_EXPR        → struct destructure
      struct AstNode *guard;   // NULL unless "pat if expr" guard form
      struct AstNode *body;    // expression or AST_BLOCK
    } match_arm;

    struct {
      struct AstNode *body; // AST_BLOCK
    } unsafe_block;

    // AST_BREAK_STMT and AST_CONTINUE_STMT carry no payload

    // ── Literals ─────────────────────────────────────────────────────────

    struct {
      unsigned long long value;
    } int_literal;
    struct {
      double value;
    } float_literal;
    struct {
      const char *value;
    } string_literal; // arena-interned, UTF-8
    struct {
      bool value;
    } bool_literal;
    struct {
      uint32_t codepoint;
    } char_literal; // Unicode codepoint

    // [1, 2, 3]  /  [] (zero-initialized)
    struct {
      struct AstNode *elems; // linked list of expressions; NULL means []
    } array_literal;

    // (ast, errs) — multiple return values
    struct {
      struct AstNode *elems; // linked list of expressions
    } tuple_expr;

    // ── Expressions ──────────────────────────────────────────────────────

    struct {
      const char *name;
    } identifier;

    // 0..10  /  0..=10
    struct {
      struct AstNode *start;
      struct AstNode *end;
      bool inclusive;
    } range_expr;

    // a + b  /  a == b  /  a & b
    struct {
      TokenKind op;
      struct AstNode *left;
      struct AstNode *right;
    } binary;

    // !x  /  -x  /  *ptr  /  &val
    struct {
      TokenKind op;
      struct AstNode *expr;
    } unary;

    // x = 5  /  self.entries[idx] = paddr | flags
    struct {
      struct AstNode *target;
      struct AstNode *value;
    } assign;

    // foo(a, b)  /  PageTable.new()  /  d.draw()
    struct {
      struct AstNode *callee; // AST_IDENTIFIER | AST_FIELD_EXPR
      struct AstNode *args;   // linked list of argument expressions
    } call;

    // arr[0]
    struct {
      struct AstNode *target;
      struct AstNode *index;
    } index;

    // foo.bar  /  self.entries
    struct {
      struct AstNode *target;
      const char *field;
    } field;

    // val as *u32  /  val as J  /  j as Point
    // promote has its own node — see below
    struct {
      CastKind kind;
      struct AstNode *expr;
      struct AstNode *target_type; // AST_TYPE_EXPR
    } cast;

    // promote(&t) as dynamic  /  promote(&t) as gc
    struct {
      struct AstNode *expr;
      MemoryRealm target; // only REALM_HEAP or REALM_GC are valid
    } promote;

    // sizeof(T) - compile-time size of type
    struct {
      struct AstNode *type; // AST_TYPE_EXPR
    } sizeof_expr;

    // alignof(T) - compile-time alignment of type
    struct {
      struct AstNode *type; // AST_TYPE_EXPR
    } alignof_expr;

    // try foo()
    struct {
      struct AstNode *expr;
    } try_expr;

    // foo() catch |e| { ... }  /  foo() catch 0.0
    struct {
      struct AstNode *expr;
      const char *err_name;    // NULL for catch <default> form
      struct AstNode *handler; // AST_BLOCK or default-value expression
    } catch_expr;

    // error.MathError.DivByZero
    struct {
      struct AstNode *path; // linked list of AST_IDENTIFIER
    } error_expr;

    // asm { "cli; hlt" }  /  asm { "mov %cr3, %rax" } -> r
    struct {
      const char *code;   // the raw assembly string
      const char *output; // bound output register name; NULL if no -> binding
    } asm_expr;

    // x: 1.0 (inside call)
    struct {
      const char *name;
      struct AstNode *value;
    } named_arg;

    // Tuple destructuring: x, y, z = tuple_expr
    struct {
      struct AstNode *targets; // linked list of AST_VAR_DECL (without init)
      struct AstNode *init;    // the tuple expression being destructured
    } tuple_destructure;

    // Struct pattern: Vec2(x: 0.0, y) or Vec2(x, y: 0.0)
    struct {
      const char *name;       // struct name (e.g., "Vec2")
      struct AstNode *fields; // linked list of AST_FIELD_PATTERN
    } struct_pattern;

    // Field pattern: x: 0.0 (literal) or x: binding (identifier) or just x
    // (shorthand)
    struct {
      const char *name;        // field name (e.g., "x")
      struct AstNode *pattern; // NULL for shorthand (x means x: x), otherwise
                               // literal or identifier
    } field_pattern;

    // volatile pointer read/write expression
    struct {
      struct AstNode *expr;
    } volatile_expr;

    // *u64 | i32 | [N]T | !T | !void | (T, U) | sl | dl | J | module.Type
    struct {
      TypeKind kind;
      const char
          *name; // TYPE_NAMED: type name (may be primitive or user-defined)
                 // TYPE_QUALIFIED: type name (e.g., "Task")
      const char *module;    // TYPE_QUALIFIED: module name (e.g., "scheduler")
      struct AstNode *inner; // TYPE_PTR    → pointee type
                             // TYPE_ARRAY  → element type
                             // TYPE_FALLIBLE → inner type (NULL = !void)
      struct AstNode *size; // TYPE_ARRAY  → size expression (AST_INT_LITERAL or
                            // AST_IDENTIFIER)
      struct AstNode *elems; // TYPE_TUPLE  → linked list of AST_TYPE_EXPR
    } type_expr;

    // AST_JSON_EXPR — placeholder for v0.1.
    // j.string() / j.pretty() / j.get() / j.set() / j.has()
    // are parsed as AST_CALL_EXPR(AST_FIELD_EXPR(...)) — no special node
    // needed. This kind is reserved for future compiler-generated JSON
    // intrinsics.

  } as;
} AstNode;

// ─────────────────────────────────────────────────────────────────────────────
// Constructors
// All strings are interned into the arena by the constructor.
// All returned pointers are valid for the lifetime of the arena.
// ─────────────────────────────────────────────────────────────────────────────

// root
AstNode *ast_new_program(Arena *arena, AstNode *declarations);

// declarations
AstNode *ast_new_func_decl(Arena *arena, MemoryRealm realm, bool is_pub,
                           bool is_main, const char *name, AstNode *params,
                           const char *ret_name, AstNode *ret_type,
                           AstNode *body, Attr *attrs);
AstNode *ast_new_var_decl(Arena *arena, bool is_const, bool is_volatile,
                          const char *name, AstNode *type, AstNode *init,
                          Attr *attrs);
AstNode *ast_new_type_decl(Arena *arena, bool is_pub, const char *name,
                           AstNode *fields, Attr *attrs);
AstNode *ast_new_variant_decl(Arena *arena, bool is_pub, const char *name,
                              AstNode *arms);
AstNode *ast_new_variant_arm(Arena *arena, const char *name, AstNode *fields);
AstNode *ast_new_schema_decl(Arena *arena, bool is_pub, const char *name,
                             const char *parent, AstNode *fields);
AstNode *ast_new_field_decl(Arena *arena, bool is_volatile, const char *name,
                            AstNode *type, AstNode *default_val, Attr *attrs);
AstNode *ast_new_method_decl(Arena *arena, bool is_pub, const char *type_name,
                             const char *iface_name, AstNode *methods);
AstNode *ast_new_interface_decl(Arena *arena, bool is_pub, const char *name,
                                AstNode *methods);
AstNode *ast_new_error_decl(Arena *arena, bool is_pub, const char *name,
                            AstNode *variants);
AstNode *ast_new_mod_decl(Arena *arena, bool is_pub, const char *name,
                          AstNode *declarations);
AstNode *ast_new_use_decl(Arena *arena, AstNode *path);
AstNode *ast_new_extern_decl(Arena *arena, bool is_func, const char *name,
                             AstNode *params, const char *ret_name,
                             AstNode *ret_type, AstNode *var_type);
AstNode *ast_new_param(Arena *arena, const char *name, AstNode *type);

// statements
AstNode *ast_new_block(Arena *arena, AstNode *statements);
AstNode *ast_new_return_stmt(Arena *arena, AstNode *value);
AstNode *ast_new_if_stmt(Arena *arena, AstNode *condition, AstNode *then_branch,
                         AstNode *else_branch);
AstNode *ast_new_while_stmt(Arena *arena, AstNode *condition, AstNode *body);
AstNode *ast_new_for_stmt(Arena *arena, AstNode *iter, CaptureKind cap_kind,
                          const char *cap_value, const char *cap_index,
                          AstNode *body);
AstNode *ast_new_loop_stmt(Arena *arena, AstNode *body);
AstNode *ast_new_match_stmt(Arena *arena, AstNode *subject, AstNode *arms);
AstNode *ast_new_match_arm(Arena *arena, AstNode *pattern, AstNode *guard,
                           AstNode *body);
AstNode *ast_new_unsafe_block(Arena *arena, AstNode *body);
AstNode *ast_new_break(Arena *arena);
AstNode *ast_new_continue(Arena *arena);

// literals
AstNode *ast_new_int_literal(Arena *arena, long long value);
AstNode *ast_new_float_literal(Arena *arena, double value);
AstNode *ast_new_string_literal(Arena *arena, const char *value);
AstNode *ast_new_bool_literal(Arena *arena, bool value);
AstNode *ast_new_char_literal(Arena *arena, uint32_t codepoint);
AstNode *ast_new_array_literal(Arena *arena, AstNode *elems);
AstNode *ast_new_tuple_expr(Arena *arena, AstNode *elems);

// expressions
AstNode *ast_new_identifier(Arena *arena, const char *name);
AstNode *ast_new_range_expr(Arena *arena, AstNode *start, AstNode *end,
                            bool inclusive);
AstNode *ast_new_binary(Arena *arena, TokenKind op, AstNode *left,
                        AstNode *right);
AstNode *ast_new_unary(Arena *arena, TokenKind op, AstNode *expr);
AstNode *ast_new_assign(Arena *arena, AstNode *target, AstNode *value);
AstNode *ast_new_call(Arena *arena, AstNode *callee, AstNode *args);
AstNode *ast_new_index(Arena *arena, AstNode *target, AstNode *index);
AstNode *ast_new_field(Arena *arena, AstNode *target, const char *field);
AstNode *ast_new_cast(Arena *arena, CastKind kind, AstNode *expr,
                      AstNode *target_type);
AstNode *ast_new_promote(Arena *arena, AstNode *expr, MemoryRealm target);
AstNode *ast_new_sizeof(Arena *arena, AstNode *type);
AstNode *ast_new_alignof(Arena *arena, AstNode *type);
AstNode *ast_new_try_expr(Arena *arena, AstNode *expr);
AstNode *ast_new_catch_expr(Arena *arena, AstNode *expr, const char *err_name,
                            AstNode *handler);
AstNode *ast_new_error_expr(Arena *arena, AstNode *path);
AstNode *ast_new_asm_expr(Arena *arena, const char *code, const char *output);
AstNode *ast_new_named_arg(Arena *arena, const char *name, AstNode *value);
AstNode *ast_new_tuple_destructure(Arena *arena, AstNode *targets,
                                   AstNode *init);
AstNode *ast_new_struct_pattern(Arena *arena, const char *name,
                                AstNode *fields);
AstNode *ast_new_field_pattern(Arena *arena, const char *name,
                               AstNode *pattern);
AstNode *ast_new_volatile_expr(Arena *arena, AstNode *expr);

// type expressions
AstNode *ast_new_type_named(Arena *arena, const char *name);
AstNode *ast_new_type_qualified(Arena *arena, const char *module,
                                const char *name);
AstNode *ast_new_type_ptr(Arena *arena, AstNode *inner);
AstNode *ast_new_type_array(Arena *arena, AstNode *size, AstNode *elem_type);
AstNode *ast_new_type_fallible(Arena *arena,
                               AstNode *inner); // inner == NULL → !void
AstNode *ast_new_type_tuple(Arena *arena, AstNode *elems);
AstNode *ast_new_type_sl(Arena *arena);
AstNode *ast_new_type_dl(Arena *arena);
AstNode *ast_new_type_j(Arena *arena);

// attributes
Attr *attr_new(Arena *arena, const char *name, AstNode *arg);

// ─────────────────────────────────────────────────────────────────────────────
// Pretty Print
// ─────────────────────────────────────────────────────────────────────────────

void ast_print(AstNode *node);
void ast_print_ext(AstNode *node, int indent);

#endif // RUNES_AST_H