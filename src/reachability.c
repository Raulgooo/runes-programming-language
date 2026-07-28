#include "reachability.h"
#include <string.h>

static void visit_node(AstNode *node);

static void visit_list(AstNode *node) {
  for (; node; node = node->next)
    visit_node(node);
}

static void mark_decl(AstNode *decl) {
  if (!decl || decl->codegen_reachable)
    return;
  decl->codegen_reachable = true;

  switch (decl->kind) {
  case AST_FUNC_DECL:
    visit_list(decl->as.func_decl.params);
    visit_node(decl->as.func_decl.body);
    break;
  case AST_EXTERN_DECL:
    visit_list(decl->as.extern_decl.params);
    break;
  case AST_VAR_DECL:
    visit_node(decl->as.var_decl.init);
    break;
  default:
    break;
  }
}

static void visit_node(AstNode *node) {
  if (!node)
    return;
  if (node->resolved_decl)
    mark_decl(node->resolved_decl);

  switch (node->kind) {
  case AST_PROGRAM:
    visit_list(node->as.program.declarations);
    break;
  case AST_BLOCK:
    visit_list(node->as.block.statements);
    break;
  case AST_VAR_DECL:
    visit_node(node->as.var_decl.init);
    break;
  case AST_TUPLE_DESTRUCTURE:
    visit_list(node->as.tuple_destructure.targets);
    visit_node(node->as.tuple_destructure.init);
    break;
  case AST_RETURN_STMT:
    visit_node(node->as.return_stmt.value);
    break;
  case AST_DEFER_STMT:
    visit_node(node->as.defer_stmt.expression);
    break;
  case AST_IF_STMT:
    visit_node(node->as.if_stmt.condition);
    visit_node(node->as.if_stmt.then_branch);
    visit_node(node->as.if_stmt.else_branch);
    break;
  case AST_WHILE_STMT:
    visit_node(node->as.while_stmt.condition);
    visit_node(node->as.while_stmt.body);
    break;
  case AST_FOR_STMT:
    visit_node(node->as.for_stmt.iter);
    visit_node(node->as.for_stmt.body);
    break;
  case AST_LOOP_STMT:
    visit_node(node->as.loop_stmt.body);
    break;
  case AST_MATCH_STMT:
    visit_node(node->as.match_stmt.subject);
    visit_list(node->as.match_stmt.arms);
    break;
  case AST_MATCH_ARM:
    visit_node(node->as.match_arm.pattern);
    visit_node(node->as.match_arm.guard);
    visit_node(node->as.match_arm.body);
    break;
  case AST_UNSAFE_BLOCK:
    visit_node(node->as.unsafe_block.body);
    break;
  case AST_REALM_BLOCK:
    break;
  case AST_ARRAY_LITERAL:
    visit_list(node->as.array_literal.elems);
    break;
  case AST_TUPLE_EXPR:
    visit_list(node->as.tuple_expr.elems);
    break;
  case AST_RANGE_EXPR:
    visit_node(node->as.range_expr.start);
    visit_node(node->as.range_expr.end);
    break;
  case AST_BINARY_EXPR:
    visit_node(node->as.binary.left);
    visit_node(node->as.binary.right);
    break;
  case AST_UNARY_EXPR:
    visit_node(node->as.unary.expr);
    break;
  case AST_ASSIGN:
    visit_node(node->as.assign.target);
    visit_node(node->as.assign.value);
    break;
  case AST_CALL_EXPR:
    visit_node(node->as.call.callee);
    visit_list(node->as.call.args);
    break;
  case AST_INDEX_EXPR:
    visit_node(node->as.index.target);
    visit_node(node->as.index.index);
    break;
  case AST_FIELD_EXPR:
    visit_node(node->as.field.target);
    break;
  case AST_CAST_EXPR:
    visit_node(node->as.cast.expr);
    break;
  case AST_PROMOTE_EXPR:
    visit_node(node->as.promote.expr);
    break;
  case AST_TRY_EXPR:
    visit_node(node->as.try_expr.expr);
    break;
  case AST_CATCH_EXPR:
    visit_node(node->as.catch_expr.expr);
    visit_node(node->as.catch_expr.handler);
    break;
  case AST_NAMED_ARG:
    visit_node(node->as.named_arg.value);
    break;
  case AST_STRUCT_PATTERN:
    visit_list(node->as.struct_pattern.fields);
    break;
  case AST_FIELD_PATTERN:
    visit_node(node->as.field_pattern.pattern);
    break;
  case AST_VOLATILE_EXPR:
    visit_node(node->as.volatile_expr.expr);
    break;
  default:
    break;
  }
}

static bool contains_main(AstNode *decl) {
  for (; decl; decl = decl->next) {
    if (decl->kind == AST_FUNC_DECL &&
        strcmp(decl->as.func_decl.name, "main") == 0)
      return true;
    if (decl->kind == AST_MOD_DECL &&
        contains_main(decl->as.mod_decl.declarations))
      return true;
  }
  return false;
}

static void mark_roots(AstNode *decl, bool executable) {
  for (; decl; decl = decl->next) {
    switch (decl->kind) {
    case AST_FUNC_DECL:
      if ((executable && strcmp(decl->as.func_decl.name, "main") == 0) ||
          (!executable && decl->as.func_decl.is_pub))
        mark_decl(decl);
      break;
    case AST_VAR_DECL:
      /* Module globals retain eager initialization and side-effect semantics. */
      mark_decl(decl);
      break;
    case AST_ASSIGN:
      visit_node(decl);
      break;
    case AST_METHOD_DECL:
      /* Interface dispatch remains conservative until whole-program devirt. */
      for (AstNode *method = decl->as.method_decl.methods; method;
           method = method->next)
        mark_decl(method);
      break;
    case AST_MOD_DECL:
      mark_roots(decl->as.mod_decl.declarations, executable);
      break;
    default:
      break;
    }
  }
}

void reachability_mark(AstNode *program) {
  if (!program || program->kind != AST_PROGRAM)
    return;
  bool executable = contains_main(program->as.program.declarations);
  mark_roots(program->as.program.declarations, executable);
}
