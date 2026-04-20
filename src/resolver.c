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
  Symbol *existing = symbol_table_lookup_local(r->st, name);
  if (existing) {
    if (existing->kind == kind) {
      return;
    }
    error(r, node->line, node->col, "duplicate declaration of '%s'", name);
    return;
  }
  Symbol sym = {.name = name, .kind = kind, .node = node, .is_pub = is_pub};
  symbol_table_define(r->st, sym);
}

static void resolve_list(Resolver *r, AstNode *node) {
  while (node) {
    resolve_node(r, node);
    node = node->next;
  }
}

static void resolve_pattern(Resolver *r, AstNode *node) {
  if (!node)
    return;
  switch (node->kind) {
  case AST_IDENTIFIER:
    if (strcmp(node->as.identifier.name, "_") != 0) {
      define_symbol(r, node, node->as.identifier.name, SYM_VAR, false);
    }
    break;
  case AST_CALL_EXPR:
    // Variant patterns: resolve_node(callee) and resolve_pattern on args
    // resolve_node on callee is needed if it's a reference to a variant
    // but in many cases it's just a name.
    // For now, let's treat args as patterns.
    {
      AstNode *arg = node->as.call.args;
      while (arg) {
        resolve_pattern(r, arg);
        arg = arg->next;
      }
    }
    break;
  case AST_STRUCT_PATTERN: {
    AstNode *field = node->as.struct_pattern.fields;
    while (field) {
      resolve_pattern(r, field);
      field = field->next;
    }
    break;
  }
  case AST_FIELD_PATTERN:
    if (node->as.field_pattern.pattern) {
      resolve_pattern(r, node->as.field_pattern.pattern);
    } else {
      // shorthand: field name is the binding
      define_symbol(r, node, node->as.field_pattern.name, SYM_VAR, false);
    }
    break;
  case AST_TUPLE_EXPR: {
    AstNode *elem = node->as.tuple_expr.elems;
    while (elem) {
      resolve_pattern(r, elem);
      elem = elem->next;
    }
    break;
  }
  default:
    // literals, etc.
    break;
  }
}

static const char *get_node_name(AstNode *node) {
  if (!node)
    return NULL;
  switch (node->kind) {
  case AST_FUNC_DECL:
    return node->as.func_decl.name;
  case AST_VAR_DECL:
    return node->as.var_decl.name;
  case AST_TYPE_DECL:
    return node->as.type_decl.name;
  case AST_VARIANT_DECL:
    return node->as.variant_decl.name;
  case AST_SCHEMA_DECL:
    return node->as.schema_decl.name;
  case AST_INTERFACE_DECL:
    return node->as.interface_decl.name;
  case AST_ERROR_DECL:
    return node->as.error_decl.name;
  case AST_MOD_DECL:
    return node->as.mod_decl.name;
  case AST_EXTERN_DECL:
    return node->as.extern_decl.name;
  default:
    return NULL;
  }
}

static SymbolKind get_node_sym_kind(AstNode *node) {
  switch (node->kind) {
  case AST_FUNC_DECL:
    return SYM_FUNC;
  case AST_VAR_DECL:
    return SYM_VAR;
  case AST_TYPE_DECL:
  case AST_VARIANT_DECL:
  case AST_SCHEMA_DECL:
  case AST_INTERFACE_DECL:
  case AST_ERROR_DECL:
    return SYM_TYPE;
  case AST_MOD_DECL:
    return SYM_MOD;
  case AST_EXTERN_DECL:
    return node->as.extern_decl.is_func ? SYM_FUNC : SYM_VAR;
  default:
    return SYM_VAR;
  }
}

static Symbol *find_symbol_in_path(Resolver *r, AstNode *path) {
  if (!path)
    return NULL;
  Symbol *current = symbol_table_lookup(r->st, path->as.identifier.name);
  if (!current)
    return NULL;

  AstNode *seg = path->next;
  while (seg) {
    if (current->kind != SYM_MOD)
      return NULL;
    AstNode *decl = current->node->as.mod_decl.declarations;
    bool found = false;
    while (decl) {
      const char *name = get_node_name(decl);
      if (name && strcmp(name, seg->as.identifier.name) == 0) {
        // Arena-allocated symbol for module path lookup
        Symbol *tmp = arena_alloc(r->st->arena, sizeof(Symbol));
        tmp->name = name;
        tmp->kind = get_node_sym_kind(decl);
        tmp->node = decl;
        tmp->is_pub = true;
        current = tmp;
        found = true;
        break;
      }
      decl = decl->next;
    }
    if (!found)
      return NULL;
    seg = seg->next;
  }
  return current;
}

// Pass 1: Collect top-level declarations
static void collect_decls(Resolver *r, AstNode *node) {
  AstNode *head = node;

  // First pass: define all non-use symbols
  while (node) {
    switch (node->kind) {
    case AST_FUNC_DECL:
      define_symbol(r, node, node->as.func_decl.name, SYM_FUNC,
                    node->as.func_decl.is_pub);
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
    case AST_VAR_DECL:
      define_symbol(r, node, node->as.var_decl.name, SYM_VAR, false);
      break;
    case AST_MOD_DECL:
      define_symbol(r, node, node->as.mod_decl.name, SYM_MOD,
                    node->as.mod_decl.is_pub);
      break;
    default:
      break;
    }
    node = node->next;
  }

  // Second pass: handle use declarations now that all symbols are defined
  node = head;
  while (node) {
    if (node->kind == AST_USE_DECL) {
      Symbol *target = find_symbol_in_path(r, node->as.use_decl.path);
      if (target) {
        AstNode *last = node->as.use_decl.path;
        while (last->next)
          last = last->next;
        const char *alias = last->as.identifier.name;
        // Only define if not already present in this local scope
        // (to avoid conflicts with local mod definitions in the same file)
        if (!symbol_table_lookup_local(r->st, alias)) {
          define_symbol(r, node, alias, target->kind, false);
        }
      } else {
        error(r, node->line, node->col, "could not resolve module path", NULL);
      }
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
    // Functions are already collected by collect_decls, so we don't
    // need to define them again here unless they are anonymous or
    // special, but for now they are all named and collected.
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
    // Only define if not already present (handled by collect_decls for
    // top-level/mod-level)
    if (!symbol_table_lookup_local(r->st, node->as.var_decl.name)) {
      define_symbol(r, node, node->as.var_decl.name, SYM_VAR, false);
    }
    if (node->as.var_decl.type) {
      resolve_node(r, node->as.var_decl.type);
    }
    break;

  case AST_BLOCK:
    symbol_table_push(r->st);
    collect_decls(r, node->as.block.statements);
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
    resolve_node(r, node->as.if_stmt.condition);
    resolve_node(r, node->as.if_stmt.then_branch);
    if (node->as.if_stmt.else_branch) {
      resolve_node(r, node->as.if_stmt.else_branch);
    }
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

  case AST_METHOD_DECL:
    resolve_list(r, node->as.method_decl.methods);
    break;

  case AST_INTERFACE_DECL:
    resolve_list(r, node->as.interface_decl.methods);
    break;

  case AST_MATCH_STMT:
    resolve_node(r, node->as.match_stmt.subject);
    resolve_list(r, node->as.match_stmt.arms);
    break;

  case AST_MATCH_ARM:
    symbol_table_push(r->st);
    resolve_pattern(r, node->as.match_arm.pattern);
    if (node->as.match_arm.guard) {
      resolve_node(r, node->as.match_arm.guard);
    }
    resolve_node(r, node->as.match_arm.body);
    symbol_table_pop(r->st);
    break;

  case AST_TRY_EXPR:
    resolve_node(r, node->as.try_expr.expr);
    break;

  case AST_CATCH_EXPR:
    resolve_node(r, node->as.catch_expr.expr);
    symbol_table_push(r->st);
    if (node->as.catch_expr.err_name) {
      define_symbol(r, node, node->as.catch_expr.err_name, SYM_VAR, false);
    }
    resolve_node(r, node->as.catch_expr.handler);
    symbol_table_pop(r->st);
    break;

  case AST_PROMOTE_EXPR:
    resolve_node(r, node->as.promote.expr);
    break;

  case AST_UNSAFE_BLOCK:
    resolve_node(r, node->as.unsafe_block.body);
    break;

  case AST_ASM_EXPR:
    // No-op as requested
    break;

  case AST_TYPE_DECL:
    resolve_list(r, node->as.type_decl.fields);
    break;

  case AST_VARIANT_DECL:
    resolve_list(r, node->as.variant_decl.arms);
    break;

  case AST_VARIANT_ARM:
    resolve_list(r, node->as.variant_arm.fields);
    break;

  case AST_SCHEMA_DECL:
    resolve_list(r, node->as.schema_decl.fields);
    break;

  case AST_FIELD_DECL:
    resolve_node(r, node->as.field_decl.type);
    if (node->as.field_decl.default_val) {
      resolve_node(r, node->as.field_decl.default_val);
    }
    break;

  case AST_ERROR_DECL:
    // Variants are currently just names (AST_IDENTIFIER in a list)
    // but we can traverse them to be safe.
    // However, do NOT resolve them as uses here, because they are definitions.
    // resolve_list(r, node->as.error_decl.variants);
    break;

  case AST_MOD_DECL:
    // Mod declarations define a name in parent, but contents are resolved later
    // collect_decls should be called for inner declarations.
    symbol_table_push(r->st);
    collect_decls(r, node->as.mod_decl.declarations);
    resolve_list(r, node->as.mod_decl.declarations);
    symbol_table_pop(r->st);
    break;

  case AST_USE_DECL:
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
