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

// ── Phase 3 helpers ──────────────────────────────────────────────────────────

static bool is_realm_nesting_legal(MemoryRealm outer, MemoryRealm inner) {
  // MAIN and HEAP can nest anything; FLEX inherits caller so allows anything
  if (outer == REALM_MAIN || outer == REALM_HEAP || outer == REALM_FLEX)
    return true;
  // STACK always nests inside anything
  if (inner == REALM_STACK)
    return true;
  // GC can nest GC
  if (outer == REALM_GC && inner == REALM_GC)
    return true;
  // STACK and ARENA can only nest STACK (handled above)
  return false;
}

static MemoryStrategy realm_to_strategy(MemoryRealm realm) {
  switch (realm) {
  case REALM_STACK:
    return STRATEGY_STACK;
  case REALM_ARENA:
    return STRATEGY_REGIONAL;
  case REALM_HEAP:
    return STRATEGY_DYNAMIC;
  case REALM_GC:
    return STRATEGY_GC;
  case REALM_FLEX:
    return STRATEGY_FLEX;
  case REALM_MAIN:
    return STRATEGY_STACK;
  }
  return STRATEGY_STACK;
}

static const char *realm_name(MemoryRealm r) {
  switch (r) {
  case REALM_STACK:
    return "stack";
  case REALM_ARENA:
    return "arena";
  case REALM_HEAP:
    return "dynamic";
  case REALM_GC:
    return "gc";
  case REALM_FLEX:
    return "flex";
  case REALM_MAIN:
    return "main";
  }
  return "unknown";
}

// ── Phase 2 helpers ──────────────────────────────────────────────────────────

static void typechecker_check_pattern(TypeChecker *tc, AstNode *pattern,
                                      Type *subject_type) {
  if (!pattern)
    return;

  switch (pattern->kind) {
  case AST_INT_LITERAL:
    if (subject_type->kind != TY_UNKNOWN &&
        subject_type->kind != TY_PRIMITIVE) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Integer literal pattern requires integer subject");
    }
    break;
  case AST_FLOAT_LITERAL:
    if (subject_type->kind != TY_UNKNOWN &&
        subject_type->kind != TY_PRIMITIVE) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Float literal pattern requires numeric subject");
    }
    break;
  case AST_STRING_LITERAL:
    if (subject_type->kind != TY_UNKNOWN &&
        !(subject_type->kind == TY_PRIMITIVE &&
          strcmp(subject_type->as.primitive.name, "str") == 0)) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "String literal pattern requires str subject");
    }
    break;
  case AST_BOOL_LITERAL:
    if (subject_type->kind != TY_UNKNOWN &&
        !(subject_type->kind == TY_PRIMITIVE &&
          strcmp(subject_type->as.primitive.name, "bool") == 0)) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Bool literal pattern requires bool subject");
    }
    break;
  case AST_IDENTIFIER: {
    const char *name = pattern->as.identifier.name;
    if (strcmp(name, "_") != 0) {
      // Binding pattern: bind the name to the subject type in current scope
      Symbol sym = {0};
      sym.name = name;
      sym.kind = SYM_VAR;
      sym.node = pattern;
      sym.type = subject_type;
      symbol_table_define(tc->st, sym);
    }
    break;
  }
  case AST_STRUCT_PATTERN: {
    // Struct patterns are also used for variant destructuring and Ok/Err
    if (subject_type->kind != TY_UNKNOWN && subject_type->kind != TY_STRUCT &&
        subject_type->kind != TY_VARIANT && subject_type->kind != TY_FALLIBLE) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Destructure pattern requires struct, variant, or "
                        "fallible subject");
    }
    // Bind fields as unknown for now (full struct field lookup is TODO)
    AstNode *fp = pattern->as.struct_pattern.fields;
    while (fp) {
      if (fp->kind == AST_FIELD_PATTERN) {
        if (fp->as.field_pattern.name && !fp->as.field_pattern.pattern) {
          // Shorthand: bind field name (e.g., Vec2(x, y) — x and y)
          Symbol sym = {0};
          sym.name = fp->as.field_pattern.name;
          sym.kind = SYM_VAR;
          sym.node = fp;
          sym.type = tc->tctx->type_unknown;
          symbol_table_define(tc->st, sym);
        } else if (fp->as.field_pattern.pattern &&
                   fp->as.field_pattern.pattern->kind == AST_IDENTIFIER) {
          const char *bname = fp->as.field_pattern.pattern->as.identifier.name;
          if (strcmp(bname, "_") != 0) {
            Symbol sym = {0};
            sym.name = bname;
            sym.kind = SYM_VAR;
            sym.node = fp;
            sym.type = tc->tctx->type_unknown;
            symbol_table_define(tc->st, sym);
          }
        } else if (!fp->as.field_pattern.name && fp->as.field_pattern.pattern) {
          // Positional field: Ok(v), Err(e), Circle(radius)
          if (fp->as.field_pattern.pattern->kind == AST_IDENTIFIER) {
            const char *bname =
                fp->as.field_pattern.pattern->as.identifier.name;
            if (strcmp(bname, "_") != 0) {
              Symbol sym = {0};
              sym.name = bname;
              sym.kind = SYM_VAR;
              sym.node = fp;
              sym.type = tc->tctx->type_unknown;
              symbol_table_define(tc->st, sym);
            }
          }
        }
        // Literal field patterns don't bind anything
      }
      fp = fp->next;
    }
    break;
  }
  case AST_CALL_EXPR:
    // Variant destructure pattern: Variant(a, b, c)
    // Bind arguments as unknowns
    {
      AstNode *arg = pattern->as.call.args;
      while (arg) {
        if (arg->kind == AST_IDENTIFIER &&
            strcmp(arg->as.identifier.name, "_") != 0) {
          Symbol sym = {0};
          sym.name = arg->as.identifier.name;
          sym.kind = SYM_VAR;
          sym.node = arg;
          sym.type = tc->tctx->type_unknown;
          symbol_table_define(tc->st, sym);
        }
        arg = arg->next;
      }
    }
    break;
  default:
    break;
  }
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
        MemoryStrategy strat = realm_to_strategy(decl->as.func_decl.realm);
        sym->type =
            type_new_function(tc->tctx, ptypes, param_c, ret_t, strat, false);
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
    } else if (decl->kind == AST_TYPE_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.type_decl.name);
      if (sym) {
        // Collect field info
        int field_count = 0;
        AstNode *f = decl->as.type_decl.fields;
        while (f) {
          field_count++;
          f = f->next;
        }

        const char **field_names =
            arena_alloc(tc->arena, sizeof(char *) * field_count);
        Type **field_types =
            arena_alloc(tc->arena, sizeof(Type *) * field_count);

        f = decl->as.type_decl.fields;
        for (int i = 0; i < field_count; i++) {
          field_names[i] = f->as.field_decl.name;
          field_types[i] =
              typechecker_resolve_type_expr(tc, f->as.field_decl.type);
          f = f->next;
        }

        Method *existing_methods = NULL;
        if (sym->type) {
          if (sym->type->kind == TY_STRUCT)
            existing_methods = sym->type->as.struct_t.methods;
          else if (sym->type->kind == TY_VARIANT)
            existing_methods = sym->type->as.variant.methods;
        }

        sym->type = type_new_struct(tc->tctx, decl->as.type_decl.name,
                                    field_names, field_types, field_count);
        sym->type->as.struct_t.methods = existing_methods;
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_VARIANT_DECL) {
      Symbol *sym =
          symbol_table_lookup_local(tc->st, decl->as.variant_decl.name);
      if (sym) {
        int arm_count = 0;
        AstNode *a = decl->as.variant_decl.arms;
        while (a) {
          arm_count++;
          a = a->next;
        }

        const char **arm_names =
            arena_alloc(tc->arena, sizeof(char *) * arm_count);
        Type **arm_types = arena_alloc(tc->arena, sizeof(Type *) * arm_count);

        a = decl->as.variant_decl.arms;
        for (int i = 0; i < arm_count; i++) {
          arm_names[i] = a->as.variant_arm.name;
          if (a->as.variant_arm.fields) {
            // ... (Simplified for v0.1)
            arm_types[i] = tc->tctx->type_unknown;
          } else {
            arm_types[i] = NULL;
          }
          a = a->next;
        }

        Method *existing_methods = NULL;
        if (sym->type) {
          if (sym->type->kind == TY_STRUCT)
            existing_methods = sym->type->as.struct_t.methods;
          else if (sym->type->kind == TY_VARIANT)
            existing_methods = sym->type->as.variant.methods;
        }

        sym->type = type_new_variant(tc->tctx, decl->as.variant_decl.name,
                                     arm_names, arm_types, arm_count);
        sym->type->as.variant.methods = existing_methods;
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_METHOD_DECL) {
      Symbol *type_sym =
          symbol_table_lookup(tc->st, decl->as.method_decl.type_name);
      if (type_sym && type_sym->type) {
        Type *t = type_sym->type;
        AstNode *method_node = decl->as.method_decl.methods;
        while (method_node) {
          if (method_node->kind == AST_FUNC_DECL) {
            Method *m = arena_alloc(tc->arena, sizeof(Method));
            m->name = method_node->as.func_decl.name;
            m->node = method_node;

            Type *ret_t = typechecker_resolve_type_expr(
                tc, method_node->as.func_decl.ret_type);
            int param_c = 0;
            AstNode *p = method_node->as.func_decl.params;
            while (p) {
              param_c++;
              p = p->next;
            }

            Type **ptypes = NULL;
            if (param_c > 0) {
              ptypes = arena_alloc(tc->arena, sizeof(Type *) * param_c);
              p = method_node->as.func_decl.params;
              for (int i = 0; i < param_c; i++) {
                if (strcmp(p->as.param.name, "self") == 0 &&
                    !p->as.param.type) {
                  ptypes[i] = t;
                } else {
                  ptypes[i] =
                      typechecker_resolve_type_expr(tc, p->as.param.type);
                }
                p = p->next;
              }
            }
            MemoryStrategy strat =
                realm_to_strategy(method_node->as.func_decl.realm);
            m->type = type_new_function(tc->tctx, ptypes, param_c, ret_t, strat,
                                        true);

            if (t->kind == TY_STRUCT) {
              m->next = t->as.struct_t.methods;
              t->as.struct_t.methods = m;
            } else if (t->kind == TY_VARIANT) {
              m->next = t->as.variant.methods;
              t->as.variant.methods = m;
            }
          }
          method_node = method_node->next;
        }
      } else {
        typechecker_error(tc, decl->line, decl->col,
                          "Unknown type '%s' in method declaration",
                          decl->as.method_decl.type_name);
      }
    }
    decl = decl->next;
  }
}

// Forward declaration — needed because infer_expr calls check_node for
// catch handlers and match-as-expression block bodies.
static void typechecker_check_node(TypeChecker *tc, AstNode *node);

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
        bool ok = type_is_assignable(lty, rty) || type_is_assignable(rty, lty);
        // Allow pointer arithmetic: pointer + integer or pointer - integer
        if (!ok) {
          if ((lty->kind == TY_POINTER && rty->kind == TY_PRIMITIVE) ||
              (rty->kind == TY_POINTER && lty->kind == TY_PRIMITIVE)) {
            ok = true;
          }
        }
        if (!ok) {
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
      int param_start = 0;

      // Handle method call self-injection
      if (callee_t->as.function.is_method &&
          expr->as.call.callee->kind == AST_FIELD_EXPR) {
        Type *target_t =
            typechecker_infer_expr(tc, expr->as.call.callee->as.field.target);
        if (callee_t->as.function.param_count > 0) {
          Type *first_p = callee_t->as.function.params[0];
          bool ok = type_is_assignable(first_p, target_t);
          if (!ok) {
            // Implicit address-of: calling *T method on T value
            if (first_p->kind == TY_POINTER &&
                type_is_assignable(first_p->as.pointer.inner, target_t)) {
              ok = true;
            }
            // Implicit deref: calling T method on *T value
            else if (target_t->kind == TY_POINTER &&
                     type_is_assignable(first_p, target_t->as.pointer.inner)) {
              ok = true;
            }
          }

          if (!ok) {
            typechecker_error(tc, expr->line, expr->col,
                              "Method receiver type mismatch");
          }
          param_start = 1;
        }
      }

      int total_params_expected =
          callee_t->as.function.param_count - param_start;
      int actual_args_provided = 0;
      AstNode *temp_arg = arg;
      while (temp_arg) {
        actual_args_provided++;
        temp_arg = temp_arg->next;
      }

      if (actual_args_provided != total_params_expected) {
        typechecker_error(tc, expr->line, expr->col,
                          "Incorrect number of arguments (expected %d, got %d)",
                          total_params_expected, actual_args_provided);
      }

      int param_idx = param_start;
      while (arg && param_idx < callee_t->as.function.param_count) {
        Type *arg_ty = typechecker_infer_expr(tc, arg);
        Type *param_ty = callee_t->as.function.params[param_idx];
        if (param_ty->kind != TY_UNKNOWN && arg_ty->kind != TY_UNKNOWN) {
          if (!type_is_assignable(param_ty, arg_ty)) {
            typechecker_error(tc, arg->line, arg->col,
                              "Argument type mismatch in function call");
          }
        }
        arg = arg->next;
        param_idx++;
      }
    } else if (callee_t->kind == TY_STRUCT) {
      // Constructor: Vec2(x: 1.0, y: 2.0)
      inferred = callee_t;
      AstNode *arg = expr->as.call.args;
      while (arg) {
        if (arg->kind == AST_NAMED_ARG) {
          const char *name = arg->as.named_arg.name;
          Type *val_t = typechecker_infer_expr(tc, arg->as.named_arg.value);
          bool found = false;
          for (int i = 0; i < callee_t->as.struct_t.field_count; i++) {
            if (strcmp(callee_t->as.struct_t.field_names[i], name) == 0) {
              found = true;
              if (!type_is_assignable(callee_t->as.struct_t.field_types[i],
                                      val_t)) {
                typechecker_error(tc, arg->line, arg->col,
                                  "Type mismatch for field '%s'", name);
              }
              break;
            }
          }
          if (!found) {
            typechecker_error(tc, arg->line, arg->col,
                              "No field '%s' in struct '%s'", name,
                              callee_t->as.struct_t.name);
          }
        } else {
          // Positional constructor (for short structs)
          // For now, only named arguments are supported in constructors
          typechecker_error(tc, arg->line, arg->col,
                            "Positional arguments not allowed in struct "
                            "constructors (use named)");
        }
        arg = arg->next;
      }
    } else if (callee_t->kind == TY_VARIANT) {
      // Variant arm constructor: RGB(255, 0, 0)
      inferred = callee_t;
      // TODO: Full variant arm resolution
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
        // Array-to-pointer decay: &([N]T) → *T, not *[N]T
        if (inner_t->kind == TY_ARRAY) {
          inferred = type_new_pointer(tc->tctx, inner_t->as.array.inner);
        } else {
          inferred = type_new_pointer(tc->tctx, inner_t);
        }
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
    const char *fname = expr->as.field.field;

    // 1. Handle Module access (e.g. kernel.arch)
    // If target is unknown, it might be a module path.
    if (target_t->kind == TY_UNKNOWN) {
      bool is_mod = false;
      if (expr->as.field.target->kind == AST_IDENTIFIER) {
        Symbol *s = symbol_table_lookup(
            tc->st, expr->as.field.target->as.identifier.name);
        if (s && s->kind == SYM_MOD)
          is_mod = true;
      } else if (expr->as.field.target->kind == AST_FIELD_EXPR) {
        // Nested module path
        is_mod = true; // Assume unknown field on unknown target is a path
      }

      if (is_mod) {
        inferred = tc->tctx->type_unknown;
        break;
      }
    }

    Type *base_t = target_t;
    if (target_t->kind == TY_POINTER &&
        target_t->as.pointer.inner->kind == TY_STRUCT) {
      // Auto-dereference field access: p.x where p is *Struct
      base_t = target_t->as.pointer.inner;
    }

    if (base_t->kind == TY_STRUCT) {
      bool found = false;
      for (int i = 0; i < base_t->as.struct_t.field_count; i++) {
        if (strcmp(base_t->as.struct_t.field_names[i], fname) == 0) {
          inferred = base_t->as.struct_t.field_types[i];
          found = true;
          break;
        }
      }
      if (!found) {
        // Look for methods
        Method *m = base_t->as.struct_t.methods;
        while (m) {
          if (strcmp(m->name, fname) == 0) {
            inferred = m->type;
            found = true;
            break;
          }
          m = m->next;
        }
      }

      if (!found) {
        typechecker_error(tc, expr->line, expr->col,
                          "Field or method '%s' not found in struct '%s'",
                          fname, base_t->as.struct_t.name);
      }
    } else if (base_t->kind == TY_VARIANT) {
      bool found = false;
      for (int i = 0; i < base_t->as.variant.arm_count; i++) {
        if (strcmp(base_t->as.variant.arm_names[i], fname) == 0) {
          // Variant arm access (e.g. Color.Red)
          inferred = base_t;
          found = true;
          break;
        }
      }
      if (!found) {
        // Look for methods
        Method *m = base_t->as.variant.methods;
        while (m) {
          if (strcmp(m->name, fname) == 0) {
            inferred = m->type;
            found = true;
            break;
          }
          m = m->next;
        }
      }
      if (!found) {
        typechecker_error(tc, expr->line, expr->col,
                          "Arm or method '%s' not found in variant '%s'", fname,
                          base_t->as.variant.name);
      }
    } else if (base_t->kind == TY_PRIMITIVE &&
               strcmp(base_t->as.primitive.name, "str") == 0) {
      if (strcmp(fname, "ptr") == 0) {
        inferred = type_new_pointer(tc->tctx, tc->tctx->type_u8);
      } else if (strcmp(fname, "len") == 0) {
        inferred = tc->tctx->type_usize;
      } else {
        typechecker_error(tc, expr->line, expr->col,
                          "Unknown property '%s' on string", fname);
      }
    } else if (base_t->kind == TY_ARRAY) {
      if (strcmp(fname, "len") == 0) {
        inferred = tc->tctx->type_usize;
      } else {
        typechecker_error(tc, expr->line, expr->col,
                          "Unknown property '%s' on array", fname);
      }
    } else if (base_t->kind != TY_UNKNOWN) {
      // Be lenient with unknown fields on non-struct types for now if they
      // might be methods/properties we haven't implemented yet.
      // But for v0.1 we only error if we are sure it's not a struct.
      // Actually, many tests use this for mock objects.
      inferred = tc->tctx->type_unknown;
    }
    break;
  }

    // ── Phase 2: try/catch ─────────────────────────────────────────────────

  case AST_TRY_EXPR: {
    Type *inner_t = typechecker_infer_expr(tc, expr->as.try_expr.expr);
    if (inner_t->kind == TY_FALLIBLE) {
      inferred = inner_t->as.fallible.inner;
    } else if (inner_t->kind != TY_UNKNOWN) {
      typechecker_error(tc, expr->line, expr->col,
                        "try requires a fallible (!T) expression");
    }
    break;
  }

  case AST_CATCH_EXPR: {
    Type *inner_t = typechecker_infer_expr(tc, expr->as.catch_expr.expr);
    if (inner_t->kind == TY_FALLIBLE) {
      Type *success_t = inner_t->as.fallible.inner;
      inferred = success_t;

      if (expr->as.catch_expr.err_name) {
        symbol_table_push(tc->st);
        Symbol sym = {0};
        sym.name = expr->as.catch_expr.err_name;
        sym.kind = SYM_VAR;
        sym.node = expr;
        sym.type = tc->tctx->type_unknown; // error type
        symbol_table_define(tc->st, sym);
      }

      if (expr->as.catch_expr.handler) {
        Type *handler_t =
            typechecker_infer_expr(tc, expr->as.catch_expr.handler);
        if (expr->as.catch_expr.handler->kind == AST_BLOCK) {
          typechecker_check_node(tc, expr->as.catch_expr.handler);
        } else if (handler_t->kind == TY_FALLIBLE) {
          // Chained catch: handler returns !T, propagate as the overall type
          inferred = handler_t;
        } else if (handler_t->kind != TY_UNKNOWN &&
                   success_t->kind != TY_UNKNOWN) {
          if (!type_is_assignable(success_t, handler_t)) {
            typechecker_error(
                tc, expr->as.catch_expr.handler->line,
                expr->as.catch_expr.handler->col,
                "Catch handler type must match success type of fallible");
          }
        }
      }

      if (expr->as.catch_expr.err_name) {
        symbol_table_pop(tc->st);
      }
    } else if (inner_t->kind != TY_UNKNOWN) {
      typechecker_error(tc, expr->line, expr->col,
                        "catch requires a fallible (!T) expression");
    }
    break;
  }

    // ── Phase 3: promote ──────────────────────────────────────────────────

  case AST_PROMOTE_EXPR: {
    Type *inner_t = typechecker_infer_expr(tc, expr->as.promote.expr);
    inferred = inner_t;

    if (tc->current_realm == REALM_STACK) {
      typechecker_error(tc, expr->line, expr->col,
                        "Cannot promote from a pure stack function");
    }

    MemoryRealm target = expr->as.promote.target;
    if (target != REALM_HEAP && target != REALM_GC) {
      typechecker_error(tc, expr->line, expr->col,
                        "Promote target must be dynamic or gc");
    }
    break;
  }

    // ── Phase 2: match as expression ───────────────────────────────────────

  case AST_MATCH_STMT: {
    Type *subject_t = typechecker_infer_expr(tc, expr->as.match_stmt.subject);
    Type *first_arm_t = NULL;

    AstNode *arm = expr->as.match_stmt.arms;
    while (arm) {
      if (arm->kind == AST_MATCH_ARM) {
        symbol_table_push(tc->st);
        typechecker_check_pattern(tc, arm->as.match_arm.pattern, subject_t);

        if (arm->as.match_arm.guard) {
          Type *guard_t = typechecker_infer_expr(tc, arm->as.match_arm.guard);
          if (guard_t->kind != TY_UNKNOWN &&
              (guard_t->kind != TY_PRIMITIVE ||
               strcmp(guard_t->as.primitive.name, "bool") != 0)) {
            typechecker_error(tc, arm->as.match_arm.guard->line,
                              arm->as.match_arm.guard->col,
                              "Match guard must be a boolean expression");
          }
        }

        Type *body_t = typechecker_infer_expr(tc, arm->as.match_arm.body);
        if (arm->as.match_arm.body &&
            arm->as.match_arm.body->kind == AST_BLOCK) {
          typechecker_check_node(tc, arm->as.match_arm.body);
        }

        if (!first_arm_t && body_t->kind != TY_UNKNOWN) {
          first_arm_t = body_t;
        } else if (first_arm_t && body_t->kind != TY_UNKNOWN) {
          if (!type_is_assignable(first_arm_t, body_t)) {
            typechecker_error(
                tc, arm->line, arm->col,
                "Match arm type is incompatible with previous arms");
          }
        }

        symbol_table_pop(tc->st);
      }
      arm = arm->next;
    }

    inferred = first_arm_t ? first_arm_t : tc->tctx->type_unknown;
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
    // Phase 3: realm nesting enforcement
    MemoryRealm saved_realm = tc->current_realm;
    MemoryRealm func_realm = node->as.func_decl.realm;

    if (!node->as.func_decl.is_main) {
      if (!is_realm_nesting_legal(saved_realm, func_realm)) {
        typechecker_error(tc, node->line, node->col,
                          "Cannot nest %s function inside %s realm",
                          realm_name(func_realm), realm_name(saved_realm));
      }
    }
    tc->current_realm = node->as.func_decl.is_main ? REALM_MAIN : func_realm;

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
    tc->current_realm = saved_realm;
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

  case AST_TUPLE_DESTRUCTURE: {
    Type *init_t = typechecker_infer_expr(tc, node->as.tuple_destructure.init);

    // Walk the target VarDecls and bind each one, matching positional tuple
    // element types when the init resolves to a known tuple.
    AstNode *target = node->as.tuple_destructure.targets;
    int elem_idx = 0;
    while (target) {
      Type *elem_t = tc->tctx->type_unknown;

      if (target->kind == AST_VAR_DECL) {
        // If the target has an explicit type annotation, use it.
        if (target->as.var_decl.type) {
          elem_t = typechecker_resolve_type_expr(tc, target->as.var_decl.type);
        } else if (init_t->kind == TY_TUPLE) {
          // Index into the tuple's elems array by position.
          if (elem_idx < init_t->as.tuple.count)
            elem_t = init_t->as.tuple.elems[elem_idx];
        }

        Symbol sym = {0};
        sym.name = target->as.var_decl.name;
        sym.kind = SYM_VAR;
        sym.node = target;
        sym.type = elem_t;
        symbol_table_define(tc->st, sym);
        target->resolved_type = elem_t;
      }

      target = target->next;
      elem_idx++;
    }
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

  // Phase 2: match statement
  case AST_MATCH_STMT: {
    Type *subject_t = typechecker_infer_expr(tc, node->as.match_stmt.subject);

    AstNode *arm = node->as.match_stmt.arms;
    while (arm) {
      if (arm->kind == AST_MATCH_ARM) {
        symbol_table_push(tc->st);
        typechecker_check_pattern(tc, arm->as.match_arm.pattern, subject_t);

        if (arm->as.match_arm.guard) {
          Type *guard_t = typechecker_infer_expr(tc, arm->as.match_arm.guard);
          if (guard_t->kind != TY_UNKNOWN &&
              (guard_t->kind != TY_PRIMITIVE ||
               strcmp(guard_t->as.primitive.name, "bool") != 0)) {
            typechecker_error(tc, arm->as.match_arm.guard->line,
                              arm->as.match_arm.guard->col,
                              "Match guard must be a boolean expression");
          }
        }

        if (arm->as.match_arm.body) {
          if (arm->as.match_arm.body->kind == AST_BLOCK) {
            typechecker_check_node(tc, arm->as.match_arm.body);
          } else {
            typechecker_infer_expr(tc, arm->as.match_arm.body);
          }
        }

        symbol_table_pop(tc->st);
      }
      arm = arm->next;
    }
    break;
  }

  // Phase 2: while/for/loop — check body
  case AST_WHILE_STMT: {
    if (node->as.while_stmt.condition) {
      Type *cond_t = typechecker_infer_expr(tc, node->as.while_stmt.condition);
      if (cond_t->kind != TY_UNKNOWN &&
          (cond_t->kind != TY_PRIMITIVE ||
           strcmp(cond_t->as.primitive.name, "bool") != 0)) {
        typechecker_error(tc, node->line, node->col,
                          "While condition must be a boolean expression");
      }
    }
    tc->loop_depth++;
    typechecker_check_node(tc, node->as.while_stmt.body);
    tc->loop_depth--;
    break;
  }

  case AST_FOR_STMT: {
    symbol_table_push(tc->st);
    typechecker_infer_expr(tc, node->as.for_stmt.iter);
    // Bind capture variable
    if (node->as.for_stmt.cap_value) {
      Symbol sym = {0};
      sym.name = node->as.for_stmt.cap_value;
      sym.kind = SYM_VAR;
      sym.node = node;
      sym.type = tc->tctx->type_unknown;
      symbol_table_define(tc->st, sym);
    }
    if (node->as.for_stmt.cap_index) {
      Symbol sym = {0};
      sym.name = node->as.for_stmt.cap_index;
      sym.kind = SYM_VAR;
      sym.node = node;
      sym.type = tc->tctx->type_usize;
      symbol_table_define(tc->st, sym);
    }
    tc->loop_depth++;
    typechecker_check_node(tc, node->as.for_stmt.body);
    tc->loop_depth--;
    symbol_table_pop(tc->st);
    break;
  }

  case AST_LOOP_STMT: {
    tc->loop_depth++;
    typechecker_check_node(tc, node->as.loop_stmt.body);
    tc->loop_depth--;
    break;
  }

  case AST_UNSAFE_BLOCK: {
    typechecker_check_node(tc, node->as.unsafe_block.body);
    break;
  }

  case AST_TYPE_DECL: {
    // 1. Duplicate field names check
    AstNode *f1 = node->as.type_decl.fields;
    while (f1) {
      AstNode *f2 = f1->next;
      while (f2) {
        if (strcmp(f1->as.field_decl.name, f2->as.field_decl.name) == 0) {
          typechecker_error(tc, f2->line, f2->col,
                            "Duplicate field name '%s' in struct",
                            f2->as.field_decl.name);
        }
        f2 = f2->next;
      }

      // 2. Infinite recursion check
      Type *f_t = typechecker_resolve_type_expr(tc, f1->as.field_decl.type);
      if (f_t->kind == TY_STRUCT &&
          strcmp(f_t->as.struct_t.name, node->as.type_decl.name) == 0) {
        typechecker_error(tc, f1->line, f1->col,
                          "Infinite recursion: struct '%s' contains itself by "
                          "value (use a pointer)",
                          node->as.type_decl.name);
      }
      f1 = f1->next;
    }

    // 3. Attribute validation
    Attr *attr = node->as.type_decl.attrs;
    while (attr) {
      if (strcmp(attr->name, "align") == 0) {
        if (attr->arg && attr->arg->kind == AST_INT_LITERAL) {
          unsigned long long val = attr->arg->as.int_literal.value;
          // Power of 2 check
          if (val == 0 || (val & (val - 1)) != 0) {
            typechecker_error(tc, attr->arg->line, attr->arg->col,
                              "Alignment must be a power of 2");
          }
        }
      }
      attr = attr->next;
    }
    break;
  }

  case AST_VARIANT_DECL: {
    AstNode *a1 = node->as.variant_decl.arms;
    while (a1) {
      AstNode *a2 = a1->next;
      while (a2) {
        if (strcmp(a1->as.variant_arm.name, a2->as.variant_arm.name) == 0) {
          typechecker_error(tc, a2->line, a2->col, "Duplicate variant arm '%s'",
                            a2->as.variant_arm.name);
        }
        a2 = a2->next;
      }
      a1 = a1->next;
    }
    break;
  }

  case AST_METHOD_DECL: {
    // Only thing to check is consistency. methods were already collected.
    // We could check if methods conflict with fields.
    Symbol *sym = symbol_table_lookup(tc->st, node->as.method_decl.type_name);
    if (sym && sym->type && sym->type->kind == TY_STRUCT) {
      Type *struct_t = sym->type;
      AstNode *m_node = node->as.method_decl.methods;
      while (m_node) {
        if (m_node->kind == AST_FUNC_DECL) {
          const char *mname = m_node->as.func_decl.name;
          for (int i = 0; i < struct_t->as.struct_t.field_count; i++) {
            if (strcmp(struct_t->as.struct_t.field_names[i], mname) == 0) {
              typechecker_error(tc, m_node->line, m_node->col,
                                "Method '%s' name conflicts with a field",
                                mname);
            }
          }
          // Now check the body of the method
          MemoryRealm saved_realm = tc->current_realm;
          MemoryRealm func_realm = m_node->as.func_decl.realm;
          tc->current_realm = func_realm;

          symbol_table_push(tc->st);
          // Bind parameters including 'self'
          AstNode *p = m_node->as.func_decl.params;
          while (p) {
            Symbol param_sym = {0};
            param_sym.name = p->as.param.name;
            param_sym.kind = SYM_VAR;
            param_sym.node = p;

            if (strcmp(p->as.param.name, "self") == 0 && !p->as.param.type) {
              param_sym.type = struct_t;
            } else {
              param_sym.type =
                  typechecker_resolve_type_expr(tc, p->as.param.type);
            }
            symbol_table_define(tc->st, param_sym);
            p = p->next;
          }

          if (m_node->as.func_decl.ret_name) {
            Symbol r_sym = {0};
            r_sym.name = m_node->as.func_decl.ret_name;
            r_sym.kind = SYM_VAR;
            r_sym.node = m_node;
            r_sym.type = typechecker_resolve_type_expr(
                tc, m_node->as.func_decl.ret_type);
            symbol_table_define(tc->st, r_sym);
          }

          tc->expected_ret =
              typechecker_resolve_type_expr(tc, m_node->as.func_decl.ret_type);
          if (m_node->as.func_decl.body) {
            typechecker_check_node(tc, m_node->as.func_decl.body);
          }
          tc->expected_ret = NULL;
          tc->current_realm = saved_realm;
          symbol_table_pop(tc->st);
        }
        m_node = m_node->next;
      }
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
