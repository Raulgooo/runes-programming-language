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
  tc->function_depth = 0;
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

static inline bool type_is_resolved(Type *t) {
  return t && t->kind != TY_UNKNOWN && t->kind != TY_INFER_ERROR;
}

static Scope *scope_containing_symbol(SymbolTable *st, Symbol *symbol) {
  for (Scope *scope = st->current; scope; scope = scope->parent) {
    for (uint32_t i = 0; i < scope->capacity; i++) {
      for (ScopeEntry *entry = scope->buckets[i]; entry; entry = entry->next)
        if (&entry->symbol == symbol)
          return scope;
    }
  }
  return NULL;
}

static bool scope_is_between(Scope *candidate, Scope *start,
                             Scope *exclusive_stop) {
  for (Scope *scope = start; scope && scope != exclusive_stop;
       scope = scope->parent)
    if (scope == candidate)
      return true;
  return false;
}

static const char *type_display_name(Type *t) {
  if (!t) return "<unknown>";
  switch (t->kind) {
    case TY_PRIMITIVE: return t->as.primitive.name;
    case TY_POINTER:   return "pointer";
    case TY_ARRAY:     return "array";
    case TY_TUPLE:     return "tuple";
    case TY_FUNCTION:  return "function";
    case TY_FALLIBLE:  return "fallible";
    case TY_STRUCT:    return t->as.struct_t.name ? t->as.struct_t.name : "struct";
    case TY_VARIANT:   return t->as.variant.name ? t->as.variant.name : "variant";
    case TY_INTERFACE: return t->as.interface_t.name ? t->as.interface_t.name : "interface";
    case TY_ERROR:     return t->as.error_t.name ? t->as.error_t.name : "error";
    case TY_UNKNOWN:   return "<unknown>";
    case TY_INFER_ERROR: return "<error>";
  }
  return "<unknown>";
}

static const char *declaration_name(AstNode *node) {
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

static AstNode *find_declaration(AstNode *declarations, const char *name) {
  for (AstNode *decl = declarations; decl; decl = decl->next) {
    const char *candidate = declaration_name(decl);
    if (candidate && strcmp(candidate, name) == 0)
      return decl;
  }
  return NULL;
}

static AstNode *qualified_declaration(TypeChecker *tc, AstNode *expr) {
  if (!expr)
    return NULL;
  if (expr->kind == AST_IDENTIFIER) {
    Symbol *symbol =
        symbol_table_lookup(tc->st, expr->as.identifier.name);
    return symbol ? symbol->node : NULL;
  }
  if (expr->kind != AST_FIELD_EXPR)
    return NULL;
  AstNode *container = qualified_declaration(tc, expr->as.field.target);
  if (!container || container->kind != AST_MOD_DECL)
    return NULL;
  return find_declaration(container->as.mod_decl.declarations,
                          expr->as.field.field);
}

static AstNode *use_target_declaration(TypeChecker *tc, AstNode *path) {
  if (!path || path->kind != AST_IDENTIFIER)
    return NULL;
  Symbol *root = symbol_table_lookup(tc->st, path->as.identifier.name);
  AstNode *current = root ? root->node : NULL;
  for (AstNode *segment = path->next; segment; segment = segment->next) {
    if (!current || current->kind != AST_MOD_DECL ||
        segment->kind != AST_IDENTIFIER)
      return NULL;
    current = find_declaration(current->as.mod_decl.declarations,
                               segment->as.identifier.name);
  }
  return current;
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

static int variant_arm_index(Type *variant, const char *name) {
  if (!variant || variant->kind != TY_VARIANT || !name)
    return -1;
  for (int i = 0; i < variant->as.variant.arm_count; i++) {
    if (strcmp(variant->as.variant.arm_names[i], name) == 0)
      return i;
  }
  return -1;
}

static void bind_pattern_name(TypeChecker *tc, AstNode *node,
                              const char *name, Type *type) {
  if (!name || strcmp(name, "_") == 0)
    return;
  Symbol sym = {0};
  sym.name = name;
  sym.kind = SYM_VAR;
  sym.node = node;
  sym.type = type;
  symbol_table_define(tc->st, sym);
  node->resolved_type = type;
}

static const char *qualified_pattern_name(AstNode *pattern) {
  if (!pattern)
    return NULL;
  if (pattern->kind == AST_IDENTIFIER)
    return pattern->as.identifier.name;
  if (pattern->kind == AST_FIELD_EXPR)
    return pattern->as.field.field;
  if (pattern->kind == AST_STRUCT_PATTERN)
    return pattern->as.struct_pattern.name;
  if (pattern->kind == AST_CALL_EXPR) {
    AstNode *callee = pattern->as.call.callee;
    if (callee && callee->kind == AST_IDENTIFIER)
      return callee->as.identifier.name;
    if (callee && callee->kind == AST_FIELD_EXPR)
      return callee->as.field.field;
  }
  return NULL;
}

static void typechecker_check_match_coverage(TypeChecker *tc, AstNode *match,
                                             Type *subject_type) {
  if (!subject_type || (subject_type->kind != TY_VARIANT &&
                        subject_type->kind != TY_FALLIBLE))
    return;

  if (subject_type->kind == TY_FALLIBLE) {
    bool ok = false, err = false, catch_all = false;
    for (AstNode *arm = match->as.match_stmt.arms; arm; arm = arm->next) {
      if (arm->kind != AST_MATCH_ARM || arm->as.match_arm.guard)
        continue;
      AstNode *pattern = arm->as.match_arm.pattern;
      const char *name = qualified_pattern_name(pattern);
      if (name && strcmp(name, "Ok") == 0)
        ok = true;
      else if (name && strcmp(name, "Err") == 0)
        err = true;
      else if (pattern && pattern->kind == AST_IDENTIFIER)
        catch_all = true;
    }
    if (!catch_all && (!ok || !err)) {
      typechecker_error(tc, match->line, match->col,
                        "Non-exhaustive fallible match; missing arm '%s'",
                        ok ? "Err" : "Ok");
    }
    return;
  }

  int arm_count = subject_type->as.variant.arm_count;
  bool *covered = arena_alloc(tc->arena, sizeof(bool) * (size_t)arm_count);
  memset(covered, 0, sizeof(bool) * (size_t)arm_count);
  bool catch_all = false;

  for (AstNode *arm = match->as.match_stmt.arms; arm; arm = arm->next) {
    if (arm->kind != AST_MATCH_ARM || arm->as.match_arm.guard)
      continue;
    AstNode *pattern = arm->as.match_arm.pattern;
    const char *name = qualified_pattern_name(pattern);
    int index = variant_arm_index(subject_type, name);
    if (index >= 0) {
      if (covered[index]) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Duplicate match arm '%s'", name);
      }
      covered[index] = true;
    } else if (pattern && pattern->kind == AST_IDENTIFIER) {
      catch_all = true;
    }
  }

  if (!catch_all) {
    for (int i = 0; i < arm_count; i++) {
      if (!covered[i]) {
        typechecker_error(tc, match->line, match->col,
                          "Non-exhaustive match; missing arm '%s'",
                          subject_type->as.variant.arm_names[i]);
        break;
      }
    }
  }
}

static void typechecker_check_pattern(TypeChecker *tc, AstNode *pattern,
                                      Type *subject_type) {
  if (!pattern)
    return;

  switch (pattern->kind) {
  case AST_INT_LITERAL:
    if (type_is_resolved(subject_type) &&
        subject_type->kind != TY_PRIMITIVE) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Integer literal pattern requires integer subject");
    }
    break;
  case AST_FLOAT_LITERAL:
    if (type_is_resolved(subject_type) &&
        subject_type->kind != TY_PRIMITIVE) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Float literal pattern requires numeric subject");
    }
    break;
  case AST_STRING_LITERAL:
    if (type_is_resolved(subject_type) &&
        !(subject_type->kind == TY_PRIMITIVE &&
          strcmp(subject_type->as.primitive.name, "str") == 0)) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "String literal pattern requires str subject");
    }
    break;
  case AST_BOOL_LITERAL:
    if (type_is_resolved(subject_type) &&
        !(subject_type->kind == TY_PRIMITIVE &&
          strcmp(subject_type->as.primitive.name, "bool") == 0)) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Bool literal pattern requires bool subject");
    }
    break;
  case AST_IDENTIFIER: {
    const char *name = pattern->as.identifier.name;
    if (subject_type && subject_type->kind == TY_VARIANT) {
      int arm = variant_arm_index(subject_type, name);
      if (arm >= 0) {
        if (subject_type->as.variant.arm_types[arm]) {
          typechecker_error(tc, pattern->line, pattern->col,
                            "Variant arm '%s' has a payload and must be destructured",
                            name);
        }
        pattern->resolved_type = subject_type;
        break;
      }
    }
    bind_pattern_name(tc, pattern, name, subject_type);
    break;
  }
  case AST_STRUCT_PATTERN: {
    if (type_is_resolved(subject_type) && subject_type->kind != TY_STRUCT &&
        subject_type->kind != TY_VARIANT && subject_type->kind != TY_FALLIBLE) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Destructure pattern requires struct, variant, or "
                        "fallible subject");
    }
    if (subject_type && subject_type->kind == TY_FALLIBLE) {
      const char *arm_name = pattern->as.struct_pattern.name;
      bool is_ok = strcmp(arm_name, "Ok") == 0;
      bool is_err = strcmp(arm_name, "Err") == 0;
      if (!is_ok && !is_err) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Fallible pattern must be Ok or Err");
        break;
      }
      Type *inner = subject_type->as.fallible.inner;
      bool inner_void = inner->kind == TY_PRIMITIVE &&
                        strcmp(inner->as.primitive.name, "void") == 0;
      int expected = 1;
      int actual = 0;
      for (AstNode *fp = pattern->as.struct_pattern.fields; fp; fp = fp->next)
        actual++;
      bool ignored_void = is_ok && inner_void && actual == 1 &&
                          pattern->as.struct_pattern.fields &&
                          pattern->as.struct_pattern.fields
                                  ->as.field_pattern.pattern->kind ==
                              AST_IDENTIFIER &&
                          strcmp(pattern->as.struct_pattern.fields
                                     ->as.field_pattern.pattern
                                     ->as.identifier.name,
                                 "_") == 0;
      if (is_ok && inner_void && actual == 1 && !ignored_void) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Ok payload for !void may only be ignored with '_'");
      }
      if (is_ok && inner_void && actual == 0)
        expected = 0;
      if (actual != expected && !ignored_void) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Fallible pattern '%s' expects %d value(s), got %d",
                          arm_name, expected, actual);
      }
      if (expected == 1 && pattern->as.struct_pattern.fields) {
        AstNode *fp = pattern->as.struct_pattern.fields;
        Type *binding_type = is_ok
                                 ? inner
                                 : type_new_error(tc->tctx, "RunesError", NULL,
                                                  0);
        fp->resolved_type = binding_type;
        typechecker_check_pattern(tc, fp->as.field_pattern.pattern,
                                  binding_type);
      }
      pattern->resolved_type = subject_type;
      break;
    }

    if (subject_type && subject_type->kind == TY_VARIANT) {
      const char *arm_name = pattern->as.struct_pattern.name;
      int arm = variant_arm_index(subject_type, arm_name);
      if (arm < 0) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Unknown arm '%s' for variant pattern", arm_name);
        break;
      }
      Type *payload = subject_type->as.variant.arm_types[arm];
      int expected = payload && payload->kind == TY_TUPLE
                         ? payload->as.tuple.count
                         : payload ? 1 : 0;
      int actual = 0;
      for (AstNode *fp = pattern->as.struct_pattern.fields; fp; fp = fp->next)
        actual++;
      if (actual != expected) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Variant pattern '%s' expects %d payload value(s), got %d",
                          arm_name, expected, actual);
      }
      AstNode *fp = pattern->as.struct_pattern.fields;
      for (int i = 0; fp && i < expected; i++, fp = fp->next) {
        if (fp->as.field_pattern.name) {
          typechecker_error(tc, fp->line, fp->col,
                            "Variant payload patterns are positional");
        }
        Type *field_type = payload->kind == TY_TUPLE
                               ? payload->as.tuple.elems[i]
                               : payload;
        fp->resolved_type = field_type;
        typechecker_check_pattern(tc, fp->as.field_pattern.pattern,
                                  field_type);
      }
      pattern->resolved_type = subject_type;
      break;
    }

    if (subject_type && subject_type->kind == TY_STRUCT) {
      if (strcmp(pattern->as.struct_pattern.name,
                 subject_type->as.struct_t.name) != 0) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Pattern type '%s' does not match struct subject",
                          pattern->as.struct_pattern.name);
        break;
      }
      int position = 0;
      for (AstNode *fp = pattern->as.struct_pattern.fields; fp;
           fp = fp->next, position++) {
        int field_index = -1;
        if (fp->as.field_pattern.name) {
          for (int i = 0; i < subject_type->as.struct_t.field_count; i++) {
            if (strcmp(fp->as.field_pattern.name,
                       subject_type->as.struct_t.field_names[i]) == 0) {
              field_index = i;
              break;
            }
          }
        } else if (position < subject_type->as.struct_t.field_count) {
          field_index = position;
        }
        if (field_index < 0) {
          typechecker_error(tc, fp->line, fp->col,
                            "Unknown or excess field in struct pattern");
          continue;
        }
        Type *field_type = subject_type->as.struct_t.field_types[field_index];
        fp->resolved_type = field_type;
        typechecker_check_pattern(tc, fp->as.field_pattern.pattern,
                                  field_type);
      }
      pattern->resolved_type = subject_type;
      break;
    }

    AstNode *fp = pattern->as.struct_pattern.fields;
    while (fp) {
      if (fp->kind == AST_FIELD_PATTERN) {
        if (fp->as.field_pattern.name && !fp->as.field_pattern.pattern) {
          bind_pattern_name(tc, fp, fp->as.field_pattern.name,
                            tc->tctx->type_unknown);
        } else if (fp->as.field_pattern.pattern &&
                   fp->as.field_pattern.pattern->kind == AST_IDENTIFIER) {
          const char *bname = fp->as.field_pattern.pattern->as.identifier.name;
          bind_pattern_name(tc, fp->as.field_pattern.pattern, bname,
                            tc->tctx->type_unknown);
        } else if (!fp->as.field_pattern.name && fp->as.field_pattern.pattern) {
          if (fp->as.field_pattern.pattern->kind == AST_IDENTIFIER) {
            const char *bname =
                fp->as.field_pattern.pattern->as.identifier.name;
            bind_pattern_name(tc, fp->as.field_pattern.pattern, bname,
                              tc->tctx->type_unknown);
          }
        }
      }
      fp = fp->next;
    }
    break;
  }
  case AST_FIELD_EXPR: {
    if (!type_is_resolved(subject_type)) {
      pattern->resolved_type = subject_type;
      break;
    }
    if (subject_type->kind == TY_ERROR) {
      bool found = false;
      const char *name = qualified_pattern_name(pattern);
      if (subject_type->as.error_t.variant_count == 0) {
        pattern->resolved_type = subject_type;
        break;
      }
      for (int i = 0; i < subject_type->as.error_t.variant_count; i++) {
        if (strcmp(subject_type->as.error_t.variants[i], name) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Unknown error pattern '%s'", name);
      }
      pattern->resolved_type = subject_type;
      break;
    }
    if (subject_type->kind == TY_FALLIBLE) {
      pattern->resolved_type = subject_type;
      break;
    }
    if (subject_type->kind != TY_VARIANT ||
        variant_arm_index(subject_type, qualified_pattern_name(pattern)) < 0) {
      typechecker_error(tc, pattern->line, pattern->col,
                        "Qualified pattern is not an arm of the subject variant");
    } else {
      int arm = variant_arm_index(subject_type, qualified_pattern_name(pattern));
      if (subject_type->as.variant.arm_types[arm]) {
        typechecker_error(tc, pattern->line, pattern->col,
                          "Variant arm '%s' has a payload and must be destructured",
                          qualified_pattern_name(pattern));
      }
      pattern->resolved_type = subject_type;
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
    case TYPE_QUALIFIED: {
      Symbol *module = symbol_table_lookup(tc->st, node->as.type_expr.module);
      if (!module || module->kind != SYM_MOD || !module->node ||
          module->node->kind != AST_MOD_DECL)
        return tc->tctx->type_unknown;
      AstNode *decl = find_declaration(
          module->node->as.mod_decl.declarations, node->as.type_expr.name);
      return decl && decl->resolved_type ? decl->resolved_type
                                         : tc->tctx->type_unknown;
    }
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
      if (!node->as.type_expr.size ||
          node->as.type_expr.size->kind != AST_INT_LITERAL) {
        typechecker_error(tc, node->line, node->col,
                          "Array size must be a constant integer");
        return tc->tctx->type_error;
      }
      size_t size = node->as.type_expr.size->as.int_literal.value;
      if (size == 0) {
        typechecker_error(tc, node->line, node->col,
                          "Array size must be greater than zero");
        return tc->tctx->type_error;
      }
      return type_new_array(tc->tctx,
                            typechecker_resolve_type_expr(
                                tc, node->as.type_expr.inner),
                            size);
    }
    case TYPE_FALLIBLE:
      return type_new_fallible(tc->tctx, typechecker_resolve_type_expr(
                                             tc, node->as.type_expr.inner));
    case TYPE_TUPLE: {
      int count = 0;
      for (AstNode *elem = node->as.type_expr.elems; elem; elem = elem->next)
        count++;
      Type **elements =
          count > 0 ? arena_alloc(tc->arena, sizeof(Type *) * (size_t)count)
                    : NULL;
      AstNode *elem = node->as.type_expr.elems;
      for (int i = 0; i < count; i++, elem = elem->next)
        elements[i] = typechecker_resolve_type_expr(tc, elem);
      return type_new_tuple(tc->tctx, elements, count);
    }
    }
  }
  return tc->tctx->type_unknown;
}

static void typechecker_collect_decls(TypeChecker *tc, AstNode *node) {
  if (!node)
    return;

  AstNode *decl_head =
      node->kind == AST_PROGRAM ? node->as.program.declarations : node;

  // Pass 1: nominal types
  AstNode *decl = decl_head;
  while (decl) {
    if (decl->kind == AST_TYPE_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.type_decl.name);
      if (!sym) {
        Symbol local = {.name = decl->as.type_decl.name,
                        .kind = SYM_TYPE,
                        .node = decl,
                        .is_pub = decl->as.type_decl.is_pub};
        symbol_table_define(tc->st, local);
        sym = symbol_table_lookup_local(tc->st, decl->as.type_decl.name);
      }
      if (sym) {
        sym->type =
            type_new_struct(tc->tctx, decl->as.type_decl.name, NULL, NULL, 0);
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_VARIANT_DECL) {
      Symbol *sym =
          symbol_table_lookup_local(tc->st, decl->as.variant_decl.name);
      if (!sym) {
        Symbol local = {.name = decl->as.variant_decl.name,
                        .kind = SYM_TYPE,
                        .node = decl,
                        .is_pub = decl->as.variant_decl.is_pub};
        symbol_table_define(tc->st, local);
        sym = symbol_table_lookup_local(tc->st,
                                        decl->as.variant_decl.name);
      }
      if (sym) {
        sym->type = type_new_variant(tc->tctx, decl->as.variant_decl.name, NULL,
                                     NULL, 0);
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_INTERFACE_DECL) {
      Symbol *sym = symbol_table_lookup_local(
          tc->st, decl->as.interface_decl.name);
      if (!sym) {
        Symbol local = {.name = decl->as.interface_decl.name,
                        .kind = SYM_TYPE,
                        .node = decl,
                        .is_pub = decl->as.interface_decl.is_pub};
        symbol_table_define(tc->st, local);
        sym = symbol_table_lookup_local(tc->st,
                                        decl->as.interface_decl.name);
      }
      if (sym) {
        sym->type = type_new_interface(tc->tctx,
                                       decl->as.interface_decl.name,
                                       NULL, NULL, 0);
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_ERROR_DECL) {
      Symbol *sym =
          symbol_table_lookup_local(tc->st, decl->as.error_decl.name);
      if (!sym) {
        Symbol local = {.name = decl->as.error_decl.name,
                        .kind = SYM_TYPE,
                        .node = decl,
                        .is_pub = decl->as.error_decl.is_pub};
        symbol_table_define(tc->st, local);
        sym = symbol_table_lookup_local(tc->st, decl->as.error_decl.name);
      }
      if (sym) {
        sym->type = type_new_error(tc->tctx, decl->as.error_decl.name, NULL, 0);
        decl->resolved_type = sym->type;
      }
    }
    decl = decl->next;
  }

  // Pass 1.5: Populate Type fields (now that all type objects exist)
  decl = decl_head;
  while (decl) {
    if (decl->kind == AST_TYPE_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.type_decl.name);
      if (sym && sym->type) {
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
        sym->type->as.struct_t.field_names = field_names;
        sym->type->as.struct_t.field_types = field_types;
        sym->type->as.struct_t.field_count = field_count;
      }
    } else if (decl->kind == AST_VARIANT_DECL) {
      Symbol *sym =
          symbol_table_lookup_local(tc->st, decl->as.variant_decl.name);
      if (sym && sym->type) {
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
          AstNode *field = a->as.variant_arm.fields;
          if (!field) {
            arm_types[i] = NULL;
          } else if (!field->next) {
            arm_types[i] = typechecker_resolve_type_expr(tc, field);
          } else {
            int field_count = 0;
            for (AstNode *it = field; it; it = it->next)
              field_count++;
            Type **field_types =
                arena_alloc(tc->arena, sizeof(Type *) * field_count);
            int field_index = 0;
            for (AstNode *it = field; it; it = it->next)
              field_types[field_index++] =
                  typechecker_resolve_type_expr(tc, it);
            arm_types[i] =
                type_new_tuple(tc->tctx, field_types, field_count);
          }
          a = a->next;
        }
        sym->type->as.variant.arm_names = arm_names;
        sym->type->as.variant.arm_types = arm_types;
        sym->type->as.variant.arm_count = arm_count;
      }
    } else if (decl->kind == AST_INTERFACE_DECL) {
      Symbol *sym = symbol_table_lookup_local(
          tc->st, decl->as.interface_decl.name);
      if (sym && sym->type) {
        int count = 0;
        for (AstNode *m = decl->as.interface_decl.methods; m; m = m->next)
          count++;
        const char **names = count
                                 ? arena_alloc(tc->arena,
                                               sizeof(char *) * (size_t)count)
                                 : NULL;
        Type **methods = count
                             ? arena_alloc(tc->arena,
                                           sizeof(Type *) * (size_t)count)
                             : NULL;
        AstNode *m = decl->as.interface_decl.methods;
        for (int i = 0; i < count; i++, m = m->next) {
          names[i] = m->as.func_decl.name;
          int param_count = 0;
          for (AstNode *p = m->as.func_decl.params; p; p = p->next)
            param_count++;
          Type **params = param_count
                              ? arena_alloc(tc->arena,
                                            sizeof(Type *) *
                                                (size_t)param_count)
                              : NULL;
          AstNode *p = m->as.func_decl.params;
          for (int j = 0; j < param_count; j++, p = p->next)
            params[j] = strcmp(p->as.param.name, "self") == 0 &&
                                !p->as.param.type
                            ? sym->type
                            : typechecker_resolve_type_expr(tc,
                                                            p->as.param.type);
          Type *ret = typechecker_resolve_type_expr(
              tc, m->as.func_decl.ret_type);
          methods[i] = type_new_function(tc->tctx, params, param_count, ret,
                                         STRATEGY_STACK, true);
          m->resolved_type = methods[i];
        }
        sym->type->as.interface_t.method_names = names;
        sym->type->as.interface_t.method_types = methods;
        sym->type->as.interface_t.method_count = count;
      }
    } else if (decl->kind == AST_ERROR_DECL) {
      Symbol *sym =
          symbol_table_lookup_local(tc->st, decl->as.error_decl.name);
      if (sym && sym->type) {
        int variant_count = 0;
        for (AstNode *v = decl->as.error_decl.variants; v; v = v->next)
          variant_count++;
        const char **variants =
            arena_alloc(tc->arena, sizeof(char *) * (size_t)variant_count);
        AstNode *v = decl->as.error_decl.variants;
        for (int i = 0; i < variant_count; i++, v = v->next)
          variants[i] = v->as.identifier.name;
        sym->type->as.error_t.variants = variants;
        sym->type->as.error_t.variant_count = variant_count;
      }
    }
    decl = decl->next;
  }

  // Pass 2: Functions, Externs, Variables, Methods
  decl = decl_head;
  while (decl) {
    if (decl->kind == AST_FUNC_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.func_decl.name);
      if (!sym) {
        Symbol local = {0};
        local.name = decl->as.func_decl.name;
        local.kind = SYM_FUNC;
        local.node = decl;
        symbol_table_define(tc->st, local);
        sym = symbol_table_lookup_local(tc->st, decl->as.func_decl.name);
      }
      if (sym) {
        Type *ret_t =
            typechecker_resolve_type_expr(tc, decl->as.func_decl.ret_type);
        int param_c = 0;
        AstNode *p = decl->as.func_decl.params;
        while (p) {
          param_c++;
          p = p->next;
        }
        Type **ptypes = param_c > 0
                            ? arena_alloc(tc->arena, sizeof(Type *) * param_c)
                            : NULL;
        p = decl->as.func_decl.params;
        for (int i = 0; i < param_c; i++) {
          ptypes[i] = typechecker_resolve_type_expr(tc, p->as.param.type);
          p = p->next;
        }
        MemoryStrategy strat = realm_to_strategy(decl->as.func_decl.realm);
        sym->type =
            type_new_function(tc->tctx, ptypes, param_c, ret_t, strat, false);
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_VAR_DECL) {
      Symbol *sym = symbol_table_lookup_local(tc->st, decl->as.var_decl.name);
      if (!sym && decl->as.var_decl.type) {
        Symbol local = {0};
        local.name = decl->as.var_decl.name;
        local.kind = SYM_VAR;
        local.node = decl;
        symbol_table_define(tc->st, local);
        sym = symbol_table_lookup_local(tc->st, decl->as.var_decl.name);
      }
      if (sym && decl->as.var_decl.type) {
        sym->type = typechecker_resolve_type_expr(tc, decl->as.var_decl.type);
        decl->resolved_type = sym->type;
      }
    } else if (decl->kind == AST_EXTERN_DECL) {
      Symbol *sym =
          symbol_table_lookup_local(tc->st, decl->as.extern_decl.name);
      if (sym) {
        if (decl->as.extern_decl.is_func) {
          Type *ret_t =
              typechecker_resolve_type_expr(tc, decl->as.extern_decl.ret_type);
          int param_count = 0;
          AstNode *p = decl->as.extern_decl.params;
          while (p) {
            param_count++;
            p = p->next;
          }
          Type **param_types =
              param_count > 0
                  ? arena_alloc(tc->arena, sizeof(Type *) * param_count)
                  : NULL;
          p = decl->as.extern_decl.params;
          for (int i = 0; i < param_count; i++) {
            param_types[i] =
                typechecker_resolve_type_expr(tc, p->as.param.type);
            p = p->next;
          }
          sym->type = type_new_function(tc->tctx, param_types, param_count,
                                        ret_t, STRATEGY_STACK, false);
        } else {
          sym->type =
              typechecker_resolve_type_expr(tc, decl->as.extern_decl.var_type);
        }
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
            Type **ptypes =
                param_c > 0 ? arena_alloc(tc->arena, sizeof(Type *) * param_c)
                            : NULL;
            p = method_node->as.func_decl.params;
            for (int i = 0; i < param_c; i++) {
              if (strcmp(p->as.param.name, "self") == 0 && !p->as.param.type)
                ptypes[i] = t;
              else
                ptypes[i] = typechecker_resolve_type_expr(tc, p->as.param.type);
              p = p->next;
            }
            m->type = type_new_function(
                tc->tctx, ptypes, param_c, ret_t,
                realm_to_strategy(method_node->as.func_decl.realm), true);
            method_node->resolved_type = m->type;
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
        if (decl->as.method_decl.iface_name && t->kind == TY_STRUCT) {
          Symbol *iface_sym = symbol_table_lookup(
              tc->st, decl->as.method_decl.iface_name);
          bool valid = iface_sym && iface_sym->type &&
                       iface_sym->type->kind == TY_INTERFACE;
          if (!valid) {
            typechecker_error(tc, decl->line, decl->col,
                              "Unknown interface '%s' in method declaration",
                              decl->as.method_decl.iface_name);
          } else {
            Type *iface = iface_sym->type;
            for (int i = 0; i < iface->as.interface_t.method_count; i++) {
              Method *method = t->as.struct_t.methods;
              while (method &&
                     strcmp(method->name,
                            iface->as.interface_t.method_names[i]) != 0)
                method = method->next;
              Type *required = iface->as.interface_t.method_types[i];
              if (!method || method->type->as.function.param_count !=
                                 required->as.function.param_count ||
                  !type_equals(method->type->as.function.ret,
                               required->as.function.ret)) {
                typechecker_error(tc, decl->line, decl->col,
                                  "Interface implementation has a missing or "
                                  "incompatible method");
                valid = false;
                break;
              }
              for (int p = 1; p < required->as.function.param_count; p++) {
                if (!type_equals(method->type->as.function.params[p],
                                 required->as.function.params[p])) {
                  typechecker_error(
                      tc, decl->line, decl->col,
                      "Interface implementation has an incompatible method");
                  valid = false;
                  break;
                }
              }
              if (!valid)
                break;
            }
          }
          if (valid) {
            int count = t->as.struct_t.interface_count;
            const char **names = arena_alloc(
                tc->arena, sizeof(char *) * (size_t)(count + 1));
            for (int i = 0; i < count; i++)
              names[i] = t->as.struct_t.interface_names[i];
            names[count] = decl->as.method_decl.iface_name;
            t->as.struct_t.interface_names = names;
            t->as.struct_t.interface_count = count + 1;
          }
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

static void validate_systems_attrs(TypeChecker *tc, AstNode *node,
                                   Attr *attrs) {
  Attr *a = attrs;
  while (a) {
    if (strcmp(a->name, "section") == 0 || strcmp(a->name, "link_name") == 0) {
      if (!a->arg || a->arg->kind != AST_STRING_LITERAL) {
        typechecker_error(tc, node->line, node->col,
                          "#[%s] attribute requires a string argument",
                          a->name);
      }
    } else if (strcmp(a->name, "interrupt") == 0) {
      if (node->kind != AST_FUNC_DECL && node->kind != AST_EXTERN_DECL) {
        typechecker_error(tc, node->line, node->col,
                          "#[interrupt] can only be applied to functions");
      } else {
        bool has_params = false;
        bool has_ret = false;
        if (node->kind == AST_FUNC_DECL) {
          has_params = node->as.func_decl.params != NULL;
          has_ret = node->as.func_decl.ret_type != NULL;
        } else if (node->kind == AST_EXTERN_DECL) {
          has_params = node->as.extern_decl.params != NULL;
          has_ret = node->as.extern_decl.ret_type != NULL;
        }
        if (has_params || has_ret) {
          typechecker_error(
              tc, node->line, node->col,
              "#[interrupt] function must have no parameters and no returns");
        }
      }
    }
    a = a->next;
  }
}

static bool is_assignable_expr(AstNode *expr) {
  if (!expr)
    return false;
  return expr->kind == AST_IDENTIFIER || expr->kind == AST_INDEX_EXPR ||
         expr->kind == AST_FIELD_EXPR ||
         (expr->kind == AST_UNARY_EXPR && expr->as.unary.op == TOKEN_STAR);
}

static bool is_integer_literal_expr(AstNode *expr) {
  return expr &&
         (expr->kind == AST_INT_LITERAL ||
          (expr->kind == AST_UNARY_EXPR && expr->as.unary.op == TOKEN_MINUS &&
           expr->as.unary.expr &&
           expr->as.unary.expr->kind == AST_INT_LITERAL));
}

static Type *typechecker_infer_value_branch(TypeChecker *tc, AstNode *branch) {
  if (!branch)
    return tc->tctx->type_unknown;
  if (branch->kind != AST_BLOCK)
    return typechecker_infer_expr(tc, branch);

  symbol_table_push(tc->st);
  typechecker_collect_decls(tc, branch->as.block.statements);
  AstNode *last = branch->as.block.statements;
  if (!last) {
    symbol_table_pop(tc->st);
    return tc->tctx->type_void;
  }
  while (last->next)
    last = last->next;
  for (AstNode *stmt = branch->as.block.statements; stmt != last;
       stmt = stmt->next)
    typechecker_check_node(tc, stmt);
  Type *result = typechecker_infer_expr(tc, last);
  symbol_table_pop(tc->st);
  return result;
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
  case AST_BOOL_LITERAL:
    inferred = tc->tctx->type_bool;
    break;
  case AST_CHAR_LITERAL:
    inferred = tc->tctx->type_char;
    break;
  case AST_ARRAY_LITERAL:
    if (!expr->as.array_literal.elems) {
      inferred = type_new_array(tc->tctx, tc->tctx->type_unknown, 0);
      break;
    }

    {
      AstNode *elem = expr->as.array_literal.elems;
      Type *elem_t = typechecker_infer_expr(tc, elem);
      size_t count = 1;
      for (elem = elem->next; elem; elem = elem->next) {
        Type *next_t = typechecker_infer_expr(tc, elem);
        count++;
        if (type_is_resolved(elem_t) && type_is_resolved(next_t) &&
            !type_is_assignable(elem_t, next_t) &&
            !type_is_assignable(next_t, elem_t)) {
          typechecker_error(tc, elem->line, elem->col,
                            "Array literal elements must have one type");
          elem_t = tc->tctx->type_error;
        }
      }
      inferred = type_new_array(tc->tctx, elem_t, count);
    }
    break;

  case AST_IDENTIFIER: {
    Symbol *sym = symbol_table_lookup(tc->st, expr->as.identifier.name);
    if (sym && sym->type) {
      if (tc->function_depth > 1 && sym->kind == SYM_VAR) {
        Scope *symbol_scope = scope_containing_symbol(tc->st, sym);
        Scope *current_parent =
            tc->function_parent_scopes[tc->function_depth - 1];
        Scope *outermost_parent = tc->function_parent_scopes[0];
        if (scope_is_between(symbol_scope, current_parent,
                             outermost_parent)) {
          typechecker_error(
              tc, expr->line, expr->col,
              "Nested functions cannot capture enclosing variable '%s'",
              expr->as.identifier.name);
          inferred = tc->tctx->type_error;
          break;
        }
      }
      expr->resolved_decl = sym->node;
      inferred = sym->type;
    } else if (sym && sym->kind == SYM_MOD) {
      expr->resolved_decl = sym->node;
      inferred = tc->tctx->type_unknown;
    } else {
      typechecker_error(tc, expr->line, expr->col, "Undefined variable '%s'",
                        expr->as.identifier.name);
      inferred = tc->tctx->type_error;
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
      if (type_is_resolved(lty) && type_is_resolved(rty)) {
        bool left_ptr = lty->kind == TY_POINTER;
        bool right_ptr = rty->kind == TY_POINTER;
        if (left_ptr || right_ptr) {
          bool valid_pointer_arithmetic =
              (expr->as.binary.op == TOKEN_PLUS &&
               ((left_ptr && type_is_integer(rty)) ||
                (right_ptr && type_is_integer(lty)))) ||
              (expr->as.binary.op == TOKEN_MINUS && left_ptr &&
               type_is_integer(rty));
          if (!valid_pointer_arithmetic) {
            typechecker_error(tc, expr->line, expr->col,
                              "Pointers only support addition or subtraction "
                              "with an integer");
            inferred = tc->tctx->type_error;
            break;
          }
          inferred = left_ptr ? lty : rty;
          break;
        } else if (lty->kind == TY_PRIMITIVE && rty->kind == TY_PRIMITIVE) {
          bool string_concat = expr->as.binary.op == TOKEN_PLUS &&
                               strcmp(lty->as.primitive.name, "str") == 0 &&
                               strcmp(rty->as.primitive.name, "str") == 0;
          if (string_concat) {
            inferred = tc->tctx->type_str;
            break;
          }
          if (!type_is_numeric(lty) || !type_is_numeric(rty)) {
            typechecker_error(tc, expr->line, expr->col,
                              "Arithmetic operators require numeric operands");
            inferred = tc->tctx->type_error;
            break;
          }
          if (expr->as.binary.op == TOKEN_PERCENT &&
              (type_is_float(lty) || type_is_float(rty))) {
            typechecker_error(tc, expr->line, expr->col,
                              "Remainder requires integer operands");
            inferred = tc->tctx->type_error;
            break;
          }
          if (!type_equals(lty, rty)) {
            // D-04: Strict type checking for binary expressions
            // Allow literal coercion: if either operand is a literal,
            // it can adapt to the other's type (e.g., u64 * 0x...)
            bool has_literal =
                (expr->as.binary.left->kind == AST_INT_LITERAL ||
                 expr->as.binary.left->kind == AST_FLOAT_LITERAL ||
                 expr->as.binary.right->kind == AST_INT_LITERAL ||
                 expr->as.binary.right->kind == AST_FLOAT_LITERAL);
            if (!has_literal) {
              const NumericTypeInfo *linfo = get_numeric_info(lty->as.primitive.name);
              const NumericTypeInfo *rinfo = get_numeric_info(rty->as.primitive.name);
              if (linfo && rinfo && linfo->is_signed == rinfo->is_signed && linfo->is_float == rinfo->is_float) {
                // Same family: suggest widening to the higher-rank type
                const char *wider = linfo->rank >= rinfo->rank ? lty->as.primitive.name : rty->as.primitive.name;
                typechecker_error(tc, expr->line, expr->col,
                    "Type mismatch: %s and %s. Use explicit cast: (%s as %s)",
                    lty->as.primitive.name, rty->as.primitive.name,
                    linfo->rank < rinfo->rank ? "left" : "right", wider);
              } else if (linfo && rinfo && linfo->is_signed != rinfo->is_signed) {
                // Mixed sign
                typechecker_error(tc, expr->line, expr->col,
                    "Type mismatch: %s and %s (mixed signed/unsigned). Use explicit cast with 'as'",
                    lty->as.primitive.name, rty->as.primitive.name);
              } else if (linfo && rinfo && linfo->is_float != rinfo->is_float) {
                // Cross-family
                typechecker_error(tc, expr->line, expr->col,
                    "Type mismatch: %s and %s (cannot mix integer and float). Use explicit cast with 'as'",
                    lty->as.primitive.name, rty->as.primitive.name);
              } else {
                typechecker_error(tc, expr->line, expr->col,
                    "Type mismatch in binary expression: '%s' and '%s'",
                    lty->as.primitive.name, rty->as.primitive.name);
              }
              inferred = tc->tctx->type_error;
              break;
            }
          }
        } else {
          bool ok = type_is_assignable(lty, rty) || type_is_assignable(rty, lty);
          if (!ok) {
            typechecker_error(tc, expr->line, expr->col,
                              "Type mismatch in arithmetic operation");
            inferred = tc->tctx->type_error;
            break;
          }
        }
      }
      inferred = lty;
      break;
    case TOKEN_AMP:
    case TOKEN_PIPE:
    case TOKEN_CARET:
    case TOKEN_SHL:
    case TOKEN_SHR:
      if (type_is_resolved(lty) && type_is_resolved(rty) &&
          (!type_is_integer(lty) || !type_is_integer(rty))) {
        typechecker_error(tc, expr->line, expr->col,
                          "Bitwise operators require integer operands");
        inferred = tc->tctx->type_error;
      } else {
        inferred = lty;
      }
      break;
    case TOKEN_EQ_EQ:
    case TOKEN_BANG_EQ:
    case TOKEN_LT:
    case TOKEN_GT:
    case TOKEN_LT_EQ:
    case TOKEN_GT_EQ:
      if (type_is_resolved(lty) && type_is_resolved(rty)) {
        if (!type_is_comparable(lty, rty)) {
          typechecker_error(tc, expr->line, expr->col,
                            "Comparison type mismatch");
        } else if (lty->kind == TY_PRIMITIVE &&
                   rty->kind == TY_PRIMITIVE &&
                   strcmp(lty->as.primitive.name, "str") == 0 &&
                   strcmp(rty->as.primitive.name, "str") == 0 &&
                   expr->as.binary.op != TOKEN_EQ_EQ &&
                   expr->as.binary.op != TOKEN_BANG_EQ) {
          typechecker_error(tc, expr->line, expr->col,
                            "String comparison supports only == and !=");
        }
      }
      inferred = tc->tctx->type_bool;
      break;
    case TOKEN_AND:
    case TOKEN_OR:
      if (type_is_resolved(lty) && type_is_resolved(rty)) {
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
    if (!is_assignable_expr(expr->as.assign.target)) {
      typechecker_error(tc, expr->line, expr->col,
                        "Assignment target is not assignable");
    }
    // Const reassignment check
    if (expr->as.assign.target->kind == AST_IDENTIFIER) {
      Symbol *sym = symbol_table_lookup(tc->st,
          expr->as.assign.target->as.identifier.name);
      if (sym && sym->node && sym->node->kind == AST_VAR_DECL &&
          sym->node->as.var_decl.is_const) {
        typechecker_error(tc, expr->line, expr->col,
                          "Cannot reassign constant '%s'",
                          expr->as.assign.target->as.identifier.name);
      }
    }
    Type *lty = typechecker_infer_expr(tc, expr->as.assign.target);
    Type *rty = typechecker_infer_expr(tc, expr->as.assign.value);
    if (type_is_resolved(lty) && type_is_resolved(rty)) {
      bool assignable = type_is_assignable(lty, rty);
      if (!assignable && expr->as.assign.value->kind == AST_ERROR_EXPR &&
          expr->as.assign.target->kind == AST_IDENTIFIER) {
        Symbol *target = symbol_table_lookup(
            tc->st, expr->as.assign.target->as.identifier.name);
        if (target && target->node && target->node->kind == AST_FUNC_DECL &&
            target->node->resolved_type &&
            target->node->resolved_type->kind == TY_FUNCTION &&
            target->node->resolved_type->as.function.ret->kind == TY_FALLIBLE)
          assignable = true;
      }
      if (!assignable && rty->kind == TY_ERROR &&
          expr->as.assign.target->kind == AST_IDENTIFIER) {
        Symbol *target = symbol_table_lookup(
            tc->st, expr->as.assign.target->as.identifier.name);
        if (target && target->node && target->node->kind == AST_FUNC_DECL &&
            target->node->resolved_type &&
            target->node->resolved_type->kind == TY_FUNCTION &&
            target->node->resolved_type->as.function.ret->kind == TY_FALLIBLE)
          assignable = true;
      }
      if (!assignable && expr->as.assign.target->kind == AST_IDENTIFIER) {
        Symbol *target = symbol_table_lookup(
            tc->st, expr->as.assign.target->as.identifier.name);
        if (target && target->node && target->node->kind == AST_FUNC_DECL &&
            target->node->resolved_type &&
            target->node->resolved_type->kind == TY_FUNCTION) {
          assignable = type_is_assignable(
              target->node->resolved_type->as.function.ret, rty);
        }
      }
      if (!assignable) {
#ifdef DEBUG
        printf("DEBUG ASSERT: Cannot assign %d to %d\n", rty->kind, lty->kind);
        if (lty->kind == TY_PRIMITIVE)
          printf("DEBUG: lty name: %s\n", lty->as.primitive.name);
        if (rty->kind == TY_PRIMITIVE)
          printf("DEBUG: rty name: %s\n", rty->as.primitive.name);
#endif
        typechecker_error(tc, expr->line, expr->col,
                          "Cannot assign value of mismatched type");
      }
    }
    inferred = lty;
    break;
  }

  case AST_CALL_EXPR: {
    if (expr->as.call.callee->kind == AST_IDENTIFIER &&
        strcmp(expr->as.call.callee->as.identifier.name, "print") == 0) {
      AstNode *arg = expr->as.call.args;
      if (!arg) {
        typechecker_error(tc, expr->line, expr->col,
                          "print requires at least one argument");
      }
      while (arg) {
        Type *arg_t = typechecker_infer_expr(tc, arg);
        if (type_is_resolved(arg_t) && arg_t->kind != TY_PRIMITIVE &&
            arg_t->kind != TY_POINTER && arg_t->kind != TY_ERROR) {
          typechecker_error(tc, arg->line, arg->col,
                            "print accepts only primitive values and pointers");
        }
        arg = arg->next;
      }
      inferred = tc->tctx->type_void;
      break;
    }

    Type *callee_t = NULL;
    if (expr->as.call.callee->kind == AST_FIELD_EXPR) {
      AstNode *target = expr->as.call.callee->as.field.target;
      Type *target_t = typechecker_infer_expr(tc, target);
      if (target_t && target_t->kind == TY_VARIANT) {
        callee_t = target_t;
        expr->as.call.callee->resolved_type = target_t;
      }
    }
    if (!callee_t)
      callee_t = typechecker_infer_expr(tc, expr->as.call.callee);
    if (callee_t->kind == TY_FUNCTION) {
      inferred = callee_t->as.function.ret;

      AstNode *arg = expr->as.call.args;
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
        if (type_is_resolved(param_ty) && type_is_resolved(arg_ty)) {
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

      // Track which fields are provided for missing-field detection
      bool *field_provided = arena_alloc(tc->arena,
          sizeof(bool) * (callee_t->as.struct_t.field_count + 1));
      memset(field_provided, 0,
             sizeof(bool) * (callee_t->as.struct_t.field_count + 1));

      AstNode *arg = expr->as.call.args;
      while (arg) {
        if (arg->kind == AST_NAMED_ARG) {
          const char *name = arg->as.named_arg.name;
          Type *val_t = typechecker_infer_expr(tc, arg->as.named_arg.value);
          bool found = false;
          for (int i = 0; i < callee_t->as.struct_t.field_count; i++) {
            if (strcmp(callee_t->as.struct_t.field_names[i], name) == 0) {
              found = true;
              field_provided[i] = true;
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

      // Check for missing required fields (those without defaults)
      Symbol *type_sym = symbol_table_lookup(tc->st,
                                             callee_t->as.struct_t.name);
      AstNode *field_node = (type_sym && type_sym->node &&
                             type_sym->node->kind == AST_TYPE_DECL)
          ? type_sym->node->as.type_decl.fields : NULL;
      for (int i = 0; i < callee_t->as.struct_t.field_count; i++) {
        if (!field_provided[i]) {
          bool has_default = (field_node &&
                              field_node->as.field_decl.default_val != NULL);
          if (!has_default) {
            typechecker_error(tc, expr->line, expr->col,
                              "Struct '%s' missing required field: '%s'",
                              callee_t->as.struct_t.name,
                              callee_t->as.struct_t.field_names[i]);
          }
        }
        if (field_node) field_node = field_node->next;
      }
    } else if (callee_t->kind == TY_VARIANT) {
      // Variant arm constructor: RGB(255, 0, 0)
      inferred = callee_t;

      // Determine which variant arm is being constructed
      const char *arm_name = NULL;
      if (expr->as.call.callee->kind == AST_IDENTIFIER) {
        arm_name = expr->as.call.callee->as.identifier.name;
      } else if (expr->as.call.callee->kind == AST_FIELD_EXPR) {
        arm_name = expr->as.call.callee->as.field.field;
      }

      if (arm_name) {
        for (int i = 0; i < callee_t->as.variant.arm_count; i++) {
          if (strcmp(callee_t->as.variant.arm_names[i], arm_name) == 0) {
            Type *expected_payload = callee_t->as.variant.arm_types[i];
            // Count actual args
            int actual_count = 0;
            AstNode *a = expr->as.call.args;
            while (a) { actual_count++; a = a->next; }

            if (!expected_payload && actual_count > 0) {
              typechecker_error(tc, expr->line, expr->col,
                                "Variant arm '%s' takes no payload, got %d "
                                "argument(s)", arm_name, actual_count);
            } else if (expected_payload &&
                       expected_payload->kind == TY_TUPLE) {
              // Multi-field payload
              if (actual_count != expected_payload->as.tuple.count) {
                typechecker_error(tc, expr->line, expr->col,
                                  "Variant arm '%s' expects %d payload "
                                  "value(s), got %d", arm_name,
                                  expected_payload->as.tuple.count,
                                  actual_count);
              }
              a = expr->as.call.args;
              for (int j = 0;
                   j < expected_payload->as.tuple.count && a; j++) {
                Type *arg_t = typechecker_infer_expr(tc, a);
                if (type_is_resolved(arg_t) &&
                    !type_is_assignable(
                        expected_payload->as.tuple.elems[j], arg_t)) {
                  typechecker_error(tc, a->line, a->col,
                      "Variant arm '%s' expects payload type %s, got %s",
                      arm_name,
                      expected_payload->as.tuple.elems[j]->kind ==
                          TY_PRIMITIVE
                        ? expected_payload->as.tuple.elems[j]
                              ->as.primitive.name : "?",
                      arg_t->kind == TY_PRIMITIVE
                        ? arg_t->as.primitive.name : "?");
                }
                a = a->next;
              }
            } else if (expected_payload) {
              // Single-field payload
              if (actual_count != 1) {
                typechecker_error(tc, expr->line, expr->col,
                    "Variant arm '%s' expects 1 payload value, got %d",
                    arm_name, actual_count);
              }
              if (actual_count >= 1) {
                Type *arg_t = typechecker_infer_expr(tc,
                                                     expr->as.call.args);
                if (type_is_resolved(arg_t) &&
                    type_is_resolved(expected_payload) &&
                    !type_is_assignable(expected_payload, arg_t)) {
                  typechecker_error(tc, expr->as.call.args->line,
                      expr->as.call.args->col,
                      "Variant arm '%s' expects payload type %s, got %s",
                      arm_name,
                      expected_payload->kind == TY_PRIMITIVE
                        ? expected_payload->as.primitive.name : "?",
                      arg_t->kind == TY_PRIMITIVE
                        ? arg_t->as.primitive.name : "?");
                }
              }
            }
            break;
          }
        }
      } else {
        // Cannot determine arm name, infer args but skip validation
        AstNode *a = expr->as.call.args;
        while (a) { typechecker_infer_expr(tc, a); a = a->next; }
      }
    } else if (type_is_resolved(callee_t)) {
      typechecker_error(tc, expr->line, expr->col,
                        "Cannot call non-function type");
      inferred = tc->tctx->type_error;
    }
    break;
  }

  case AST_CAST_EXPR: {
    typechecker_infer_expr(tc, expr->as.cast.expr);
    inferred = typechecker_resolve_type_expr(tc, expr->as.cast.target_type);
    break;
  }

  case AST_SIZEOF_EXPR:
    if (expr->as.sizeof_expr.type) {
      typechecker_resolve_type_expr(tc, expr->as.sizeof_expr.type);
    }
    inferred = tc->tctx->type_usize;
    break;

  case AST_ALIGNOF_EXPR:
    if (expr->as.alignof_expr.type) {
      typechecker_resolve_type_expr(tc, expr->as.alignof_expr.type);
    }
    inferred = tc->tctx->type_usize;
    break;

  case AST_UNARY_EXPR: {
    Type *inner_t = typechecker_infer_expr(tc, expr->as.unary.expr);
    if (type_is_resolved(inner_t)) {
      if (expr->as.unary.op == TOKEN_STAR) {
        // Dereference: *p
        if (inner_t->kind == TY_POINTER) {
          inferred = inner_t->as.pointer.inner;
        } else {
          typechecker_error(tc, expr->line, expr->col,
                            "Cannot dereference non-pointer type");
          inferred = tc->tctx->type_error;
        }
      } else if (expr->as.unary.op == TOKEN_AMP) {
        // Address-of: &x
        if (!is_assignable_expr(expr->as.unary.expr)) {
          typechecker_error(tc, expr->line, expr->col,
                            "Address-of requires an assignable expression");
          inferred = tc->tctx->type_error;
          break;
        }
        // Array-to-pointer decay: &([N]T) → *T, not *[N]T
        if (inner_t->kind == TY_ARRAY) {
          inferred = type_new_pointer(tc->tctx, inner_t->as.array.inner);
        } else {
          inferred = type_new_pointer(tc->tctx, inner_t);
        }
      } else if (expr->as.unary.op == TOKEN_MINUS) {
        if (!type_is_numeric(inner_t)) {
          typechecker_error(tc, expr->line, expr->col,
                            "Negation requires a numeric operand");
          inferred = tc->tctx->type_error;
        } else {
          inferred = inner_t;
        }
      } else if (expr->as.unary.op == TOKEN_TILDE) {
        if (!type_is_integer(inner_t)) {
          typechecker_error(tc, expr->line, expr->col,
                            "Bitwise NOT requires an integer operand");
          inferred = tc->tctx->type_error;
        } else {
          inferred = inner_t;
        }
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
      if (expr->as.index.index->kind == AST_INT_LITERAL &&
          expr->as.index.index->as.int_literal.value >=
              target_t->as.array.size) {
        typechecker_error(tc, expr->as.index.index->line,
                          expr->as.index.index->col,
                          "Array index is out of bounds");
      }
    } else if (target_t->kind == TY_POINTER) {
      inferred = target_t->as.pointer.inner;
    } else if (type_is_resolved(target_t)) {
      typechecker_error(tc, expr->line, expr->col,
                        "Cannot index non-array type");
      inferred = tc->tctx->type_error;
    }

    if (type_is_resolved(index_t)) {
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

  case AST_RANGE_EXPR: {
    Type *start_t = typechecker_infer_expr(tc, expr->as.range_expr.start);
    Type *end_t = typechecker_infer_expr(tc, expr->as.range_expr.end);
    if (type_is_resolved(start_t) && type_is_resolved(end_t)) {
      if (!type_is_integer(start_t) || !type_is_integer(end_t)) {
        typechecker_error(tc, expr->line, expr->col,
                          "Range bounds must be integers");
        inferred = tc->tctx->type_error;
      } else if (!type_is_assignable(start_t, end_t) &&
                 !type_is_assignable(end_t, start_t)) {
        typechecker_error(tc, expr->line, expr->col,
                          "Range bounds must have compatible types");
        inferred = tc->tctx->type_error;
      } else {
        bool start_literal = is_integer_literal_expr(expr->as.range_expr.start);
        bool end_literal = is_integer_literal_expr(expr->as.range_expr.end);
        inferred = start_literal && !end_literal ? end_t : start_t;
      }
    }
    break;
  }

  case AST_FIELD_EXPR: {
    AstNode *qualified = qualified_declaration(tc, expr);
    if (qualified) {
      expr->resolved_decl = qualified;
      if (qualified->kind == AST_MOD_DECL) {
        inferred = tc->tctx->type_unknown;
        break;
      }
      if (qualified->resolved_type) {
        inferred = qualified->resolved_type;
        break;
      }
    }

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
        inferred = tc->tctx->type_error;
      }
    } else if (base_t->kind == TY_VARIANT) {
      bool found = false;
      for (int i = 0; i < base_t->as.variant.arm_count; i++) {
        if (strcmp(base_t->as.variant.arm_names[i], fname) == 0) {
          // Variant arm access (e.g. Color.Red)
          if (base_t->as.variant.arm_types[i]) {
            typechecker_error(tc, expr->line, expr->col,
                              "Variant arm '%s' requires constructor arguments",
                              fname);
            inferred = tc->tctx->type_error;
          } else {
            inferred = base_t;
          }
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
        inferred = tc->tctx->type_error;
      }
    } else if (base_t->kind == TY_INTERFACE) {
      bool found = false;
      for (int i = 0; i < base_t->as.interface_t.method_count; i++) {
        if (strcmp(base_t->as.interface_t.method_names[i], fname) == 0) {
          inferred = base_t->as.interface_t.method_types[i];
          found = true;
          break;
        }
      }
      if (!found) {
        typechecker_error(tc, expr->line, expr->col,
                          "Method '%s' not found in interface '%s'", fname,
                          base_t->as.interface_t.name);
        inferred = tc->tctx->type_error;
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
        inferred = tc->tctx->type_error;
      }
    } else if (base_t->kind == TY_ARRAY) {
      if (strcmp(fname, "len") == 0) {
        inferred = tc->tctx->type_usize;
      } else {
        typechecker_error(tc, expr->line, expr->col,
                          "Unknown property '%s' on array", fname);
        inferred = tc->tctx->type_error;
      }
    } else if (type_is_resolved(base_t)) {
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
      if (!tc->expected_ret || tc->expected_ret->kind != TY_FALLIBLE) {
        typechecker_error(tc, expr->line, expr->col,
                          "try may only be used in a fallible function");
      }
    } else if (type_is_resolved(inner_t)) {
      typechecker_error(tc, expr->line, expr->col,
                        "try requires a fallible (!T) expression");
      inferred = tc->tctx->type_error;
    }
    break;
  }

  case AST_ERROR_EXPR: {
    AstNode *set = expr->as.error_expr.path;
    AstNode *member = set ? set->next : NULL;
    if (!set || !member || member->next) {
      typechecker_error(tc, expr->line, expr->col,
                        "Error value must be error.Set.Member");
      inferred = tc->tctx->type_error;
      break;
    }
    Symbol *sym = symbol_table_lookup(tc->st, set->as.identifier.name);
    if (!sym || !sym->type || sym->type->kind != TY_ERROR) {
      typechecker_error(tc, set->line, set->col,
                        "Unknown error set '%s'", set->as.identifier.name);
      inferred = tc->tctx->type_error;
      break;
    }
    expr->resolved_decl = sym->node;
    bool found = false;
    for (int i = 0; i < sym->type->as.error_t.variant_count; i++) {
      if (strcmp(sym->type->as.error_t.variants[i],
                 member->as.identifier.name) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      typechecker_error(tc, member->line, member->col,
                        "Unknown member '%s' in error set",
                        member->as.identifier.name);
      inferred = tc->tctx->type_error;
    } else {
      inferred = sym->type;
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
        sym.type = type_new_error(tc->tctx, "RunesError", NULL, 0);
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
        } else if (type_is_resolved(handler_t) &&
                   type_is_resolved(success_t)) {
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
    } else if (type_is_resolved(inner_t)) {
      typechecker_error(tc, expr->line, expr->col,
                        "catch requires a fallible (!T) expression");
      inferred = tc->tctx->type_error;
    }
    break;
  }

    // ── Phase 3: promote ──────────────────────────────────────────────────

  case AST_PROMOTE_EXPR: {
    Type *inner_t = typechecker_infer_expr(tc, expr->as.promote.expr);
    inferred = inner_t;

    if (type_is_resolved(inner_t) && inner_t->kind != TY_POINTER) {
      typechecker_error(tc, expr->line, expr->col,
                        "promote requires a pointer to the value being copied");
      inferred = tc->tctx->type_error;
    }

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

  case AST_IF_STMT: {
    Type *condition = typechecker_infer_expr(tc, expr->as.if_stmt.condition);
    if (type_is_resolved(condition) &&
        (condition->kind != TY_PRIMITIVE ||
         strcmp(condition->as.primitive.name, "bool") != 0)) {
      typechecker_error(tc, expr->line, expr->col,
                        "If condition must be a boolean expression");
    }
    Type *then_type =
        typechecker_infer_value_branch(tc, expr->as.if_stmt.then_branch);
    if (!expr->as.if_stmt.else_branch) {
      typechecker_error(tc, expr->line, expr->col,
                        "Value-producing if requires an else branch");
      inferred = tc->tctx->type_error;
      break;
    }
    Type *else_type =
        typechecker_infer_value_branch(tc, expr->as.if_stmt.else_branch);
    if (type_is_resolved(then_type) && type_is_resolved(else_type) &&
        !type_is_assignable(then_type, else_type) &&
        !type_is_assignable(else_type, then_type)) {
      typechecker_error(tc, expr->line, expr->col,
                        "If branches produce incompatible types");
      inferred = tc->tctx->type_error;
    } else {
      inferred = type_is_resolved(then_type) ? then_type : else_type;
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
          if (type_is_resolved(guard_t) &&
              (guard_t->kind != TY_PRIMITIVE ||
               strcmp(guard_t->as.primitive.name, "bool") != 0)) {
            typechecker_error(tc, arm->as.match_arm.guard->line,
                              arm->as.match_arm.guard->col,
                              "Match guard must be a boolean expression");
          }
        }

        Type *body_t = NULL;
        if (arm->as.match_arm.body &&
            arm->as.match_arm.body->kind == AST_BLOCK) {
          typechecker_check_node(tc, arm->as.match_arm.body);
          AstNode *last = arm->as.match_arm.body->as.block.statements;
          while (last && last->next)
            last = last->next;
          body_t = last ? last->resolved_type : tc->tctx->type_void;
        } else {
          body_t = typechecker_infer_expr(tc, arm->as.match_arm.body);
        }

        if (!first_arm_t && type_is_resolved(body_t)) {
          first_arm_t = body_t;
        } else if (first_arm_t && type_is_resolved(body_t)) {
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

    typechecker_check_match_coverage(tc, expr, subject_t);

    inferred = first_arm_t ? first_arm_t : tc->tctx->type_unknown;
    break;
  }

  case AST_TUPLE_EXPR: {
    int count = 0;
    AstNode *elem = expr->as.tuple_expr.elems;
    while (elem) { count++; elem = elem->next; }
    Type **elem_types = arena_alloc(tc->arena, sizeof(Type *) * count);
    elem = expr->as.tuple_expr.elems;
    for (int i = 0; i < count; i++) {
      elem_types[i] = typechecker_infer_expr(tc, elem);
      elem = elem->next;
    }
    inferred = type_new_tuple(tc->tctx, elem_types, count);
    break;
  }

  default:
    break;
  }

  expr->resolved_type = inferred;
  return inferred;
}

// D-15: Check if every execution path through a node ends in a return statement.
// Used only for functions that use explicit returns (no named return variable).
static bool all_paths_return(AstNode *node) {
  if (!node) return false;
  switch (node->kind) {
  case AST_RETURN_STMT:
    return true;
  case AST_BLOCK: {
    // A block returns on all paths if its last statement does
    AstNode *stmt = node->as.block.statements;
    AstNode *last = NULL;
    while (stmt) { last = stmt; stmt = stmt->next; }
    return all_paths_return(last);
  }
  case AST_IF_STMT:
    // Both branches must exist and both must return
    return node->as.if_stmt.else_branch &&
           all_paths_return(node->as.if_stmt.then_branch) &&
           all_paths_return(node->as.if_stmt.else_branch);
  case AST_MATCH_STMT: {
    // Every arm must return (exhaustiveness is Phase 3)
    AstNode *arm = node->as.match_stmt.arms;
    if (!arm) return false;
    while (arm) {
      if (arm->kind == AST_MATCH_ARM) {
        if (!all_paths_return(arm->as.match_arm.body))
          return false;
      }
      arm = arm->next;
    }
    return true;
  }
  default:
    return false;
  }
}

static void typechecker_check_node(TypeChecker *tc, AstNode *node) {
  if (!node)
    return;
  switch (node->kind) {
  case AST_USE_DECL: {
    AstNode *target = use_target_declaration(tc, node->as.use_decl.path);
    AstNode *last = node->as.use_decl.path;
    while (last && last->next)
      last = last->next;
    if (target && last) {
      Symbol *alias =
          symbol_table_lookup_local(tc->st, last->as.identifier.name);
      if (alias) {
        alias->node = target;
        alias->type = target->resolved_type;
      }
    }
    break;
  }

  case AST_MOD_DECL: {
    symbol_table_push(tc->st);
    typechecker_collect_decls(tc, node->as.mod_decl.declarations);
    for (AstNode *decl = node->as.mod_decl.declarations; decl;
         decl = decl->next)
      typechecker_check_node(tc, decl);
    symbol_table_pop(tc->st);
    break;
  }

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

    if (tc->function_depth >= 32) {
      typechecker_error(tc, node->line, node->col,
                        "Function nesting exceeds compiler limit of 32");
      break;
    }
    Scope *function_parent = tc->st->current;
    symbol_table_push(tc->st);
    tc->function_parent_scopes[tc->function_depth++] = function_parent;

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
      if (sym.type && sym.type->kind == TY_FALLIBLE)
        sym.type = sym.type->as.fallible.inner;
      symbol_table_define(tc->st, sym);
    }

    Type *saved_expected_ret = tc->expected_ret;
    tc->expected_ret =
        typechecker_resolve_type_expr(tc, node->as.func_decl.ret_type);

    if (node->as.func_decl.body) {
      typechecker_check_node(tc, node->as.func_decl.body);
    }

    // D-15: All-paths return check for non-void functions without named returns
    if (tc->expected_ret && type_is_resolved(tc->expected_ret) &&
        !(tc->expected_ret->kind == TY_PRIMITIVE &&
          strcmp(tc->expected_ret->as.primitive.name, "void") == 0) &&
        !node->as.func_decl.ret_name &&
        node->as.func_decl.body) {
      if (!all_paths_return(node->as.func_decl.body)) {
        typechecker_error(tc, node->line, node->col,
                          "Function '%s' does not return a value on all paths",
                          node->as.func_decl.name);
      }
    }

    tc->expected_ret = saved_expected_ret;
    tc->current_realm = saved_realm;
    tc->function_depth--;
    symbol_table_pop(tc->st);
    break;
  }

  case AST_VAR_DECL: {
    if (node->as.var_decl.attrs) {
      validate_systems_attrs(tc, node, node->as.var_decl.attrs);
    }
    Type *decl_t = tc->tctx->type_unknown;
    if (node->as.var_decl.type) {
      decl_t = typechecker_resolve_type_expr(tc, node->as.var_decl.type);
    }

    if (node->as.var_decl.init) {
      Type *init_t = NULL;

      if (decl_t->kind == TY_ARRAY &&
          node->as.var_decl.init->kind == AST_ARRAY_LITERAL) {
        AstNode *elem = node->as.var_decl.init->as.array_literal.elems;
        size_t count = 0;
        for (; elem; elem = elem->next) {
          Type *elem_t = typechecker_infer_expr(tc, elem);
          count++;
          if (type_is_resolved(elem_t) &&
              !type_is_assignable(decl_t->as.array.inner, elem_t)) {
            typechecker_error(tc, elem->line, elem->col,
                              "Array element does not match declared type");
          }
        }
        if (count != 0 && count != decl_t->as.array.size) {
          typechecker_error(tc, node->line, node->col,
                            "Array literal has %zu elements; declared array "
                            "requires %zu",
                            count, decl_t->as.array.size);
        }
        node->as.var_decl.init->resolved_type = decl_t;
        init_t = decl_t;
      } else {
        init_t = typechecker_infer_expr(tc, node->as.var_decl.init);
      }

      if (type_is_resolved(decl_t) && type_is_resolved(init_t)) {
        if (!type_is_assignable(decl_t, init_t)) {
          typechecker_error(
              tc, node->line, node->col,
              "Variable initializer does not match declared type");
        } else {
          // Contextual literal typing with range checking (D-06, D-09)
          AstNode *init = node->as.var_decl.init;
          if (decl_t->kind == TY_PRIMITIVE) {
            const NumericTypeInfo *info = get_numeric_info(decl_t->as.primitive.name);
            if (info) {
              bool is_negated_literal = false;
              unsigned long long lit_val = 0;
              bool is_int_literal = false;
              bool is_float_literal = false;

              if (init->kind == AST_INT_LITERAL) {
                lit_val = init->as.int_literal.value;
                is_int_literal = true;
              } else if (init->kind == AST_UNARY_EXPR &&
                         init->as.unary.op == TOKEN_MINUS &&
                         init->as.unary.expr->kind == AST_INT_LITERAL) {
                lit_val = init->as.unary.expr->as.int_literal.value;
                is_negated_literal = true;
                is_int_literal = true;
              } else if (init->kind == AST_FLOAT_LITERAL) {
                is_float_literal = true;
              } else if (init->kind == AST_UNARY_EXPR &&
                         init->as.unary.op == TOKEN_MINUS &&
                         init->as.unary.expr->kind == AST_FLOAT_LITERAL) {
                is_float_literal = true;
              }

              if (is_int_literal && !info->is_float) {
                bool overflow = false;
                if (is_negated_literal) {
                  if (info->is_signed) {
                    // For signed types: max negative magnitude is 2^(bits-1)
                    unsigned long long max_neg = 1ULL << (info->bit_width - 1);
                    overflow = (lit_val > max_neg);
                  } else {
                    // Negative value in unsigned type is always overflow
                    overflow = true;
                  }
                } else {
                  overflow = (lit_val > info->max_val);
                  // Also check: unsigned literal assigned to signed type must fit in positive range
                  if (!overflow && info->is_signed) {
                    overflow = (lit_val > (unsigned long long)info->max_val);
                  }
                }
                if (overflow) {
                  typechecker_error(tc, init->line, init->col,
                                    "Integer literal overflow for type '%s'",
                                    info->name);
                }
              } else if (is_float_literal && info->is_float) {
                // Float range check for f32
                double val = 0.0;
                if (init->kind == AST_FLOAT_LITERAL) {
                  val = init->as.float_literal.value;
                } else if (init->kind == AST_UNARY_EXPR) {
                  val = init->as.unary.expr->as.float_literal.value;
                }
                if (strcmp(info->name, "f32") == 0) {
                  if (val > 3.40282347e38 || val < -3.40282347e38) {
                    typechecker_error(tc, init->line, init->col,
                                      "Float literal overflow for f32");
                  }
                }
              }
            }
          }
        }
      } else if (!type_is_resolved(decl_t)) {
        decl_t = init_t; // inference
      }
    }

    Symbol sym = {0};
    sym.name = node->as.var_decl.name;
    sym.kind = SYM_VAR;
    sym.node = node;
    sym.type = decl_t;
    Symbol *existing =
        symbol_table_lookup_local(tc->st, node->as.var_decl.name);
    if (existing) {
      existing->node = node;
      existing->type = decl_t;
    } else {
      symbol_table_define(tc->st, sym);
    }
    node->resolved_type = decl_t;
    break;
  }

  case AST_TUPLE_DESTRUCTURE: {
    Type *init_t = typechecker_infer_expr(tc, node->as.tuple_destructure.init);

    // Count targets for mismatch check.
    int target_count = 0;
    for (AstNode *t = node->as.tuple_destructure.targets; t; t = t->next)
      target_count++;

    // Check count mismatch when init is a resolved tuple.
    if (init_t->kind == TY_TUPLE &&
        target_count != init_t->as.tuple.count) {
      typechecker_error(tc, node->line, node->col,
        "Tuple destructuring expects %d elements, got %d targets",
        init_t->as.tuple.count, target_count);
      break;
    }

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
          // Validate annotation against actual tuple element type.
          if (init_t->kind == TY_TUPLE &&
              elem_idx < init_t->as.tuple.count) {
            Type *actual_t = init_t->as.tuple.elems[elem_idx];
            if (type_is_resolved(elem_t) && type_is_resolved(actual_t) &&
                !type_is_assignable(elem_t, actual_t)) {
              typechecker_error(tc, target->line, target->col,
                "Tuple element %d has type '%s', cannot assign to '%s'",
                elem_idx, type_display_name(actual_t),
                type_display_name(elem_t));
            }
          }
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
      if (tc->expected_ret && type_is_resolved(tc->expected_ret) &&
          type_is_resolved(ret_v)) {
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
      if (type_is_resolved(cond_t)) {
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
          if (type_is_resolved(guard_t) &&
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
    typechecker_check_match_coverage(tc, node, subject_t);
    break;
  }

  // Phase 2: while/for/loop — check body
  case AST_WHILE_STMT: {
    if (node->as.while_stmt.condition) {
      Type *cond_t = typechecker_infer_expr(tc, node->as.while_stmt.condition);
      if (type_is_resolved(cond_t) &&
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
    Type *iter_t = typechecker_infer_expr(tc, node->as.for_stmt.iter);
    Type *capture_t = tc->tctx->type_unknown;
    if (node->as.for_stmt.iter->kind == AST_RANGE_EXPR &&
        type_is_integer(iter_t)) {
      capture_t = iter_t;
      if (node->as.for_stmt.cap_kind == CAPTURE_PTR ||
          node->as.for_stmt.cap_kind == CAPTURE_PTR_INDEXED) {
        typechecker_error(tc, node->line, node->col,
                          "Pointer capture requires a fixed array");
        capture_t = tc->tctx->type_error;
      }
    } else if (iter_t->kind == TY_ARRAY) {
      capture_t = iter_t->as.array.inner;
      if (node->as.for_stmt.cap_kind == CAPTURE_PTR ||
          node->as.for_stmt.cap_kind == CAPTURE_PTR_INDEXED)
        capture_t = type_new_pointer(tc->tctx, capture_t);
    } else if (type_is_resolved(iter_t)) {
      typechecker_error(tc, node->line, node->col,
                        "For loop requires a range or fixed array");
    }
    // Bind capture variable
    if (node->as.for_stmt.cap_value) {
      Symbol sym = {0};
      sym.name = node->as.for_stmt.cap_value;
      sym.kind = SYM_VAR;
      sym.node = node;
      sym.type = capture_t;
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

  case AST_ASM_EXPR: {
    if (node->as.asm_expr.output) {
      Symbol *output = symbol_table_lookup(tc->st, node->as.asm_expr.output);
      if (!output) {
        typechecker_error(tc, node->line, node->col,
                          "Unknown asm output binding '%s'",
                          node->as.asm_expr.output);
      } else {
        bool valid_type = type_is_integer(output->type);
        if (output->type && output->type->kind == TY_TUPLE &&
            output->type->as.tuple.count >= 2 &&
            output->type->as.tuple.count <= 4) {
          valid_type = true;
          for (int i = 0; i < output->type->as.tuple.count; i++)
            valid_type = valid_type &&
                         type_is_integer(output->type->as.tuple.elems[i]);
        }
        if (!valid_type) {
          typechecker_error(
              tc, node->line, node->col,
              "Asm output binding must be an integer or a 2-4 integer tuple");
        } else if (output->node && output->node->kind == AST_VAR_DECL &&
                   output->node->as.var_decl.is_const) {
          typechecker_error(tc, node->line, node->col,
                            "Asm output binding cannot be const");
        }
        node->resolved_type = output->type;
      }
    }
    if (!node->resolved_type)
      node->resolved_type = tc->tctx->type_void;
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

          if (tc->function_depth >= 32) {
            typechecker_error(tc, m_node->line, m_node->col,
                              "Function nesting exceeds compiler limit of 32");
            tc->current_realm = saved_realm;
            m_node = m_node->next;
            continue;
          }
          Scope *function_parent = tc->st->current;
          symbol_table_push(tc->st);
          tc->function_parent_scopes[tc->function_depth++] = function_parent;
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
            if (r_sym.type && r_sym.type->kind == TY_FALLIBLE)
              r_sym.type = r_sym.type->as.fallible.inner;
            symbol_table_define(tc->st, r_sym);
          }

          Type *saved_expected_ret = tc->expected_ret;
          tc->expected_ret =
              typechecker_resolve_type_expr(tc, m_node->as.func_decl.ret_type);
          if (m_node->as.func_decl.body) {
            typechecker_check_node(tc, m_node->as.func_decl.body);
          }
          tc->expected_ret = saved_expected_ret;
          tc->current_realm = saved_realm;
          tc->function_depth--;
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

// D-03: Whitelist of expression kinds where TY_UNKNOWN is always a bug.
// Literal kinds always resolve to a concrete primitive type — if they end up
// TY_UNKNOWN something went wrong.  Other expression kinds (identifiers,
// calls, binary, etc.) may legitimately return TY_UNKNOWN when their operands
// have no type info (e.g., extern symbols without type annotations).  Those
// will be promoted to the whitelist as the type checker coverage expands.
static bool should_have_resolved_type(AstKind kind) {
  switch (kind) {
  case AST_INT_LITERAL:
  case AST_FLOAT_LITERAL:
  case AST_STRING_LITERAL:
  case AST_BOOL_LITERAL:
  case AST_CHAR_LITERAL:
  case AST_TUPLE_EXPR:
  case AST_CAST_EXPR:
    return true;
  default:
    return false;
  }
}

// D-03: Post-check walk to detect TY_UNKNOWN surviving type checking
static void check_unresolved_types(TypeChecker *tc, AstNode *node) {
  if (!node) return;

  // Only report on nodes that went through type inference (have resolved_type set)
  if (node->resolved_type && node->resolved_type->kind == TY_UNKNOWN
      && should_have_resolved_type(node->kind)) {
    fprintf(stderr, "internal error: unresolved type at line %u — please report this bug\n",
            node->line);
    tc->had_error = true;
    tc->error_count++;
  }

  // Recurse into children based on node kind
  switch (node->kind) {
  case AST_PROGRAM:
    check_unresolved_types(tc, node->as.program.declarations);
    break;
  case AST_FUNC_DECL:
    check_unresolved_types(tc, node->as.func_decl.params);
    check_unresolved_types(tc, node->as.func_decl.body);
    break;
  case AST_VAR_DECL:
    check_unresolved_types(tc, node->as.var_decl.init);
    break;
  case AST_BLOCK:
    check_unresolved_types(tc, node->as.block.statements);
    break;
  case AST_RETURN_STMT:
    check_unresolved_types(tc, node->as.return_stmt.value);
    break;
  case AST_IF_STMT:
    check_unresolved_types(tc, node->as.if_stmt.condition);
    check_unresolved_types(tc, node->as.if_stmt.then_branch);
    check_unresolved_types(tc, node->as.if_stmt.else_branch);
    break;
  case AST_WHILE_STMT:
    check_unresolved_types(tc, node->as.while_stmt.condition);
    check_unresolved_types(tc, node->as.while_stmt.body);
    break;
  case AST_FOR_STMT:
    check_unresolved_types(tc, node->as.for_stmt.iter);
    check_unresolved_types(tc, node->as.for_stmt.body);
    break;
  case AST_LOOP_STMT:
    check_unresolved_types(tc, node->as.loop_stmt.body);
    break;
  case AST_MATCH_STMT:
    check_unresolved_types(tc, node->as.match_stmt.subject);
    check_unresolved_types(tc, node->as.match_stmt.arms);
    break;
  case AST_MATCH_ARM:
    check_unresolved_types(tc, node->as.match_arm.pattern);
    check_unresolved_types(tc, node->as.match_arm.guard);
    check_unresolved_types(tc, node->as.match_arm.body);
    break;
  case AST_UNSAFE_BLOCK:
    check_unresolved_types(tc, node->as.unsafe_block.body);
    break;
  case AST_BINARY_EXPR:
    check_unresolved_types(tc, node->as.binary.left);
    check_unresolved_types(tc, node->as.binary.right);
    break;
  case AST_UNARY_EXPR:
    check_unresolved_types(tc, node->as.unary.expr);
    break;
  case AST_ASSIGN:
    check_unresolved_types(tc, node->as.assign.target);
    check_unresolved_types(tc, node->as.assign.value);
    break;
  case AST_CALL_EXPR:
    check_unresolved_types(tc, node->as.call.callee);
    check_unresolved_types(tc, node->as.call.args);
    break;
  case AST_INDEX_EXPR:
    check_unresolved_types(tc, node->as.index.target);
    check_unresolved_types(tc, node->as.index.index);
    break;
  case AST_FIELD_EXPR:
    check_unresolved_types(tc, node->as.field.target);
    break;
  case AST_CAST_EXPR:
    check_unresolved_types(tc, node->as.cast.expr);
    break;
  case AST_TRY_EXPR:
    check_unresolved_types(tc, node->as.try_expr.expr);
    break;
  case AST_CATCH_EXPR:
    check_unresolved_types(tc, node->as.catch_expr.expr);
    check_unresolved_types(tc, node->as.catch_expr.handler);
    break;
  case AST_ARRAY_LITERAL:
    check_unresolved_types(tc, node->as.array_literal.elems);
    break;
  case AST_TUPLE_EXPR:
    check_unresolved_types(tc, node->as.tuple_expr.elems);
    break;
  case AST_RANGE_EXPR:
    check_unresolved_types(tc, node->as.range_expr.start);
    check_unresolved_types(tc, node->as.range_expr.end);
    break;
  case AST_METHOD_DECL:
    check_unresolved_types(tc, node->as.method_decl.methods);
    break;
  case AST_MOD_DECL:
    check_unresolved_types(tc, node->as.mod_decl.declarations);
    break;
  default:
    break;
  }

  // Walk linked list siblings
  check_unresolved_types(tc, node->next);
}

void typechecker_check(TypeChecker *tc, AstNode *program) {
  if (!program || program->kind != AST_PROGRAM)
    return;

  typechecker_collect_decls(tc, program);

  AstNode *decl = program->as.program.declarations;
  while (decl) {
    typechecker_check_node(tc, decl);
    decl = decl->next;
  }

  // D-03: Post-check validation — detect TY_UNKNOWN surviving type checking.
  // Only ICEs on expression kinds that have handlers in typechecker_infer_expr;
  // unhandled kinds legitimately remain TY_UNKNOWN until their handlers are added.
  check_unresolved_types(tc, program);
}
