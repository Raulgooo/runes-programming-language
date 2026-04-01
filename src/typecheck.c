#include "typecheck.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void typechecker_init(TypeChecker *tc, Arena *arena, TypeContext *tctx,
                      SymbolTable *st) {
  tc->arena = arena;
  tc->tctx = tctx;
  tc->st = st;
  tc->error_count = 0;
  tc->had_error = false;
  tc->expected_ret = NULL;
  tc->current_realm = REALM_MAIN;
  tc->loop_depth = 0;
}

void typechecker_error(TypeChecker *tc, uint32_t line, uint32_t col,
                       const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "[Type Error] %u:%u: ", line, col);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);

  tc->error_count++;
  tc->had_error = true;
}

Type *typechecker_resolve_type_expr(TypeChecker *tc, AstNode *node) {
  if (!node)
    return tc->tctx->type_void;

  if (node->kind == AST_TYPE_EXPR) {
    switch (node->as.type_expr.kind) {
    case TYPE_NAMED: {
      const char *name = node->as.type_expr.name;
      if (strcmp(name, "i8") == 0)
        return tc->tctx->type_i8;
      if (strcmp(name, "i16") == 0)
        return tc->tctx->type_i16;
      if (strcmp(name, "i32") == 0)
        return tc->tctx->type_i32;
      if (strcmp(name, "i64") == 0)
        return tc->tctx->type_i64;
      if (strcmp(name, "u8") == 0)
        return tc->tctx->type_u8;
      if (strcmp(name, "u16") == 0)
        return tc->tctx->type_u16;
      if (strcmp(name, "u32") == 0)
        return tc->tctx->type_u32;
      if (strcmp(name, "u64") == 0)
        return tc->tctx->type_u64;
      if (strcmp(name, "f32") == 0)
        return tc->tctx->type_f32;
      if (strcmp(name, "f64") == 0)
        return tc->tctx->type_f64;
      if (strcmp(name, "bool") == 0)
        return tc->tctx->type_bool;
      if (strcmp(name, "str") == 0)
        return tc->tctx->type_str;
      if (strcmp(name, "char") == 0)
        return tc->tctx->type_char;
      if (strcmp(name, "usize") == 0)
        return tc->tctx->type_usize;
      if (strcmp(name, "void") == 0)
        return tc->tctx->type_void;

      Symbol *sym = symbol_table_lookup(tc->st, name);
      if (sym && sym->type)
        return sym->type;

      return tc->tctx->type_unknown;
    }
    case TYPE_PTR:
      return type_new_pointer(tc->tctx, typechecker_resolve_type_expr(
                                            tc, node->as.type_expr.inner));
    case TYPE_ARRAY: {
      size_t size = 0;
      if (node->as.type_expr.size &&
          node->as.type_expr.size->kind == AST_INT_LITERAL) {
        size = node->as.type_expr.size->as.int_literal.value;
      }
      return type_new_array(
          tc->tctx, typechecker_resolve_type_expr(tc, node->as.type_expr.inner),
          size);
    }
    case TYPE_FALLIBLE:
      return type_new_fallible(tc->tctx, typechecker_resolve_type_expr(
                                             tc, node->as.type_expr.inner));
    default:
      break;
    }
  }
  return tc->tctx->type_unknown;
}

static void typechecker_collect_decls(TypeChecker *tc, AstNode *decl) {
  while (decl) {
    if (decl->kind == AST_FUNC_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.func_decl.name);
      if (!sym) {
        Symbol new_sym = {0};
        new_sym.name = decl->as.func_decl.name;
        new_sym.kind = SYM_FUNC;
        new_sym.node = decl;
        symbol_table_define(tc->st, new_sym);
        sym = symbol_table_lookup_local(tc->st, decl->as.func_decl.name);
      }
      if (sym) {
        Type *ret_t =
            typechecker_resolve_type_expr(tc, decl->as.func_decl.ret_type);

        AstNode *param = decl->as.func_decl.params;
        int param_c = 0;
        while (param) {
          param_c++;
          param = param->next;
        }

        Type **ptypes = NULL;
        if (param_c > 0) {
          ptypes = arena_alloc(tc->arena, sizeof(Type *) * param_c);
          param = decl->as.func_decl.params;
          for (int i = 0; i < param_c; i++) {
            ptypes[i] = typechecker_resolve_type_expr(tc, param->as.param.type);
            param = param->next;
          }
        }
        sym->type =
            type_new_function(tc->tctx, ptypes, param_c, ret_t, STRATEGY_STACK);
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_VAR_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.var_decl.name);
      if (!sym) {
        Symbol new_sym = {0};
        new_sym.name = decl->as.var_decl.name;
        new_sym.kind = SYM_VAR;
        new_sym.node = decl;
        symbol_table_define(tc->st, new_sym);
        sym = symbol_table_lookup_local(tc->st, decl->as.var_decl.name);
      }
      if (sym && decl->as.var_decl.type) {
        sym->type = typechecker_resolve_type_expr(tc, decl->as.var_decl.type);
        decl->resolved_type = sym->type;
      }
    }
    decl = decl->next;
  }
}

Type *typechecker_infer_expr(TypeChecker *tc, AstNode *expr) {
  if (!expr)
    return tc->tctx->type_unknown;
  if (expr->resolved_type)
    return expr->resolved_type;

  Type *inferred = tc->tctx->type_unknown;

  switch (expr->kind) {
  case AST_INT_LITERAL:
    inferred = tc->tctx->type_i32;
    break;
  case AST_FLOAT_LITERAL:
    inferred = tc->tctx->type_f64;
    break;
  case AST_STRING_LITERAL:
    inferred = tc->tctx->type_str;
    break;
  case AST_ARRAY_LITERAL: {
    // Basic array literal inference: if empty, unknown inner?
    // In Runes, literals are often used in VAR_DECL which provides the type.
    // For now, return a generic array type or unknown.
    inferred = tc->tctx->type_unknown;
    break;
  }
  case AST_BOOL_LITERAL:
    inferred = tc->tctx->type_bool;
    break;
  case AST_CHAR_LITERAL:
    inferred = tc->tctx->type_char;
    break;

  case AST_IDENTIFIER: {
    Symbol *sym = symbol_table_lookup(tc->st, expr->as.identifier.name);
    if (sym && sym->type) {
      inferred = sym->type;
    } else if (sym && !sym->type) {
      inferred = tc->tctx->type_unknown;
    } else {
      typechecker_error(tc, expr->line, expr->col, "Undefined variable '%s'",
                        expr->as.identifier.name);
    }
    break;
  }

  case AST_BINARY_EXPR: {
    Type *lty = typechecker_infer_expr(tc, expr->as.binary.left);
    Type *rty = typechecker_infer_expr(tc, expr->as.binary.right);

    switch (expr->as.binary.op) {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
      if (lty->kind != TY_UNKNOWN && rty->kind != TY_UNKNOWN) {
        if (!type_is_assignable(lty, rty) && !type_is_assignable(rty, lty)) {
          typechecker_error(tc, expr->line, expr->col,
                            "Type mismatch in arithmetic operation");
        }
      }
      inferred = lty;
      break;
    case TOKEN_EQ_EQ:
    case TOKEN_BANG_EQ:
    case TOKEN_LT:
    case TOKEN_GT:
    case TOKEN_LT_EQ:
    case TOKEN_GT_EQ:
      if (lty->kind != TY_UNKNOWN && rty->kind != TY_UNKNOWN) {
        if (!type_is_comparable(lty, rty)) {
          typechecker_error(tc, expr->line, expr->col,
                            "Comparison type mismatch");
        }
      }
      inferred = tc->tctx->type_bool;
      break;
    case TOKEN_AND:
    case TOKEN_OR:
      if (lty->kind != TY_UNKNOWN && rty->kind != TY_UNKNOWN) {
        if (lty->kind != TY_PRIMITIVE ||
            strcmp(lty->as.primitive.name, "bool") != 0 ||
            rty->kind != TY_PRIMITIVE ||
            strcmp(rty->as.primitive.name, "bool") != 0) {
          typechecker_error(tc, expr->line, expr->col,
                            "Logical operators require boolean operands");
        }
      }
      inferred = tc->tctx->type_bool;
      break;
    default:
      break;
    }
    break;
  }

  case AST_ASSIGN: {
    Type *lty = typechecker_infer_expr(tc, expr->as.assign.target);
    Type *rty = typechecker_infer_expr(tc, expr->as.assign.value);
    if (lty->kind != TY_UNKNOWN && rty->kind != TY_UNKNOWN) {
      if (!type_is_assignable(lty, rty)) {
        printf("DEBUG ASSERT: Cannot assign %d to %d\n", rty->kind, lty->kind);
        if (lty->kind == TY_PRIMITIVE)
          printf("DEBUG: lty name: %s\n", lty->as.primitive.name);
        if (rty->kind == TY_PRIMITIVE)
          printf("DEBUG: rty name: %s\n", rty->as.primitive.name);
        typechecker_error(tc, expr->line, expr->col,
                          "Cannot assign value of mismatched type");
      }
    }
    inferred = lty;
    break;
  }

  case AST_CALL_EXPR: {
    Type *callee_t = typechecker_infer_expr(tc, expr->as.call.callee);
    if (callee_t->kind == TY_FUNCTION) {
      inferred = callee_t->as.function.ret;

      AstNode *arg = expr->as.call.args;
      int arg_count = 0;
      while (arg && arg_count < callee_t->as.function.param_count) {
        Type *arg_ty = typechecker_infer_expr(tc, arg);
        Type *param_ty = callee_t->as.function.params[arg_count];
        if (param_ty->kind != TY_UNKNOWN && arg_ty->kind != TY_UNKNOWN) {
          if (!type_is_assignable(param_ty, arg_ty)) {
            typechecker_error(tc, arg->line, arg->col,
                              "Argument type mismatch in function call");
          }
        }
        arg = arg->next;
        arg_count++;
      }
      if (arg || arg_count < callee_t->as.function.param_count) {
        typechecker_error(tc, expr->line, expr->col,
                          "Incorrect number of arguments in function call");
      }
    } else if (callee_t->kind != TY_UNKNOWN) {
      typechecker_error(tc, expr->line, expr->col,
                        "Cannot call non-function type");
    }
    break;
  }

  case AST_UNARY_EXPR: {
    Type *inner_t = typechecker_infer_expr(tc, expr->as.unary.expr);
    if (inner_t->kind != TY_UNKNOWN) {
      if (expr->as.unary.op == TOKEN_STAR) {
        // Dereference: *p
        if (inner_t->kind == TY_POINTER) {
          inferred = inner_t->as.pointer.inner;
        } else {
          typechecker_error(tc, expr->line, expr->col,
                            "Cannot dereference non-pointer type");
        }
      } else if (expr->as.unary.op == TOKEN_AMP) {
        // Address-of: &x
        inferred = type_new_pointer(tc->tctx, inner_t);
      } else if (expr->as.unary.op == TOKEN_MINUS) {
        // Negation requires numeric type
        inferred = inner_t;
      } else if (expr->as.unary.op == TOKEN_BANG) {
        // Logical NOT requires boolean
        if (inner_t->kind != TY_PRIMITIVE ||
            strcmp(inner_t->as.primitive.name, "bool") != 0) {
          typechecker_error(tc, expr->line, expr->col,
                            "Logical NOT requires boolean operand");
        }
        inferred = inner_t;
      }
    }
    break;
  }

  case AST_INDEX_EXPR: {
    Type *target_t = typechecker_infer_expr(tc, expr->as.index.target);
    Type *index_t = typechecker_infer_expr(tc, expr->as.index.index);

    if (target_t->kind == TY_ARRAY) {
      inferred = target_t->as.array.inner;
    } else if (target_t->kind != TY_UNKNOWN) {
      typechecker_error(tc, expr->line, expr->col,
                        "Cannot index non-array type");
    }

    if (index_t->kind != TY_UNKNOWN) {
      // Index must be an integer (simplification: just check if it's a
      // primitive starting with 'i' or 'u')
      if (index_t->kind != TY_PRIMITIVE ||
          (index_t->as.primitive.name[0] != 'i' &&
           index_t->as.primitive.name[0] != 'u')) {
        typechecker_error(tc, expr->as.index.index->line,
                          expr->as.index.index->col,
                          "Array index must be an integer");
      }
    }
    break;
  }

  case AST_FIELD_EXPR: {
    Type *target_t = typechecker_infer_expr(tc, expr->as.field.target);
    if (target_t->kind == TY_STRUCT) {
      // TODO: Look up field in struct
      // For now, return unknown or try some basic checks if we had field lists
      inferred = tc->tctx->type_unknown;
    } else if (target_t->kind == TY_POINTER &&
               target_t->as.pointer.inner->kind == TY_STRUCT) {
      // Auto-dereference field access: p.x where p is *Struct
      inferred = tc->tctx->type_unknown;
    } else if (target_t->kind != TY_UNKNOWN) {
      // typechecker_error(tc, expr->line, expr->col, "Cannot access field on
      // non-struct type");
    }
    break;
  }

  default:
    break;
  }

  expr->resolved_type = inferred;
  return inferred;
}

static void typechecker_check_node(TypeChecker *tc, AstNode *node) {
  if (!node)
    return;
  switch (node->kind) {
  case AST_FUNC_DECL: {
    symbol_table_push(tc->st);

    AstNode *p = node->as.func_decl.params;
    while (p) {
      Symbol sym = {0};
      sym.name = p->as.param.name;
      sym.kind = SYM_VAR;
      sym.node = p;
      sym.type = typechecker_resolve_type_expr(tc, p->as.param.type);
      symbol_table_define(tc->st, sym);
      p = p->next;
    }

    if (node->as.func_decl.ret_name) {
      Symbol sym = {0};
      sym.name = node->as.func_decl.ret_name;
      sym.kind = SYM_VAR;
      sym.node = node;
      sym.type = typechecker_resolve_type_expr(tc, node->as.func_decl.ret_type);
      symbol_table_define(tc->st, sym);
    }

    tc->expected_ret =
        typechecker_resolve_type_expr(tc, node->as.func_decl.ret_type);

    if (node->as.func_decl.body) {
      typechecker_check_node(tc, node->as.func_decl.body);
    }

    tc->expected_ret = NULL;
    symbol_table_pop(tc->st);
    break;
  }

  case AST_VAR_DECL: {
    Type *decl_t = tc->tctx->type_unknown;
    if (node->as.var_decl.type) {
      decl_t = typechecker_resolve_type_expr(tc, node->as.var_decl.type);
    }

    if (node->as.var_decl.init) {
      Type *init_t = typechecker_infer_expr(tc, node->as.var_decl.init);
      if (decl_t->kind != TY_UNKNOWN && init_t->kind != TY_UNKNOWN) {
        if (!type_is_assignable(decl_t, init_t)) {
          typechecker_error(
              tc, node->line, node->col,
              "Variable initializer does not match declared type");
        } else if (node->as.var_decl.init->kind == AST_INT_LITERAL &&
                   decl_t->kind == TY_PRIMITIVE) {
          // Basic Range Checking
          unsigned long long val = node->as.var_decl.init->as.int_literal.value;
          const char *tn = decl_t->as.primitive.name;
          bool overflow = false;
          if (strcmp(tn, "i8") == 0) {
            if (val > 127)
              overflow = true;
          } else if (strcmp(tn, "u8") == 0) {
            if (val > 255)
              overflow = true;
          } else if (strcmp(tn, "i16") == 0) {
            if (val > 32767)
              overflow = true;
          } else if (strcmp(tn, "u16") == 0) {
            if (val > 65535)
              overflow = true;
          } else if (strcmp(tn, "u32") == 0) {
            if (val > 4294967295ULL)
              overflow = true;
          }
          if (overflow) {
            typechecker_error(tc, node->as.var_decl.init->line,
                              node->as.var_decl.init->col,
                              "Integer literal overflow for target type");
          }
        } else if (node->as.var_decl.init->kind == AST_FLOAT_LITERAL &&
                   decl_t->kind == TY_PRIMITIVE) {
          double val = node->as.var_decl.init->as.float_literal.value;
          const char *tn = decl_t->as.primitive.name;
          if (strcmp(tn, "f32") == 0) {
            // Basic float range check:
            // f32 is approx +/- 3.4e38
            if (val > 3.40282347e38 || val < -3.40282347e38) {
              typechecker_error(tc, node->as.var_decl.init->line,
                                node->as.var_decl.init->col,
                                "Float literal overflow for f32");
            }
          }
        }
      } else {
        decl_t = init_t; // inference
      }
    }

    Symbol sym = {0};
    sym.name = node->as.var_decl.name;
    sym.kind = SYM_VAR;
    sym.node = node;
    sym.type = decl_t;
    symbol_table_define(tc->st, sym);
    node->resolved_type = decl_t;
    break;
  }

  case AST_RETURN_STMT: {
    if (node->as.return_stmt.value) {
      Type *ret_v = typechecker_infer_expr(tc, node->as.return_stmt.value);
      if (tc->expected_ret && tc->expected_ret->kind != TY_UNKNOWN &&
          ret_v->kind != TY_UNKNOWN) {
        if (!type_is_assignable(tc->expected_ret, ret_v)) {
          typechecker_error(tc, node->line, node->col, "Return type mismatch");
        }
      }
    }
    break;
  }

  case AST_BLOCK: {
    symbol_table_push(tc->st);
    // Recursively collect declarations in the block before checking statements
    typechecker_collect_decls(tc, node->as.block.statements);
    AstNode *stmt = node->as.block.statements;
    while (stmt) {
      typechecker_check_node(tc, stmt);
      stmt = stmt->next;
    }
    symbol_table_pop(tc->st);
    break;
  }

  case AST_IF_STMT: {
    if (node->as.if_stmt.condition) {
      Type *cond_t = typechecker_infer_expr(tc, node->as.if_stmt.condition);
      if (cond_t->kind != TY_UNKNOWN) {
        if (cond_t->kind != TY_PRIMITIVE ||
            strcmp(cond_t->as.primitive.name, "bool") != 0) {
          typechecker_error(tc, node->line, node->col,
                            "If condition must be a boolean expression");
        }
      }
    }
    typechecker_check_node(tc, node->as.if_stmt.then_branch);
    if (node->as.if_stmt.else_branch) {
      typechecker_check_node(tc, node->as.if_stmt.else_branch);
    }
    break;
  }

  default:
    typechecker_infer_expr(tc, node);
    break;
  }
}

void typechecker_check(TypeChecker *tc, AstNode *program) {
  if (!program || program->kind != AST_PROGRAM)
    return;

  typechecker_collect_decls(tc, program->as.program.declarations);

  AstNode *decl = program->as.program.declarations;
  while (decl) {
    typechecker_check_node(tc, decl);
    decl = decl->next;
  }
}
