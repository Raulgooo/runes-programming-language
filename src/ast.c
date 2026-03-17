#include "ast.h"
#include "utils/arena.h"
#include <string.h>

static AstNode *ast_alloc(Arena *arena, AstKind kind) {
  AstNode *n = arena_alloc(arena, sizeof(AstNode));
  n->kind = kind;
  n->next = NULL;
  n->line = 0; // Will be set by parser
  n->col = 0;
  return n;
}

// ── Root ─────────────────────────────────────────────────────────────

AstNode *ast_new_program(Arena *arena, AstNode *declarations) {
  AstNode *n = ast_alloc(arena, AST_PROGRAM);
  n->as.program.declarations = declarations;
  return n;
}

// ── Declarations ─────────────────────────────────────────────────────

AstNode *ast_new_func_decl(Arena *arena, MemoryRealm realm, bool is_pub,
                           bool is_main, const char *name, AstNode *params,
                           const char *ret_name, AstNode *ret_type,
                           AstNode *body, Attr *attrs) {
  AstNode *n = ast_alloc(arena, AST_FUNC_DECL);
  n->as.func_decl.realm = realm;
  n->as.func_decl.is_pub = is_pub;
  n->as.func_decl.is_main = is_main;
  n->as.func_decl.name = arena_strdup(arena, name);
  n->as.func_decl.params = params;
  n->as.func_decl.ret_name = ret_name ? arena_strdup(arena, ret_name) : NULL;
  n->as.func_decl.ret_type = ret_type;
  n->as.func_decl.body = body;
  n->as.func_decl.attrs = attrs;
  return n;
}

AstNode *ast_new_var_decl(Arena *arena, bool is_const, bool is_volatile,
                          const char *name, AstNode *type, AstNode *init) {
  AstNode *n = ast_alloc(arena, AST_VAR_DECL);
  n->as.var_decl.is_const = is_const;
  n->as.var_decl.is_volatile = is_volatile;
  n->as.var_decl.name = arena_strdup(arena, name);
  n->as.var_decl.type = type;
  n->as.var_decl.init = init;
  return n;
}

AstNode *ast_new_type_decl(Arena *arena, bool is_pub, const char *name,
                           AstNode *fields, Attr *attrs) {
  AstNode *n = ast_alloc(arena, AST_TYPE_DECL);
  n->as.type_decl.is_pub = is_pub;
  n->as.type_decl.name = arena_strdup(arena, name);
  n->as.type_decl.fields = fields;
  n->as.type_decl.attrs = attrs;
  return n;
}

AstNode *ast_new_variant_decl(Arena *arena, bool is_pub, const char *name,
                              AstNode *arms) {
  AstNode *n = ast_alloc(arena, AST_VARIANT_DECL);
  n->as.variant_decl.is_pub = is_pub;
  n->as.variant_decl.name = arena_strdup(arena, name);
  n->as.variant_decl.arms = arms;
  return n;
}

AstNode *ast_new_variant_arm(Arena *arena, const char *name, AstNode *fields) {
  AstNode *n = ast_alloc(arena, AST_VARIANT_ARM);
  n->as.variant_arm.name = arena_strdup(arena, name);
  n->as.variant_arm.fields = fields;
  return n;
}

AstNode *ast_new_schema_decl(Arena *arena, bool is_pub, const char *name,
                             const char *parent, AstNode *fields) {
  AstNode *n = ast_alloc(arena, AST_SCHEMA_DECL);
  n->as.schema_decl.is_pub = is_pub;
  n->as.schema_decl.name = arena_strdup(arena, name);
  n->as.schema_decl.parent = parent ? arena_strdup(arena, parent) : NULL;
  n->as.schema_decl.fields = fields;
  return n;
}

AstNode *ast_new_field_decl(Arena *arena, bool is_volatile, const char *name,
                            AstNode *type, AstNode *default_val, Attr *attrs) {
  AstNode *n = ast_alloc(arena, AST_FIELD_DECL);
  n->as.field_decl.is_volatile = is_volatile;
  n->as.field_decl.name = arena_strdup(arena, name);
  n->as.field_decl.type = type;
  n->as.field_decl.default_val = default_val;
  n->as.field_decl.attrs = attrs;
  return n;
}

AstNode *ast_new_method_decl(Arena *arena, bool is_pub, const char *type_name,
                             const char *iface_name, AstNode *methods) {
  AstNode *n = ast_alloc(arena, AST_METHOD_DECL);
  n->as.method_decl.is_pub = is_pub;
  n->as.method_decl.type_name = arena_strdup(arena, type_name);
  n->as.method_decl.iface_name =
      iface_name ? arena_strdup(arena, iface_name) : NULL;
  n->as.method_decl.methods = methods;
  return n;
}

AstNode *ast_new_interface_decl(Arena *arena, bool is_pub, const char *name,
                                AstNode *methods) {
  AstNode *n = ast_alloc(arena, AST_INTERFACE_DECL);
  n->as.interface_decl.is_pub = is_pub;
  n->as.interface_decl.name = arena_strdup(arena, name);
  n->as.interface_decl.methods = methods;
  return n;
}

AstNode *ast_new_error_decl(Arena *arena, bool is_pub, const char *name,
                            AstNode *variants) {
  AstNode *n = ast_alloc(arena, AST_ERROR_DECL);
  n->as.error_decl.is_pub = is_pub;
  n->as.error_decl.name = arena_strdup(arena, name);
  n->as.error_decl.variants = variants;
  return n;
}

AstNode *ast_new_mod_decl(Arena *arena, bool is_pub, const char *name,
                          AstNode *declarations) {
  AstNode *n = ast_alloc(arena, AST_MOD_DECL);
  n->as.mod_decl.is_pub = is_pub;
  n->as.mod_decl.name = arena_strdup(arena, name);
  n->as.mod_decl.declarations = declarations;
  return n;
}

AstNode *ast_new_use_decl(Arena *arena, AstNode *path) {
  AstNode *n = ast_alloc(arena, AST_USE_DECL);
  n->as.use_decl.path = path;
  return n;
}

AstNode *ast_new_extern_decl(Arena *arena, bool is_func, const char *name,
                             AstNode *params, const char *ret_name,
                             AstNode *ret_type, AstNode *var_type) {
  AstNode *n = ast_alloc(arena, AST_EXTERN_DECL);
  n->as.extern_decl.is_func = is_func;
  n->as.extern_decl.name = arena_strdup(arena, name);
  n->as.extern_decl.params = params;
  n->as.extern_decl.ret_name = ret_name ? arena_strdup(arena, ret_name) : NULL;
  n->as.extern_decl.ret_type = ret_type;
  n->as.extern_decl.var_type = var_type;
  return n;
}

AstNode *ast_new_param(Arena *arena, const char *name, AstNode *type) {
  AstNode *n = ast_alloc(arena, AST_PARAM);
  n->as.param.name = arena_strdup(arena, name);
  n->as.param.type = type;
  return n;
}

// ── Statements ───────────────────────────────────────────────────────

AstNode *ast_new_block(Arena *arena, AstNode *statements) {
  AstNode *n = ast_alloc(arena, AST_BLOCK);
  n->as.block.statements = statements;
  return n;
}

AstNode *ast_new_return_stmt(Arena *arena, AstNode *value) {
  AstNode *n = ast_alloc(arena, AST_RETURN_STMT);
  n->as.return_stmt.value = value;
  return n;
}

AstNode *ast_new_if_stmt(Arena *arena, AstNode *condition, AstNode *then_branch,
                         AstNode *else_branch) {
  AstNode *n = ast_alloc(arena, AST_IF_STMT);
  n->as.if_stmt.condition = condition;
  n->as.if_stmt.then_branch = then_branch;
  n->as.if_stmt.else_branch = else_branch;
  return n;
}

AstNode *ast_new_while_stmt(Arena *arena, AstNode *condition, AstNode *body) {
  AstNode *n = ast_alloc(arena, AST_WHILE_STMT);
  n->as.while_stmt.condition = condition;
  n->as.while_stmt.body = body;
  return n;
}

AstNode *ast_new_for_stmt(Arena *arena, AstNode *iter, CaptureKind cap_kind,
                          const char *cap_value, const char *cap_index,
                          AstNode *body) {
  AstNode *n = ast_alloc(arena, AST_FOR_STMT);
  n->as.for_stmt.iter = iter;
  n->as.for_stmt.cap_kind = cap_kind;
  n->as.for_stmt.cap_value = arena_strdup(arena, cap_value);
  n->as.for_stmt.cap_index = cap_index ? arena_strdup(arena, cap_index) : NULL;
  n->as.for_stmt.body = body;
  return n;
}

AstNode *ast_new_loop_stmt(Arena *arena, AstNode *body) {
  AstNode *n = ast_alloc(arena, AST_LOOP_STMT);
  n->as.loop_stmt.body = body;
  return n;
}

AstNode *ast_new_match_stmt(Arena *arena, AstNode *subject, AstNode *arms) {
  AstNode *n = ast_alloc(arena, AST_MATCH_STMT);
  n->as.match_stmt.subject = subject;
  n->as.match_stmt.arms = arms;
  return n;
}

AstNode *ast_new_match_arm(Arena *arena, AstNode *pattern, AstNode *guard,
                           AstNode *body) {
  AstNode *n = ast_alloc(arena, AST_MATCH_ARM);
  n->as.match_arm.pattern = pattern;
  n->as.match_arm.guard = guard;
  n->as.match_arm.body = body;
  return n;
}

AstNode *ast_new_unsafe_block(Arena *arena, AstNode *body) {
  AstNode *n = ast_alloc(arena, AST_UNSAFE_BLOCK);
  n->as.unsafe_block.body = body;
  return n;
}

AstNode *ast_new_break(Arena *arena) { return ast_alloc(arena, AST_BREAK_STMT); }

AstNode *ast_new_continue(Arena *arena) {
  return ast_alloc(arena, AST_CONTINUE_STMT);
}

// ── Literals ─────────────────────────────────────────────────────────

AstNode *ast_new_int_literal(Arena *arena, long long value) {
  AstNode *n = ast_alloc(arena, AST_INT_LITERAL);
  n->as.int_literal.value = value;
  return n;
}

AstNode *ast_new_float_literal(Arena *arena, double value) {
  AstNode *n = ast_alloc(arena, AST_FLOAT_LITERAL);
  n->as.float_literal.value = value;
  return n;
}

AstNode *ast_new_string_literal(Arena *arena, const char *value) {
  AstNode *n = ast_alloc(arena, AST_STRING_LITERAL);
  n->as.string_literal.value = arena_strdup(arena, value);
  return n;
}

AstNode *ast_new_bool_literal(Arena *arena, bool value) {
  AstNode *n = ast_alloc(arena, AST_BOOL_LITERAL);
  n->as.bool_literal.value = value;
  return n;
}

AstNode *ast_new_char_literal(Arena *arena, uint32_t codepoint) {
  AstNode *n = ast_alloc(arena, AST_CHAR_LITERAL);
  n->as.char_literal.codepoint = codepoint;
  return n;
}

AstNode *ast_new_array_literal(Arena *arena, AstNode *elems) {
  AstNode *n = ast_alloc(arena, AST_ARRAY_LITERAL);
  n->as.array_literal.elems = elems;
  return n;
}

AstNode *ast_new_tuple_expr(Arena *arena, AstNode *elems) {
  AstNode *n = ast_alloc(arena, AST_TUPLE_EXPR);
  n->as.tuple_expr.elems = elems;
  return n;
}

// ── Expressions ──────────────────────────────────────────────────────

AstNode *ast_new_identifier(Arena *arena, const char *name) {
  AstNode *n = ast_alloc(arena, AST_IDENTIFIER);
  n->as.identifier.name = arena_strdup(arena, name);
  return n;
}

AstNode *ast_new_range_expr(Arena *arena, AstNode *start, AstNode *end,
                            bool inclusive) {
  AstNode *n = ast_alloc(arena, AST_RANGE_EXPR);
  n->as.range_expr.start = start;
  n->as.range_expr.end = end;
  n->as.range_expr.inclusive = inclusive;
  return n;
}

AstNode *ast_new_binary(Arena *arena, TokenKind op, AstNode *left,
                        AstNode *right) {
  AstNode *n = ast_alloc(arena, AST_BINARY_EXPR);
  n->as.binary.op = op;
  n->as.binary.left = left;
  n->as.binary.right = right;
  return n;
}

AstNode *ast_new_unary(Arena *arena, TokenKind op, AstNode *expr) {
  AstNode *n = ast_alloc(arena, AST_UNARY_EXPR);
  n->as.unary.op = op;
  n->as.unary.expr = expr;
  return n;
}

AstNode *ast_new_assign(Arena *arena, AstNode *target, AstNode *value) {
  AstNode *n = ast_alloc(arena, AST_ASSIGN);
  n->as.assign.target = target;
  n->as.assign.value = value;
  return n;
}

AstNode *ast_new_call(Arena *arena, AstNode *callee, AstNode *args) {
  AstNode *n = ast_alloc(arena, AST_CALL_EXPR);
  n->as.call.callee = callee;
  n->as.call.args = args;
  return n;
}

AstNode *ast_new_index(Arena *arena, AstNode *target, AstNode *index) {
  AstNode *n = ast_alloc(arena, AST_INDEX_EXPR);
  n->as.index.target = target;
  n->as.index.index = index;
  return n;
}

AstNode *ast_new_field(Arena *arena, AstNode *target, const char *field) {
  AstNode *n = ast_alloc(arena, AST_FIELD_EXPR);
  n->as.field.target = target;
  n->as.field.field = arena_strdup(arena, field);
  return n;
}

AstNode *ast_new_cast(Arena *arena, CastKind kind, AstNode *expr,
                      AstNode *target_type) {
  AstNode *n = ast_alloc(arena, AST_CAST_EXPR);
  n->as.cast.kind = kind;
  n->as.cast.expr = expr;
  n->as.cast.target_type = target_type;
  return n;
}

AstNode *ast_new_promote(Arena *arena, AstNode *expr, MemoryRealm target) {
  AstNode *n = ast_alloc(arena, AST_PROMOTE_EXPR);
  n->as.promote.expr = expr;
  n->as.promote.target = target;
  return n;
}

AstNode *ast_new_try_expr(Arena *arena, AstNode *expr) {
  AstNode *n = ast_alloc(arena, AST_TRY_EXPR);
  n->as.try_expr.expr = expr;
  return n;
}

AstNode *ast_new_catch_expr(Arena *arena, AstNode *expr, const char *err_name,
                            AstNode *handler) {
  AstNode *n = ast_alloc(arena, AST_CATCH_EXPR);
  n->as.catch_expr.expr = expr;
  n->as.catch_expr.err_name = err_name ? arena_strdup(arena, err_name) : NULL;
  n->as.catch_expr.handler = handler;
  return n;
}

AstNode *ast_new_error_expr(Arena *arena, AstNode *path) {
  AstNode *n = ast_alloc(arena, AST_ERROR_EXPR);
  n->as.error_expr.path = path;
  return n;
}

AstNode *ast_new_asm_expr(Arena *arena, const char *code, const char *output) {
  AstNode *n = ast_alloc(arena, AST_ASM_EXPR);
  n->as.asm_expr.code = arena_strdup(arena, code);
  n->as.asm_expr.output = output ? arena_strdup(arena, output) : NULL;
  return n;
}

AstNode *ast_new_named_arg(Arena *arena, const char *name, AstNode *value) {
  AstNode *n = ast_alloc(arena, AST_NAMED_ARG);
  n->as.named_arg.name = arena_strdup(arena, name);
  n->as.named_arg.value = value;
  return n;
}

AstNode *ast_new_volatile_expr(Arena *arena, AstNode *expr) {
  AstNode *n = ast_alloc(arena, AST_VOLATILE_EXPR);
  n->as.volatile_expr.expr = expr;
  return n;
}

// ── Type Expressions ──────────────────────────────────────────────────────

AstNode *ast_new_type_named(Arena *arena, const char *name) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_NAMED;
  n->as.type_expr.name = arena_strdup(arena, name);
  n->as.type_expr.module = NULL;
  return n;
}

AstNode *ast_new_type_qualified(Arena *arena, const char *module, const char *name) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_QUALIFIED;
  n->as.type_expr.module = arena_strdup(arena, module);
  n->as.type_expr.name = arena_strdup(arena, name);
  return n;
}

AstNode *ast_new_type_ptr(Arena *arena, AstNode *inner) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_PTR;
  n->as.type_expr.inner = inner;
  return n;
}

AstNode *ast_new_type_array(Arena *arena, AstNode *size, AstNode *elem_type) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_ARRAY;
  n->as.type_expr.size = size;
  n->as.type_expr.inner = elem_type;
  return n;
}

AstNode *ast_new_type_fallible(Arena *arena, AstNode *inner) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_FALLIBLE;
  n->as.type_expr.inner = inner;
  return n;
}

AstNode *ast_new_type_tuple(Arena *arena, AstNode *elems) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_TUPLE;
  n->as.type_expr.elems = elems;
  return n;
}

AstNode *ast_new_type_sl(Arena *arena) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_SL;
  return n;
}

AstNode *ast_new_type_dl(Arena *arena) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_DL;
  return n;
}

AstNode *ast_new_type_j(Arena *arena) {
  AstNode *n = ast_alloc(arena, AST_TYPE_EXPR);
  n->as.type_expr.kind = TYPE_J;
  return n;
}

// ── Attributes ───────────────────────────────────────────────────────

Attr *attr_new(Arena *arena, const char *name, AstNode *arg) {
  Attr *a = arena_alloc(arena, sizeof(Attr));
  a->name = arena_strdup(arena, name);
  a->arg = arg;
  a->next = NULL;
  return a;
}