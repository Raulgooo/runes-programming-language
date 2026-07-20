#include "codegen.h"
#include "lexer.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

static void cg_error(Codegen *cg, const AstNode *node, const char *message) {
  fprintf(stderr, "[Codegen Error] %u:%u: %s\n", node ? node->line : 0,
          node ? node->col : 0, message);
  cg->had_error = true;
  cg->error_count++;
}

static void indent(FILE *out, int depth) {
  for (int i = 0; i < depth; i++)
    fputs("  ", out);
}

static const char *c_named_type(const char *name) {
    if (!name)
      return "void";
    if (strcmp(name, "i8") == 0)
      return "int8_t";
    if (strcmp(name, "i16") == 0)
      return "int16_t";
    if (strcmp(name, "i32") == 0)
      return "int32_t";
    if (strcmp(name, "i64") == 0)
      return "int64_t";
    if (strcmp(name, "u8") == 0)
      return "uint8_t";
    if (strcmp(name, "u16") == 0)
      return "uint16_t";
    if (strcmp(name, "u32") == 0)
      return "uint32_t";
    if (strcmp(name, "u64") == 0)
      return "uint64_t";
    if (strcmp(name, "usize") == 0)
      return "size_t";
    if (strcmp(name, "bool") == 0)
      return "bool";
    if (strcmp(name, "str") == 0)
      return "const char *";
    if (strcmp(name, "char") == 0)
      return "char";
    if (strcmp(name, "f32") == 0)
      return "float";
    if (strcmp(name, "f64") == 0)
      return "double";
    if (strcmp(name, "void") == 0)
      return "void";
  return NULL;
}

static const char *c_type_name(AstNode *type) {
  if (!type || type->kind != AST_TYPE_EXPR)
    return "void";
  if (type->as.type_expr.kind != TYPE_NAMED)
    return NULL;
  const char *builtin = c_named_type(type->as.type_expr.name);
  return builtin ? builtin : type->as.type_expr.name;
}

static bool ast_type_suffix(AstNode *type, char *buffer, size_t buffer_size) {
  if (!type || type->kind != AST_TYPE_EXPR)
    return false;
  switch (type->as.type_expr.kind) {
  case TYPE_NAMED:
    return snprintf(buffer, buffer_size, "%s", type->as.type_expr.name) > 0;
  case TYPE_QUALIFIED:
    return snprintf(buffer, buffer_size, "%s_%s", type->as.type_expr.module,
                    type->as.type_expr.name) > 0;
  case TYPE_PTR: {
    char inner[512];
    return ast_type_suffix(type->as.type_expr.inner, inner, sizeof(inner)) &&
           snprintf(buffer, buffer_size, "ptr_%s", inner) > 0;
  }
  case TYPE_ARRAY: {
    if (!type->as.type_expr.size ||
        type->as.type_expr.size->kind != AST_INT_LITERAL)
      return false;
    char inner[512];
    return ast_type_suffix(type->as.type_expr.inner, inner, sizeof(inner)) &&
           snprintf(buffer, buffer_size, "array_%llu_%s",
                    type->as.type_expr.size->as.int_literal.value, inner) > 0;
  }
  case TYPE_TUPLE: {
    size_t used = 0;
    for (AstNode *elem = type->as.type_expr.elems; elem; elem = elem->next) {
      char suffix[512];
      if (!ast_type_suffix(elem, suffix, sizeof(suffix)))
        return false;
      int written = snprintf(buffer + used, buffer_size - used, "%s%s",
                             used ? "_" : "", suffix);
      if (written < 0 || (size_t)written >= buffer_size - used)
        return false;
      used += (size_t)written;
    }
    return used > 0;
  }
  case TYPE_FALLIBLE:
    return ast_type_suffix(type->as.type_expr.inner, buffer, buffer_size);
  }
  return false;
}

static const char *registered_decl_name(Codegen *cg, const AstNode *decl) {
  for (int i = 0; i < cg->named_decl_count; i++) {
    if (cg->named_decls[i] == decl)
      return cg->named_decl_names[i];
  }
  return NULL;
}

static const char *registered_type_name(Codegen *cg, Type *type) {
  for (int i = 0; i < cg->named_type_count; i++) {
    if (cg->named_types[i] == type)
      return cg->named_type_names[i];
  }
  if (!type)
    return NULL;
  if (type->kind == TY_STRUCT)
    return type->as.struct_t.name;
  if (type->kind == TY_VARIANT)
    return type->as.variant.name;
  if (type->kind == TY_INTERFACE)
    return type->as.interface_t.name;
  return NULL;
}

static const char *codegen_decl_source_name(AstNode *decl) {
  switch (decl->kind) {
  case AST_FUNC_DECL:
    return decl->as.func_decl.name;
  case AST_VAR_DECL:
    return decl->as.var_decl.name;
  case AST_TYPE_DECL:
    return decl->as.type_decl.name;
  case AST_VARIANT_DECL:
    return decl->as.variant_decl.name;
  case AST_INTERFACE_DECL:
    return decl->as.interface_decl.name;
  case AST_ERROR_DECL:
    return decl->as.error_decl.name;
  case AST_MOD_DECL:
    return decl->as.mod_decl.name;
  case AST_EXTERN_DECL:
    return decl->as.extern_decl.name;
  default:
    return NULL;
  }
}

static bool register_nested_node(Codegen *cg, AstNode *node,
                                 const char *prefix);

static bool register_nested_list(Codegen *cg, AstNode *node,
                                 const char *prefix) {
  for (; node; node = node->next)
    if (!register_nested_node(cg, node, prefix))
      return false;
  return true;
}

static bool register_nested_node(Codegen *cg, AstNode *node,
                                 const char *prefix) {
  if (!node)
    return true;
  if (node->kind == AST_FUNC_DECL) {
    if (cg->named_decl_count >= 256 || cg->nested_function_count >= 128)
      return false;
    char full_name[256];
    snprintf(full_name, sizeof(full_name), "%s_%s", prefix,
             node->as.func_decl.name);
    int index = cg->named_decl_count++;
    cg->named_decls[index] = node;
    snprintf(cg->named_decl_names[index], sizeof(cg->named_decl_names[index]),
             "%s", full_name);
    cg->nested_functions[cg->nested_function_count++] = node;
    return register_nested_node(cg, node->as.func_decl.body, full_name);
  }
  switch (node->kind) {
  case AST_BLOCK:
    return register_nested_list(cg, node->as.block.statements, prefix);
  case AST_IF_STMT:
    return register_nested_node(cg, node->as.if_stmt.then_branch, prefix) &&
           register_nested_node(cg, node->as.if_stmt.else_branch, prefix);
  case AST_WHILE_STMT:
    return register_nested_node(cg, node->as.while_stmt.body, prefix);
  case AST_FOR_STMT:
    return register_nested_node(cg, node->as.for_stmt.body, prefix);
  case AST_LOOP_STMT:
    return register_nested_node(cg, node->as.loop_stmt.body, prefix);
  case AST_UNSAFE_BLOCK:
    return register_nested_node(cg, node->as.unsafe_block.body, prefix);
  case AST_MATCH_STMT:
    for (AstNode *arm = node->as.match_stmt.arms; arm; arm = arm->next)
      if (!register_nested_node(cg, arm->as.match_arm.body, prefix))
        return false;
    return true;
  default:
    return true;
  }
}

static bool register_codegen_decls(Codegen *cg, AstNode *decl,
                                   const char *prefix) {
  for (; decl; decl = decl->next) {
    const char *source_name = codegen_decl_source_name(decl);
    char full_name[256];
    if (source_name) {
      if (!prefix && decl->kind == AST_FUNC_DECL &&
          strcmp(source_name, "main") != 0)
        snprintf(full_name, sizeof(full_name), "runes_%s", source_name);
      else
        snprintf(full_name, sizeof(full_name), "%s%s%s", prefix ? prefix : "",
                 prefix ? "_" : "", source_name);
    }
    if (source_name && decl->kind != AST_MOD_DECL) {
      if (cg->named_decl_count >= 256)
        return false;
      int index = cg->named_decl_count++;
      cg->named_decls[index] = decl;
      snprintf(cg->named_decl_names[index],
               sizeof(cg->named_decl_names[index]), "%s", full_name);
      if ((decl->kind == AST_TYPE_DECL || decl->kind == AST_VARIANT_DECL ||
           decl->kind == AST_INTERFACE_DECL) && decl->resolved_type) {
        if (cg->named_type_count >= 128)
          return false;
        int type_index = cg->named_type_count++;
        cg->named_types[type_index] = decl->resolved_type;
        snprintf(cg->named_type_names[type_index],
                 sizeof(cg->named_type_names[type_index]), "%s", full_name);
      }
    }
    if (decl->kind == AST_METHOD_DECL) {
      if (decl->as.method_decl.iface_name) {
        if (cg->interface_impl_count >= 64)
          return false;
        cg->interface_impls[cg->interface_impl_count++] = decl;
      }
      for (AstNode *method = decl->as.method_decl.methods; method;
           method = method->next) {
        if (cg->named_decl_count >= 256)
          return false;
        int index = cg->named_decl_count++;
        cg->named_decls[index] = method;
        snprintf(cg->named_decl_names[index],
                 sizeof(cg->named_decl_names[index]), "%s%s%s_%s",
                 prefix ? prefix : "", prefix ? "_" : "",
                 decl->as.method_decl.type_name, method->as.func_decl.name);
        if (!register_nested_node(cg, method->as.func_decl.body,
                                  cg->named_decl_names[index]))
          return false;
      }
    } else if (decl->kind == AST_MOD_DECL) {
      if (!register_codegen_decls(cg, decl->as.mod_decl.declarations,
                                  full_name))
        return false;
    } else if (decl->kind == AST_FUNC_DECL) {
      if (!register_nested_node(cg, decl->as.func_decl.body, full_name))
        return false;
    }
  }
  return true;
}

static bool semantic_type_suffix(Codegen *cg, Type *type, char *buffer,
                                 size_t buffer_size) {
  if (!type || buffer_size == 0)
    return false;
  if (type->kind == TY_PRIMITIVE)
    return snprintf(buffer, buffer_size, "%s", type->as.primitive.name) > 0;
  if (type->kind == TY_STRUCT || type->kind == TY_VARIANT)
    return snprintf(buffer, buffer_size, "%s",
                    registered_type_name(cg, type)) > 0;
  if (type->kind == TY_POINTER) {
    char inner[512];
    if (!semantic_type_suffix(cg, type->as.pointer.inner, inner, sizeof(inner)))
      return false;
    return snprintf(buffer, buffer_size, "ptr_%s", inner) > 0;
  }
  if (type->kind == TY_ARRAY) {
    char inner[512];
    if (!semantic_type_suffix(cg, type->as.array.inner, inner, sizeof(inner)))
      return false;
    return snprintf(buffer, buffer_size, "array_%zu_%s", type->as.array.size,
                    inner) > 0;
  }
  if (type->kind == TY_TUPLE) {
    int written = snprintf(buffer, buffer_size, "tuple");
    if (written < 0 || (size_t)written >= buffer_size)
      return false;
    size_t used = (size_t)written;
    for (int i = 0; i < type->as.tuple.count; i++) {
      char elem[512];
      if (!semantic_type_suffix(cg, type->as.tuple.elems[i], elem,
                                sizeof(elem)))
        return false;
      written = snprintf(buffer + used, buffer_size - used, "_%s", elem);
      if (written < 0 || (size_t)written >= buffer_size - used)
        return false;
      used += (size_t)written;
    }
    return true;
  }
  return false;
}

static bool tuple_type_name(Codegen *cg, Type *tuple, char *buffer,
                            size_t buffer_size) {
  if (!tuple || tuple->kind != TY_TUPLE)
    return false;
  char suffix[768];
  if (!semantic_type_suffix(cg, tuple, suffix, sizeof(suffix)))
    return false;
  return snprintf(buffer, buffer_size, "RunesTuple_%s", suffix + 6) > 0;
}

static bool array_type_name(Codegen *cg, Type *array, char *buffer,
                            size_t buffer_size) {
  if (!array || array->kind != TY_ARRAY)
    return false;
  char suffix[768];
  if (!semantic_type_suffix(cg, array->as.array.inner, suffix,
                            sizeof(suffix)))
    return false;
  return snprintf(buffer, buffer_size, "RunesArray_%zu_%s",
                  array->as.array.size, suffix) > 0;
}

static bool result_type_name(Codegen *cg, Type *fallible, char *buffer,
                             size_t buffer_size) {
  if (!fallible || fallible->kind != TY_FALLIBLE)
    return false;
  char suffix[768];
  if (!semantic_type_suffix(cg, fallible->as.fallible.inner, suffix,
                            sizeof(suffix)))
    return false;
  return snprintf(buffer, buffer_size, "RunesResult_%s", suffix) > 0;
}

static bool build_semantic_decl(Codegen *cg, Type *type, const char *name,
                                char *buffer,
                                size_t buffer_size) {
  if (!type) {
    return false;
  }
  if (type->kind == TY_PRIMITIVE) {
    const char *base = c_named_type(type->as.primitive.name);
    return base && snprintf(buffer, buffer_size, "%s %s", base, name) > 0;
  }
  if (type->kind == TY_STRUCT)
    return snprintf(buffer, buffer_size, "%s %s",
                    registered_type_name(cg, type),
                    name) > 0;
  if (type->kind == TY_VARIANT)
    return snprintf(buffer, buffer_size, "%s %s",
                    registered_type_name(cg, type),
                    name) > 0;
  if (type->kind == TY_INTERFACE)
    return snprintf(buffer, buffer_size, "%s %s",
                    registered_type_name(cg, type), name) > 0;
  if (type->kind == TY_ERROR)
    return snprintf(buffer, buffer_size, "RunesError %s", name) > 0;
  if (type->kind == TY_FALLIBLE) {
    char result_name[1024];
    return result_type_name(cg, type, result_name, sizeof(result_name)) &&
           snprintf(buffer, buffer_size, "%s %s", result_name, name) > 0;
  }
  if (type->kind == TY_TUPLE) {
    char tuple_name[1024];
    return tuple_type_name(cg, type, tuple_name, sizeof(tuple_name)) &&
           snprintf(buffer, buffer_size, "%s %s", tuple_name, name) > 0;
  }
  if (type->kind == TY_POINTER) {
    char declarator[512];
    bool pointee_is_array = type->as.pointer.inner &&
                            type->as.pointer.inner->kind == TY_ARRAY;
    int written = snprintf(declarator, sizeof(declarator),
                           pointee_is_array ? "(*%s)" : "*%s", name);
    if (written < 0 || (size_t)written >= sizeof(declarator))
      return false;
    return build_semantic_decl(cg, type->as.pointer.inner, declarator, buffer,
                               buffer_size);
  }
  if (type->kind == TY_ARRAY) {
    char declarator[512];
    int written = snprintf(declarator, sizeof(declarator), "%s[%zu]", name,
                           type->as.array.size);
    if (written < 0 || (size_t)written >= sizeof(declarator))
      return false;
    return build_semantic_decl(cg, type->as.array.inner, declarator, buffer,
                               buffer_size);
  }
  return false;
}

static bool emit_semantic_decl(Codegen *cg, Type *type, const char *name,
                               const AstNode *error_node) {
  char declaration[1024];
  if (!type) {
    cg_error(cg, error_node, "missing semantic type for C declaration");
    return false;
  }
  if (!build_semantic_decl(cg, type, name, declaration, sizeof(declaration))) {
    cg_error(cg, error_node, "unsupported semantic type for C declaration");
    return false;
  }
  fputs(declaration, cg->out);
  return true;
}

static bool build_c_decl(Codegen *cg, AstNode *type, const char *name,
                         char *buffer,
                         size_t buffer_size) {
  if (!type || type->kind != AST_TYPE_EXPR)
    return false;

  if (type->as.type_expr.kind == TYPE_NAMED) {
    const char *base = c_type_name(type);
    if (base)
      return snprintf(buffer, buffer_size, "%s %s", base, name) > 0;
    if (cg->current_prefix)
      return snprintf(buffer, buffer_size, "%s_%s %s", cg->current_prefix,
                      type->as.type_expr.name, name) > 0;
    return snprintf(buffer, buffer_size, "%s %s", type->as.type_expr.name,
                    name) > 0;
  }

  if (type->as.type_expr.kind == TYPE_QUALIFIED) {
    return snprintf(buffer, buffer_size, "%s_%s %s",
                    type->as.type_expr.module, type->as.type_expr.name,
                    name) > 0;
  }

  if (type->as.type_expr.kind == TYPE_TUPLE) {
    char suffix[768];
    if (!ast_type_suffix(type, suffix, sizeof(suffix)))
      return false;
    return snprintf(buffer, buffer_size, "RunesTuple_%s %s", suffix, name) >
           0;
  }

  char declarator[512];
  if (type->as.type_expr.kind == TYPE_PTR) {
    bool pointee_is_array = type->as.type_expr.inner &&
                            type->as.type_expr.inner->kind == AST_TYPE_EXPR &&
                            type->as.type_expr.inner->as.type_expr.kind ==
                                TYPE_ARRAY;
    int written = snprintf(declarator, sizeof(declarator),
                           pointee_is_array ? "(*%s)" : "*%s", name);
    if (written < 0 || (size_t)written >= sizeof(declarator))
      return false;
    return build_c_decl(cg, type->as.type_expr.inner, declarator, buffer,
                        buffer_size);
  }

  if (type->as.type_expr.kind == TYPE_ARRAY) {
    AstNode *size = type->as.type_expr.size;
    if (!size || size->kind != AST_INT_LITERAL)
      return false;
    int written = snprintf(declarator, sizeof(declarator), "%s[%llu]", name,
                           size->as.int_literal.value);
    if (written < 0 || (size_t)written >= sizeof(declarator))
      return false;
    return build_c_decl(cg, type->as.type_expr.inner, declarator, buffer,
                        buffer_size);
  }

  return false;
}

static bool emit_c_decl(Codegen *cg, AstNode *type, const char *name,
                        const AstNode *error_node) {
  char declaration[1024];
  if (!build_c_decl(cg, type, name, declaration, sizeof(declaration))) {
    cg_error(cg, error_node, "unsupported type for C emission");
    return false;
  }
  fputs(declaration, cg->out);
  return true;
}

static const char *c_op(TokenKind op) {
  switch (op) {
  case TOKEN_PLUS:
    return "+";
  case TOKEN_MINUS:
    return "-";
  case TOKEN_STAR:
    return "*";
  case TOKEN_SLASH:
    return "/";
  case TOKEN_PERCENT:
    return "%";
  case TOKEN_EQ_EQ:
    return "==";
  case TOKEN_BANG_EQ:
    return "!=";
  case TOKEN_LT:
    return "<";
  case TOKEN_LT_EQ:
    return "<=";
  case TOKEN_GT:
    return ">";
  case TOKEN_GT_EQ:
    return ">=";
  case TOKEN_AMP:
    return "&";
  case TOKEN_PIPE:
    return "|";
  case TOKEN_CARET:
    return "^";
  case TOKEN_SHL:
    return "<<";
  case TOKEN_SHR:
    return ">>";
  case TOKEN_AND:
    return "&&";
  case TOKEN_OR:
    return "||";
  default:
    return NULL;
  }
}

static void emit_c_string(FILE *out, const char *s) {
  fputc('"', out);
  for (; s && *s; s++) {
    switch (*s) {
    case '\n':
      fputs("\\n", out);
      break;
    case '\r':
      fputs("\\r", out);
      break;
    case '\t':
      fputs("\\t", out);
      break;
    case '\\':
      fputs("\\\\", out);
      break;
    case '"':
      fputs("\\\"", out);
      break;
    default:
      fputc(*s, out);
      break;
    }
  }
  fputc('"', out);
}

static void emit_inline_asm_string(FILE *out, const char *s,
                                   bool extended_asm) {
  fputc('"', out);
  for (; s && *s; s++) {
    switch (*s) {
    case '\n':
      fputs("\\n", out);
      break;
    case '\r':
      fputs("\\r", out);
      break;
    case '\t':
      fputs("\\t", out);
      break;
    case '\\':
      fputs("\\\\", out);
      break;
    case '"':
      fputs("\\\"", out);
      break;
    case '%':
      fputs(extended_asm ? "%%" : "%", out);
      break;
    default:
      fputc(*s, out);
      break;
    }
  }
  fputc('"', out);
}

static bool emit_expr(Codegen *cg, AstNode *expr);

static AstNode *find_interface_impl(Codegen *cg, Type *interface,
                                    Type *concrete) {
  if (!interface || interface->kind != TY_INTERFACE || !concrete ||
      concrete->kind != TY_STRUCT)
    return NULL;
  for (int i = 0; i < cg->interface_impl_count; i++) {
    AstNode *impl = cg->interface_impls[i];
    if (strcmp(impl->as.method_decl.iface_name,
               interface->as.interface_t.name) == 0 &&
        strcmp(impl->as.method_decl.type_name,
               concrete->as.struct_t.name) == 0)
      return impl;
  }
  return NULL;
}

static bool emit_interface_value(Codegen *cg, Type *interface,
                                 AstNode *arg) {
  Type *concrete = arg->resolved_type;
  AstNode *impl = find_interface_impl(cg, interface, concrete);
  if (!impl) {
    cg_error(cg, arg, "missing interface implementation for C emission");
    return false;
  }
  bool addressable = arg->kind == AST_IDENTIFIER ||
                     (arg->kind == AST_CALL_EXPR && arg->resolved_type &&
                      arg->resolved_type->kind == TY_STRUCT);
  if (!addressable) {
    cg_error(cg, arg, "interface argument must be an addressable value");
    return false;
  }
  const char *iface_name = registered_type_name(cg, interface);
  const char *concrete_name = registered_type_name(cg, concrete);
  fprintf(cg->out, "(%s){ .data = &", iface_name);
  if (!emit_expr(cg, arg))
    return false;
  for (int i = 0; i < interface->as.interface_t.method_count; i++)
    fprintf(cg->out, ", .%s = %s_%s_%s_adapter",
            interface->as.interface_t.method_names[i], iface_name,
            concrete_name, interface->as.interface_t.method_names[i]);
  fputs(" }", cg->out);
  return true;
}

static bool emit_arg_list(Codegen *cg, AstNode *arg) {
  while (arg) {
    if (!emit_expr(cg, arg))
      return false;
    if (arg->next)
      fputs(", ", cg->out);
    arg = arg->next;
  }
  return true;
}

static bool emit_typed_arg_list(Codegen *cg, AstNode *arg,
                                Type *function_type) {
  int index = 0;
  while (arg) {
    Type *expected = function_type && function_type->kind == TY_FUNCTION &&
                             index < function_type->as.function.param_count
                         ? function_type->as.function.params[index]
                         : NULL;
    if (expected && expected->kind == TY_INTERFACE && arg->resolved_type &&
        arg->resolved_type->kind != TY_INTERFACE) {
      if (!emit_interface_value(cg, expected, arg))
        return false;
    } else if (!emit_expr(cg, arg)) {
      return false;
    }
    if (arg->next)
      fputs(", ", cg->out);
    arg = arg->next;
    index++;
  }
  return true;
}

static int variant_arm_index(Type *variant, const char *name) {
  if (!variant || variant->kind != TY_VARIANT || !name)
    return -1;
  for (int i = 0; i < variant->as.variant.arm_count; i++) {
    if (strcmp(variant->as.variant.arm_names[i], name) == 0)
      return i;
  }
  return -1;
}

static const char *variant_callee_arm(AstNode *callee) {
  if (!callee)
    return NULL;
  if (callee->kind == AST_IDENTIFIER)
    return callee->as.identifier.name;
  if (callee->kind == AST_FIELD_EXPR)
    return callee->as.field.field;
  return NULL;
}

static bool emit_variant_value(Codegen *cg, Type *variant,
                               const char *arm_name, AstNode *args,
                               const AstNode *error_node) {
  int arm = variant_arm_index(variant, arm_name);
  if (arm < 0) {
    cg_error(cg, error_node, "unknown variant arm during C emission");
    return false;
  }
  const char *variant_name = registered_type_name(cg, variant);
  fprintf(cg->out, "(%s){ .tag = %s_%s", variant_name, variant_name,
          arm_name);
  Type *payload = variant->as.variant.arm_types[arm];
  if (payload) {
    fprintf(cg->out, ", .data.%s = { ", arm_name);
    int field = 0;
    for (AstNode *arg = args; arg; arg = arg->next, field++) {
      fprintf(cg->out, "._%d = ", field);
      if (!emit_expr(cg, arg))
        return false;
      if (arg->next)
        fputs(", ", cg->out);
    }
    fputs(" }", cg->out);
  }
  fputs(" }", cg->out);
  return true;
}

static bool emit_expr(Codegen *cg, AstNode *expr) {
  if (!expr)
    return false;

  switch (expr->kind) {
  case AST_INT_LITERAL:
    fprintf(cg->out, "%llu", expr->as.int_literal.value);
    return true;
  case AST_FLOAT_LITERAL:
    fprintf(cg->out, "%.17g", expr->as.float_literal.value);
    return true;
  case AST_STRING_LITERAL:
    emit_c_string(cg->out, expr->as.string_literal.value);
    return true;
  case AST_BOOL_LITERAL:
    fputs(expr->as.bool_literal.value ? "true" : "false", cg->out);
    return true;
  case AST_CHAR_LITERAL:
    fprintf(cg->out, "((char)%u)", expr->as.char_literal.codepoint);
    return true;
  case AST_ERROR_EXPR: {
    AstNode *set = expr->as.error_expr.path;
    AstNode *member = set ? set->next : NULL;
    if (!set || !member) {
      cg_error(cg, expr, "invalid error value during C emission");
      return false;
    }
    const char *set_name = registered_decl_name(cg, expr->resolved_decl);
    fprintf(cg->out, "%s_%s", set_name ? set_name : set->as.identifier.name,
            member->as.identifier.name);
    return true;
  }
  case AST_IDENTIFIER:
    {
      bool named_result = expr->resolved_decl &&
                          expr->resolved_decl->kind == AST_FUNC_DECL &&
                          expr->resolved_decl->as.func_decl.ret_name &&
                          strcmp(expr->resolved_decl->as.func_decl.ret_name,
                                 expr->as.identifier.name) == 0;
      const char *name = named_result
                             ? cg->current_result_c_name
                             : registered_decl_name(cg, expr->resolved_decl);
      if (expr->resolved_decl && expr->resolved_decl->kind == AST_EXTERN_DECL &&
          strcmp(expr->resolved_decl->as.extern_decl.name, "sqrt") == 0)
        name = "sqrtf";
      fputs(name ? name : expr->as.identifier.name, cg->out);
    }
    return true;
  case AST_ARRAY_LITERAL: {
    fputc('{', cg->out);
    AstNode *elem = expr->as.array_literal.elems;
    while (elem) {
      if (!emit_expr(cg, elem))
        return false;
      if (elem->next)
        fputs(", ", cg->out);
      elem = elem->next;
    }
    fputc('}', cg->out);
    return true;
  }
  case AST_TUPLE_EXPR: {
    if (!expr->resolved_type || expr->resolved_type->kind != TY_TUPLE) {
      cg_error(cg, expr, "tuple expression has no resolved tuple type");
      return false;
    }
    char tuple_name[1024];
    if (!tuple_type_name(cg, expr->resolved_type, tuple_name,
                         sizeof(tuple_name)))
      return false;
    fprintf(cg->out, "(%s){", tuple_name);
    int index = 0;
    for (AstNode *elem = expr->as.tuple_expr.elems; elem;
         elem = elem->next, index++) {
      fprintf(cg->out, "._%d = ", index);
      if (!emit_expr(cg, elem))
        return false;
      if (elem->next)
        fputs(", ", cg->out);
    }
    fputc('}', cg->out);
    return true;
  }
  case AST_INDEX_EXPR:
    if (!emit_expr(cg, expr->as.index.target))
      return false;
    fputc('[', cg->out);
    if (!emit_expr(cg, expr->as.index.index))
      return false;
    fputc(']', cg->out);
    return true;
  case AST_FIELD_EXPR:
    if (expr->as.field.target->resolved_type &&
        expr->as.field.target->resolved_type->kind == TY_PRIMITIVE &&
        strcmp(expr->as.field.target->resolved_type->as.primitive.name,
               "str") == 0) {
      if (strcmp(expr->as.field.field, "len") == 0) {
        fputs("strlen(", cg->out);
        if (!emit_expr(cg, expr->as.field.target))
          return false;
        fputc(')', cg->out);
        return true;
      }
      if (strcmp(expr->as.field.field, "ptr") == 0)
        return emit_expr(cg, expr->as.field.target);
    }
    if (expr->resolved_type && expr->resolved_type->kind == TY_VARIANT &&
        expr->as.field.target->resolved_type &&
        expr->as.field.target->resolved_type->kind == TY_VARIANT) {
      return emit_variant_value(cg, expr->resolved_type,
                                expr->as.field.field, NULL, expr);
    }
    {
      const char *qualified = registered_decl_name(cg, expr->resolved_decl);
      if (qualified) {
        fputs(qualified, cg->out);
        return true;
      }
    }
    if (!emit_expr(cg, expr->as.field.target))
      return false;
    fputs(expr->as.field.target->resolved_type &&
                  expr->as.field.target->resolved_type->kind == TY_POINTER
              ? "->"
              : ".",
          cg->out);
    fputs(expr->as.field.field, cg->out);
    return true;
  case AST_UNARY_EXPR:
    if (expr->as.unary.op == TOKEN_AMP && expr->as.unary.expr->resolved_type &&
        expr->as.unary.expr->resolved_type->kind == TY_ARRAY) {
      return emit_expr(cg, expr->as.unary.expr);
    }
    if (expr->as.unary.op != TOKEN_STAR &&
        expr->as.unary.op != TOKEN_AMP &&
        expr->as.unary.op != TOKEN_MINUS &&
        expr->as.unary.op != TOKEN_BANG &&
        expr->as.unary.op != TOKEN_TILDE) {
      cg_error(cg, expr, "unsupported unary operator");
      return false;
    }
    fputs(token_kind_to_string(expr->as.unary.op), cg->out);
    fputc('(', cg->out);
    if (!emit_expr(cg, expr->as.unary.expr))
      return false;
    fputc(')', cg->out);
    return true;
  case AST_CAST_EXPR: {
    char target[1024];
    if (!build_c_decl(cg, expr->as.cast.target_type, "", target,
                      sizeof(target))) {
      cg_error(cg, expr, "unsupported cast target for C emission");
      return false;
    }
    fprintf(cg->out, "((%s)", target);
    if (!emit_expr(cg, expr->as.cast.expr))
      return false;
    fputc(')', cg->out);
    return true;
  }
  case AST_SIZEOF_EXPR:
  case AST_ALIGNOF_EXPR: {
    AstNode *type = expr->kind == AST_SIZEOF_EXPR
                        ? expr->as.sizeof_expr.type
                        : expr->as.alignof_expr.type;
    char declaration[1024];
    if (!build_c_decl(cg, type, "", declaration, sizeof(declaration))) {
      cg_error(cg, expr, "unsupported type in size/alignment expression");
      return false;
    }
    fprintf(cg->out, "%s(%s)",
            expr->kind == AST_SIZEOF_EXPR ? "sizeof" : "_Alignof",
            declaration);
    return true;
  }
  case AST_PROMOTE_EXPR:
    fputs("runes_promote_copy(", cg->out);
    if (!emit_expr(cg, expr->as.promote.expr))
      return false;
    fputs(", sizeof *(", cg->out);
    if (!emit_expr(cg, expr->as.promote.expr))
      return false;
    fputs("))", cg->out);
    return true;
  case AST_BINARY_EXPR: {
    bool string_operands =
        expr->as.binary.left->resolved_type &&
        expr->as.binary.right->resolved_type &&
        expr->as.binary.left->resolved_type->kind == TY_PRIMITIVE &&
        expr->as.binary.right->resolved_type->kind == TY_PRIMITIVE &&
        strcmp(expr->as.binary.left->resolved_type->as.primitive.name, "str") ==
            0 &&
        strcmp(expr->as.binary.right->resolved_type->as.primitive.name,
               "str") == 0;
    if (string_operands && expr->as.binary.op == TOKEN_PLUS) {
      fputs("runes_str_concat(", cg->out);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fputs(", ", cg->out);
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fputc(')', cg->out);
      return true;
    }
    if (string_operands && (expr->as.binary.op == TOKEN_EQ_EQ ||
                            expr->as.binary.op == TOKEN_BANG_EQ)) {
      fputs("(strcmp(", cg->out);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fputs(", ", cg->out);
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fprintf(cg->out, ") %s 0)",
              expr->as.binary.op == TOKEN_EQ_EQ ? "==" : "!=");
      return true;
    }
    const char *op = c_op(expr->as.binary.op);
    if (!op) {
      cg_error(cg, expr, "unsupported binary operator");
      return false;
    }
    fputc('(', cg->out);
    if (!emit_expr(cg, expr->as.binary.left))
      return false;
    fprintf(cg->out, " %s ", op);
    if (!emit_expr(cg, expr->as.binary.right))
      return false;
    fputc(')', cg->out);
    return true;
  }
  case AST_CALL_EXPR:
    if (expr->as.call.callee->kind == AST_FIELD_EXPR &&
        expr->as.call.callee->resolved_type &&
        expr->as.call.callee->resolved_type->kind == TY_FUNCTION &&
        expr->as.call.callee->resolved_type->as.function.is_method) {
      AstNode *callee = expr->as.call.callee;
      AstNode *receiver = callee->as.field.target;
      Type *receiver_type = receiver->resolved_type;
      Type *owner = receiver_type && receiver_type->kind == TY_POINTER
                        ? receiver_type->as.pointer.inner
                        : receiver_type;
      if (owner && owner->kind == TY_INTERFACE) {
        if (!emit_expr(cg, receiver))
          return false;
        fprintf(cg->out, ".%s(", callee->as.field.field);
        if (!emit_expr(cg, receiver))
          return false;
        fputs(".data", cg->out);
        if (expr->as.call.args)
          fputs(", ", cg->out);
        if (!emit_arg_list(cg, expr->as.call.args))
          return false;
        fputc(')', cg->out);
        return true;
      }
      const char *owner_name = owner && owner->kind == TY_STRUCT
                                   ? registered_type_name(cg, owner)
                                   : owner && owner->kind == TY_VARIANT
                                         ? registered_type_name(cg, owner)
                                         : NULL;
      if (!owner_name) {
        cg_error(cg, expr, "unsupported method receiver for C emission");
        return false;
      }
      fprintf(cg->out, "%s_%s(", owner_name, callee->as.field.field);
      int explicit_arg_count = 0;
      for (AstNode *arg = expr->as.call.args; arg; arg = arg->next)
        explicit_arg_count++;
      bool has_receiver =
          callee->resolved_type->as.function.param_count > explicit_arg_count;
      Type *expected = callee->resolved_type->as.function.param_count > 0
                           ? callee->resolved_type->as.function.params[0]
                           : NULL;
      if (has_receiver) {
        if (expected && expected->kind == TY_POINTER && receiver_type &&
            receiver_type->kind != TY_POINTER)
          fputc('&', cg->out);
        else if (expected && expected->kind != TY_POINTER && receiver_type &&
                 receiver_type->kind == TY_POINTER)
          fputc('*', cg->out);
        if (!emit_expr(cg, receiver))
          return false;
        if (expr->as.call.args)
          fputs(", ", cg->out);
      }
      if (!emit_arg_list(cg, expr->as.call.args))
        return false;
      fputc(')', cg->out);
      return true;
    }
    if (expr->resolved_type && expr->resolved_type->kind == TY_VARIANT) {
      return emit_variant_value(cg, expr->resolved_type,
                                variant_callee_arm(expr->as.call.callee),
                                expr->as.call.args, expr);
    }
    Type *constructed_struct =
        expr->resolved_type && expr->resolved_type->kind == TY_STRUCT
            ? expr->resolved_type
            : expr->as.call.callee->resolved_decl &&
                      expr->as.call.callee->resolved_decl->kind == AST_TYPE_DECL
                  ? expr->as.call.callee->resolved_decl->resolved_type
                  : NULL;
    if (constructed_struct && constructed_struct->kind == TY_STRUCT) {
      const char *struct_name = registered_type_name(cg, constructed_struct);
      fprintf(cg->out, "(%s){", struct_name);
      AstNode *arg = expr->as.call.args;
      while (arg) {
        if (arg->kind != AST_NAMED_ARG) {
          cg_error(cg, arg, "struct C emission requires named arguments");
          return false;
        }
        fprintf(cg->out, ".%s = ", arg->as.named_arg.name);
        if (!emit_expr(cg, arg->as.named_arg.value))
          return false;
        if (arg->next)
          fputs(", ", cg->out);
        arg = arg->next;
      }
      fputc('}', cg->out);
      return true;
    }
    if (expr->as.call.callee->kind == AST_IDENTIFIER &&
        strcmp(expr->as.call.callee->as.identifier.name, "print") == 0) {
      cg_error(cg, expr, "print is only supported as a statement");
      return false;
    } else if (!emit_expr(cg, expr->as.call.callee)) {
      return false;
    } else {
      fputc('(', cg->out);
    }
    Type *callee_type = expr->as.call.callee->resolved_type;
    if (!emit_typed_arg_list(cg, expr->as.call.args, callee_type))
      return false;
    fputc(')', cg->out);
    return true;
  default:
    cg_error(cg, expr, "unsupported expression for C emission");
    return false;
  }
}

static bool emit_stmt(Codegen *cg, AstNode *stmt, int depth);
static bool emit_match(Codegen *cg, AstNode *match, int depth,
                       AstNode *target, const char *target_name);
static bool emit_if_value(Codegen *cg, AstNode *if_expr, int depth,
                          AstNode *target, const char *target_name);

static bool register_runtime_initializer(Codegen *cg, AstNode *node) {
  if (cg->runtime_initializer_count >= 128) {
    cg_error(cg, node, "too many runtime global initializers");
    return false;
  }
  cg->runtime_initializers[cg->runtime_initializer_count++] = node;
  return true;
}

static bool is_c_constant_expr(AstNode *expr) {
  if (!expr)
    return true;
  switch (expr->kind) {
  case AST_INT_LITERAL:
  case AST_FLOAT_LITERAL:
  case AST_STRING_LITERAL:
  case AST_BOOL_LITERAL:
  case AST_CHAR_LITERAL:
    return true;
  case AST_ARRAY_LITERAL:
    for (AstNode *elem = expr->as.array_literal.elems; elem; elem = elem->next)
      if (!is_c_constant_expr(elem))
        return false;
    return true;
  case AST_UNARY_EXPR:
    return expr->as.unary.op != TOKEN_STAR &&
           is_c_constant_expr(expr->as.unary.expr);
  case AST_CAST_EXPR:
    return is_c_constant_expr(expr->as.cast.expr);
  default:
    return false;
  }
}

static bool emit_fallible_return(Codegen *cg, int depth,
                                 const char *error_expr, bool force_error) {
  char result_type[1024];
  if (!result_type_name(cg, cg->current_fallible, result_type,
                        sizeof(result_type)))
    return false;
  Type *inner = cg->current_fallible->as.fallible.inner;
  bool is_void = inner->kind == TY_PRIMITIVE &&
                 strcmp(inner->as.primitive.name, "void") == 0;
  indent(cg->out, depth);
  fprintf(cg->out, "return (%s){ .ok = ", result_type);
  if (force_error)
    fputs("false", cg->out);
  else
    fprintf(cg->out, "%s == RUNES_ERROR_NONE", error_expr);
  fprintf(cg->out, ", .error = %s", error_expr);
  if (!is_void && cg->current_result_c_name)
    fprintf(cg->out, ", .value = %s", cg->current_result_c_name);
  fputs(" };\n", cg->out);
  return true;
}

static bool emit_block(Codegen *cg, AstNode *block, int depth) {
  if (!block || block->kind != AST_BLOCK)
    return false;

  AstNode *stmt = block->as.block.statements;
  while (stmt) {
    if (!emit_stmt(cg, stmt, depth))
      return false;
    stmt = stmt->next;
  }
  return true;
}

static bool emit_assignment_target(Codegen *cg, AstNode *target,
                                   const char *target_name, AstNode *value,
                                   int depth) {
  indent(cg->out, depth);
  if (target_name)
    fputs(target_name, cg->out);
  else if (target) {
    if (!emit_expr(cg, target))
      return false;
  } else {
    cg_error(cg, value, "missing result target during C emission");
    return false;
  }
  fputs(" = ", cg->out);
  if (!emit_expr(cg, value))
    return false;
  fputs(";\n", cg->out);
  return true;
}

static bool emit_fallible_expr(Codegen *cg, AstNode *expr, int depth,
                               AstNode *target, const char *target_name) {
  bool is_try = expr->kind == AST_TRY_EXPR;
  bool is_catch = expr->kind == AST_CATCH_EXPR;
  if (!is_try && !is_catch)
    return false;
  AstNode *source = is_try ? expr->as.try_expr.expr
                           : expr->as.catch_expr.expr;
  Type *fallible = source->resolved_type;
  if (!fallible || fallible->kind != TY_FALLIBLE) {
    cg_error(cg, expr, "missing fallible type during C emission");
    return false;
  }
  char temp_name[64];
  snprintf(temp_name, sizeof(temp_name), "__runes_result_%u", cg->temp_id++);
  indent(cg->out, depth);
  if (!emit_semantic_decl(cg, fallible, temp_name, expr))
    return false;
  bool chained_source = source->kind == AST_CATCH_EXPR &&
                        source->resolved_type &&
                        source->resolved_type->kind == TY_FALLIBLE;
  if (chained_source) {
    fputs(";\n", cg->out);
    AstNode *caught = source->as.catch_expr.expr;
    Type *caught_type = caught->resolved_type;
    if (!caught_type || caught_type->kind != TY_FALLIBLE) {
      cg_error(cg, source, "invalid chained catch source during C emission");
      return false;
    }
    char caught_name[64];
    snprintf(caught_name, sizeof(caught_name), "__runes_result_%u",
             cg->temp_id++);
    indent(cg->out, depth);
    if (!emit_semantic_decl(cg, caught_type, caught_name, source))
      return false;
    fputs(" = ", cg->out);
    if (!emit_expr(cg, caught))
      return false;
    fputs(";\n", cg->out);
    indent(cg->out, depth);
    fprintf(cg->out, "if (%s.ok) {\n", caught_name);
    indent(cg->out, depth + 1);
    fprintf(cg->out, "%s = %s;\n", temp_name, caught_name);
    indent(cg->out, depth);
    fputs("} else {\n", cg->out);
    if (source->as.catch_expr.err_name) {
      indent(cg->out, depth + 1);
      fprintf(cg->out, "RunesError %s = %s.error;\n",
              source->as.catch_expr.err_name, caught_name);
    }
    AstNode *handler = source->as.catch_expr.handler;
    if (!handler || !handler->resolved_type ||
        handler->resolved_type->kind != TY_FALLIBLE ||
        handler->kind == AST_CATCH_EXPR) {
      cg_error(cg, source,
               "unsupported chained catch handler during C emission");
      return false;
    }
    indent(cg->out, depth + 1);
    fprintf(cg->out, "%s = ", temp_name);
    if (!emit_expr(cg, handler))
      return false;
    fputs(";\n", cg->out);
    indent(cg->out, depth);
    fputs("}\n", cg->out);
  } else {
    fputs(" = ", cg->out);
    if (!emit_expr(cg, source))
      return false;
    fputs(";\n", cg->out);
  }

  Type *inner = fallible->as.fallible.inner;
  bool is_void = inner->kind == TY_PRIMITIVE &&
                 strcmp(inner->as.primitive.name, "void") == 0;
  if (is_try) {
    if (!cg->current_fallible) {
      cg_error(cg, expr, "try requires a fallible C function context");
      return false;
    }
    indent(cg->out, depth);
    fprintf(cg->out, "if (!%s.ok) {\n", temp_name);
    char error_access[128];
    snprintf(error_access, sizeof(error_access), "%s.error", temp_name);
    if (!emit_fallible_return(cg, depth + 1, error_access, true))
      return false;
    indent(cg->out, depth);
    fputs("}\n", cg->out);
    if (!is_void && (target || target_name)) {
      indent(cg->out, depth);
      if (target_name)
        fputs(target_name, cg->out);
      else if (!emit_expr(cg, target))
        return false;
      fprintf(cg->out, " = %s.value;\n", temp_name);
    }
    return true;
  }

  indent(cg->out, depth);
  fprintf(cg->out, "if (%s.ok) {\n", temp_name);
  if (!is_void && (target || target_name)) {
    indent(cg->out, depth + 1);
    if (target_name)
      fputs(target_name, cg->out);
    else if (!emit_expr(cg, target))
      return false;
    fprintf(cg->out, " = %s.value;\n", temp_name);
  }
  indent(cg->out, depth);
  fputs("} else {\n", cg->out);
  if (expr->as.catch_expr.err_name) {
    indent(cg->out, depth + 1);
    fprintf(cg->out, "RunesError %s = %s.error;\n",
            expr->as.catch_expr.err_name, temp_name);
  }
  AstNode *handler = expr->as.catch_expr.handler;
  if (handler->kind == AST_BLOCK) {
    if (!emit_block(cg, handler, depth + 1))
      return false;
  } else if (target || target_name) {
    if (!emit_assignment_target(cg, target, target_name, handler, depth + 1))
      return false;
  } else {
    indent(cg->out, depth + 1);
    if (!emit_expr(cg, handler))
      return false;
    fputs(";\n", cg->out);
  }
  indent(cg->out, depth);
  fputs("}\n", cg->out);
  return true;
}

static bool emit_value_branch(Codegen *cg, AstNode *branch, int depth,
                              AstNode *target, const char *target_name) {
  AstNode *result = branch;
  if (branch->kind == AST_BLOCK) {
    result = branch->as.block.statements;
    if (!result) {
      cg_error(cg, branch, "value-producing branch has no result");
      return false;
    }
    while (result->next) {
      if (!emit_stmt(cg, result, depth))
        return false;
      result = result->next;
    }
  }
  if (result->kind == AST_IF_STMT)
    return emit_if_value(cg, result, depth, target, target_name);
  if (result->kind == AST_MATCH_STMT)
    return emit_match(cg, result, depth, target, target_name);
  return emit_assignment_target(cg, target, target_name, result, depth);
}

static bool emit_if_value(Codegen *cg, AstNode *if_expr, int depth,
                          AstNode *target, const char *target_name) {
  if (!if_expr->as.if_stmt.else_branch) {
    cg_error(cg, if_expr, "value-producing if has no else branch");
    return false;
  }
  indent(cg->out, depth);
  fputs("if (", cg->out);
  if (!emit_expr(cg, if_expr->as.if_stmt.condition))
    return false;
  fputs(") {\n", cg->out);
  if (!emit_value_branch(cg, if_expr->as.if_stmt.then_branch, depth + 1,
                         target, target_name))
    return false;
  indent(cg->out, depth);
  fputs("} else {\n", cg->out);
  if (!emit_value_branch(cg, if_expr->as.if_stmt.else_branch, depth + 1,
                         target, target_name))
    return false;
  indent(cg->out, depth);
  fputs("}\n", cg->out);
  return true;
}

static bool emit_var_decl(Codegen *cg, AstNode *stmt, int depth) {
  indent(cg->out, depth);
  if (stmt->as.var_decl.is_const)
    fputs("const ", cg->out);
  if (stmt->as.var_decl.is_volatile)
    fputs("volatile ", cg->out);
  if (stmt->resolved_type && stmt->resolved_type->kind != TY_UNKNOWN &&
      stmt->resolved_type->kind != TY_INFER_ERROR) {
    if (!emit_semantic_decl(cg, stmt->resolved_type,
                            stmt->as.var_decl.name, stmt))
      return false;
  } else if (stmt->as.var_decl.type) {
    if (!emit_c_decl(cg, stmt->as.var_decl.type, stmt->as.var_decl.name, stmt))
      return false;
  } else if (!emit_semantic_decl(cg, stmt->resolved_type,
                                 stmt->as.var_decl.name, stmt)) {
    return false;
  }
  bool array_copy = stmt->resolved_type &&
                    stmt->resolved_type->kind == TY_ARRAY &&
                    stmt->as.var_decl.init &&
                    stmt->as.var_decl.init->kind != AST_ARRAY_LITERAL;
  bool match_init = stmt->as.var_decl.init &&
                    stmt->as.var_decl.init->kind == AST_MATCH_STMT;
  bool if_init = stmt->as.var_decl.init &&
                 stmt->as.var_decl.init->kind == AST_IF_STMT;
  bool runtime_init = depth == 0 && stmt->as.var_decl.init &&
                      !is_c_constant_expr(stmt->as.var_decl.init);
  bool fallible_init = stmt->as.var_decl.init &&
                       (stmt->as.var_decl.init->kind == AST_TRY_EXPR ||
                        stmt->as.var_decl.init->kind == AST_CATCH_EXPR);
  if (depth == 0 && array_copy) {
    cg_error(cg, stmt,
             "global array copy is not a constant C initializer");
    return false;
  }
  if (stmt->as.var_decl.init && !array_copy && !match_init && !if_init &&
      !fallible_init && !runtime_init) {
    fputs(" = ", cg->out);
    if (stmt->resolved_type && stmt->resolved_type->kind == TY_INTERFACE &&
        stmt->as.var_decl.init->resolved_type &&
        stmt->as.var_decl.init->resolved_type->kind != TY_INTERFACE) {
      if (!emit_interface_value(cg, stmt->resolved_type,
                                stmt->as.var_decl.init))
        return false;
    } else if (!emit_expr(cg, stmt->as.var_decl.init)) {
      return false;
    }
  }
  fputs(";\n", cg->out);
  if (runtime_init && !if_init && !match_init && !fallible_init)
    return register_runtime_initializer(cg, stmt);
  if (array_copy) {
    bool returned_array = stmt->as.var_decl.init->kind == AST_CALL_EXPR &&
                          stmt->as.var_decl.init->resolved_type &&
                          stmt->as.var_decl.init->resolved_type->kind ==
                              TY_ARRAY;
    if (returned_array) {
      char wrapper[1024];
      char temp[64];
      snprintf(temp, sizeof(temp), "__runes_array_%u", cg->temp_id++);
      if (!array_type_name(cg, stmt->resolved_type, wrapper,
                           sizeof(wrapper)))
        return false;
      indent(cg->out, depth);
      fprintf(cg->out, "%s %s = ", wrapper, temp);
      if (!emit_expr(cg, stmt->as.var_decl.init))
        return false;
      fputs(";\n", cg->out);
      indent(cg->out, depth);
      fprintf(cg->out, "memcpy(%s, %s.data, sizeof %s);\n",
              stmt->as.var_decl.name, temp, stmt->as.var_decl.name);
    } else {
      indent(cg->out, depth);
      fprintf(cg->out, "memcpy(%s, ", stmt->as.var_decl.name);
      if (!emit_expr(cg, stmt->as.var_decl.init))
        return false;
      fprintf(cg->out, ", sizeof %s);\n", stmt->as.var_decl.name);
    }
  }
  if (match_init)
    return emit_match(cg, stmt->as.var_decl.init, depth, NULL,
                      stmt->as.var_decl.name);
  if (if_init) {
    if (depth == 0) {
      return register_runtime_initializer(cg, stmt);
    }
    return emit_if_value(cg, stmt->as.var_decl.init, depth, NULL,
                         stmt->as.var_decl.name);
  }
  if (fallible_init)
    return emit_fallible_expr(cg, stmt->as.var_decl.init, depth, NULL,
                              stmt->as.var_decl.name);
  return true;
}

static bool emit_print_arg(Codegen *cg, AstNode *arg, int depth) {
  Type *type = arg->resolved_type;
  if (!type) {
    cg_error(cg, arg, "print argument has no resolved type");
    return false;
  }

  indent(cg->out, depth);
  if (type->kind == TY_POINTER) {
    fputs("printf(\"%p\", (void *)", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(");\n", cg->out);
    return true;
  }

  if (type->kind == TY_ERROR) {
    fputs("printf(\"%u\", (unsigned)", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(");\n", cg->out);
    return true;
  }

  if (type->kind != TY_PRIMITIVE) {
    cg_error(cg, arg, "unsupported print argument for C emission");
    return false;
  }

  const char *name = type->as.primitive.name;
  if (strcmp(name, "str") == 0) {
    fputs("fputs(", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(", stdout);\n", cg->out);
  } else if (strcmp(name, "char") == 0) {
    fputs("fputc(", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(", stdout);\n", cg->out);
  } else if (strcmp(name, "bool") == 0) {
    fputs("fputs(", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(" ? \"true\" : \"false\", stdout);\n", cg->out);
  } else if (type_is_float(type)) {
    fputs("printf(\"%.17g\", (double)", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(");\n", cg->out);
  } else if (type_is_integer(type)) {
    const NumericTypeInfo *info = get_numeric_info(name);
    fputs(info && info->is_signed ? "printf(\"%lld\", (long long)"
                                  : "printf(\"%llu\", (unsigned long long)",
          cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(");\n", cg->out);
  } else {
    cg_error(cg, arg, "unsupported primitive print argument");
    return false;
  }
  return true;
}

static bool emit_print_stmt(Codegen *cg, AstNode *stmt, int depth) {
  AstNode *arg = stmt->as.call.args;
  bool first = true;
  while (arg) {
    if (!first) {
      indent(cg->out, depth);
      fputs("fputc(' ', stdout);\n", cg->out);
    }
    if (!emit_print_arg(cg, arg, depth))
      return false;
    first = false;
    arg = arg->next;
  }
  indent(cg->out, depth);
  fputs("fputc('\\n', stdout);\n", cg->out);
  return true;
}

static bool emit_for_stmt(Codegen *cg, AstNode *stmt, int depth) {
  AstNode *iter = stmt->as.for_stmt.iter;
  unsigned id = cg->temp_id++;
  if (iter->kind == AST_RANGE_EXPR) {
    if (stmt->as.for_stmt.cap_index) {
      indent(cg->out, depth);
      fprintf(cg->out, "size_t %s = 0;\n", stmt->as.for_stmt.cap_index);
    }
    indent(cg->out, depth);
    fputs("for (", cg->out);
    if (!emit_semantic_decl(cg, iter->resolved_type,
                            stmt->as.for_stmt.cap_value, stmt))
      return false;
    fputs(" = ", cg->out);
    if (!emit_expr(cg, iter->as.range_expr.start))
      return false;
    fprintf(cg->out, "; %s %s ", stmt->as.for_stmt.cap_value,
            iter->as.range_expr.inclusive ? "<=" : "<");
    if (!emit_expr(cg, iter->as.range_expr.end))
      return false;
    fprintf(cg->out, "; ++%s", stmt->as.for_stmt.cap_value);
    if (stmt->as.for_stmt.cap_index)
      fprintf(cg->out, ", ++%s", stmt->as.for_stmt.cap_index);
    fputs(") {\n", cg->out);
  } else if (iter->resolved_type && iter->resolved_type->kind == TY_ARRAY) {
    Type *element = iter->resolved_type->as.array.inner;
    indent(cg->out, depth);
    fprintf(cg->out,
            "for (size_t __runes_i_%u = 0; __runes_i_%u < %zu; "
            "++__runes_i_%u) {\n",
            id, id, iter->resolved_type->as.array.size, id);
    if (stmt->as.for_stmt.cap_index) {
      indent(cg->out, depth + 1);
      fprintf(cg->out, "size_t %s = __runes_i_%u;\n",
              stmt->as.for_stmt.cap_index, id);
    }
    indent(cg->out, depth + 1);
    if (stmt->as.for_stmt.cap_kind == CAPTURE_PTR ||
        stmt->as.for_stmt.cap_kind == CAPTURE_PTR_INDEXED) {
      const char *base = element->kind == TY_PRIMITIVE
                             ? c_named_type(element->as.primitive.name)
                             : NULL;
      if (!base) {
        cg_error(cg, stmt, "unsupported pointer capture type");
        return false;
      }
      fprintf(cg->out, "%s *%s = &", base, stmt->as.for_stmt.cap_value);
    } else {
      if (!emit_semantic_decl(cg, element, stmt->as.for_stmt.cap_value, stmt))
        return false;
      fputs(" = ", cg->out);
    }
    if (!emit_expr(cg, iter))
      return false;
    fprintf(cg->out, "[__runes_i_%u];\n", id);
  } else {
    cg_error(cg, stmt, "unsupported for-loop iterable for C emission");
    return false;
  }

  if (!emit_block(cg, stmt->as.for_stmt.body, depth + 1))
    return false;
  indent(cg->out, depth);
  fputs("}\n", cg->out);
  return true;
}

static bool emit_pattern_condition(Codegen *cg, AstNode *pattern,
                                   Type *subject_type,
                                   const char *value_expr) {
  if (!pattern) {
    fputs("false", cg->out);
    return true;
  }

  switch (pattern->kind) {
  case AST_IDENTIFIER: {
    const char *name = pattern->as.identifier.name;
    int arm = variant_arm_index(subject_type, name);
    if (arm >= 0) {
      fprintf(cg->out, "(%s.tag == %s_%s)", value_expr,
              registered_type_name(cg, subject_type), name);
    } else {
      fputs("true", cg->out);
    }
    return true;
  }
  case AST_FIELD_EXPR: {
    const char *name = pattern->as.field.field;
    int arm = variant_arm_index(subject_type, name);
    if (arm < 0 && subject_type && subject_type->kind == TY_ERROR) {
      const char *set_name =
          registered_decl_name(cg, pattern->as.field.target->resolved_decl);
      if (!set_name && pattern->as.field.target->kind == AST_IDENTIFIER)
        set_name = pattern->as.field.target->as.identifier.name;
      fprintf(cg->out, "(%s == %s_%s)", value_expr, set_name, name);
      return true;
    }
    if (arm < 0) {
      cg_error(cg, pattern, "unsupported qualified match pattern");
      return false;
    }
    fprintf(cg->out, "(%s.tag == %s_%s)", value_expr,
            registered_type_name(cg, subject_type), name);
    return true;
  }
  case AST_INT_LITERAL:
  case AST_FLOAT_LITERAL:
  case AST_BOOL_LITERAL:
  case AST_CHAR_LITERAL:
    fprintf(cg->out, "(%s == ", value_expr);
    if (!emit_expr(cg, pattern))
      return false;
    fputc(')', cg->out);
    return true;
  case AST_STRING_LITERAL:
    fprintf(cg->out, "(strcmp(%s, ", value_expr);
    if (!emit_expr(cg, pattern))
      return false;
    fputs(") == 0)", cg->out);
    return true;
  case AST_STRUCT_PATTERN: {
    const char *name = pattern->as.struct_pattern.name;
    if (subject_type && subject_type->kind == TY_FALLIBLE) {
      if (strcmp(name, "Ok") == 0) {
        fprintf(cg->out, "(%s.ok)", value_expr);
        return true;
      }
      if (strcmp(name, "Err") == 0) {
        fprintf(cg->out, "(!%s.ok)", value_expr);
        return true;
      }
      cg_error(cg, pattern, "unknown fallible match pattern");
      return false;
    }
    if (subject_type && subject_type->kind == TY_VARIANT) {
      int arm = variant_arm_index(subject_type, name);
      if (arm < 0) {
        cg_error(cg, pattern, "unknown variant arm in match pattern");
        return false;
      }
      fprintf(cg->out, "(%s.tag == %s_%s", value_expr,
              registered_type_name(cg, subject_type), name);
      Type *payload = subject_type->as.variant.arm_types[arm];
      int field_index = 0;
      for (AstNode *field = pattern->as.struct_pattern.fields; field;
           field = field->next, field_index++) {
        AstNode *inner = field->as.field_pattern.pattern;
        if (inner && inner->kind != AST_IDENTIFIER) {
          char access[1024];
          snprintf(access, sizeof(access), "%s.data.%s._%d", value_expr,
                   name, field_index);
          Type *field_type = payload && payload->kind == TY_TUPLE
                                 ? payload->as.tuple.elems[field_index]
                                 : payload;
          fputs(" && ", cg->out);
          if (!emit_pattern_condition(cg, inner, field_type, access))
            return false;
        }
      }
      fputc(')', cg->out);
      return true;
    }

    if (subject_type && subject_type->kind == TY_STRUCT) {
      fputc('(', cg->out);
      bool first = true;
      int position = 0;
      for (AstNode *field = pattern->as.struct_pattern.fields; field;
           field = field->next, position++) {
        AstNode *inner = field->as.field_pattern.pattern;
        if (!inner || inner->kind == AST_IDENTIFIER)
          continue;
        const char *field_name = field->as.field_pattern.name
                                     ? field->as.field_pattern.name
                                     : subject_type->as.struct_t
                                           .field_names[position];
        int field_index = position;
        for (int i = 0; i < subject_type->as.struct_t.field_count; i++) {
          if (strcmp(subject_type->as.struct_t.field_names[i], field_name) == 0)
            field_index = i;
        }
        char access[1024];
        snprintf(access, sizeof(access), "%s.%s", value_expr, field_name);
        if (!first)
          fputs(" && ", cg->out);
        if (!emit_pattern_condition(cg, inner,
                                    subject_type->as.struct_t
                                        .field_types[field_index],
                                    access))
          return false;
        first = false;
      }
      fputs(first ? "true)" : ")", cg->out);
      return true;
    }
    break;
  }
  default:
    break;
  }

  cg_error(cg, pattern, "unsupported match pattern for C emission");
  return false;
}

static bool emit_pattern_bindings(Codegen *cg, AstNode *pattern,
                                  Type *subject_type,
                                  const char *value_expr, int depth) {
  if (!pattern)
    return true;
  if (pattern->kind == AST_IDENTIFIER) {
    const char *name = pattern->as.identifier.name;
    if (strcmp(name, "_") == 0 || variant_arm_index(subject_type, name) >= 0)
      return true;
    indent(cg->out, depth);
    if (!emit_semantic_decl(cg, subject_type, name, pattern))
      return false;
    fprintf(cg->out, " = %s;\n", value_expr);
    return true;
  }
  if (pattern->kind != AST_STRUCT_PATTERN)
    return true;

  const char *arm_name = pattern->as.struct_pattern.name;
  Type *payload = NULL;
  bool fallible_pattern = subject_type && subject_type->kind == TY_FALLIBLE;
  if (subject_type && subject_type->kind == TY_VARIANT) {
    int arm = variant_arm_index(subject_type, arm_name);
    if (arm >= 0)
      payload = subject_type->as.variant.arm_types[arm];
  }
  int position = 0;
  for (AstNode *field = pattern->as.struct_pattern.fields; field;
       field = field->next, position++) {
    AstNode *inner = field->as.field_pattern.pattern;
    if (!inner || inner->kind != AST_IDENTIFIER ||
        strcmp(inner->as.identifier.name, "_") == 0)
      continue;
    Type *field_type = field->resolved_type;
    char access[1024];
    if (fallible_pattern) {
      snprintf(access, sizeof(access), "%s.%s", value_expr,
               strcmp(arm_name, "Ok") == 0 ? "value" : "error");
      if (!field_type) {
        field_type = strcmp(arm_name, "Ok") == 0
                         ? subject_type->as.fallible.inner
                         : pattern->as.struct_pattern.fields->resolved_type;
      }
    } else if (subject_type && subject_type->kind == TY_VARIANT) {
      snprintf(access, sizeof(access), "%s.data.%s._%d", value_expr,
               arm_name, position);
      if (!field_type)
        field_type = payload && payload->kind == TY_TUPLE
                         ? payload->as.tuple.elems[position]
                         : payload;
    } else if (subject_type && subject_type->kind == TY_STRUCT) {
      const char *field_name = field->as.field_pattern.name
                                   ? field->as.field_pattern.name
                                   : subject_type->as.struct_t
                                         .field_names[position];
      snprintf(access, sizeof(access), "%s.%s", value_expr, field_name);
    } else {
      cg_error(cg, pattern, "unsupported destructuring binding");
      return false;
    }
    indent(cg->out, depth);
    if (!emit_semantic_decl(cg, field_type, inner->as.identifier.name, inner))
      return false;
    fprintf(cg->out, " = %s;\n", access);
  }
  return true;
}

static bool emit_match_result(Codegen *cg, AstNode *body, int depth,
                              AstNode *target, const char *target_name) {
  if (!target && !target_name) {
    if (body->kind == AST_BLOCK)
      return emit_block(cg, body, depth);
    if (body->kind == AST_CALL_EXPR || body->kind == AST_ASSIGN ||
        body->kind == AST_IF_STMT || body->kind == AST_MATCH_STMT)
      return emit_stmt(cg, body, depth);
    indent(cg->out, depth);
    if (!emit_expr(cg, body))
      return false;
    fputs(";\n", cg->out);
    return true;
  }

  AstNode *result = body;
  if (body->kind == AST_BLOCK) {
    result = body->as.block.statements;
    if (!result) {
      cg_error(cg, body, "match expression arm block has no result");
      return false;
    }
    while (result->next) {
      if (!emit_stmt(cg, result, depth))
        return false;
      result = result->next;
    }
  }
  if (result->kind == AST_IF_STMT)
    return emit_if_value(cg, result, depth, target, target_name);
  if (result->kind == AST_MATCH_STMT)
    return emit_match(cg, result, depth, target, target_name);
  indent(cg->out, depth);
  if (target_name)
    fputs(target_name, cg->out);
  else if (!emit_expr(cg, target))
    return false;
  fputs(" = ", cg->out);
  if (!emit_expr(cg, result))
    return false;
  fputs(";\n", cg->out);
  return true;
}

static bool emit_match(Codegen *cg, AstNode *match, int depth,
                       AstNode *target, const char *target_name) {
  unsigned id = cg->temp_id++;
  Type *subject_type = match->as.match_stmt.subject->resolved_type;
  char subject_name[64];
  char matched_name[64];
  snprintf(subject_name, sizeof(subject_name), "__runes_match_%u", id);
  snprintf(matched_name, sizeof(matched_name), "__runes_matched_%u", id);

  indent(cg->out, depth);
  fputs("{\n", cg->out);
  indent(cg->out, depth + 1);
  if (!emit_semantic_decl(cg, subject_type, subject_name, match))
    return false;
  fputs(" = ", cg->out);
  if (!emit_expr(cg, match->as.match_stmt.subject))
    return false;
  fputs(";\n", cg->out);
  indent(cg->out, depth + 1);
  fprintf(cg->out, "bool %s = false;\n", matched_name);

  for (AstNode *arm = match->as.match_stmt.arms; arm; arm = arm->next) {
    indent(cg->out, depth + 1);
    fprintf(cg->out, "if (!%s && ", matched_name);
    if (!emit_pattern_condition(cg, arm->as.match_arm.pattern, subject_type,
                                subject_name))
      return false;
    fputs(") {\n", cg->out);
    if (!emit_pattern_bindings(cg, arm->as.match_arm.pattern, subject_type,
                               subject_name, depth + 2))
      return false;
    int body_depth = depth + 2;
    if (arm->as.match_arm.guard) {
      indent(cg->out, depth + 2);
      fputs("if (", cg->out);
      if (!emit_expr(cg, arm->as.match_arm.guard))
        return false;
      fputs(") {\n", cg->out);
      body_depth++;
    }
    indent(cg->out, body_depth);
    fprintf(cg->out, "%s = true;\n", matched_name);
    if (!emit_match_result(cg, arm->as.match_arm.body, body_depth, target,
                           target_name))
      return false;
    if (arm->as.match_arm.guard) {
      indent(cg->out, depth + 2);
      fputs("}\n", cg->out);
    }
    indent(cg->out, depth + 1);
    fputs("}\n", cg->out);
  }
  indent(cg->out, depth + 1);
  fprintf(cg->out, "if (!%s) abort();\n", matched_name);
  indent(cg->out, depth);
  fputs("}\n", cg->out);
  return true;
}

static bool emit_stmt(Codegen *cg, AstNode *stmt, int depth) {
  switch (stmt->kind) {
  case AST_VAR_DECL:
    return emit_var_decl(cg, stmt, depth);
  case AST_FUNC_DECL:
    return true;
  case AST_TUPLE_DESTRUCTURE: {
    Type *tuple = stmt->as.tuple_destructure.init->resolved_type;
    if (!tuple || tuple->kind != TY_TUPLE) {
      cg_error(cg, stmt, "tuple destructure has no resolved tuple type");
      return false;
    }
    unsigned id = cg->temp_id++;
    char temp[64];
    snprintf(temp, sizeof(temp), "__runes_tuple_%u", id);
    indent(cg->out, depth);
    if (!emit_semantic_decl(cg, tuple, temp, stmt))
      return false;
    fputs(" = ", cg->out);
    if (!emit_expr(cg, stmt->as.tuple_destructure.init))
      return false;
    fputs(";\n", cg->out);
    int index = 0;
    for (AstNode *target = stmt->as.tuple_destructure.targets; target;
         target = target->next, index++) {
      if (!emit_var_decl(cg, target, depth))
        return false;
      indent(cg->out, depth);
      fprintf(cg->out, "%s = %s._%d;\n", target->as.var_decl.name, temp,
              index);
    }
    return true;
  }
  case AST_ASSIGN:
    if ((stmt->as.assign.value->kind == AST_ERROR_EXPR ||
         (stmt->as.assign.value->resolved_type &&
          stmt->as.assign.value->resolved_type->kind == TY_ERROR)) &&
        cg->current_fallible && cg->current_result_name &&
        stmt->as.assign.target->kind == AST_IDENTIFIER &&
        strcmp(stmt->as.assign.target->as.identifier.name,
               cg->current_result_name) == 0) {
      indent(cg->out, depth);
      fprintf(cg->out, "%s = ", cg->current_error_name);
      if (!emit_expr(cg, stmt->as.assign.value))
        return false;
      fputs(";\n", cg->out);
      return true;
    }
    if (stmt->as.assign.value->kind == AST_MATCH_STMT)
      return emit_match(cg, stmt->as.assign.value, depth,
                        stmt->as.assign.target, NULL);
    if (stmt->as.assign.value->kind == AST_IF_STMT)
      return emit_if_value(cg, stmt->as.assign.value, depth,
                           stmt->as.assign.target, NULL);
    if (stmt->as.assign.value->kind == AST_TRY_EXPR ||
        stmt->as.assign.value->kind == AST_CATCH_EXPR)
      return emit_fallible_expr(cg, stmt->as.assign.value, depth,
                                stmt->as.assign.target, NULL);
    if (cg->current_fallible && cg->current_result_name &&
        stmt->as.assign.target->kind == AST_IDENTIFIER &&
        strcmp(stmt->as.assign.target->as.identifier.name,
               cg->current_result_name) == 0 &&
        stmt->as.assign.value->resolved_type &&
        stmt->as.assign.value->resolved_type->kind == TY_FALLIBLE) {
      char temp_name[64];
      snprintf(temp_name, sizeof(temp_name), "__runes_result_%u",
               cg->temp_id++);
      indent(cg->out, depth);
      if (!emit_semantic_decl(cg, stmt->as.assign.value->resolved_type,
                              temp_name, stmt))
        return false;
      fputs(" = ", cg->out);
      if (!emit_expr(cg, stmt->as.assign.value))
        return false;
      fputs(";\n", cg->out);
      indent(cg->out, depth);
      fprintf(cg->out, "if (!%s.ok) {\n", temp_name);
      char error_access[128];
      snprintf(error_access, sizeof(error_access), "%s.error", temp_name);
      if (!emit_fallible_return(cg, depth + 1, error_access, true))
        return false;
      indent(cg->out, depth);
      fputs("}\n", cg->out);
      indent(cg->out, depth);
      fprintf(cg->out, "%s = %s.value;\n", cg->current_result_c_name,
              temp_name);
      return true;
    }
    indent(cg->out, depth);
    if (stmt->as.assign.target->resolved_type &&
        stmt->as.assign.target->resolved_type->kind == TY_ARRAY) {
      bool returned_array = stmt->as.assign.value->kind == AST_CALL_EXPR &&
                            stmt->as.assign.value->resolved_type &&
                            stmt->as.assign.value->resolved_type->kind ==
                                TY_ARRAY;
      if (returned_array) {
        char wrapper[1024];
        char temp[64];
        snprintf(temp, sizeof(temp), "__runes_array_%u", cg->temp_id++);
        if (!array_type_name(cg, stmt->as.assign.target->resolved_type,
                             wrapper, sizeof(wrapper)))
          return false;
        fprintf(cg->out, "%s %s = ", wrapper, temp);
        if (!emit_expr(cg, stmt->as.assign.value))
          return false;
        fputs(";\n", cg->out);
        indent(cg->out, depth);
        fputs("memcpy(", cg->out);
        if (!emit_expr(cg, stmt->as.assign.target))
          return false;
        fprintf(cg->out, ", %s.data, sizeof ", temp);
        if (!emit_expr(cg, stmt->as.assign.target))
          return false;
        fputs(");\n", cg->out);
        return true;
      }
      fputs("memcpy(", cg->out);
      if (!emit_expr(cg, stmt->as.assign.target))
        return false;
      fputs(", ", cg->out);
      if (stmt->as.assign.value->kind == AST_ARRAY_LITERAL) {
        char array_decl[1024];
        if (!build_semantic_decl(
                cg, stmt->as.assign.target->resolved_type, "", array_decl,
                sizeof(array_decl)))
          return false;
        fprintf(cg->out, "(%s)", array_decl);
      }
      if (!emit_expr(cg, stmt->as.assign.value))
        return false;
      fputs(", sizeof ", cg->out);
      if (!emit_expr(cg, stmt->as.assign.target))
        return false;
      fputs(");\n", cg->out);
      return true;
    }
    if (!emit_expr(cg, stmt->as.assign.target))
      return false;
    fputs(" = ", cg->out);
    if (stmt->as.assign.target->resolved_type &&
        stmt->as.assign.target->resolved_type->kind == TY_INTERFACE &&
        stmt->as.assign.value->resolved_type &&
        stmt->as.assign.value->resolved_type->kind != TY_INTERFACE) {
      if (!emit_interface_value(cg, stmt->as.assign.target->resolved_type,
                                stmt->as.assign.value))
        return false;
    } else if (!emit_expr(cg, stmt->as.assign.value)) {
      return false;
    }
    fputs(";\n", cg->out);
    return true;
  case AST_CALL_EXPR:
    if (stmt->as.call.callee->kind == AST_IDENTIFIER &&
        strcmp(stmt->as.call.callee->as.identifier.name, "print") == 0)
      return emit_print_stmt(cg, stmt, depth);
    indent(cg->out, depth);
    if (!emit_expr(cg, stmt))
      return false;
    fputs(";\n", cg->out);
    return true;
  case AST_IF_STMT:
    indent(cg->out, depth);
    fputs("if (", cg->out);
    if (!emit_expr(cg, stmt->as.if_stmt.condition))
      return false;
    fputs(") {\n", cg->out);
    if (!emit_block(cg, stmt->as.if_stmt.then_branch, depth + 1))
      return false;
    indent(cg->out, depth);
    fputs("}", cg->out);
    if (stmt->as.if_stmt.else_branch) {
      fputs(" else {\n", cg->out);
      if (stmt->as.if_stmt.else_branch->kind == AST_IF_STMT) {
        if (!emit_stmt(cg, stmt->as.if_stmt.else_branch, depth + 1))
          return false;
      } else if (stmt->as.if_stmt.else_branch->kind == AST_BLOCK) {
        if (!emit_block(cg, stmt->as.if_stmt.else_branch, depth + 1))
          return false;
      } else {
        cg_error(cg, stmt->as.if_stmt.else_branch,
                 "invalid else branch during C emission");
        return false;
      }
      indent(cg->out, depth);
      fputs("}", cg->out);
    }
    fputc('\n', cg->out);
    return true;
  case AST_WHILE_STMT:
    indent(cg->out, depth);
    fputs("while (", cg->out);
    if (!emit_expr(cg, stmt->as.while_stmt.condition))
      return false;
    fputs(") {\n", cg->out);
    if (!emit_block(cg, stmt->as.while_stmt.body, depth + 1))
      return false;
    indent(cg->out, depth);
    fputs("}\n", cg->out);
    return true;
  case AST_FOR_STMT:
    return emit_for_stmt(cg, stmt, depth);
  case AST_MATCH_STMT:
    return emit_match(cg, stmt, depth, NULL, NULL);
  case AST_TRY_EXPR:
  case AST_CATCH_EXPR:
    return emit_fallible_expr(cg, stmt, depth, NULL, NULL);
  case AST_LOOP_STMT:
    indent(cg->out, depth);
    fputs("for (;;) {\n", cg->out);
    if (!emit_block(cg, stmt->as.loop_stmt.body, depth + 1))
      return false;
    indent(cg->out, depth);
    fputs("}\n", cg->out);
    return true;
  case AST_BREAK_STMT:
    indent(cg->out, depth);
    fputs("break;\n", cg->out);
    return true;
  case AST_CONTINUE_STMT:
    indent(cg->out, depth);
    fputs("continue;\n", cg->out);
    return true;
  case AST_RETURN_STMT:
    if (cg->current_fallible) {
      if (stmt->as.return_stmt.value &&
          stmt->as.return_stmt.value->kind == AST_ERROR_EXPR) {
        char error_name[1024];
        AstNode *set = stmt->as.return_stmt.value->as.error_expr.path;
        AstNode *member = set ? set->next : NULL;
        if (!set || !member)
          return false;
        snprintf(error_name, sizeof(error_name), "%s_%s",
                 set->as.identifier.name, member->as.identifier.name);
        return emit_fallible_return(cg, depth, error_name, true);
      }
      if (!stmt->as.return_stmt.value)
        return emit_fallible_return(cg, depth, cg->current_error_name, false);
    }
    indent(cg->out, depth);
    fputs("return", cg->out);
    if (stmt->as.return_stmt.value) {
      fputc(' ', cg->out);
      if (!emit_expr(cg, stmt->as.return_stmt.value))
        return false;
    } else if (cg->current_result_c_name) {
      fprintf(cg->out, " %s", cg->current_result_c_name);
    }
    fputs(";\n", cg->out);
    return true;
  case AST_BLOCK:
    indent(cg->out, depth);
    fputs("{\n", cg->out);
    if (!emit_block(cg, stmt, depth + 1))
      return false;
    indent(cg->out, depth);
    fputs("}\n", cg->out);
    return true;
  case AST_UNSAFE_BLOCK:
    indent(cg->out, depth);
    fputs("{\n", cg->out);
    if (!emit_block(cg, stmt->as.unsafe_block.body, depth + 1))
      return false;
    indent(cg->out, depth);
    fputs("}\n", cg->out);
    return true;
  case AST_ASM_EXPR:
    indent(cg->out, depth);
    fputs("__asm__ volatile (", cg->out);
    emit_inline_asm_string(cg->out, stmt->as.asm_expr.code,
                           stmt->as.asm_expr.output != NULL);
    const char *asm_output = stmt->as.asm_expr.output;
    if (asm_output && cg->current_result_name && cg->current_result_c_name &&
        strcmp(asm_output, cg->current_result_name) == 0)
      asm_output = cg->current_result_c_name;
    if (asm_output && stmt->resolved_type &&
        stmt->resolved_type->kind == TY_TUPLE) {
      static const char *constraints[] = {"a", "b", "c", "d"};
      fputs(" : ", cg->out);
      for (int i = 0; i < stmt->resolved_type->as.tuple.count; i++) {
        if (i)
          fputs(", ", cg->out);
        fprintf(cg->out, "\"=%s\"(%s._%d)", constraints[i],
                asm_output, i);
      }
    } else if (asm_output) {
      fprintf(cg->out, " : \"=a\"(%s)", asm_output);
    }
    fputs(");\n", cg->out);
    return true;
  default:
    cg_error(cg, stmt, "unsupported statement for C emission");
    return false;
  }
}

static bool emit_params(Codegen *cg, AstNode *param, Type *function_type) {
  if (!param) {
    fputs("void", cg->out);
    return true;
  }

  int index = 0;
  while (param) {
    Type *semantic_param = function_type && function_type->kind == TY_FUNCTION &&
                                   index < function_type->as.function.param_count
                               ? function_type->as.function.params[index]
                               : NULL;
    if (semantic_param && semantic_param->kind != TY_UNKNOWN &&
        semantic_param->kind != TY_INFER_ERROR) {
      if (!emit_semantic_decl(cg, semantic_param, param->as.param.name, param))
        return false;
    } else if (param->as.param.type) {
      if (!emit_c_decl(cg, param->as.param.type, param->as.param.name, param))
        return false;
    } else if (!function_type || function_type->kind != TY_FUNCTION ||
               index >= function_type->as.function.param_count ||
               !emit_semantic_decl(cg, function_type->as.function.params[index],
                                   param->as.param.name, param)) {
      return false;
    }
    if (param->next)
      fputs(", ", cg->out);
    param = param->next;
    index++;
  }
  return true;
}

static bool emit_func_header(Codegen *cg, AstNode *decl, const char *c_name,
                             bool is_main) {
  Type *function_type = decl->resolved_type;
  if (is_main) {
    fputs("int main(", cg->out);
  } else {
    fputs("static ", cg->out);
    Type *return_type = decl->resolved_type &&
                                decl->resolved_type->kind == TY_FUNCTION
                            ? decl->resolved_type->as.function.ret
                            : NULL;
    if (!decl->as.func_decl.ret_type) {
      fprintf(cg->out, "void %s", c_name);
    } else if (return_type && return_type->kind == TY_ARRAY) {
      char array_name[1024];
      if (!array_type_name(cg, return_type, array_name, sizeof(array_name)))
        return false;
      fprintf(cg->out, "%s %s", array_name, c_name);
    } else if (return_type) {
      if (!emit_semantic_decl(cg, return_type, c_name, decl))
        return false;
    } else if (!emit_c_decl(cg, decl->as.func_decl.ret_type, c_name, decl)) {
      return false;
    }
    fputc('(', cg->out);
  }
  if (!emit_params(cg, decl->as.func_decl.params, function_type))
    return false;
  fputc(')', cg->out);
  return true;
}

static bool emit_extern_declaration(Codegen *cg, AstNode *decl) {
  if (strcmp(decl->as.extern_decl.name, "memset") == 0 ||
      strcmp(decl->as.extern_decl.name, "memcpy") == 0 ||
      strcmp(decl->as.extern_decl.name, "memcmp") == 0 ||
      strcmp(decl->as.extern_decl.name, "memmove") == 0 ||
      strcmp(decl->as.extern_decl.name, "sqrt") == 0)
    return true;
  fputs("extern ", cg->out);
  if (decl->as.extern_decl.is_func) {
    Type *return_type = decl->resolved_type &&
                                decl->resolved_type->kind == TY_FUNCTION
                            ? decl->resolved_type->as.function.ret
                            : NULL;
    if (!decl->as.extern_decl.ret_type) {
      fprintf(cg->out, "void %s", decl->as.extern_decl.name);
    } else if (return_type && return_type->kind == TY_FALLIBLE) {
      if (!emit_semantic_decl(cg, return_type, decl->as.extern_decl.name,
                              decl))
        return false;
    } else if (return_type && return_type->kind == TY_TUPLE) {
      if (!emit_semantic_decl(cg, return_type, decl->as.extern_decl.name,
                              decl))
        return false;
    } else if (!emit_c_decl(cg, decl->as.extern_decl.ret_type,
                            decl->as.extern_decl.name, decl)) {
      return false;
    }
    fputc('(', cg->out);
    if (!emit_params(cg, decl->as.extern_decl.params, decl->resolved_type))
      return false;
    fputs(");\n", cg->out);
  } else {
    if (!emit_c_decl(cg, decl->as.extern_decl.var_type,
                     decl->as.extern_decl.name, decl))
      return false;
    fputs(";\n", cg->out);
  }
  return true;
}

static bool emit_struct_definition(Codegen *cg, AstNode *decl) {
  const char *type_name = registered_decl_name(cg, decl);
  fprintf(cg->out, "struct %s {\n", type_name);
  for (AstNode *field = decl->as.type_decl.fields; field;
       field = field->next) {
    fputs("  ", cg->out);
    if (!emit_c_decl(cg, field->as.field_decl.type,
                     field->as.field_decl.name, field))
      return false;
    fputs(";\n", cg->out);
  }
  fputs("};\n\n", cg->out);
  return true;
}

static bool emit_variant_definition(Codegen *cg, AstNode *decl) {
  const char *type_name = registered_decl_name(cg, decl);
  fprintf(cg->out, "typedef enum %s_Tag {\n", type_name);
  for (AstNode *arm = decl->as.variant_decl.arms; arm; arm = arm->next) {
    fprintf(cg->out, "  %s_%s%s\n", type_name, arm->as.variant_arm.name,
            arm->next ? "," : "");
  }
  fprintf(cg->out, "} %s_Tag;\n\n", type_name);

  bool has_payload = false;
  for (AstNode *arm = decl->as.variant_decl.arms; arm; arm = arm->next) {
    if (arm->as.variant_arm.fields) {
      has_payload = true;
      break;
    }
  }

  fprintf(cg->out, "struct %s {\n  %s_Tag tag;\n", type_name, type_name);
  if (has_payload) {
    fputs("  union {\n", cg->out);
    for (AstNode *arm = decl->as.variant_decl.arms; arm; arm = arm->next) {
      if (!arm->as.variant_arm.fields)
        continue;
      fprintf(cg->out, "    struct {\n");
      int field_index = 0;
      for (AstNode *field = arm->as.variant_arm.fields; field;
           field = field->next, field_index++) {
        fputs("      ", cg->out);
        char name[32];
        snprintf(name, sizeof(name), "_%d", field_index);
        if (!emit_c_decl(cg, field, name, field))
          return false;
        fputs(";\n", cg->out);
      }
      fprintf(cg->out, "    } %s;\n", arm->as.variant_arm.name);
    }
    fputs("  } data;\n", cg->out);
  }
  fputs("};\n\n", cg->out);
  return true;
}

static bool emit_interface_definition(Codegen *cg, AstNode *decl) {
  const char *name = registered_decl_name(cg, decl);
  fprintf(cg->out, "struct %s {\n  void *data;\n", name);
  for (AstNode *method = decl->as.interface_decl.methods; method;
       method = method->next) {
    Type *function = method->resolved_type;
    Type *ret = function && function->kind == TY_FUNCTION
                    ? function->as.function.ret
                    : NULL;
    fputs("  ", cg->out);
    char pointer_name[320];
    snprintf(pointer_name, sizeof(pointer_name), "(*%s)",
             method->as.func_decl.name);
    if (!emit_semantic_decl(cg, ret, pointer_name, method))
      return false;
    fputs("(void *data", cg->out);
    AstNode *param = method->as.func_decl.params;
    int index = 0;
    while (param) {
      if (index > 0) {
        fputs(", ", cg->out);
        if (!emit_semantic_decl(cg, function->as.function.params[index],
                                param->as.param.name, param))
          return false;
      }
      index++;
      param = param->next;
    }
    fputs(");\n", cg->out);
  }
  fputs("};\n\n", cg->out);
  return true;
}

static AstNode *find_impl_method(AstNode *impl, const char *name) {
  for (AstNode *method = impl->as.method_decl.methods; method;
       method = method->next)
    if (strcmp(method->as.func_decl.name, name) == 0)
      return method;
  return NULL;
}

static bool emit_interface_adapters(Codegen *cg, bool prototypes) {
  for (int i = 0; i < cg->interface_impl_count; i++) {
    AstNode *impl = cg->interface_impls[i];
    Type *concrete = NULL;
    if (impl->as.method_decl.methods &&
        impl->as.method_decl.methods->resolved_type &&
        impl->as.method_decl.methods->resolved_type->kind == TY_FUNCTION &&
        impl->as.method_decl.methods->resolved_type->as.function.param_count)
      concrete = impl->as.method_decl.methods->resolved_type
                     ->as.function.params[0];
    if (!concrete || concrete->kind != TY_STRUCT)
      continue;
    Type *interface = NULL;
    for (int t = 0; t < cg->named_type_count; t++)
      if (cg->named_types[t]->kind == TY_INTERFACE &&
          strcmp(cg->named_types[t]->as.interface_t.name,
                 impl->as.method_decl.iface_name) == 0) {
        interface = cg->named_types[t];
        break;
      }
    if (!interface)
      continue;
    const char *iface_name = registered_type_name(cg, interface);
    const char *concrete_name = registered_type_name(cg, concrete);
    for (int m = 0; m < interface->as.interface_t.method_count; m++) {
      AstNode *method = find_impl_method(
          impl, interface->as.interface_t.method_names[m]);
      if (!method) {
        cg_error(cg, impl,
                 "interface implementation is missing a required method");
        return false;
      }
      Type *function = method->resolved_type;
      Type *ret = function->as.function.ret;
      char adapter[768];
      snprintf(adapter, sizeof(adapter), "%s_%s_%s_adapter", iface_name,
               concrete_name, method->as.func_decl.name);
      if (!emit_semantic_decl(cg, ret, adapter, method))
        return false;
      fputs("(void *data", cg->out);
      AstNode *param = method->as.func_decl.params;
      int index = 0;
      while (param) {
        if (index > 0) {
          fputs(", ", cg->out);
          if (!emit_semantic_decl(cg, function->as.function.params[index],
                                  param->as.param.name, param))
            return false;
        }
        index++;
        param = param->next;
      }
      if (prototypes) {
        fputs(");\n", cg->out);
        continue;
      }
      fputs(") {\n  ", cg->out);
      bool is_void = ret->kind == TY_PRIMITIVE &&
                     strcmp(ret->as.primitive.name, "void") == 0;
      if (!is_void)
        fputs("return ", cg->out);
      fprintf(cg->out, "%s_%s(*(%s *)data", concrete_name,
              method->as.func_decl.name, concrete_name);
      param = method->as.func_decl.params;
      if (param)
        param = param->next;
      for (; param; param = param->next)
        fprintf(cg->out, ", %s", param->as.param.name);
      fputs(");\n}\n", cg->out);
    }
  }
  return true;
}

static bool emit_func(Codegen *cg, AstNode *decl, const char *c_name,
                      bool is_main) {
  Type *saved_fallible = cg->current_fallible;
  const char *saved_result = cg->current_result_name;
  const char *saved_result_c = cg->current_result_c_name;
  const char *saved_error = cg->current_error_name;
  Type *return_type = decl->resolved_type &&
                              decl->resolved_type->kind == TY_FUNCTION
                          ? decl->resolved_type->as.function.ret
                          : NULL;
  bool is_fallible = return_type && return_type->kind == TY_FALLIBLE;
  cg->current_fallible = is_fallible ? return_type : NULL;
  cg->current_result_name = decl->as.func_decl.ret_name;
  char result_c_name[64];
  if (decl->as.func_decl.ret_name) {
    snprintf(result_c_name, sizeof(result_c_name), "__runes_return_%u",
             cg->temp_id++);
    cg->current_result_c_name = result_c_name;
  } else {
    cg->current_result_c_name = NULL;
  }
  cg->current_error_name = is_fallible ? "__runes_error" : NULL;
  if (!emit_func_header(cg, decl, c_name, is_main))
    return false;
  fputs(" {\n", cg->out);

  if (is_main) {
    for (int i = 0; i < cg->runtime_initializer_count; i++) {
      AstNode *initializer = cg->runtime_initializers[i];
      if (initializer->kind == AST_VAR_DECL) {
        if (initializer->as.var_decl.init->kind == AST_IF_STMT) {
          if (!emit_if_value(cg, initializer->as.var_decl.init, 1, NULL,
                             registered_decl_name(cg, initializer)))
            return false;
        } else if (!emit_assignment_target(
                       cg, NULL, registered_decl_name(cg, initializer),
                       initializer->as.var_decl.init, 1)) {
          return false;
        }
      } else if (!emit_stmt(cg, initializer, 1)) {
        return false;
      }
    }
  }

  if (decl->as.func_decl.ret_name && !is_main) {
    Type *local_type = is_fallible ? return_type->as.fallible.inner
                                   : return_type;
    bool is_void = local_type && local_type->kind == TY_PRIMITIVE &&
                   strcmp(local_type->as.primitive.name, "void") == 0;
    if (!is_void) {
      indent(cg->out, 1);
      if (!emit_semantic_decl(cg, local_type, cg->current_result_c_name,
                              decl))
        return false;
      fputs(" = {0};\n", cg->out);
    }
  }
  if (is_fallible) {
    indent(cg->out, 1);
    fputs("RunesError __runes_error = RUNES_ERROR_NONE;\n", cg->out);
  }

  if (!emit_block(cg, decl->as.func_decl.body, 1))
    return false;

  if (is_main) {
    indent(cg->out, 1);
    fputs("return 0;\n", cg->out);
  } else if (is_fallible) {
    char result_type[1024];
    if (!result_type_name(cg, return_type, result_type, sizeof(result_type))) {
      cg_error(cg, decl, "unsupported fallible result type");
      return false;
    }
    indent(cg->out, 1);
    fprintf(cg->out,
            "return (%s){ .ok = __runes_error == RUNES_ERROR_NONE, "
            ".error = __runes_error",
            result_type);
    Type *inner = return_type->as.fallible.inner;
    bool is_void = inner->kind == TY_PRIMITIVE &&
                   strcmp(inner->as.primitive.name, "void") == 0;
    if (!is_void && decl->as.func_decl.ret_name)
      fprintf(cg->out, ", .value = %s", cg->current_result_c_name);
    fputs(" };\n", cg->out);
  } else if (decl->as.func_decl.ret_name) {
    indent(cg->out, 1);
    if (return_type && return_type->kind == TY_ARRAY) {
      char array_name[1024];
      if (!array_type_name(cg, return_type, array_name, sizeof(array_name)))
        return false;
      fprintf(cg->out, "%s __runes_array_result = {0};\n", array_name);
      indent(cg->out, 1);
      fprintf(cg->out,
              "memcpy(__runes_array_result.data, %s, sizeof %s);\n",
              cg->current_result_c_name, cg->current_result_c_name);
      indent(cg->out, 1);
      fputs("return __runes_array_result;\n", cg->out);
    } else {
      fprintf(cg->out, "return %s;\n", cg->current_result_c_name);
    }
  }

  fputs("}\n\n", cg->out);
  cg->current_fallible = saved_fallible;
  cg->current_result_name = saved_result;
  cg->current_result_c_name = saved_result_c;
  cg->current_error_name = saved_error;
  return true;
}

static bool emit_result_definition(Codegen *cg, Type *fallible,
                                   const AstNode *error_node) {
  if (!fallible || fallible->kind != TY_FALLIBLE)
    return true;
  for (int i = 0; i < cg->emitted_result_count; i++) {
    if (type_equals(cg->emitted_results[i], fallible))
      return true;
  }
  if (cg->emitted_result_count >= 64) {
    cg_error(cg, error_node, "too many distinct fallible result types");
    return false;
  }
  cg->emitted_results[cg->emitted_result_count++] = fallible;
  char name[1024];
  if (!result_type_name(cg, fallible, name, sizeof(name))) {
    cg_error(cg, error_node, "unsupported fallible result type");
    return false;
  }
  fprintf(cg->out,
          "typedef struct %s {\n  bool ok;\n  RunesError error;\n", name);
  Type *inner = fallible->as.fallible.inner;
  bool is_void = inner->kind == TY_PRIMITIVE &&
                 strcmp(inner->as.primitive.name, "void") == 0;
  if (!is_void) {
    fputs("  ", cg->out);
    if (!emit_semantic_decl(cg, inner, "value", error_node))
      return false;
    fputs(";\n", cg->out);
  }
  fprintf(cg->out, "} %s;\n\n", name);
  return true;
}

static bool emit_tuple_definition(Codegen *cg, Type *tuple,
                                  const AstNode *error_node) {
  if (!tuple || tuple->kind != TY_TUPLE)
    return true;
  for (int i = 0; i < cg->emitted_tuple_count; i++) {
    if (type_equals(cg->emitted_tuples[i], tuple))
      return true;
  }
  if (cg->emitted_tuple_count >= 64) {
    cg_error(cg, error_node, "too many distinct tuple types");
    return false;
  }
  cg->emitted_tuples[cg->emitted_tuple_count++] = tuple;
  char name[1024];
  if (!tuple_type_name(cg, tuple, name, sizeof(name))) {
    cg_error(cg, error_node, "unsupported tuple type");
    return false;
  }
  fprintf(cg->out, "typedef struct %s {\n", name);
  for (int i = 0; i < tuple->as.tuple.count; i++) {
    fputs("  ", cg->out);
    char field[32];
    snprintf(field, sizeof(field), "_%d", i);
    if (!emit_semantic_decl(cg, tuple->as.tuple.elems[i], field, error_node))
      return false;
    fputs(";\n", cg->out);
  }
  fprintf(cg->out, "} %s;\n\n", name);
  return true;
}

static bool emit_array_definition(Codegen *cg, Type *array,
                                  const AstNode *error_node) {
  for (int i = 0; i < cg->emitted_array_count; i++)
    if (type_equals(cg->emitted_arrays[i], array))
      return true;
  if (cg->emitted_array_count >= 64) {
    cg_error(cg, error_node, "too many distinct fixed array types");
    return false;
  }
  cg->emitted_arrays[cg->emitted_array_count++] = array;
  char name[1024];
  if (!array_type_name(cg, array, name, sizeof(name)))
    return false;
  fprintf(cg->out, "typedef struct %s {\n  ", name);
  if (!emit_semantic_decl(cg, array->as.array.inner, "data", error_node))
    return false;
  fprintf(cg->out, "[%zu];\n} %s;\n\n", array->as.array.size, name);
  return true;
}

static bool emit_tuple_dependencies(Codegen *cg, Type *type,
                                    const AstNode *error_node) {
  if (!type)
    return true;
  if (type->kind == TY_TUPLE) {
    for (int i = 0; i < type->as.tuple.count; i++) {
      if (!emit_tuple_dependencies(cg, type->as.tuple.elems[i], error_node))
        return false;
    }
    return emit_tuple_definition(cg, type, error_node);
  }
  if (type->kind == TY_ARRAY)
    return emit_tuple_dependencies(cg, type->as.array.inner, error_node);
  if (type->kind == TY_FALLIBLE)
    return emit_tuple_dependencies(cg, type->as.fallible.inner, error_node);
  if (type->kind == TY_FUNCTION) {
    for (int i = 0; i < type->as.function.param_count; i++) {
      if (!emit_tuple_dependencies(cg, type->as.function.params[i], error_node))
        return false;
    }
    if (type->as.function.ret && type->as.function.ret->kind == TY_ARRAY &&
        !emit_array_definition(cg, type->as.function.ret, error_node))
      return false;
    return emit_tuple_dependencies(cg, type->as.function.ret, error_node);
  }
  if (type->kind == TY_STRUCT) {
    for (int i = 0; i < type->as.struct_t.field_count; i++) {
      Type *field = type->as.struct_t.field_types[i];
      if (field->kind != TY_POINTER &&
          !emit_tuple_dependencies(cg, field, error_node))
        return false;
    }
  } else if (type->kind == TY_VARIANT) {
    for (int i = 0; i < type->as.variant.arm_count; i++) {
      if (!emit_tuple_dependencies(cg, type->as.variant.arm_types[i],
                                   error_node))
        return false;
    }
  }
  return true;
}

static bool emit_ast_tuple_dependencies(Codegen *cg, AstNode *node) {
  for (; node; node = node->next) {
    if (node->resolved_type &&
        !emit_tuple_dependencies(cg, node->resolved_type, node))
      return false;
    switch (node->kind) {
    case AST_PROGRAM:
      if (!emit_ast_tuple_dependencies(cg, node->as.program.declarations))
        return false;
      break;
    case AST_MOD_DECL:
      if (!emit_ast_tuple_dependencies(cg, node->as.mod_decl.declarations))
        return false;
      break;
    case AST_FUNC_DECL:
      if (!emit_ast_tuple_dependencies(cg, node->as.func_decl.params) ||
          !emit_ast_tuple_dependencies(cg, node->as.func_decl.body))
        return false;
      break;
    case AST_METHOD_DECL:
      if (!emit_ast_tuple_dependencies(cg, node->as.method_decl.methods))
        return false;
      break;
    case AST_BLOCK:
      if (!emit_ast_tuple_dependencies(cg, node->as.block.statements))
        return false;
      break;
    case AST_VAR_DECL:
      if (!emit_ast_tuple_dependencies(cg, node->as.var_decl.init))
        return false;
      break;
    case AST_TUPLE_DESTRUCTURE:
      if (!emit_ast_tuple_dependencies(cg,
                                       node->as.tuple_destructure.targets) ||
          !emit_ast_tuple_dependencies(cg, node->as.tuple_destructure.init))
        return false;
      break;
    case AST_ASSIGN:
      if (!emit_ast_tuple_dependencies(cg, node->as.assign.target) ||
          !emit_ast_tuple_dependencies(cg, node->as.assign.value))
        return false;
      break;
    case AST_RETURN_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.return_stmt.value))
        return false;
      break;
    case AST_IF_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.if_stmt.condition) ||
          !emit_ast_tuple_dependencies(cg, node->as.if_stmt.then_branch) ||
          !emit_ast_tuple_dependencies(cg, node->as.if_stmt.else_branch))
        return false;
      break;
    case AST_WHILE_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.while_stmt.condition) ||
          !emit_ast_tuple_dependencies(cg, node->as.while_stmt.body))
        return false;
      break;
    case AST_FOR_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.for_stmt.iter) ||
          !emit_ast_tuple_dependencies(cg, node->as.for_stmt.body))
        return false;
      break;
    case AST_LOOP_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.loop_stmt.body))
        return false;
      break;
    case AST_UNSAFE_BLOCK:
      if (!emit_ast_tuple_dependencies(cg, node->as.unsafe_block.body))
        return false;
      break;
    case AST_MATCH_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.match_stmt.subject) ||
          !emit_ast_tuple_dependencies(cg, node->as.match_stmt.arms))
        return false;
      break;
    case AST_MATCH_ARM:
      if (!emit_ast_tuple_dependencies(cg, node->as.match_arm.guard) ||
          !emit_ast_tuple_dependencies(cg, node->as.match_arm.body))
        return false;
      break;
    case AST_BINARY_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.binary.left) ||
          !emit_ast_tuple_dependencies(cg, node->as.binary.right))
        return false;
      break;
    case AST_UNARY_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.unary.expr))
        return false;
      break;
    case AST_CALL_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.call.callee) ||
          !emit_ast_tuple_dependencies(cg, node->as.call.args))
        return false;
      break;
    case AST_NAMED_ARG:
      if (!emit_ast_tuple_dependencies(cg, node->as.named_arg.value))
        return false;
      break;
    case AST_INDEX_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.index.target) ||
          !emit_ast_tuple_dependencies(cg, node->as.index.index))
        return false;
      break;
    case AST_FIELD_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.field.target))
        return false;
      break;
    case AST_CAST_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.cast.expr))
        return false;
      break;
    case AST_PROMOTE_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.promote.expr))
        return false;
      break;
    case AST_TRY_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.try_expr.expr))
        return false;
      break;
    case AST_CATCH_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.catch_expr.expr) ||
          !emit_ast_tuple_dependencies(cg, node->as.catch_expr.handler))
        return false;
      break;
    case AST_ARRAY_LITERAL:
      if (!emit_ast_tuple_dependencies(cg, node->as.array_literal.elems))
        return false;
      break;
    case AST_TUPLE_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.tuple_expr.elems))
        return false;
      break;
    default:
      break;
    }
  }
  return true;
}

typedef enum {
  EMIT_TYPEDEFS,
  EMIT_TYPE_DEFINITIONS,
  EMIT_TUPLES,
  EMIT_INTERFACES,
  EMIT_RESULTS,
  EMIT_EXTERNS,
  EMIT_GLOBALS,
  EMIT_PROTOTYPES,
  EMIT_FUNCTIONS,
  EMIT_ERRORS,
} EmitPhase;

static bool emit_nested_function_phase(Codegen *cg, EmitPhase phase) {
  for (int i = 0; i < cg->nested_function_count; i++) {
    AstNode *function = cg->nested_functions[i];
    const char *name = registered_decl_name(cg, function);
    if (phase == EMIT_TUPLES) {
      if (!emit_tuple_dependencies(cg, function->resolved_type, function))
        return false;
    } else if (phase == EMIT_RESULTS) {
      Type *type = function->resolved_type;
      if (type && type->kind == TY_FUNCTION && type->as.function.ret &&
          type->as.function.ret->kind == TY_FALLIBLE &&
          !emit_result_definition(cg, type->as.function.ret, function))
        return false;
    } else if (phase == EMIT_PROTOTYPES) {
      if (!emit_func_header(cg, function, name, false))
        return false;
      fputs(";\n", cg->out);
    } else if (phase == EMIT_FUNCTIONS) {
      if (!emit_func(cg, function, name, false))
        return false;
    }
  }
  return true;
}

static bool emit_declaration_phase(Codegen *cg, AstNode *decl,
                                   const char *prefix, EmitPhase phase) {
  const char *saved_prefix = cg->current_prefix;
  cg->current_prefix = prefix;
  for (; decl; decl = decl->next) {
    if (decl->kind == AST_MOD_DECL) {
      char nested_prefix[256];
      snprintf(nested_prefix, sizeof(nested_prefix), "%s%s%s",
               prefix ? prefix : "", prefix ? "_" : "",
               decl->as.mod_decl.name);
      if (!emit_declaration_phase(cg, decl->as.mod_decl.declarations,
                                  nested_prefix, phase))
        return false;
      cg->current_prefix = prefix;
      continue;
    }

    if (phase == EMIT_TYPEDEFS && decl->kind == AST_TYPE_DECL) {
      const char *name = registered_decl_name(cg, decl);
      fprintf(cg->out, "typedef struct %s %s;\n", name, name);
    } else if (phase == EMIT_TYPEDEFS && decl->kind == AST_VARIANT_DECL) {
      const char *name = registered_decl_name(cg, decl);
      fprintf(cg->out, "typedef struct %s %s;\n", name, name);
    } else if (phase == EMIT_TYPEDEFS &&
               decl->kind == AST_INTERFACE_DECL) {
      const char *name = registered_decl_name(cg, decl);
      fprintf(cg->out, "typedef struct %s %s;\n", name, name);
    } else if (phase == EMIT_TYPE_DEFINITIONS &&
               decl->kind == AST_TYPE_DECL) {
      if (!emit_struct_definition(cg, decl))
        return false;
    } else if (phase == EMIT_TYPE_DEFINITIONS &&
               decl->kind == AST_VARIANT_DECL) {
      if (!emit_variant_definition(cg, decl))
        return false;
    } else if (phase == EMIT_TUPLES) {
      if (decl->resolved_type &&
          !emit_tuple_dependencies(cg, decl->resolved_type, decl))
        return false;
      if (decl->kind == AST_METHOD_DECL) {
        for (AstNode *method = decl->as.method_decl.methods; method;
             method = method->next) {
          if (!emit_tuple_dependencies(cg, method->resolved_type, method))
            return false;
        }
      }
    } else if (phase == EMIT_INTERFACES &&
               decl->kind == AST_INTERFACE_DECL) {
      if (!emit_interface_definition(cg, decl))
        return false;
    } else if (phase == EMIT_RESULTS) {
      Type *fallible = NULL;
      if ((decl->kind == AST_FUNC_DECL || decl->kind == AST_EXTERN_DECL) &&
          decl->resolved_type && decl->resolved_type->kind == TY_FUNCTION &&
          decl->resolved_type->as.function.ret->kind == TY_FALLIBLE)
        fallible = decl->resolved_type->as.function.ret;
      if (fallible && !emit_result_definition(cg, fallible, decl))
        return false;
      if (decl->kind == AST_METHOD_DECL) {
        for (AstNode *method = decl->as.method_decl.methods; method;
             method = method->next) {
          if (method->resolved_type &&
              method->resolved_type->kind == TY_FUNCTION &&
              method->resolved_type->as.function.ret->kind == TY_FALLIBLE &&
              !emit_result_definition(
                  cg, method->resolved_type->as.function.ret, method))
            return false;
        }
      }
    } else if (phase == EMIT_EXTERNS && decl->kind == AST_EXTERN_DECL) {
      if (!emit_extern_declaration(cg, decl))
        return false;
    } else if (phase == EMIT_GLOBALS && decl->kind == AST_VAR_DECL) {
      if (!emit_var_decl(cg, decl, 0))
        return false;
    } else if (phase == EMIT_GLOBALS && decl->kind == AST_ASSIGN) {
      if (!register_runtime_initializer(cg, decl))
        return false;
    } else if (phase == EMIT_PROTOTYPES && decl->kind == AST_FUNC_DECL) {
      const char *name = registered_decl_name(cg, decl);
      bool is_main = !prefix && strcmp(decl->as.func_decl.name, "main") == 0;
      if (!emit_func_header(cg, decl, name, is_main))
        return false;
      fputs(";\n", cg->out);
    } else if (phase == EMIT_PROTOTYPES && decl->kind == AST_METHOD_DECL) {
      for (AstNode *method = decl->as.method_decl.methods; method;
           method = method->next) {
        const char *name = registered_decl_name(cg, method);
        if (!emit_func_header(cg, method, name, false))
          return false;
        fputs(";\n", cg->out);
      }
    } else if (phase == EMIT_FUNCTIONS && decl->kind == AST_FUNC_DECL) {
      const char *name = registered_decl_name(cg, decl);
      bool is_main = !prefix && strcmp(decl->as.func_decl.name, "main") == 0;
      if (!emit_func(cg, decl, name, is_main))
        return false;
    } else if (phase == EMIT_FUNCTIONS && decl->kind == AST_METHOD_DECL) {
      for (AstNode *method = decl->as.method_decl.methods; method;
           method = method->next) {
        const char *name = registered_decl_name(cg, method);
        if (!emit_func(cg, method, name, false))
          return false;
      }
    } else if (phase == EMIT_ERRORS && decl->kind == AST_ERROR_DECL) {
      const char *name = registered_decl_name(cg, decl);
      for (AstNode *variant = decl->as.error_decl.variants; variant;
           variant = variant->next)
        fprintf(cg->out, ",\n  %s_%s", name,
                variant->as.identifier.name);
    }
  }
  cg->current_prefix = saved_prefix;
  return true;
}

void codegen_init(Codegen *cg, FILE *out) {
  cg->out = out;
  cg->had_error = false;
  cg->error_count = 0;
  cg->temp_id = 0;
  cg->current_fallible = NULL;
  cg->current_result_name = NULL;
  cg->current_result_c_name = NULL;
  cg->current_error_name = NULL;
  cg->emitted_result_count = 0;
  cg->emitted_tuple_count = 0;
  cg->emitted_array_count = 0;
  cg->named_decl_count = 0;
  cg->named_type_count = 0;
  cg->runtime_initializer_count = 0;
  cg->nested_function_count = 0;
  cg->interface_impl_count = 0;
  cg->current_prefix = NULL;
}

bool codegen_emit_c(Codegen *cg, AstNode *program) {
  if (!program || program->kind != AST_PROGRAM)
    return false;

  fputs("#include <stdbool.h>\n", cg->out);
  fputs("#include <stddef.h>\n", cg->out);
  fputs("#include <stdint.h>\n", cg->out);
  fputs("#include <math.h>\n", cg->out);
  fputs("#include <stdio.h>\n\n", cg->out);
  fputs("#include <stdlib.h>\n", cg->out);
  fputs("#include <string.h>\n\n", cg->out);
  fputs("static inline void *runes_promote_copy(const void *source, size_t size) "
        "{\n"
        "  void *copy = malloc(size);\n"
        "  if (!copy) abort();\n"
        "  return memcpy(copy, source, size);\n"
        "}\n\n",
        cg->out);
  fputs("static inline const char *runes_str_concat(const char *left, "
        "const char *right) {\n"
        "  size_t left_len = strlen(left);\n"
        "  size_t right_len = strlen(right);\n"
        "  char *result = malloc(left_len + right_len + 1);\n"
        "  if (!result) abort();\n"
        "  memcpy(result, left, left_len);\n"
        "  memcpy(result + left_len, right, right_len + 1);\n"
        "  return result;\n"
        "}\n\n",
        cg->out);

  AstNode *declarations = program->as.program.declarations;
  if (!register_codegen_decls(cg, declarations, NULL)) {
    cg_error(cg, program, "too many declarations for C name registry");
    return false;
  }

  fputs("typedef uint32_t RunesError;\n", cg->out);
  fputs("enum {\n  RUNES_ERROR_NONE = 0", cg->out);
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_ERRORS))
    return false;
  fputs("\n};\n\n", cg->out);

  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_TYPEDEFS))
    return false;
  fputc('\n', cg->out);
  if (!emit_declaration_phase(cg, declarations, NULL,
                              EMIT_TYPE_DEFINITIONS))
    return false;
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_TUPLES))
    return false;
  if (!emit_nested_function_phase(cg, EMIT_TUPLES))
    return false;
  if (!emit_ast_tuple_dependencies(cg, declarations))
    return false;
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_INTERFACES))
    return false;
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_RESULTS))
    return false;
  if (!emit_nested_function_phase(cg, EMIT_RESULTS))
    return false;
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_EXTERNS))
    return false;
  fputc('\n', cg->out);
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_GLOBALS))
    return false;
  fputc('\n', cg->out);
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_PROTOTYPES))
    return false;
  if (!emit_nested_function_phase(cg, EMIT_PROTOTYPES))
    return false;
  if (!emit_interface_adapters(cg, true))
    return false;
  fputc('\n', cg->out);
  if (!emit_interface_adapters(cg, false))
    return false;
  if (!emit_nested_function_phase(cg, EMIT_FUNCTIONS))
    return false;
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_FUNCTIONS))
    return false;

  return !cg->had_error;
}
