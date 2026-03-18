#include "resolver.h"
#include <stdio.h>
#include <string.h>

static void resolve_node(Resolver *r, AstNode *node);

static void error(Resolver *r, uint32_t line, uint32_t col, const char *fmt,
                  const char *arg) {
  fprintf(stderr, "Error at %u:%u: ", line, col);
  fprintf(stderr, fmt, arg);
  fprintf(stderr, "\n");
  r->error_count++;
  r->had_error = true;
}

static void define_symbol(Resolver *r, AstNode *node, const char *name,
                          SymbolKind kind, bool is_pub) {
  if (!name)
    return;
  Symbol sym = {.name = name, .kind = kind, .node = node, .is_pub = is_pub};
  if (!symbol_table_define(r->st, sym)) {
    error(r, node->line, node->col, "duplicate declaration of '%s'", name);
  }
}

static void resolve_list(Resolver *r, AstNode *node) {
  while (node) {
    resolve_node(r, node);
    node = node->next;
  }
}

// Pass 1: Collect top-level declarations
static void collect_decls(Resolver *r, AstNode *node) {
  while (node) {
    switch (node->kind) {
    case AST_FUNC_DECL:
      define_symbol(r, node, node->as.func_decl.name, SYM_FUNC,
                    node->as.func_decl.is_pub);
      break;
    case AST_VAR_DECL:
      define_symbol(r, node, node->as.var_decl.name, SYM_VAR, false);
      break;
    case AST_TYPE_DECL:
      define_symbol(r, node, node->as.type_decl.name, SYM_TYPE,
                    node->as.type_decl.is_pub);
      break;
    case AST_VARIANT_DECL:
      define_symbol(r, node, node->as.variant_decl.name, SYM_TYPE,
                    node->as.variant_decl.is_pub);
      break;
    case AST_SCHEMA_DECL:
      define_symbol(r, node, node->as.schema_decl.name, SYM_TYPE,
                    node->as.schema_decl.is_pub);
      break;
    case AST_INTERFACE_DECL:
      define_symbol(r, node, node->as.interface_decl.name, SYM_TYPE,
                    node->as.interface_decl.is_pub);
      break;
    case AST_ERROR_DECL:
      define_symbol(r, node, node->as.error_decl.name, SYM_TYPE,
                    node->as.error_decl.is_pub);
      break;
    case AST_EXTERN_DECL: {
      SymbolKind kind = node->as.extern_decl.is_func ? SYM_FUNC : SYM_VAR;
      define_symbol(r, node, node->as.extern_decl.name, kind, true);
      break;
    }
    case AST_MOD_DECL:
      define_symbol(r, node, node->as.mod_decl.name, SYM_MOD,
                    node->as.mod_decl.is_pub);
      break;
    default:
      break;
    }
    node = node->next;
  }
}

static void resolve_node(Resolver *r, AstNode *node) {
  if (!node)
    return;

  switch (node->kind) {
  case AST_PROGRAM:
    collect_decls(r, node->as.program.declarations);
    resolve_list(r, node->as.program.declarations);
    break;

  case AST_FUNC_DECL:
    if (symbol_table_lookup(r->st, node->as.func_decl.name) == NULL) {
      define_symbol(r, node, node->as.func_decl.name, SYM_FUNC,
                    node->as.func_decl.is_pub);
    }
    symbol_table_push(r->st);
    resolve_list(r, node->as.func_decl.params);
    if (node->as.func_decl.ret_name) {
      define_symbol(r, node, node->as.func_decl.ret_name, SYM_VAR, false);
    }
    if (node->as.func_decl.body) {
      resolve_node(r, node->as.func_decl.body);
    }
    symbol_table_pop(r->st);
    break;

  case AST_PARAM:
    define_symbol(r, node, node->as.param.name, SYM_VAR, false);
    resolve_node(r, node->as.param.type);
    break;

  case AST_VAR_DECL:
    if (node->as.var_decl.init) {
      resolve_node(r, node->as.var_decl.init);
    }
    if (symbol_table_lookup(r->st, node->as.var_decl.name) == NULL) {
      define_symbol(r, node, node->as.var_decl.name, SYM_VAR, false);
    }
    if (node->as.var_decl.type) {
      resolve_node(r, node->as.var_decl.type);
    }
    break;

  case AST_BLOCK:
    symbol_table_push(r->st);
    resolve_list(r, node->as.block.statements);
    symbol_table_pop(r->st);
    break;

  case AST_IDENTIFIER:
    if (symbol_table_lookup(r->st, node->as.identifier.name) == NULL) {
      error(r, node->line, node->col, "undefined identifier '%s'",
            node->as.identifier.name);
    }
    break;

  case AST_BINARY_EXPR:
    resolve_node(r, node->as.binary.left);
    resolve_node(r, node->as.binary.right);
    break;

  case AST_UNARY_EXPR:
    resolve_node(r, node->as.unary.expr);
    break;

  case AST_CALL_EXPR:
    resolve_node(r, node->as.call.callee);
    resolve_list(r, node->as.call.args);
    break;

  case AST_IF_STMT:
    symbol_table_push(r->st);
    resolve_node(r, node->as.if_stmt.condition);
    resolve_node(r, node->as.if_stmt.then_branch);
    if (node->as.if_stmt.else_branch) {
      resolve_node(r, node->as.if_stmt.else_branch);
    }
    symbol_table_pop(r->st);
    break;

  case AST_WHILE_STMT:
    symbol_table_push(r->st);
    resolve_node(r, node->as.while_stmt.condition);
    resolve_node(r, node->as.while_stmt.body);
    symbol_table_pop(r->st);
    break;

  case AST_FOR_STMT:
    resolve_node(r, node->as.for_stmt.iter);
    symbol_table_push(r->st);
    if (node->as.for_stmt.cap_value) {
      define_symbol(r, node, node->as.for_stmt.cap_value, SYM_VAR, false);
    }
    if (node->as.for_stmt.cap_index) {
      define_symbol(r, node, node->as.for_stmt.cap_index, SYM_VAR, false);
    }
    resolve_node(r, node->as.for_stmt.body);
    symbol_table_pop(r->st);
    break;

  case AST_LOOP_STMT:
    resolve_node(r, node->as.loop_stmt.body);
    break;

  case AST_RETURN_STMT:
    if (node->as.return_stmt.value) {
      resolve_node(r, node->as.return_stmt.value);
    }
    break;

  case AST_ASSIGN:
    resolve_node(r, node->as.assign.target);
    resolve_node(r, node->as.assign.value);
    break;

  case AST_FIELD_EXPR:
    resolve_node(r, node->as.field.target);
    break;

  case AST_INDEX_EXPR:
    resolve_node(r, node->as.index.target);
    resolve_node(r, node->as.index.index);
    break;

  case AST_TYPE_EXPR:
    if (node->as.type_expr.kind == TYPE_NAMED) {
      // Skip primitive types
    } else if (node->as.type_expr.kind == TYPE_TUPLE) {
      resolve_list(r, node->as.type_expr.elems);
    } else if (node->as.type_expr.inner) {
      resolve_node(r, node->as.type_expr.inner);
    }
    break;

  case AST_TUPLE_EXPR:
    resolve_list(r, node->as.tuple_expr.elems);
    break;

  case AST_TUPLE_DESTRUCTURE:
    resolve_node(r, node->as.tuple_destructure.init);
    resolve_list(r, node->as.tuple_destructure.targets);
    break;

  default:
    break;
  }
}

void resolver_init(Resolver *r, SymbolTable *st) {
  r->st = st;
  r->error_count = 0;
  r->had_error = false;
}

void resolver_resolve(Resolver *r, AstNode *node) { resolve_node(r, node); }
