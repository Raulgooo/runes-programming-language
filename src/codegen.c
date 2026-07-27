#include "codegen.h"
#include "lexer.h"
#include "types.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool grow_registry(Codegen *cg, void **items, int *capacity, int count,
                          size_t item_size, size_t alignment) {
  if (count < *capacity)
    return true;
  int next_capacity = *capacity ? *capacity * 2 : 16;
  void *grown = arena_alloc_aligned(cg->arena,
                                    item_size * (size_t)next_capacity,
                                    alignment);
  if (!grown)
    return false;
  if (*items && count > 0)
    memcpy(grown, *items, item_size * (size_t)count);
  *items = grown;
  *capacity = next_capacity;
  return true;
}

#define GROW_REGISTRY(cg, field, count, capacity, item_type)                   \
  grow_registry((cg), (void **)&(cg)->field, &(cg)->capacity, (count),         \
                sizeof(item_type), _Alignof(item_type))

static const char *format_name(Codegen *cg, const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int length = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (length < 0) {
    va_end(args);
    return NULL;
  }
  char *name = arena_alloc(cg->arena, (size_t)length + 1);
  vsnprintf(name, (size_t)length + 1, format, args);
  va_end(args);
  return name;
}

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
      return "RunesStr";
    if (strcmp(name, "char") == 0)
      return "uint32_t";
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
           snprintf(buffer, buffer_size,
                    type->as.type_expr.nullable
                        ? (type->as.type_expr.readonly ? "optconstptr_%s"
                                                      : "optptr_%s")
                        : (type->as.type_expr.readonly ? "constptr_%s"
                                                      : "ptr_%s"),
                    inner) > 0;
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
  case TYPE_SLICE: {
    char inner[512];
    return ast_type_suffix(type->as.type_expr.inner, inner, sizeof(inner)) &&
           snprintf(buffer, buffer_size,
                    type->as.type_expr.readonly ? "const_slice_%s"
                                                : "slice_%s",
                    inner) > 0;
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
  case TYPE_FUNCTION: {
    size_t used = 0;
    int written = snprintf(buffer, buffer_size, "closure_%d",
                           (int)type->as.type_expr.realm);
    if (written < 0 || (size_t)written >= buffer_size)
      return false;
    used = (size_t)written;
    for (AstNode *parameter = type->as.type_expr.elems; parameter;
         parameter = parameter->next) {
      char suffix[512];
      if (!ast_type_suffix(parameter, suffix, sizeof(suffix)))
        return false;
      written = snprintf(buffer + used, buffer_size - used, "_%s", suffix);
      if (written < 0 || (size_t)written >= buffer_size - used)
        return false;
      used += (size_t)written;
    }
    char result[512];
    if (!ast_type_suffix(type->as.type_expr.inner, result, sizeof(result)))
      return false;
    return snprintf(buffer + used, buffer_size - used, "_ret_%s", result) > 0;
  }
  }
  return false;
}

static const char *registered_decl_name(Codegen *cg, const AstNode *decl) {
  for (int i = 0; i < cg->named_decl_count; i++) {
    if (cg->named_decls[i].decl == decl)
      return cg->named_decls[i].name;
  }
  return NULL;
}

static const char *registered_type_name(Codegen *cg, Type *type) {
  for (int i = 0; i < cg->named_type_count; i++) {
    if (cg->named_types[i].type == type)
      return cg->named_types[i].name;
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
static bool type_needs_gc_trace(Type *type);

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
    if (!GROW_REGISTRY(cg, named_decls, cg->named_decl_count,
                       named_decl_capacity, CodegenNamedDecl) ||
        !GROW_REGISTRY(cg, nested_functions, cg->nested_function_count,
                       nested_function_capacity, AstNode *))
      return false;
    const char *nested_name = node->as.func_decl.name;
    const char *full_name = format_name(cg, "%sD%zu_%s", prefix,
                                        strlen(nested_name), nested_name);
    if (!full_name)
      return false;
    int index = cg->named_decl_count++;
    cg->named_decls[index].decl = node;
    cg->named_decls[index].name = full_name;
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
    const char *full_name = NULL;
    if (source_name) {
      if (decl->kind == AST_EXTERN_DECL)
        full_name = source_name;
      else if (!prefix && decl->kind == AST_FUNC_DECL &&
               strcmp(source_name, "main") == 0)
        full_name = source_name;
      else
        full_name = format_name(cg, "%sD%zu_%s",
                                prefix ? prefix : "runes_",
                                strlen(source_name), source_name);
      if (!full_name)
        return false;
    }
    if (source_name) {
      if (!GROW_REGISTRY(cg, named_decls, cg->named_decl_count,
                         named_decl_capacity, CodegenNamedDecl))
        return false;
      int index = cg->named_decl_count++;
      cg->named_decls[index].decl = decl;
      cg->named_decls[index].name = full_name;
      if ((decl->kind == AST_TYPE_DECL || decl->kind == AST_VARIANT_DECL ||
           decl->kind == AST_INTERFACE_DECL) && decl->resolved_type) {
        if (!GROW_REGISTRY(cg, named_types, cg->named_type_count,
                           named_type_capacity, CodegenNamedType))
          return false;
        int type_index = cg->named_type_count++;
        cg->named_types[type_index].type = decl->resolved_type;
        cg->named_types[type_index].name = full_name;
      }
      if (decl->kind == AST_VAR_DECL && decl->resolved_type &&
          type_needs_gc_trace(decl->resolved_type)) {
        if (!GROW_REGISTRY(cg, gc_globals, cg->gc_global_count,
                           gc_global_capacity, AstNode *))
          return false;
        cg->gc_globals[cg->gc_global_count++] = decl;
      }
    }
    if (decl->kind == AST_METHOD_DECL) {
      if (decl->as.method_decl.iface_name) {
        if (!GROW_REGISTRY(cg, interface_impls, cg->interface_impl_count,
                           interface_impl_capacity, AstNode *))
          return false;
        cg->interface_impls[cg->interface_impl_count++] = decl;
      }
      for (AstNode *method = decl->as.method_decl.methods; method;
           method = method->next) {
        if (!GROW_REGISTRY(cg, named_decls, cg->named_decl_count,
                           named_decl_capacity, CodegenNamedDecl))
          return false;
        int index = cg->named_decl_count++;
        cg->named_decls[index].decl = method;
        if (decl->as.method_decl.iface_name) {
          cg->named_decls[index].name = format_name(
              cg, "%sD7___ifaceD%zu_%sD%zu_%sD%zu_%s",
              prefix ? prefix : "runes_",
              strlen(decl->as.method_decl.iface_name),
              decl->as.method_decl.iface_name,
              strlen(decl->as.method_decl.type_name),
              decl->as.method_decl.type_name,
              strlen(method->as.func_decl.name), method->as.func_decl.name);
        } else {
          cg->named_decls[index].name = format_name(
              cg, "%sD8___methodD%zu_%sD%zu_%s",
              prefix ? prefix : "runes_",
              strlen(decl->as.method_decl.type_name),
              decl->as.method_decl.type_name,
              strlen(method->as.func_decl.name), method->as.func_decl.name);
        }
        if (!cg->named_decls[index].name)
          return false;
        if (!register_nested_node(cg, method->as.func_decl.body,
                                  cg->named_decls[index].name))
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
  if (type->kind == TY_STRUCT || type->kind == TY_VARIANT ||
      type->kind == TY_INTERFACE)
    return snprintf(buffer, buffer_size, "%s",
                    registered_type_name(cg, type)) > 0;
  if (type->kind == TY_POINTER) {
    char inner[512];
    if (!semantic_type_suffix(cg, type->as.pointer.inner, inner, sizeof(inner)))
      return false;
    return snprintf(buffer, buffer_size,
                    type->as.pointer.nullable
                        ? (type->as.pointer.readonly ? "optconstptr_%s"
                                                    : "optptr_%s")
                        : (type->as.pointer.readonly ? "constptr_%s"
                                                    : "ptr_%s"),
                    inner) > 0;
  }
  if (type->kind == TY_ARRAY) {
    char inner[512];
    if (!semantic_type_suffix(cg, type->as.array.inner, inner, sizeof(inner)))
      return false;
    return snprintf(buffer, buffer_size, "array_%zu_%s", type->as.array.size,
                    inner) > 0;
  }
  if (type->kind == TY_SLICE) {
    char inner[512];
    if (!semantic_type_suffix(cg, type->as.slice.inner, inner, sizeof(inner)))
      return false;
    return snprintf(buffer, buffer_size,
                    type->as.slice.readonly ? "const_slice_%s"
                                            : "slice_%s",
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
  if (type->kind == TY_FALLIBLE) {
    char inner[512];
    if (!semantic_type_suffix(cg, type->as.fallible.inner, inner,
                              sizeof(inner)))
      return false;
    return snprintf(buffer, buffer_size, "fallible_%s", inner) > 0;
  }
  if (type->kind == TY_FUNCTION) {
    size_t used = 0;
    int written = snprintf(buffer, buffer_size, "closure_%d",
                           (int)type->as.function.strategy);
    if (written < 0 || (size_t)written >= buffer_size)
      return false;
    used = (size_t)written;
    for (int i = 0; i < type->as.function.param_count; i++) {
      char parameter[512];
      if (!semantic_type_suffix(cg, type->as.function.params[i], parameter,
                                sizeof(parameter)))
        return false;
      written = snprintf(buffer + used, buffer_size - used, "_%s", parameter);
      if (written < 0 || (size_t)written >= buffer_size - used)
        return false;
      used += (size_t)written;
    }
    char result[512];
    if (!semantic_type_suffix(cg, type->as.function.ret, result,
                              sizeof(result)))
      return false;
    return snprintf(buffer + used, buffer_size - used, "_ret_%s", result) > 0;
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

static bool slice_type_name(Codegen *cg, Type *slice, char *buffer,
                            size_t buffer_size) {
  if (!slice || slice->kind != TY_SLICE)
    return false;
  char suffix[768];
  if (!semantic_type_suffix(cg, slice->as.slice.inner, suffix,
                            sizeof(suffix)))
    return false;
  return snprintf(buffer, buffer_size,
                  slice->as.slice.readonly ? "RunesConstSlice_%s"
                                           : "RunesSlice_%s",
                  suffix) > 0;
}

static bool closure_type_name(Codegen *cg, Type *function, char *buffer,
                              size_t buffer_size) {
  if (!function || function->kind != TY_FUNCTION)
    return false;
  char suffix[768];
  if (!semantic_type_suffix(cg, function, suffix, sizeof(suffix)))
    return false;
  return snprintf(buffer, buffer_size, "RunesClosure_%s", suffix + 8) > 0;
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

static bool descriptor_name(Codegen *cg, Type *type, char *buffer,
                            size_t buffer_size) {
  char suffix[768];
  return semantic_type_suffix(cg, type, suffix, sizeof(suffix)) &&
         snprintf(buffer, buffer_size, "runes_type_%s", suffix) > 0;
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
    if (!type->as.pointer.readonly)
      return build_semantic_decl(cg, type->as.pointer.inner, declarator,
                                 buffer, buffer_size);
    char mutable_decl[2048];
    if (!build_semantic_decl(cg, type->as.pointer.inner, declarator,
                             mutable_decl, sizeof(mutable_decl)))
      return false;
    return snprintf(buffer, buffer_size, "const %s", mutable_decl) > 0;
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
  if (type->kind == TY_SLICE) {
    char slice_name[1024];
    return slice_type_name(cg, type, slice_name, sizeof(slice_name)) &&
           snprintf(buffer, buffer_size, "%s %s", slice_name, name) > 0;
  }
  if (type->kind == TY_FUNCTION) {
    char closure_name[1024];
    return closure_type_name(cg, type, closure_name, sizeof(closure_name)) &&
           snprintf(buffer, buffer_size, "%s %s", closure_name, name) > 0;
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

  if (type->as.type_expr.kind == TYPE_SLICE) {
    char suffix[768];
    if (!ast_type_suffix(type->as.type_expr.inner, suffix, sizeof(suffix)))
      return false;
    return snprintf(buffer, buffer_size,
                    type->as.type_expr.readonly ? "RunesConstSlice_%s %s"
                                                : "RunesSlice_%s %s",
                    suffix, name) > 0;
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

static void emit_c_string(FILE *out, const char *s, size_t length) {
  fputc('"', out);
  for (size_t i = 0; s && i < length; i++) {
    unsigned char byte = (unsigned char)s[i];
    switch (byte) {
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
      if (byte >= 0x20 && byte <= 0x7e)
        fputc(byte, out);
      else
        fprintf(out, "\\%03o", byte);
      break;
    }
  }
  fputc('"', out);
}

static Attr *find_attr(Attr *attrs, const char *name) {
  for (Attr *attr = attrs; attr; attr = attr->next)
    if (strcmp(attr->name, name) == 0)
      return attr;
  return NULL;
}

static bool validate_codegen_declarations(Codegen *cg, AstNode *declaration) {
  for (; declaration; declaration = declaration->next) {
    if (declaration->kind == AST_FUNC_DECL &&
        find_attr(declaration->as.func_decl.attrs, "interrupt")) {
      cg_error(cg, declaration,
               "#[interrupt] is not supported by the v0.1 C backend; use an "
               "external assembly entry stub");
      return false;
    }
    if (declaration->kind == AST_METHOD_DECL) {
      if (!validate_codegen_declarations(
              cg, declaration->as.method_decl.methods))
        return false;
    } else if (declaration->kind == AST_MOD_DECL) {
      if (!validate_codegen_declarations(
              cg, declaration->as.mod_decl.declarations))
        return false;
    }
  }
  return true;
}

static bool emit_string_attr_argument(Codegen *cg, Attr *attr,
                                      const AstNode *node) {
  if (!attr || !attr->arg || attr->arg->kind != AST_STRING_LITERAL) {
    cg_error(cg, node, "attribute requires a string literal argument");
    return false;
  }
  emit_c_string(cg->out, attr->arg->as.string_literal.value,
                attr->arg->as.string_literal.length);
  return true;
}

static bool emit_function_abi_suffix(Codegen *cg, AstNode *decl,
                                     bool prototype) {
  Attr *attrs = decl->as.func_decl.attrs;
  Attr *interrupt = find_attr(attrs, "interrupt");
  if (interrupt) {
    cg_error(cg, decl,
             "#[interrupt] is not supported by the v0.1 C backend; use an "
             "external assembly entry stub");
    return false;
  }
  Attr *link_name = find_attr(attrs, "link_name");
  if (prototype && link_name) {
    fputs(" __asm__(", cg->out);
    if (!emit_string_attr_argument(cg, link_name, decl))
      return false;
    fputc(')', cg->out);
  }
  Attr *section = find_attr(attrs, "section");
  if (prototype && section) {
    fputs(" __attribute__((", cg->out);
    fputs("section(", cg->out);
    if (!emit_string_attr_argument(cg, section, decl))
      return false;
    fputc(')', cg->out);
    fputs("))", cg->out);
  }
  return true;
}

static bool emit_callconv_prefix_for(Codegen *cg, Attr *attrs,
                                     AstNode *decl) {
  Attr *callconv = find_attr(attrs, "callconv");
  if (!callconv)
    return true;
  if (!callconv->arg || callconv->arg->kind != AST_STRING_LITERAL) {
    cg_error(cg, decl, "#[callconv] requires a string literal argument");
    return false;
  }
  const char *value = callconv->arg->as.string_literal.value;
  size_t length = callconv->arg->as.string_literal.length;
  if (length == 6 && memcmp(value, "sysv64", 6) == 0)
    fputs("__attribute__((sysv_abi)) ", cg->out);
  else if (length == 5 && memcmp(value, "win64", 5) == 0)
    fputs("__attribute__((ms_abi)) ", cg->out);
  else {
    cg_error(cg, decl,
             "unsupported calling convention for Linux x86-64");
    return false;
  }
  return true;
}

static bool emit_callconv_prefix(Codegen *cg, AstNode *decl) {
  return emit_callconv_prefix_for(cg, decl->as.func_decl.attrs, decl);
}

static bool emit_object_abi_suffix(Codegen *cg, AstNode *decl, int depth) {
  Attr *attrs = decl->as.var_decl.attrs;
  Attr *link_name = find_attr(attrs, "link_name");
  Attr *section = find_attr(attrs, "section");
  Attr *align = find_attr(attrs, "align");
  if (depth > 0 && (link_name || section)) {
    cg_error(cg, decl,
             "#[link_name] and #[section] are only valid on global storage");
    return false;
  }
  if (link_name) {
    fputs(" __asm__(", cg->out);
    if (!emit_string_attr_argument(cg, link_name, decl))
      return false;
    fputc(')', cg->out);
  }
  if (section || align) {
    fputs(" __attribute__((", cg->out);
    if (section) {
      fputs("section(", cg->out);
      if (!emit_string_attr_argument(cg, section, decl))
        return false;
      fputc(')', cg->out);
    }
    if (align) {
      if (section)
        fputs(", ", cg->out);
      if (!align->arg || align->arg->kind != AST_INT_LITERAL) {
        cg_error(cg, decl, "#[align] requires an integer literal argument");
        return false;
      }
      fprintf(cg->out, "aligned(%llu)",
              align->arg->as.int_literal.value);
    }
    fputs("))", cg->out);
  }
  return true;
}

static bool emit_type_layout_suffix(Codegen *cg, AstNode *decl) {
  Attr *packed = find_attr(decl->as.type_decl.attrs, "packed");
  Attr *align = find_attr(decl->as.type_decl.attrs, "align");
  if (!packed && !align)
    return true;
  fputs(" __attribute__((", cg->out);
  if (packed)
    fputs("packed", cg->out);
  if (align) {
    if (packed)
      fputs(", ", cg->out);
    if (!align->arg || align->arg->kind != AST_INT_LITERAL) {
      cg_error(cg, decl, "#[align] requires an integer literal argument");
      return false;
    }
    fprintf(cg->out, "aligned(%llu)", align->arg->as.int_literal.value);
  }
  fputs("))", cg->out);
  return true;
}

static int function_capture_index(AstNode *function, AstNode *declaration,
                                  const char *name) {
  if (!function)
    return -1;
  for (int i = 0; i < function->as.func_decl.capture_count; i++)
    if (function->as.func_decl.captures[i] == declaration &&
        strcmp(function->as.func_decl.capture_names[i], name) == 0)
      return i;
  return -1;
}

static void emit_inline_asm_string(FILE *out, const char *s, size_t length,
                                   bool extended_asm) {
  fputc('"', out);
  for (size_t i = 0; s && i < length; i++) {
    switch (s[i]) {
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
      fputc(s[i], out);
      break;
    }
  }
  fputc('"', out);
}

static bool emit_expr(Codegen *cg, AstNode *expr);

static bool emit_condition(Codegen *cg, AstNode *expr) {
  bool already_grouped =
      expr && expr->kind == AST_BINARY_EXPR &&
      (expr->as.binary.op == TOKEN_EQ_EQ ||
       expr->as.binary.op == TOKEN_BANG_EQ);
  if (!already_grouped)
    fputc('(', cg->out);
  if (!emit_expr(cg, expr))
    return false;
  if (!already_grouped)
    fputc(')', cg->out);
  return true;
}

static bool emit_coerced_expr(Codegen *cg, Type *target, AstNode *expr) {
  Type *source = expr ? expr->resolved_type : NULL;
  bool integer_literal = expr &&
                         (expr->kind == AST_INT_LITERAL ||
                          (expr->kind == AST_UNARY_EXPR &&
                           expr->as.unary.op == TOKEN_MINUS &&
                           expr->as.unary.expr &&
                           expr->as.unary.expr->kind == AST_INT_LITERAL));
  bool float_literal = expr &&
                       (expr->kind == AST_FLOAT_LITERAL ||
                        (expr->kind == AST_UNARY_EXPR &&
                         expr->as.unary.op == TOKEN_MINUS &&
                         expr->as.unary.expr &&
                         expr->as.unary.expr->kind == AST_FLOAT_LITERAL));
  if (target && target->kind == TY_FUNCTION && expr &&
      expr->kind == AST_IDENTIFIER && expr->resolved_decl &&
      expr->resolved_decl->kind == AST_FUNC_DECL) {
    AstNode *function = expr->resolved_decl;
    if (function->as.func_decl.is_main) {
      cg_error(cg, expr, "main cannot be used as a function value");
      return false;
    }
    if (function->as.func_decl.lexical_parent) {
      int capture = function_capture_index(cg->current_function, function,
                                           expr->as.identifier.name);
      if (capture >= 0)
        fprintf(cg->out, "(*__runes_capture_%d)", capture);
      else
        fputs(expr->as.identifier.name, cg->out);
      return true;
    }
    char closure_name[1024];
    const char *function_name = registered_decl_name(cg, function);
    if (!closure_type_name(cg, target, closure_name, sizeof(closure_name)) ||
        !function_name)
      return false;
    fprintf(cg->out,
            "(%s){ .call = %s__closure_call, .env = NULL, .env_type = NULL }",
            closure_name, function_name);
    return true;
  }
  if (target && target->kind == TY_PRIMITIVE &&
      ((integer_literal && type_is_integer(target)) ||
       (float_literal && type_is_float(target)))) {
    char c_type[256];
    if (!build_semantic_decl(cg, target, "", c_type, sizeof(c_type)))
      return false;
    fprintf(cg->out, "((%s)(", c_type);
    if (!emit_expr(cg, expr))
      return false;
    fputs("))", cg->out);
    return true;
  }
  if (target && target->kind == TY_SLICE && source &&
      source->kind == TY_ARRAY) {
    char slice_name[1024];
    if (!slice_type_name(cg, target, slice_name, sizeof(slice_name)))
      return false;
    fprintf(cg->out, "(%s){ .ptr = ", slice_name);
    if (!emit_expr(cg, expr))
      return false;
    if (expr->kind == AST_CALL_EXPR)
      fputs(".data", cg->out);
    fprintf(cg->out, ", .len = %zu }", source->as.array.size);
    return true;
  }
  if (target && target->kind == TY_SLICE && target->as.slice.readonly &&
      source && source->kind == TY_SLICE && !source->as.slice.readonly) {
    char slice_name[1024];
    if (!slice_type_name(cg, target, slice_name, sizeof(slice_name)))
      return false;
    fprintf(cg->out, "%s_from_mut(", slice_name);
    if (!emit_expr(cg, expr))
      return false;
    fputc(')', cg->out);
    return true;
  }
  return emit_expr(cg, expr);
}

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
  char concrete_descriptor[1024];
  if (!descriptor_name(cg, concrete, concrete_descriptor,
                       sizeof(concrete_descriptor)))
    return false;
  fprintf(cg->out, "(%s){ .data = &", iface_name);
  if (!emit_expr(cg, arg))
    return false;
  fprintf(cg->out, ", .type = &%s", concrete_descriptor);
  for (int i = 0; i < interface->as.interface_t.method_count; i++)
    fprintf(cg->out, ", .%s = %s_%s_%s_adapter",
            interface->as.interface_t.method_names[i], iface_name,
            concrete_name, interface->as.interface_t.method_names[i]);
  fputs(" }", cg->out);
  return true;
}

static bool emit_typed_arg_list_from(Codegen *cg, AstNode *arg,
                                     Type *function_type, int index) {
  while (arg) {
    Type *expected = function_type && function_type->kind == TY_FUNCTION &&
                             index < function_type->as.function.param_count
                         ? function_type->as.function.params[index]
                         : NULL;
    if (expected && expected->kind == TY_INTERFACE && arg->resolved_type &&
        arg->resolved_type->kind != TY_INTERFACE) {
      if (!emit_interface_value(cg, expected, arg))
        return false;
    } else if (!emit_coerced_expr(cg, expected, arg)) {
      return false;
    }
    if (arg->next)
      fputs(", ", cg->out);
    arg = arg->next;
    index++;
  }
  return true;
}

static bool emit_typed_arg_list(Codegen *cg, AstNode *arg,
                                Type *function_type) {
  return emit_typed_arg_list_from(cg, arg, function_type, 0);
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
      Type *expected = payload->kind == TY_TUPLE &&
                               field < payload->as.tuple.count
                           ? payload->as.tuple.elems[field]
                           : field == 0 ? payload : NULL;
      if (!emit_coerced_expr(cg, expected, arg))
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
    fprintf(cg->out, "%llu%s", expr->as.int_literal.value,
            expr->as.int_literal.value > (unsigned long long)LLONG_MAX
                ? "ULL"
                : "");
    return true;
  case AST_FLOAT_LITERAL:
    fprintf(cg->out, "%.17g", expr->as.float_literal.value);
    return true;
  case AST_STRING_LITERAL:
    fputs("((RunesStr){ .ptr = (const uint8_t *)", cg->out);
    emit_c_string(cg->out, expr->as.string_literal.value,
                  expr->as.string_literal.length);
    fprintf(cg->out, ", .len = %zu })", expr->as.string_literal.length);
    return true;
  case AST_BOOL_LITERAL:
    fputs(expr->as.bool_literal.value ? "true" : "false", cg->out);
    return true;
  case AST_CHAR_LITERAL:
    fprintf(cg->out, "((uint32_t)%u)", expr->as.char_literal.codepoint);
    return true;
  case AST_NULL_LITERAL:
    fputs("NULL", cg->out);
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
      int capture = function_capture_index(
          cg->current_function, expr->resolved_decl,
          expr->as.identifier.name);
      if (capture >= 0) {
        fprintf(cg->out, "(*__runes_capture_%d)", capture);
        return true;
      }
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
      Type *element_type =
          expr->resolved_type && expr->resolved_type->kind == TY_ARRAY
              ? expr->resolved_type->as.array.inner
              : NULL;
      if (element_type ? !emit_coerced_expr(cg, element_type, elem)
                       : !emit_expr(cg, elem))
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
      Type *element_type =
          index < expr->resolved_type->as.tuple.count
              ? expr->resolved_type->as.tuple.elems[index]
              : NULL;
      if (element_type ? !emit_coerced_expr(cg, element_type, elem)
                       : !emit_expr(cg, elem))
        return false;
      if (elem->next)
        fputs(", ", cg->out);
    }
    fputc('}', cg->out);
    return true;
  }
  case AST_INDEX_EXPR:
    if (expr->as.index.target->resolved_type &&
        expr->as.index.target->resolved_type->kind == TY_PRIMITIVE &&
        strcmp(expr->as.index.target->resolved_type->as.primitive.name,
               "str") == 0) {
      if (expr->as.index.index->kind == AST_RANGE_EXPR) {
        AstNode *range = expr->as.index.index;
        fputs("runes_str_slice_bytes(", cg->out);
        if (!emit_expr(cg, expr->as.index.target))
          return false;
        fputs(", (size_t)(", cg->out);
        if (!emit_expr(cg, range->as.range_expr.start))
          return false;
        fputs("), (size_t)(", cg->out);
        if (!emit_expr(cg, range->as.range_expr.end))
          return false;
        fprintf(cg->out, "), %s, %u, %u)",
                range->as.range_expr.inclusive ? "true" : "false",
                expr->line, expr->col);
      } else {
        fputs("runes_str_byte_at(", cg->out);
        if (!emit_expr(cg, expr->as.index.target))
          return false;
        fputs(", (size_t)(", cg->out);
        if (!emit_expr(cg, expr->as.index.index))
          return false;
        fprintf(cg->out, "), %u, %u)", expr->line, expr->col);
      }
      return true;
    }
    if (expr->as.index.target->resolved_type &&
        expr->as.index.target->resolved_type->kind == TY_SLICE) {
      Type *slice = expr->as.index.target->resolved_type;
      char slice_name[1024];
      if (!slice_type_name(cg, slice, slice_name, sizeof(slice_name)))
        return false;
      if (expr->as.index.index->kind == AST_RANGE_EXPR) {
        AstNode *range = expr->as.index.index;
        fprintf(cg->out, "%s_sub(", slice_name);
        if (!emit_expr(cg, expr->as.index.target))
          return false;
        fputs(", (size_t)(", cg->out);
        if (!emit_expr(cg, range->as.range_expr.start))
          return false;
        fputs("), (size_t)(", cg->out);
        if (!emit_expr(cg, range->as.range_expr.end))
          return false;
        fprintf(cg->out, "), %s, %u, %u)",
                range->as.range_expr.inclusive ? "true" : "false",
                expr->line, expr->col);
        return true;
      }
      fprintf(cg->out, "*%s_at(", slice_name);
      if (!emit_expr(cg, expr->as.index.target))
        return false;
      fputs(", (size_t)(", cg->out);
      if (!emit_expr(cg, expr->as.index.index))
        return false;
      fprintf(cg->out, "), %u, %u)", expr->line, expr->col);
      return true;
    }
    if (!emit_expr(cg, expr->as.index.target))
      return false;
    fputc('[', cg->out);
    if (expr->as.index.target->resolved_type &&
        expr->as.index.target->resolved_type->kind == TY_ARRAY) {
      fputs("runes_checked_index((size_t)(", cg->out);
      if (!emit_expr(cg, expr->as.index.index))
        return false;
      fprintf(cg->out, "), %zu, %u, %u)",
              expr->as.index.target->resolved_type->as.array.size, expr->line,
              expr->col);
    } else if (!emit_expr(cg, expr->as.index.index)) {
      return false;
    }
    fputc(']', cg->out);
    return true;
  case AST_FIELD_EXPR:
    if (expr->as.field.target->resolved_type &&
        expr->as.field.target->resolved_type->kind == TY_SLICE &&
        (strcmp(expr->as.field.field, "len") == 0 ||
         strcmp(expr->as.field.field, "ptr") == 0)) {
      fputc('(', cg->out);
      if (!emit_expr(cg, expr->as.field.target))
        return false;
      fprintf(cg->out, ").%s", expr->as.field.field);
      return true;
    }
    if (expr->as.field.target->resolved_type &&
        expr->as.field.target->resolved_type->kind == TY_PRIMITIVE &&
        strcmp(expr->as.field.target->resolved_type->as.primitive.name,
               "str") == 0) {
      if (strcmp(expr->as.field.field, "len") == 0) {
        fputc('(', cg->out);
        if (!emit_expr(cg, expr->as.field.target))
          return false;
        fputs(").len", cg->out);
        return true;
      }
      if (strcmp(expr->as.field.field, "ptr") == 0) {
        fputc('(', cg->out);
        if (!emit_expr(cg, expr->as.field.target))
          return false;
        fputs(").ptr", cg->out);
        return true;
      }
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
    fputc('(', cg->out);
    if (!emit_expr(cg, expr->as.field.target))
      return false;
    fputc(')', cg->out);
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
    if (expr->as.unary.op == TOKEN_MINUS && expr->as.unary.expr->kind ==
                                                    AST_INT_LITERAL) {
      unsigned long long magnitude =
          expr->as.unary.expr->as.int_literal.value;
      if (magnitude == (unsigned long long)LLONG_MAX + 1ULL)
        fputs("(-9223372036854775807LL - 1LL)", cg->out);
      else
        fprintf(cg->out, "(-%lluLL)", magnitude);
      return true;
    }
    if (expr->as.unary.op == TOKEN_MINUS && expr->resolved_type &&
        type_is_integer(expr->resolved_type)) {
      fprintf(cg->out, "runes_checked_neg_%s(",
              expr->resolved_type->as.primitive.name);
      if (!emit_expr(cg, expr->as.unary.expr))
        return false;
      fprintf(cg->out, ", %u, %u)", expr->line, expr->col);
      return true;
    }
    if (expr->as.unary.op == TOKEN_TILDE && expr->resolved_type &&
        type_is_integer(expr->resolved_type)) {
      const char *type_name =
          c_named_type(expr->resolved_type->as.primitive.name);
      if (!type_name) {
        cg_error(cg, expr, "unsupported bitwise-not result type");
        return false;
      }
      fprintf(cg->out, "((%s)~(", type_name);
      if (!emit_expr(cg, expr->as.unary.expr))
        return false;
      fputs("))", cg->out);
      return true;
    }
    fputs(token_kind_to_string(expr->as.unary.op), cg->out);
    fputc('(', cg->out);
    if (!emit_expr(cg, expr->as.unary.expr))
      return false;
    fputc(')', cg->out);
    return true;
  case AST_CAST_EXPR: {
    bool typed_allocation =
        expr->resolved_type && expr->resolved_type->kind == TY_POINTER &&
        expr->as.cast.expr && expr->as.cast.expr->kind == AST_CALL_EXPR &&
        expr->as.cast.expr->as.call.callee->kind == AST_IDENTIFIER &&
        strcmp(expr->as.cast.expr->as.call.callee->as.identifier.name,
               "alloc") == 0;
    if (typed_allocation) {
      Type *allocated = expr->resolved_type->as.pointer.inner;
      char descriptor[1024], c_type[1024];
      if (!descriptor_name(cg, allocated, descriptor, sizeof(descriptor)) ||
          !build_semantic_decl(cg, allocated, "", c_type,
                               sizeof(c_type)))
        return false;
      fprintf(cg->out, "((%s *)runes_alloc_typed(", c_type);
      if (!emit_expr(cg, expr->as.cast.expr->as.call.args))
        return false;
      fprintf(cg->out, ", _Alignof(%s), &%s, %u, %u))", c_type,
              descriptor, expr->line, expr->col);
      return true;
    }
    bool pointer_to_string =
        expr->resolved_type && expr->resolved_type->kind == TY_PRIMITIVE &&
        strcmp(expr->resolved_type->as.primitive.name, "str") == 0 &&
        expr->as.cast.expr->resolved_type &&
        expr->as.cast.expr->resolved_type->kind == TY_POINTER;
    if (pointer_to_string) {
      fputs("runes_str_from_c((const char *)(", cg->out);
      if (!emit_expr(cg, expr->as.cast.expr))
        return false;
      fputs("))", cg->out);
      return true;
    }
    if (expr->resolved_type && expr->resolved_type->kind == TY_PRIMITIVE &&
        strcmp(expr->resolved_type->as.primitive.name, "char") == 0 &&
        expr->as.cast.expr->resolved_type &&
        !type_equals(expr->as.cast.expr->resolved_type,
                     expr->resolved_type)) {
      fputs("runes_char_from_u64((uint64_t)(", cg->out);
      if (!emit_expr(cg, expr->as.cast.expr))
        return false;
      fprintf(cg->out, "), %u, %u)", expr->line, expr->col);
      return true;
    }
    char target[1024];
    if (!expr->resolved_type ||
        !build_semantic_decl(cg, expr->resolved_type, "", target,
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
    bool built = type && type->resolved_type
                     ? build_semantic_decl(cg, type->resolved_type, "",
                                           declaration, sizeof(declaration))
                     : build_c_decl(cg, type, "", declaration,
                                    sizeof(declaration));
    if (!built) {
      cg_error(cg, expr, "unsupported type in size/alignment expression");
      return false;
    }
    fprintf(cg->out, "%s(%s)",
            expr->kind == AST_SIZEOF_EXPR ? "sizeof" : "_Alignof",
            declaration);
    return true;
  }
  case AST_PROMOTE_EXPR:
    if (!expr->as.promote.expr->resolved_type ||
        expr->as.promote.expr->resolved_type->kind != TY_POINTER) {
      cg_error(cg, expr, "promotion source is not a resolved pointer");
      return false;
    }
    char promotion_descriptor[1024];
    if (!descriptor_name(cg,
                         expr->as.promote.expr->resolved_type->as.pointer.inner,
                         promotion_descriptor,
                         sizeof(promotion_descriptor))) {
      cg_error(cg, expr, "missing deep-promotion type descriptor");
      return false;
    }
    fputs(expr->as.promote.target == REALM_GC ? "runes_promote_gc("
                                               : "runes_promote_dynamic(",
          cg->out);
    if (!emit_expr(cg, expr->as.promote.expr))
      return false;
    fprintf(cg->out, ", &%s, %u, %u)", promotion_descriptor, expr->line,
            expr->col);
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
      fprintf(cg->out, ", %u, %u)", expr->line, expr->col);
      return true;
    }
    if (string_operands && (expr->as.binary.op == TOKEN_EQ_EQ ||
                            expr->as.binary.op == TOKEN_BANG_EQ)) {
      fputs(expr->as.binary.op == TOKEN_EQ_EQ ? "(runes_str_equal("
                                               : "(!runes_str_equal(",
            cg->out);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fputs(", ", cg->out);
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fputs("))", cg->out);
      return true;
    }
    if (string_operands && (expr->as.binary.op == TOKEN_LT ||
                            expr->as.binary.op == TOKEN_LT_EQ ||
                            expr->as.binary.op == TOKEN_GT ||
                            expr->as.binary.op == TOKEN_GT_EQ)) {
      fputs("(runes_str_compare(", cg->out);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fputs(", ", cg->out);
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fprintf(cg->out, ") %s 0)", c_op(expr->as.binary.op));
      return true;
    }
    if (expr->resolved_type && type_is_integer(expr->resolved_type) &&
        (expr->as.binary.op == TOKEN_AMP ||
         expr->as.binary.op == TOKEN_PIPE ||
         expr->as.binary.op == TOKEN_CARET)) {
      const char *type_name =
          c_named_type(expr->resolved_type->as.primitive.name);
      const char *op = c_op(expr->as.binary.op);
      if (!type_name || !op) {
        cg_error(cg, expr, "unsupported bitwise result type");
        return false;
      }
      fprintf(cg->out, "((%s)(", type_name);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fprintf(cg->out, " %s ", op);
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fputs("))", cg->out);
      return true;
    }
    if (expr->resolved_type && type_is_integer(expr->resolved_type) &&
        (expr->as.binary.op == TOKEN_SHL ||
         expr->as.binary.op == TOKEN_SHR)) {
      Type *count_type = expr->as.binary.right->resolved_type;
      const NumericTypeInfo *value_info =
          get_numeric_info(expr->resolved_type->as.primitive.name);
      const NumericTypeInfo *count_info =
          count_type && count_type->kind == TY_PRIMITIVE
              ? get_numeric_info(count_type->as.primitive.name)
              : NULL;
      if (!value_info || !count_info || count_info->is_float) {
        cg_error(cg, expr, "invalid checked shift operand types");
        return false;
      }
      fprintf(cg->out, "runes_checked_%s_%s(",
              expr->as.binary.op == TOKEN_SHL ? "shl" : "shr",
              expr->resolved_type->as.primitive.name);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fprintf(cg->out, ", runes_checked_shift_count_%s(",
              count_info->is_signed ? "signed" : "unsigned");
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fprintf(cg->out, ", %d, %u, %u), %u, %u)", value_info->bit_width,
              expr->line, expr->col, expr->line, expr->col);
      return true;
    }
    const char *checked_op = NULL;
    if (expr->resolved_type && type_is_integer(expr->resolved_type)) {
      switch (expr->as.binary.op) {
      case TOKEN_PLUS: checked_op = "add"; break;
      case TOKEN_MINUS: checked_op = "sub"; break;
      case TOKEN_STAR: checked_op = "mul"; break;
      case TOKEN_SLASH: checked_op = "div"; break;
      case TOKEN_PERCENT: checked_op = "rem"; break;
      default: break;
      }
    }
    if (checked_op) {
      fprintf(cg->out, "runes_checked_%s_%s(", checked_op,
              expr->resolved_type->as.primitive.name);
      if (!emit_expr(cg, expr->as.binary.left))
        return false;
      fputs(", ", cg->out);
      if (!emit_expr(cg, expr->as.binary.right))
        return false;
      fprintf(cg->out, ", %u, %u)", expr->line, expr->col);
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
    if (expr->as.call.callee->kind == AST_IDENTIFIER &&
        strcmp(expr->as.call.callee->as.identifier.name, "unwrap") == 0) {
      char pointer_type[1024];
      if (!build_semantic_decl(cg, expr->resolved_type, "", pointer_type,
                               sizeof(pointer_type))) {
        cg_error(cg, expr, "unsupported nullable pointer unwrap type");
        return false;
      }
      fprintf(cg->out, "((%s)runes_unwrap_ptr(", pointer_type);
      if (!emit_expr(cg, expr->as.call.args))
        return false;
      fprintf(cg->out, ", %u, %u))", expr->line, expr->col);
      return true;
    }
    if (expr->as.call.callee->kind == AST_IDENTIFIER) {
      const char *builtin = expr->as.call.callee->as.identifier.name;
      AstNode *arg = expr->as.call.args;
      if (strcmp(builtin, "alloc") == 0 ||
          strcmp(builtin, "raw_alloc") == 0) {
        fprintf(cg->out, "%s(", strcmp(builtin, "alloc") == 0
                                      ? "runes_alloc"
                                      : "runes_raw_alloc");
        if (!emit_expr(cg, arg))
          return false;
        if (strcmp(builtin, "alloc") == 0)
          fputs(", _Alignof(max_align_t)", cg->out);
        fprintf(cg->out, ", %u, %u)", expr->line, expr->col);
        return true;
      }
      if (strcmp(builtin, "raw_alloc_aligned") == 0) {
        fputs("runes_raw_alloc_aligned(", cg->out);
        if (!emit_expr(cg, arg))
          return false;
        fputs(", ", cg->out);
        if (!emit_expr(cg, arg->next))
          return false;
        fprintf(cg->out, ", %u, %u)", expr->line, expr->col);
        return true;
      }
      if (strcmp(builtin, "raw_free") == 0) {
        fputs("runes_raw_free((void *)", cg->out);
        if (!emit_expr(cg, arg))
          return false;
        fputc(')', cg->out);
        return true;
      }
      if (strcmp(builtin, "slice") == 0 ||
          strcmp(builtin, "const_slice") == 0) {
        if (!expr->resolved_type || expr->resolved_type->kind != TY_SLICE) {
          cg_error(cg, expr, "slice constructor has no resolved slice type");
          return false;
        }
        char slice_name[1024];
        if (!slice_type_name(cg, expr->resolved_type, slice_name,
                             sizeof(slice_name)))
          return false;
        fprintf(cg->out, "(%s){ .ptr = ", slice_name);
        if (!emit_expr(cg, arg))
          return false;
        fputs(", .len = (size_t)(", cg->out);
        if (!emit_expr(cg, arg->next))
          return false;
        fputs(") }", cg->out);
        return true;
      }
    }
    if (expr->as.call.callee->kind == AST_IDENTIFIER &&
        expr->as.call.callee->resolved_type &&
        expr->as.call.callee->resolved_type->kind == TY_FUNCTION &&
        expr->as.call.callee->resolved_decl &&
        (expr->as.call.callee->resolved_decl->kind == AST_VAR_DECL ||
         expr->as.call.callee->resolved_decl->kind == AST_PARAM ||
         expr->as.call.callee->resolved_decl->kind == AST_IDENTIFIER ||
         expr->as.call.callee->resolved_decl->kind == AST_FIELD_PATTERN ||
         (expr->as.call.callee->resolved_decl->kind == AST_FUNC_DECL &&
          expr->as.call.callee->resolved_decl->as.func_decl.lexical_parent &&
          (expr->as.call.callee->resolved_decl->as.func_decl.is_move ||
           function_capture_index(
               cg->current_function, expr->as.call.callee->resolved_decl,
               expr->as.call.callee->as.identifier.name) >= 0) &&
          (expr->as.call.callee->resolved_decl->as.func_decl.lexical_parent ==
               cg->current_function ||
           function_capture_index(
               cg->current_function, expr->as.call.callee->resolved_decl,
               expr->as.call.callee->as.identifier.name) >= 0)))) {
      AstNode *callee = expr->as.call.callee;
      bool local_move_function =
          callee->resolved_decl->kind == AST_FUNC_DECL;
      int closure_capture =
          local_move_function
              ? function_capture_index(cg->current_function,
                                       callee->resolved_decl,
                                       callee->as.identifier.name)
              : -1;
      char closure_name[1024];
      if (!closure_type_name(cg, callee->resolved_type, closure_name,
                             sizeof(closure_name)))
        return false;
      fprintf(cg->out, "%s_invoke(", closure_name);
      if (closure_capture >= 0)
        fprintf(cg->out, "(*__runes_capture_%d)", closure_capture);
      else if (local_move_function)
        fputs(callee->as.identifier.name, cg->out);
      else if (!emit_expr(cg, callee))
        return false;
      if (expr->as.call.args) {
        fputs(", ", cg->out);
        if (!emit_typed_arg_list(cg, expr->as.call.args,
                                 callee->resolved_type))
          return false;
      }
      fputc(')', cg->out);
      return true;
    }
    if ((expr->as.call.callee->kind == AST_FIELD_EXPR ||
         expr->as.call.callee->kind == AST_INDEX_EXPR) &&
        expr->as.call.callee->resolved_type &&
        expr->as.call.callee->resolved_type->kind == TY_FUNCTION &&
        !expr->as.call.callee->resolved_type->as.function.is_method &&
        (!expr->as.call.callee->resolved_decl ||
         expr->as.call.callee->resolved_decl->kind != AST_FUNC_DECL)) {
      AstNode *callee = expr->as.call.callee;
      char closure_name[1024];
      if (!closure_type_name(cg, callee->resolved_type, closure_name,
                             sizeof(closure_name)))
        return false;
      fprintf(cg->out, "%s_invoke(", closure_name);
      if (!emit_expr(cg, callee))
        return false;
      if (expr->as.call.args) {
        fputs(", ", cg->out);
        if (!emit_typed_arg_list(cg, expr->as.call.args,
                                 callee->resolved_type))
          return false;
      }
      fputc(')', cg->out);
      return true;
    }
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
        if (!emit_typed_arg_list_from(cg, expr->as.call.args,
                                      callee->resolved_type, 1))
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
      const char *selected_method =
          callee->resolved_decl
              ? registered_decl_name(cg, callee->resolved_decl)
              : NULL;
      if (selected_method)
        fprintf(cg->out, "%s(", selected_method);
      else
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
      if (!emit_typed_arg_list_from(cg, expr->as.call.args,
                                    callee->resolved_type,
                                    has_receiver ? 1 : 0))
        return false;
      fputc(')', cg->out);
      return true;
    }
    if (expr->as.call.callee->kind != AST_IDENTIFIER &&
        expr->as.call.callee->kind != AST_FIELD_EXPR &&
        expr->as.call.callee->kind != AST_INDEX_EXPR &&
        expr->as.call.callee->resolved_type &&
        expr->as.call.callee->resolved_type->kind == TY_FUNCTION &&
        !expr->as.call.callee->resolved_type->as.function.is_method) {
      AstNode *callee = expr->as.call.callee;
      char closure_name[1024];
      if (!closure_type_name(cg, callee->resolved_type, closure_name,
                             sizeof(closure_name)))
        return false;
      fprintf(cg->out, "%s_invoke(", closure_name);
      if (!emit_expr(cg, callee))
        return false;
      if (expr->as.call.args) {
        fputs(", ", cg->out);
        if (!emit_typed_arg_list(cg, expr->as.call.args,
                                 callee->resolved_type))
          return false;
      }
      fputc(')', cg->out);
      return true;
    }
    if (expr->as.call.callee->resolved_type &&
        expr->as.call.callee->resolved_type->kind == TY_VARIANT) {
      return emit_variant_value(cg, expr->as.call.callee->resolved_type,
                                variant_callee_arm(expr->as.call.callee),
                                expr->as.call.args, expr);
    }
    Type *constructed_struct =
        expr->as.call.callee->resolved_type &&
                expr->as.call.callee->resolved_type->kind == TY_STRUCT
            ? expr->as.call.callee->resolved_type
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
        Type *field_type = NULL;
        for (int i = 0; i < constructed_struct->as.struct_t.field_count; i++)
          if (strcmp(constructed_struct->as.struct_t.field_names[i],
                     arg->as.named_arg.name) == 0) {
            field_type = constructed_struct->as.struct_t.field_types[i];
            break;
          }
        if (!emit_coerced_expr(cg, field_type, arg->as.named_arg.value))
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
    AstNode *callee_decl = expr->as.call.callee->resolved_decl;
    if (callee_decl && callee_decl->kind == AST_FUNC_DECL &&
        callee_decl->as.func_decl.capture_count) {
      if (expr->as.call.args)
        fputs(", ", cg->out);
      for (int i = 0; i < callee_decl->as.func_decl.capture_count; i++) {
        AstNode *capture_decl = callee_decl->as.func_decl.captures[i];
        const char *capture_name =
            callee_decl->as.func_decl.capture_names[i];
        int inherited = function_capture_index(cg->current_function,
                                               capture_decl, capture_name);
        if (inherited >= 0) {
          fprintf(cg->out, "__runes_capture_%d", inherited);
        } else if (capture_decl == cg->current_function &&
                   cg->current_result_name &&
                   strcmp(capture_name, cg->current_result_name) == 0) {
          fprintf(cg->out, "&%s", cg->current_result_c_name);
        } else {
          fprintf(cg->out, "&%s", capture_name);
        }
        if (i + 1 < callee_decl->as.func_decl.capture_count)
          fputs(", ", cg->out);
      }
    }
    fputc(')', cg->out);
    return true;
  default:
    cg_error(cg, expr, "unsupported expression for C emission");
    return false;
  }
}

static bool emit_stmt(Codegen *cg, AstNode *stmt, int depth);

static bool emit_deferred_from(Codegen *cg, int start, int depth) {
  for (int index = cg->deferred_count - 1; index >= start; index--) {
    AstNode *defer = cg->deferred[index];
    if (!emit_stmt(cg, defer->as.defer_stmt.expression, depth))
      return false;
  }
  return true;
}

static bool register_defer(Codegen *cg, AstNode *node) {
  if (!GROW_REGISTRY(cg, deferred, cg->deferred_count, deferred_capacity,
                     AstNode *))
    return false;
  cg->deferred[cg->deferred_count++] = node;
  return true;
}
static bool emit_match(Codegen *cg, AstNode *match, int depth,
                       AstNode *target, const char *target_name);
static bool emit_if_value(Codegen *cg, AstNode *if_expr, int depth,
                          AstNode *target, const char *target_name);

static bool register_runtime_initializer(Codegen *cg, AstNode *node) {
  if (!GROW_REGISTRY(cg, runtime_initializers,
                     cg->runtime_initializer_count,
                     runtime_initializer_capacity, AstNode *)) {
    cg_error(cg, node, "could not grow runtime initializer registry");
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
  if (!emit_deferred_from(cg, 0, depth))
    return false;
  if (cg->current_arena_scope || cg->current_gc_frame) {
    if (force_error) {
      indent(cg->out, depth);
      fprintf(cg->out, "%s = %s;\n", cg->current_error_name, error_expr);
    }
    indent(cg->out, depth);
    fputs("goto __runes_cleanup;\n", cg->out);
    cg->current_cleanup_used = true;
    return true;
  }
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

static bool emit_gc_root(Codegen *cg, Type *type, const char *value_address,
                         const AstNode *node, int depth) {
  if (!cg->current_gc_frame || !type_needs_gc_trace(type))
    return true;
  char descriptor[1024];
  if (!descriptor_name(cg, type, descriptor, sizeof(descriptor)))
    return false;
  const char *token = format_name(cg, "__runes_gc_root_%u", cg->temp_id++);
  if (!token ||
      !GROW_REGISTRY(cg, gc_root_tokens, cg->gc_root_count,
                     gc_root_capacity, const char *))
    return false;
  cg->gc_root_tokens[cg->gc_root_count++] = token;
  indent(cg->out, depth);
  fprintf(cg->out,
          "void *%s = runes_gc_root_push((void *)(%s), &%s, %u, %u);\n",
          token, value_address, descriptor, node->line, node->col);
  indent(cg->out, depth);
  fprintf(cg->out, "(void)%s;\n", token);
  return true;
}

static void emit_freeze_gc_roots(Codegen *cg, int start, int depth,
                                 const AstNode *node) {
  for (int i = cg->gc_root_count - 1; i >= start; i--) {
    indent(cg->out, depth);
    fprintf(cg->out, "runes_gc_root_freeze(%s, %u, %u);\n",
            cg->gc_root_tokens[i], node ? node->line : 0,
            node ? node->col : 0);
  }
}

static bool emit_block(Codegen *cg, AstNode *block, int depth) {
  if (!block || block->kind != AST_BLOCK)
    return false;

  int gc_root_start = cg->gc_root_count;
  int defer_start = cg->deferred_count;
  AstNode *stmt = block->as.block.statements;
  while (stmt) {
    if (!emit_stmt(cg, stmt, depth))
      return false;
    if (cg->current_gc_frame && stmt->kind != AST_RETURN_STMT &&
        stmt->kind != AST_BREAK_STMT &&
        stmt->kind != AST_CONTINUE_STMT) {
      indent(cg->out, depth);
      fputs("runes_gc_commit_allocations();\n", cg->out);
    }
    stmt = stmt->next;
  }
  if (cg->current_gc_frame) {
    emit_freeze_gc_roots(cg, gc_root_start, depth, block);
    cg->gc_root_count = gc_root_start;
  }
  if (!emit_deferred_from(cg, defer_start, depth))
    return false;
  cg->deferred_count = defer_start;
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
  fputs("if ", cg->out);
  if (!emit_condition(cg, if_expr->as.if_stmt.condition))
    return false;
  fputs(" {\n", cg->out);
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
  const char *emitted_name = depth == 0 ? registered_decl_name(cg, stmt)
                                        : stmt->as.var_decl.name;
  if (!emitted_name)
    emitted_name = stmt->as.var_decl.name;
  bool semantic_string = stmt->resolved_type &&
                         stmt->resolved_type->kind == TY_PRIMITIVE &&
                         strcmp(stmt->resolved_type->as.primitive.name,
                                "str") == 0;
  if (stmt->as.var_decl.is_const && !semantic_string)
    fputs("const ", cg->out);
  if (stmt->as.var_decl.is_volatile)
    fputs("volatile ", cg->out);
  if (stmt->resolved_type && stmt->resolved_type->kind != TY_UNKNOWN &&
      stmt->resolved_type->kind != TY_INFER_ERROR) {
    if (!emit_semantic_decl(cg, stmt->resolved_type, emitted_name, stmt))
      return false;
  } else if (stmt->as.var_decl.type) {
    if (!emit_c_decl(cg, stmt->as.var_decl.type, emitted_name, stmt))
      return false;
  } else if (!emit_semantic_decl(cg, stmt->resolved_type, emitted_name,
                                 stmt)) {
    return false;
  }
  if (!emit_object_abi_suffix(cg, stmt, depth))
    return false;
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
  bool gc_rooted = cg->current_gc_frame &&
                   type_needs_gc_trace(stmt->resolved_type);
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
    } else if (!emit_coerced_expr(cg, stmt->resolved_type,
                                  stmt->as.var_decl.init)) {
      return false;
    }
  } else if (gc_rooted) {
    fputs(" = {0}", cg->out);
  }
  fputs(";\n", cg->out);
  if (gc_rooted) {
    char address[512];
    snprintf(address, sizeof(address), "&%s", emitted_name);
    if (!emit_gc_root(cg, stmt->resolved_type, address, stmt, depth))
      return false;
  }
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
              emitted_name, temp, emitted_name);
    } else {
      indent(cg->out, depth);
      fprintf(cg->out, "memcpy(%s, ", emitted_name);
      if (!emit_expr(cg, stmt->as.var_decl.init))
        return false;
      fprintf(cg->out, ", sizeof %s);\n", emitted_name);
    }
  }
  if (match_init)
    return emit_match(cg, stmt->as.var_decl.init, depth, NULL,
                      emitted_name);
  if (if_init) {
    if (depth == 0) {
      return register_runtime_initializer(cg, stmt);
    }
    return emit_if_value(cg, stmt->as.var_decl.init, depth, NULL,
                         emitted_name);
  }
  if (fallible_init)
    return emit_fallible_expr(cg, stmt->as.var_decl.init, depth, NULL,
                              emitted_name);
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
    fputs("runes_str_write_stdout(", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fputs(");\n", cg->out);
  } else if (strcmp(name, "char") == 0) {
    fputs("runes_char_write_stdout(", cg->out);
    if (!emit_expr(cg, arg))
      return false;
    fprintf(cg->out, ", %u, %u);\n", arg->line, arg->col);
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
  while (arg) {
    if (!emit_print_arg(cg, arg, depth))
      return false;
    arg = arg->next;
  }
  indent(cg->out, depth);
  fputs("fputc('\\n', stdout);\n", cg->out);
  return true;
}

static bool emit_for_stmt(Codegen *cg, AstNode *stmt, int depth) {
  AstNode *iter = stmt->as.for_stmt.iter;
  unsigned id = cg->temp_id++;
  int outer_gc_root_start = cg->gc_root_count;
  int loop_gc_root_start = cg->gc_root_count;
  Type *capture_type = NULL;
  Type capture_pointer = {.kind = TY_POINTER};
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
    capture_type = element;
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
      capture_pointer.as.pointer.inner = element;
      capture_pointer.as.pointer.nullable = false;
      capture_type = &capture_pointer;
      if (!emit_semantic_decl(cg, capture_type,
                              stmt->as.for_stmt.cap_value, stmt))
        return false;
      fputs(" = &", cg->out);
    } else {
      if (!emit_semantic_decl(cg, element, stmt->as.for_stmt.cap_value, stmt))
        return false;
      fputs(" = ", cg->out);
    }
    if (!emit_expr(cg, iter))
      return false;
    fprintf(cg->out, "[__runes_i_%u];\n", id);
  } else if (iter->resolved_type && iter->resolved_type->kind == TY_SLICE) {
    Type *slice = iter->resolved_type;
    Type *element = slice->as.slice.inner;
    char slice_temp[64];
    snprintf(slice_temp, sizeof(slice_temp), "__runes_slice_%u", id);
    indent(cg->out, depth);
    if (!emit_semantic_decl(cg, slice, slice_temp, stmt))
      return false;
    fputs(" = ", cg->out);
    if (!emit_expr(cg, iter))
      return false;
    fputs(";\n", cg->out);
    if (cg->current_gc_frame) {
      char address[128];
      snprintf(address, sizeof(address), "&%s", slice_temp);
      if (!emit_gc_root(cg, slice, address, stmt, depth))
        return false;
    }
    loop_gc_root_start = cg->gc_root_count;
    indent(cg->out, depth);
    fprintf(cg->out,
            "for (size_t __runes_i_%u = 0; __runes_i_%u < %s.len; "
            "++__runes_i_%u) {\n",
            id, id, slice_temp, id);
    if (stmt->as.for_stmt.cap_index) {
      indent(cg->out, depth + 1);
      fprintf(cg->out, "size_t %s = __runes_i_%u;\n",
              stmt->as.for_stmt.cap_index, id);
    }
    indent(cg->out, depth + 1);
    if (stmt->as.for_stmt.cap_kind == CAPTURE_PTR ||
        stmt->as.for_stmt.cap_kind == CAPTURE_PTR_INDEXED) {
      capture_pointer.as.pointer.inner = element;
      capture_pointer.as.pointer.nullable = false;
      capture_type = &capture_pointer;
      if (!emit_semantic_decl(cg, capture_type,
                              stmt->as.for_stmt.cap_value, stmt))
        return false;
      fprintf(cg->out, " = &%s.ptr[__runes_i_%u];\n", slice_temp, id);
    } else {
      capture_type = element;
      if (!emit_semantic_decl(cg, element, stmt->as.for_stmt.cap_value, stmt))
        return false;
      fprintf(cg->out, " = %s.ptr[__runes_i_%u];\n", slice_temp, id);
    }
  } else {
    cg_error(cg, stmt, "unsupported for-loop iterable for C emission");
    return false;
  }

  if (capture_type && cg->current_gc_frame) {
    char address[512];
    snprintf(address, sizeof(address), "&%s", stmt->as.for_stmt.cap_value);
    if (!emit_gc_root(cg, capture_type, address, stmt, depth + 1))
      return false;
  }

  if (cg->loop_codegen_depth >= 64) {
    cg_error(cg, stmt, "loop nesting exceeds GC cleanup limit");
    return false;
  }
  cg->loop_gc_root_starts[cg->loop_codegen_depth] = loop_gc_root_start;
  cg->loop_defer_starts[cg->loop_codegen_depth++] = cg->deferred_count;
  if (!emit_block(cg, stmt->as.for_stmt.body, depth + 1))
    return false;
  cg->loop_codegen_depth--;
  if (cg->current_gc_frame) {
    emit_freeze_gc_roots(cg, loop_gc_root_start, depth + 1, stmt);
    cg->gc_root_count = loop_gc_root_start;
  }
  indent(cg->out, depth);
  fputs("}\n", cg->out);
  if (cg->current_gc_frame) {
    emit_freeze_gc_roots(cg, outer_gc_root_start, depth, stmt);
    cg->gc_root_count = outer_gc_root_start;
  }
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
    fprintf(cg->out, "runes_str_equal(%s, ", value_expr);
    if (!emit_expr(cg, pattern))
      return false;
    fputc(')', cg->out);
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
    if (cg->current_gc_frame) {
      char address[512];
      snprintf(address, sizeof(address), "&%s", name);
      if (!emit_gc_root(cg, subject_type, address, pattern, depth))
        return false;
    }
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
    if (cg->current_gc_frame) {
      char address[512];
      snprintf(address, sizeof(address), "&%s", inner->as.identifier.name);
      if (!emit_gc_root(cg, field_type, address, inner, depth))
        return false;
    }
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
  int match_gc_root_start = cg->gc_root_count;

  indent(cg->out, depth);
  fputs("{\n", cg->out);
  indent(cg->out, depth + 1);
  if (!emit_semantic_decl(cg, subject_type, subject_name, match))
    return false;
  fputs(" = ", cg->out);
  if (!emit_expr(cg, match->as.match_stmt.subject))
    return false;
  fputs(";\n", cg->out);
  if (cg->current_gc_frame) {
    char address[128];
    snprintf(address, sizeof(address), "&%s", subject_name);
    if (!emit_gc_root(cg, subject_type, address, match, depth + 1))
      return false;
  }
  indent(cg->out, depth + 1);
  fprintf(cg->out, "bool %s = false;\n", matched_name);

  for (AstNode *arm = match->as.match_stmt.arms; arm; arm = arm->next) {
    int arm_gc_root_start = cg->gc_root_count;
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
      fputs("if ", cg->out);
      if (!emit_condition(cg, arm->as.match_arm.guard))
        return false;
      fputs(" {\n", cg->out);
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
    if (cg->current_gc_frame) {
      emit_freeze_gc_roots(cg, arm_gc_root_start, depth + 2, arm);
      cg->gc_root_count = arm_gc_root_start;
    }
    indent(cg->out, depth + 1);
    fputs("}\n", cg->out);
  }
  indent(cg->out, depth + 1);
  fprintf(cg->out, "if (!%s) abort();\n", matched_name);
  if (cg->current_gc_frame) {
    emit_freeze_gc_roots(cg, match_gc_root_start, depth + 1, match);
    cg->gc_root_count = match_gc_root_start;
  }
  indent(cg->out, depth);
  fputs("}\n", cg->out);
  return true;
}

static bool emit_stmt(Codegen *cg, AstNode *stmt, int depth) {
  switch (stmt->kind) {
  case AST_DEFER_STMT:
    if (!register_defer(cg, stmt)) {
      cg_error(cg, stmt, "too many deferred expressions");
      return false;
    }
    return true;
  case AST_VAR_DECL:
    return emit_var_decl(cg, stmt, depth);
  case AST_FUNC_DECL: {
    if (!stmt->as.func_decl.lexical_parent)
      return true;
    const char *implementation = registered_decl_name(cg, stmt);
    char closure_name[1024];
    if (!implementation ||
        !closure_type_name(cg, stmt->resolved_type, closure_name,
                           sizeof(closure_name)))
      return false;
    char environment[1200];
    snprintf(environment, sizeof(environment), "__runes_env_%s",
             implementation);
    bool allocated_environment =
        stmt->as.func_decl.is_move && stmt->as.func_decl.capture_count &&
        stmt->as.func_decl.lexical_parent->as.func_decl.realm != REALM_STACK;
    if (stmt->as.func_decl.capture_count) {
      indent(cg->out, depth);
      if (allocated_environment)
        fprintf(cg->out,
                "%s__closure_env *%s = runes_alloc_typed(sizeof(*%s), "
                "_Alignof(%s__closure_env), &%s__closure_env_type, %u, %u);\n",
                implementation, environment, environment, implementation,
                implementation, stmt->line, stmt->col);
      else
        fprintf(cg->out, "%s__closure_env %s = {0};\n", implementation,
                environment);
      for (int i = 0; i < stmt->as.func_decl.capture_count; i++) {
        AstNode *capture_decl = stmt->as.func_decl.captures[i];
        const char *capture_name = stmt->as.func_decl.capture_names[i];
        int inherited = function_capture_index(cg->current_function,
                                               capture_decl, capture_name);
        indent(cg->out, depth);
        fprintf(cg->out, "%s%scapture_%d = ", environment,
                allocated_environment ? "->" : ".", i);
        if (stmt->as.func_decl.is_move) {
          if (inherited >= 0)
            fprintf(cg->out, "(*__runes_capture_%d)", inherited);
          else if (capture_decl == cg->current_function &&
                   cg->current_result_name &&
                   strcmp(capture_name, cg->current_result_name) == 0)
            fputs(cg->current_result_c_name, cg->out);
          else
            fputs(capture_name, cg->out);
        } else {
          if (inherited >= 0)
            fprintf(cg->out, "__runes_capture_%d", inherited);
          else if (capture_decl == cg->current_function &&
                   cg->current_result_name &&
                   strcmp(capture_name, cg->current_result_name) == 0)
            fprintf(cg->out, "&%s", cg->current_result_c_name);
          else
            fprintf(cg->out, "&%s", capture_name);
        }
        fputs(";\n", cg->out);
      }
    }
    indent(cg->out, depth);
    fprintf(cg->out,
            "%s %s = { .call = %s__closure_call, .env = %s, "
            ".env_type = %s };\n",
            closure_name, stmt->as.func_decl.name, implementation,
            stmt->as.func_decl.capture_count
                ? (allocated_environment ? environment
                                         : format_name(cg, "&%s", environment))
                : "NULL",
            stmt->as.func_decl.is_move && stmt->as.func_decl.capture_count
                ? format_name(cg, "&%s__closure_env_type", implementation)
                : "NULL");
    if (cg->current_gc_frame) {
      char address[512];
      snprintf(address, sizeof(address), "&%s", stmt->as.func_decl.name);
      if (!emit_gc_root(cg, stmt->resolved_type, address, stmt, depth))
        return false;
    }
    indent(cg->out, depth);
    fprintf(cg->out, "(void)%s;\n", stmt->as.func_decl.name);
    return true;
  }
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
    } else if (!emit_coerced_expr(cg,
                                  stmt->as.assign.target->resolved_type,
                                  stmt->as.assign.value)) {
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
    fputs("if ", cg->out);
    if (!emit_condition(cg, stmt->as.if_stmt.condition))
      return false;
    fputs(" {\n", cg->out);
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
    fputs("while ", cg->out);
    if (!emit_condition(cg, stmt->as.while_stmt.condition))
      return false;
    fputs(" {\n", cg->out);
    if (cg->loop_codegen_depth >= 64) {
      cg_error(cg, stmt, "loop nesting exceeds GC cleanup limit");
      return false;
    }
    cg->loop_gc_root_starts[cg->loop_codegen_depth] = cg->gc_root_count;
    cg->loop_defer_starts[cg->loop_codegen_depth++] = cg->deferred_count;
    if (!emit_block(cg, stmt->as.while_stmt.body, depth + 1))
      return false;
    cg->loop_codegen_depth--;
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
    if (cg->loop_codegen_depth >= 64) {
      cg_error(cg, stmt, "loop nesting exceeds GC cleanup limit");
      return false;
    }
    cg->loop_gc_root_starts[cg->loop_codegen_depth] = cg->gc_root_count;
    cg->loop_defer_starts[cg->loop_codegen_depth++] = cg->deferred_count;
    if (!emit_block(cg, stmt->as.loop_stmt.body, depth + 1))
      return false;
    cg->loop_codegen_depth--;
    indent(cg->out, depth);
    fputs("}\n", cg->out);
    return true;
  case AST_BREAK_STMT:
    if (cg->loop_codegen_depth > 0 &&
        !emit_deferred_from(
            cg, cg->loop_defer_starts[cg->loop_codegen_depth - 1], depth))
      return false;
    if (cg->current_gc_frame && cg->loop_codegen_depth > 0)
      emit_freeze_gc_roots(
          cg, cg->loop_gc_root_starts[cg->loop_codegen_depth - 1], depth,
          stmt);
    indent(cg->out, depth);
    fputs("break;\n", cg->out);
    return true;
  case AST_CONTINUE_STMT:
    if (cg->loop_codegen_depth > 0 &&
        !emit_deferred_from(
            cg, cg->loop_defer_starts[cg->loop_codegen_depth - 1], depth))
      return false;
    if (cg->current_gc_frame && cg->loop_codegen_depth > 0)
      emit_freeze_gc_roots(
          cg, cg->loop_gc_root_starts[cg->loop_codegen_depth - 1], depth,
          stmt);
    indent(cg->out, depth);
    fputs("continue;\n", cg->out);
    return true;
  case AST_RETURN_STMT:
    {
    bool stored_return = false;
    if (cg->deferred_count > 0 && stmt->as.return_stmt.value &&
        stmt->as.return_stmt.value->kind != AST_ERROR_EXPR &&
        cg->current_result_c_name) {
      indent(cg->out, depth);
      fprintf(cg->out, "%s = ", cg->current_result_c_name);
      Type *target = cg->current_fallible
                         ? cg->current_fallible->as.fallible.inner
                         : cg->current_return_type;
      if (!emit_coerced_expr(cg, target, stmt->as.return_stmt.value))
        return false;
      fputs(";\n", cg->out);
      stored_return = true;
    }
    if (cg->current_fallible) {
      if (stmt->as.return_stmt.value &&
          stmt->as.return_stmt.value->kind == AST_ERROR_EXPR) {
        char error_name[1024];
        AstNode *set = stmt->as.return_stmt.value->as.error_expr.path;
        AstNode *member = set ? set->next : NULL;
        if (!set || !member)
          return false;
        const char *set_name = registered_decl_name(
            cg, stmt->as.return_stmt.value->resolved_decl);
        snprintf(error_name, sizeof(error_name), "%s_%s",
                 set_name ? set_name : set->as.identifier.name,
                 member->as.identifier.name);
        return emit_fallible_return(cg, depth, error_name, true);
      }
      if (!stmt->as.return_stmt.value)
        return emit_fallible_return(cg, depth, cg->current_error_name, false);
      if (!cg->current_arena_scope && !cg->current_gc_frame) {
        char result_type[1024];
        if (!result_type_name(cg, cg->current_fallible, result_type,
                              sizeof(result_type)))
          return false;
        if (!emit_deferred_from(cg, 0, depth))
          return false;
        indent(cg->out, depth);
        fprintf(cg->out,
                "return (%s){ .ok = true, .error = RUNES_ERROR_NONE, "
                ".value = ",
                result_type);
        if (stored_return)
          fputs(cg->current_result_c_name, cg->out);
        else if (!emit_coerced_expr(
                     cg, cg->current_fallible->as.fallible.inner,
                     stmt->as.return_stmt.value))
          return false;
        fputs(" };\n", cg->out);
        return true;
      }
    }
    if (cg->current_arena_scope || cg->current_gc_frame) {
      bool returns_named_result =
          stmt->as.return_stmt.value &&
          stmt->as.return_stmt.value->kind == AST_IDENTIFIER &&
          cg->current_result_name &&
          strcmp(stmt->as.return_stmt.value->as.identifier.name,
                 cg->current_result_name) == 0;
      if (stmt->as.return_stmt.value && cg->current_result_c_name &&
          !returns_named_result && !stored_return) {
        indent(cg->out, depth);
        fprintf(cg->out, "%s = ", cg->current_result_c_name);
        Type *target = cg->current_fallible
                           ? cg->current_fallible->as.fallible.inner
                           : cg->current_return_type;
        if (!emit_coerced_expr(cg, target, stmt->as.return_stmt.value))
          return false;
        fputs(";\n", cg->out);
      }
      if (!emit_deferred_from(cg, 0, depth))
        return false;
      indent(cg->out, depth);
      fputs("goto __runes_cleanup;\n", cg->out);
      cg->current_cleanup_used = true;
      return true;
    }
    if (!emit_deferred_from(cg, 0, depth))
      return false;
    indent(cg->out, depth);
    fputs("return", cg->out);
    if (stored_return) {
      fprintf(cg->out, " %s", cg->current_result_c_name);
    } else if (stmt->as.return_stmt.value) {
      fputc(' ', cg->out);
      if (!emit_coerced_expr(cg, cg->current_return_type,
                             stmt->as.return_stmt.value))
        return false;
    } else if (cg->current_result_c_name) {
      fprintf(cg->out, " %s", cg->current_result_c_name);
    }
    fputs(";\n", cg->out);
    return true;
    }
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
                           stmt->as.asm_expr.code_length,
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
    if (!emit_callconv_prefix(cg, decl))
      return false;
    fputs("int main(", cg->out);
  } else {
    if (find_attr(decl->as.func_decl.attrs, "link_name"))
      fputs("RUNES_MAYBE_UNUSED ", cg->out);
    else
      fputs("static RUNES_MAYBE_UNUSED ", cg->out);
    if (!emit_callconv_prefix(cg, decl))
      return false;
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
  bool has_params = decl->as.func_decl.params != NULL;
  if (has_params &&
      !emit_params(cg, decl->as.func_decl.params, function_type))
    return false;
  for (int i = 0; i < decl->as.func_decl.capture_count; i++) {
    if (has_params || i > 0)
      fputs(", ", cg->out);
    char capture_name[64];
    snprintf(capture_name, sizeof(capture_name), "(*__runes_capture_%d)", i);
    if (!emit_semantic_decl(cg, decl->as.func_decl.capture_types[i],
                            capture_name, decl))
      return false;
  }
  if (!has_params && decl->as.func_decl.capture_count == 0)
    fputs("void", cg->out);
  fputc(')', cg->out);
  return true;
}

static bool emit_extern_declaration(Codegen *cg, AstNode *decl) {
  if (strcmp(decl->as.extern_decl.name, "memset") == 0 ||
      strcmp(decl->as.extern_decl.name, "memcpy") == 0 ||
      strcmp(decl->as.extern_decl.name, "memcmp") == 0 ||
      strcmp(decl->as.extern_decl.name, "memmove") == 0 ||
      strcmp(decl->as.extern_decl.name, "sqrt") == 0 ||
      strcmp(decl->as.extern_decl.name, "alloc") == 0 ||
      strcmp(decl->as.extern_decl.name, "raw_alloc") == 0 ||
      strcmp(decl->as.extern_decl.name, "raw_alloc_aligned") == 0 ||
      strcmp(decl->as.extern_decl.name, "raw_free") == 0)
    return true;
  fputs("extern ", cg->out);
  if (!emit_callconv_prefix_for(cg, decl->as.extern_decl.attrs, decl))
    return false;
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
    if (decl->as.extern_decl.params) {
      if (!emit_params(cg, decl->as.extern_decl.params, decl->resolved_type))
        return false;
    } else {
      fputs("void", cg->out);
    }
    fputc(')', cg->out);
    Attr *link_name = find_attr(decl->as.extern_decl.attrs, "link_name");
    if (link_name) {
      fputs(" __asm__(", cg->out);
      if (!emit_string_attr_argument(cg, link_name, decl))
        return false;
      fputc(')', cg->out);
    }
    fputs(";\n", cg->out);
  } else {
    if (!emit_c_decl(cg, decl->as.extern_decl.var_type,
                     decl->as.extern_decl.name, decl))
      return false;
    Attr *link_name = find_attr(decl->as.extern_decl.attrs, "link_name");
    if (link_name) {
      fputs(" __asm__(", cg->out);
      if (!emit_string_attr_argument(cg, link_name, decl))
        return false;
      fputc(')', cg->out);
    }
    fputs(";\n", cg->out);
  }
  return true;
}

static bool emit_struct_definition(Codegen *cg, AstNode *decl) {
  const char *type_name = registered_decl_name(cg, decl);
  fprintf(cg->out, "struct %s {\n", type_name);
  int field_index = 0;
  for (AstNode *field = decl->as.type_decl.fields; field;
       field = field->next, field_index++) {
    fputs("  ", cg->out);
    if (field->as.field_decl.is_volatile)
      fputs("volatile ", cg->out);
    Type *semantic = decl->resolved_type &&
                             decl->resolved_type->kind == TY_STRUCT &&
                             field_index <
                                 decl->resolved_type->as.struct_t.field_count
                         ? decl->resolved_type->as.struct_t
                               .field_types[field_index]
                         : NULL;
    if (semantic ? !emit_semantic_decl(cg, semantic,
                                       field->as.field_decl.name, field)
                 : !emit_c_decl(cg, field->as.field_decl.type,
                                field->as.field_decl.name, field))
      return false;
    fputs(";\n", cg->out);
  }
  fputc('}', cg->out);
  if (!emit_type_layout_suffix(cg, decl))
    return false;
  fputs(";\n\n", cg->out);
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
    int arm_index = 0;
    for (AstNode *arm = decl->as.variant_decl.arms; arm;
         arm = arm->next, arm_index++) {
      if (!arm->as.variant_arm.fields)
        continue;
      fprintf(cg->out, "    struct {\n");
      int field_index = 0;
      Type *payload = decl->resolved_type &&
                              decl->resolved_type->kind == TY_VARIANT &&
                              arm_index < decl->resolved_type->as.variant.arm_count
                          ? decl->resolved_type->as.variant.arm_types[arm_index]
                          : NULL;
      for (AstNode *field = arm->as.variant_arm.fields; field;
           field = field->next, field_index++) {
        fputs("      ", cg->out);
        char name[32];
        snprintf(name, sizeof(name), "_%d", field_index);
        Type *semantic = payload && payload->kind == TY_TUPLE &&
                                 field_index < payload->as.tuple.count
                             ? payload->as.tuple.elems[field_index]
                             : field_index == 0 ? payload : NULL;
        if (semantic ? !emit_semantic_decl(cg, semantic, name, field)
                     : !emit_c_decl(cg, field, name, field))
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
  fprintf(cg->out,
          "struct %s {\n  void *data;\n  const RunesTypeDescriptor *type;\n",
          name);
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
      if (cg->named_types[t].type->kind == TY_INTERFACE &&
          strcmp(cg->named_types[t].type->as.interface_t.name,
                 impl->as.method_decl.iface_name) == 0) {
        interface = cg->named_types[t].type;
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
      const char *implementation_name = registered_decl_name(cg, method);
      if (!implementation_name) {
        cg_error(cg, method,
                 "missing registered interface implementation symbol");
        return false;
      }
      fprintf(cg->out, "%s(*(%s *)data", implementation_name,
              concrete_name);
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
  Type *saved_return_type = cg->current_return_type;
  const char *saved_result = cg->current_result_name;
  const char *saved_result_c = cg->current_result_c_name;
  const char *saved_error = cg->current_error_name;
  bool saved_arena_scope = cg->current_arena_scope;
  bool saved_gc_frame = cg->current_gc_frame;
  bool saved_gc_scope = cg->current_gc_scope;
  bool saved_cleanup_used = cg->current_cleanup_used;
  AstNode *saved_function = cg->current_function;
  int saved_gc_root_count = cg->gc_root_count;
  int saved_loop_depth = cg->loop_codegen_depth;
  int saved_deferred_count = cg->deferred_count;
  Type *return_type = decl->resolved_type &&
                              decl->resolved_type->kind == TY_FUNCTION
                          ? decl->resolved_type->as.function.ret
                          : NULL;
  bool is_fallible = return_type && return_type->kind == TY_FALLIBLE;
  cg->current_return_type = return_type;
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
  cg->current_arena_scope =
      !is_main && decl->as.func_decl.realm == REALM_ARENA;
  cg->current_gc_frame = is_main || decl->as.func_decl.realm == REALM_GC ||
                         decl->as.func_decl.realm == REALM_HEAP ||
                         decl->as.func_decl.realm == REALM_FLEX;
  cg->current_gc_scope =
      !is_main && decl->as.func_decl.realm == REALM_GC;
  cg->current_cleanup_used = false;
  cg->current_function = decl;
  cg->gc_root_count = 0;
  cg->loop_codegen_depth = 0;
  cg->deferred_count = 0;
  if (!emit_func_header(cg, decl, c_name, is_main))
    return false;
  if (!emit_function_abi_suffix(cg, decl, false))
    return false;
  fputs(" {\n", cg->out);

  if (cg->current_gc_frame) {
    indent(cg->out, 1);
    fprintf(cg->out,
            "void *__runes_gc_frame = %s(%u, %u);\n",
            decl->as.func_decl.realm == REALM_FLEX
                ? "runes_gc_frame_enter_if_active"
                : "runes_gc_frame_enter",
            decl->line, decl->col);
  }
  if (cg->current_gc_scope) {
    indent(cg->out, 1);
    fprintf(cg->out, "runes_gc_scope_enter(%u, %u);\n", decl->line,
            decl->col);
  }

  for (AstNode *param = decl->as.func_decl.params; param;
       param = param->next) {
    indent(cg->out, 1);
    fprintf(cg->out, "(void)%s;\n", param->as.param.name);
    if (type_needs_gc_trace(param->resolved_type)) {
      char address[512];
      snprintf(address, sizeof(address), "&%s", param->as.param.name);
      if (!emit_gc_root(cg, param->resolved_type, address, param, 1))
        return false;
    }
  }
  for (int i = 0; i < decl->as.func_decl.capture_count; i++) {
    indent(cg->out, 1);
    fprintf(cg->out, "(void)__runes_capture_%d;\n", i);
    char address[64];
    snprintf(address, sizeof(address), "__runes_capture_%d", i);
    if (!emit_gc_root(cg, decl->as.func_decl.capture_types[i], address,
                      decl, 1))
      return false;
  }

  if (cg->current_arena_scope) {
    indent(cg->out, 1);
    fprintf(cg->out,
            "void *__runes_arena_scope = runes_arena_scope_enter(%u, %u);\n",
            decl->line, decl->col);
  }

  if (is_main) {
    for (int i = 0; i < cg->gc_global_count; i++) {
      AstNode *global = cg->gc_globals[i];
      const char *name = registered_decl_name(cg, global);
      char address[1024];
      snprintf(address, sizeof(address), "&%s", name);
      if (!emit_gc_root(cg, global->resolved_type, address, global, 1))
        return false;
    }
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
      if (type_needs_gc_trace(local_type)) {
        char address[512];
        snprintf(address, sizeof(address), "&%s",
                 cg->current_result_c_name);
        if (!emit_gc_root(cg, local_type, address, decl, 1))
          return false;
      }
    }
  }
  if (is_fallible) {
    indent(cg->out, 1);
    fputs("RunesError __runes_error = RUNES_ERROR_NONE;\n", cg->out);
  }

  if (!emit_block(cg, decl->as.func_decl.body, 1))
    return false;

  if (cg->current_arena_scope || cg->current_gc_frame) {
    if (cg->current_cleanup_used)
      fputs("__runes_cleanup:\n", cg->out);
  }
  if (cg->current_arena_scope) {
    indent(cg->out, 1);
    fputs("runes_arena_scope_leave(__runes_arena_scope);\n", cg->out);
  }
  if (cg->current_gc_frame && cg->current_result_c_name) {
    Type *protected_type = is_fallible ? return_type->as.fallible.inner
                                       : return_type;
    if (type_needs_gc_trace(protected_type)) {
      char descriptor[1024];
      if (!descriptor_name(cg, protected_type, descriptor,
                           sizeof(descriptor)))
        return false;
      indent(cg->out, 1);
      fprintf(cg->out, "runes_gc_protect_value(&%s, &%s);\n",
              cg->current_result_c_name, descriptor);
    }
  }
  if (cg->current_gc_scope) {
    indent(cg->out, 1);
    fputs("runes_gc_scope_leave();\n", cg->out);
  }
  if (cg->current_gc_frame) {
    indent(cg->out, 1);
    fputs("runes_gc_frame_leave(__runes_gc_frame);\n", cg->out);
  }

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
  cg->current_return_type = saved_return_type;
  cg->current_result_name = saved_result;
  cg->current_result_c_name = saved_result_c;
  cg->current_error_name = saved_error;
  cg->current_arena_scope = saved_arena_scope;
  cg->current_gc_frame = saved_gc_frame;
  cg->current_gc_scope = saved_gc_scope;
  cg->current_cleanup_used = saved_cleanup_used;
  cg->current_function = saved_function;
  cg->gc_root_count = saved_gc_root_count;
  cg->loop_codegen_depth = saved_loop_depth;
  cg->deferred_count = saved_deferred_count;
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
  if (!GROW_REGISTRY(cg, emitted_results, cg->emitted_result_count,
                     emitted_result_capacity, Type *)) {
    cg_error(cg, error_node, "could not grow fallible result registry");
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
  if (!GROW_REGISTRY(cg, emitted_tuples, cg->emitted_tuple_count,
                     emitted_tuple_capacity, Type *)) {
    cg_error(cg, error_node, "could not grow tuple type registry");
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
  if (!GROW_REGISTRY(cg, emitted_arrays, cg->emitted_array_count,
                     emitted_array_capacity, Type *)) {
    cg_error(cg, error_node, "could not grow fixed array type registry");
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

static bool emit_slice_definition(Codegen *cg, Type *slice,
                                  const AstNode *error_node) {
  if (!slice || slice->kind != TY_SLICE)
    return true;
  for (int i = 0; i < cg->emitted_slice_count; i++)
    if (type_equals(cg->emitted_slices[i], slice))
      return true;
  Type *mutable_version = NULL;
  char mutable_name[1024] = {0};
  if (slice->as.slice.readonly) {
    mutable_version = arena_alloc(cg->arena, sizeof(*mutable_version));
    *mutable_version = *slice;
    mutable_version->as.slice.readonly = false;
    if (!emit_slice_definition(cg, mutable_version, error_node) ||
        !slice_type_name(cg, mutable_version, mutable_name,
                         sizeof(mutable_name)))
      return false;
  }
  if (!GROW_REGISTRY(cg, emitted_slices, cg->emitted_slice_count,
                     emitted_slice_capacity, Type *)) {
    cg_error(cg, error_node, "could not grow slice type registry");
    return false;
  }
  cg->emitted_slices[cg->emitted_slice_count++] = slice;

  char name[1024];
  char suffix[768];
  if (!slice_type_name(cg, slice, name, sizeof(name)) ||
      !semantic_type_suffix(cg, slice->as.slice.inner, suffix,
                            sizeof(suffix))) {
    cg_error(cg, error_node, "unsupported slice element type");
    return false;
  }

  fprintf(cg->out, "typedef struct %s {\n  ", name);
  if (slice->as.slice.readonly)
    fputs("const ", cg->out);
  Type pointer = {.kind = TY_POINTER};
  pointer.as.pointer.inner = slice->as.slice.inner;
  pointer.as.pointer.nullable = false;
  if (!emit_semantic_decl(cg, &pointer, "ptr", error_node))
    return false;
  fprintf(cg->out, ";\n  size_t len;\n} %s;\n", name);

  fputs("static inline RUNES_MAYBE_UNUSED ", cg->out);
  if (slice->as.slice.readonly)
    fputs("const ", cg->out);
  if (!emit_semantic_decl(cg, &pointer, "", error_node))
    return false;
  fprintf(cg->out,
          " %s_at(%s slice, size_t index, unsigned line, unsigned column) {\n"
          "  return &slice.ptr[runes_checked_index(index, slice.len, line, column)];\n"
          "}\n",
          name, name);
  fprintf(cg->out,
          "static inline RUNES_MAYBE_UNUSED %s %s_sub(%s slice, size_t start, size_t end, "
          "bool inclusive, unsigned line, unsigned column) {\n"
          "  if (inclusive) {\n"
          "    if (end == SIZE_MAX) { fprintf(stderr, \"Runes bounds error at %%u:%%u: slice end overflow\\n\", line, column); abort(); }\n"
          "    end++;\n"
          "  }\n"
          "  if (start > end || end > slice.len) { fprintf(stderr, \"Runes bounds error at %%u:%%u: invalid slice range %%zu..%%zu for length %%zu\\n\", line, column, start, end, slice.len); abort(); }\n"
          "  return (%s){ .ptr = slice.ptr + start, .len = end - start };\n"
          "}\n\n",
          name, name, name, name);
  if (mutable_version)
    fprintf(cg->out,
            "static inline RUNES_MAYBE_UNUSED %s %s_from_mut(%s source) { return (%s){ "
            ".ptr = source.ptr, .len = source.len }; }\n\n",
            name, name, mutable_name, name);
  return true;
}

static bool emit_closure_definition(Codegen *cg, Type *function,
                                    const AstNode *error_node) {
  if (!function || function->kind != TY_FUNCTION)
    return true;
  for (int i = 0; i < cg->emitted_closure_count; i++)
    if (type_equals(cg->emitted_closures[i], function))
      return true;
  if (!GROW_REGISTRY(cg, emitted_closures, cg->emitted_closure_count,
                     emitted_closure_capacity, Type *)) {
    cg_error(cg, error_node, "could not grow closure type registry");
    return false;
  }
  cg->emitted_closures[cg->emitted_closure_count++] = function;

  char name[1024];
  if (!closure_type_name(cg, function, name, sizeof(name))) {
    cg_error(cg, error_node, "unsupported closure signature");
    return false;
  }
  fprintf(cg->out, "typedef struct %s {\n  ", name);
  Type *result = function->as.function.ret;
  char call_name[] = "(*call)";
  if (result && result->kind == TY_ARRAY) {
    char array_name[1024];
    if (!array_type_name(cg, result, array_name, sizeof(array_name)))
      return false;
    fprintf(cg->out, "%s %s", array_name, call_name);
  } else if (!emit_semantic_decl(cg, result, call_name, error_node)) {
    return false;
  }
  fputs("(void *", cg->out);
  for (int i = 0; i < function->as.function.param_count; i++) {
    fputs(", ", cg->out);
    char parameter[32];
    snprintf(parameter, sizeof(parameter), "p%d", i);
    if (!emit_semantic_decl(cg, function->as.function.params[i], parameter,
                            error_node))
      return false;
  }
  fprintf(cg->out,
          ");\n  void *env;\n  const RunesTypeDescriptor *env_type;\n} %s;\n\n",
          name);
  return true;
}

static bool emit_closure_invoke_definition(Codegen *cg, Type *function,
                                           const AstNode *error_node) {
  char name[1024];
  if (!closure_type_name(cg, function, name, sizeof(name)))
    return false;
  Type *result = function->as.function.ret;
  fputs("static inline RUNES_MAYBE_UNUSED ", cg->out);
  if (result && result->kind == TY_ARRAY) {
    char array_name[1024];
    if (!array_type_name(cg, result, array_name, sizeof(array_name)))
      return false;
    fputs(array_name, cg->out);
  } else if (!emit_semantic_decl(cg, result, "", error_node)) {
    return false;
  }
  fprintf(cg->out, " %s_invoke(%s closure", name, name);
  for (int i = 0; i < function->as.function.param_count; i++) {
    fputs(", ", cg->out);
    char parameter[32];
    snprintf(parameter, sizeof(parameter), "p%d", i);
    if (!emit_semantic_decl(cg, function->as.function.params[i], parameter,
                            error_node))
      return false;
  }
  fputs(") {\n  ", cg->out);
  bool returns_void = result && result->kind == TY_PRIMITIVE &&
                      strcmp(result->as.primitive.name, "void") == 0;
  if (!returns_void)
    fputs("return ", cg->out);
  fputs("closure.call(closure.env", cg->out);
  for (int i = 0; i < function->as.function.param_count; i++)
    fprintf(cg->out, ", p%d", i);
  fputs(");\n}\n\n", cg->out);
  return true;
}

static bool emit_closure_invoke_definitions(Codegen *cg,
                                            const AstNode *error_node) {
  for (int i = 0; i < cg->emitted_closure_count; i++)
    if (!emit_closure_invoke_definition(cg, cg->emitted_closures[i],
                                        error_node))
      return false;
  return true;
}

static AstNode *declaration_for_type(Codegen *cg, Type *type) {
  for (int i = 0; i < cg->named_decl_count; i++) {
    AstNode *declaration = cg->named_decls[i].decl;
    if (declaration && declaration->resolved_type == type &&
        (declaration->kind == AST_TYPE_DECL ||
         declaration->kind == AST_VARIANT_DECL))
      return declaration;
  }
  return NULL;
}

static bool emit_complete_type(Codegen *cg, Type *type,
                               const AstNode *error_node) {
  if (!type || type->kind == TY_POINTER || type->kind == TY_PRIMITIVE ||
      type->kind == TY_ERROR || type->kind == TY_INTERFACE ||
      type->kind == TY_NULL || type->kind == TY_UNKNOWN ||
      type->kind == TY_INFER_ERROR)
    return true;
  if (type->kind == TY_FUNCTION) {
    for (int i = 0; i < type->as.function.param_count; i++) {
      Type *parameter = type->as.function.params[i];
      if (!emit_complete_type(cg, parameter, error_node) ||
          (parameter->kind == TY_ARRAY &&
           !emit_array_definition(cg, parameter, error_node)) ||
          (parameter->kind == TY_FALLIBLE &&
           !emit_result_definition(cg, parameter, error_node)))
        return false;
    }
    Type *result = type->as.function.ret;
    if (!emit_complete_type(cg, result, error_node) ||
        (result->kind == TY_ARRAY &&
         !emit_array_definition(cg, result, error_node)) ||
        (result->kind == TY_FALLIBLE &&
         !emit_result_definition(cg, result, error_node)))
      return false;
    return emit_closure_definition(cg, type, error_node);
  }
  if (type->kind == TY_SLICE)
    return emit_slice_definition(cg, type, error_node);
  if (type->kind == TY_FALLIBLE)
    return emit_complete_type(cg, type->as.fallible.inner, error_node);
  if (type->kind == TY_ARRAY)
    return emit_complete_type(cg, type->as.array.inner, error_node);
  for (int i = 0; i < cg->completed_type_count; i++)
    if (cg->completed_types[i] == type)
      return true;
  for (int i = 0; i < cg->active_type_count; i++) {
    if (cg->active_types[i] == type) {
      cg_error(cg, error_node,
               "aggregate type has an impossible by-value dependency cycle");
      return false;
    }
  }
  if (!GROW_REGISTRY(cg, active_types, cg->active_type_count,
                     active_type_capacity, Type *))
    return false;
  cg->active_types[cg->active_type_count++] = type;

  if (type->kind == TY_TUPLE) {
    for (int i = 0; i < type->as.tuple.count; i++)
      if (!emit_complete_type(cg, type->as.tuple.elems[i], error_node))
        return false;
  } else if (type->kind == TY_STRUCT) {
    for (int i = 0; i < type->as.struct_t.field_count; i++)
      if (!emit_complete_type(cg, type->as.struct_t.field_types[i],
                              error_node))
        return false;
  } else if (type->kind == TY_VARIANT) {
    for (int i = 0; i < type->as.variant.arm_count; i++)
      if (!emit_complete_type(cg, type->as.variant.arm_types[i], error_node))
        return false;
  }

  if (type->kind == TY_TUPLE) {
    if (!emit_tuple_definition(cg, type, error_node))
      return false;
  } else {
    AstNode *declaration = declaration_for_type(cg, type);
    if (!declaration) {
      cg_error(cg, error_node, "missing aggregate type declaration");
      return false;
    }
    if (type->kind == TY_STRUCT) {
      if (!emit_struct_definition(cg, declaration))
        return false;
    } else if (type->kind == TY_VARIANT &&
               !emit_variant_definition(cg, declaration)) {
      return false;
    }
  }
  cg->active_type_count--;
  if (!GROW_REGISTRY(cg, completed_types, cg->completed_type_count,
                     completed_type_capacity, Type *))
    return false;
  cg->completed_types[cg->completed_type_count++] = type;
  return true;
}

static bool emit_named_type_definitions(Codegen *cg, AstNode *declaration) {
  for (; declaration; declaration = declaration->next) {
    if (declaration->kind == AST_MOD_DECL) {
      if (!emit_named_type_definitions(
              cg, declaration->as.mod_decl.declarations))
        return false;
    } else if ((declaration->kind == AST_TYPE_DECL ||
                declaration->kind == AST_VARIANT_DECL) &&
               !emit_complete_type(cg, declaration->resolved_type,
                                   declaration)) {
      return false;
    }
  }
  return true;
}

static bool emit_type_descriptor(Codegen *cg, Type *type,
                                 const AstNode *error_node);

static bool type_needs_gc_trace(Type *type) {
  if (!type)
    return false;
  switch (type->kind) {
  case TY_POINTER:
  case TY_SLICE:
  case TY_INTERFACE:
  case TY_FUNCTION:
    return true;
  case TY_PRIMITIVE:
    return strcmp(type->as.primitive.name, "str") == 0;
  case TY_ARRAY:
    return type_needs_gc_trace(type->as.array.inner);
  case TY_TUPLE:
    for (int i = 0; i < type->as.tuple.count; i++)
      if (type_needs_gc_trace(type->as.tuple.elems[i]))
        return true;
    return false;
  case TY_STRUCT:
    for (int i = 0; i < type->as.struct_t.field_count; i++)
      if (type_needs_gc_trace(type->as.struct_t.field_types[i]))
        return true;
    return false;
  case TY_VARIANT:
    for (int i = 0; i < type->as.variant.arm_count; i++)
      if (type_needs_gc_trace(type->as.variant.arm_types[i]))
        return true;
    return false;
  case TY_FALLIBLE:
    return type_needs_gc_trace(type->as.fallible.inner);
  default:
    return false;
  }
}

static bool emit_clone_value_call(Codegen *cg, Type *type,
                                  const char *destination,
                                  const char *source) {
  char descriptor[1024];
  if (!descriptor_name(cg, type, descriptor, sizeof(descriptor)))
    return false;
  fprintf(cg->out,
          "  runes_clone_value(context, &(%s), &(%s), &%s);\n",
          destination, source, descriptor);
  return true;
}

static bool emit_descriptor_dependencies(Codegen *cg, Type *type,
                                         const AstNode *error_node) {
  switch (type->kind) {
  case TY_POINTER:
    if (type->as.pointer.inner &&
        type->as.pointer.inner->kind == TY_PRIMITIVE &&
        strcmp(type->as.pointer.inner->as.primitive.name, "void") == 0)
      return true;
    return emit_type_descriptor(cg, type->as.pointer.inner, error_node);
  case TY_SLICE:
    return emit_type_descriptor(cg, type->as.slice.inner, error_node);
  case TY_ARRAY:
    return emit_type_descriptor(cg, type->as.array.inner, error_node);
  case TY_TUPLE:
    for (int i = 0; i < type->as.tuple.count; i++)
      if (!emit_type_descriptor(cg, type->as.tuple.elems[i], error_node))
        return false;
    return true;
  case TY_STRUCT:
    for (int i = 0; i < type->as.struct_t.field_count; i++)
      if (!emit_type_descriptor(cg, type->as.struct_t.field_types[i],
                                error_node))
        return false;
    return true;
  case TY_VARIANT:
    for (int i = 0; i < type->as.variant.arm_count; i++) {
      Type *payload = type->as.variant.arm_types[i];
      if (payload && !emit_type_descriptor(cg, payload, error_node))
        return false;
    }
    return true;
  case TY_FALLIBLE:
    return emit_type_descriptor(cg, type->as.fallible.inner, error_node);
  default:
    return true;
  }
}

static bool emit_type_descriptor(Codegen *cg, Type *type,
                                 const AstNode *error_node) {
  if (!type || type->kind == TY_UNKNOWN || type->kind == TY_INFER_ERROR ||
      type->kind == TY_NULL) {
    cg_error(cg, error_node, "unsupported type in deep promotion");
    return false;
  }
  for (int i = 0; i < cg->emitted_descriptor_count; i++)
    if (type_equals(cg->emitted_descriptors[i], type))
      return true;
  if (!GROW_REGISTRY(cg, emitted_descriptors, cg->emitted_descriptor_count,
                     emitted_descriptor_capacity, Type *)) {
    cg_error(cg, error_node, "could not grow promotion descriptor registry");
    return false;
  }
  cg->emitted_descriptors[cg->emitted_descriptor_count++] = type;

  char descriptor[1024];
  char clone[1100];
  char trace[1100];
  char c_type[1024];
  if (!descriptor_name(cg, type, descriptor, sizeof(descriptor)) ||
      !build_semantic_decl(cg, type, "", c_type, sizeof(c_type))) {
    cg_error(cg, error_node, "unsupported promotion descriptor type");
    return false;
  }
  snprintf(clone, sizeof(clone), "%s_clone", descriptor);
  snprintf(trace, sizeof(trace), "%s_trace", descriptor);
  fprintf(cg->out,
          "static const RunesTypeDescriptor %s RUNES_MAYBE_UNUSED;\n",
          descriptor);

  bool pointer_to_void =
      type->kind == TY_POINTER && type->as.pointer.inner &&
      type->as.pointer.inner->kind == TY_PRIMITIVE &&
      strcmp(type->as.pointer.inner->as.primitive.name, "void") == 0;
  bool has_clone = (type->kind == TY_POINTER && !pointer_to_void) ||
                   type->kind == TY_SLICE ||
                   type->kind == TY_ARRAY ||
                   type->kind == TY_TUPLE || type->kind == TY_STRUCT ||
                   type->kind == TY_VARIANT || type->kind == TY_INTERFACE ||
                   type->kind == TY_FUNCTION || type->kind == TY_FALLIBLE ||
                   (type->kind == TY_PRIMITIVE &&
                    strcmp(type->as.primitive.name, "str") == 0);
  bool has_trace = type_needs_gc_trace(type);
  if (has_clone)
    fprintf(cg->out,
            "static void %s(void *, const void *, RunesCloneContext *);\n",
            clone);
  if (has_trace)
    fprintf(cg->out,
            "static void %s(const void *, RunesGcVisit, void *);\n",
            trace);
  if (!emit_descriptor_dependencies(cg, type, error_node))
    return false;

  if (has_clone) {
    fprintf(cg->out,
            "static void %s(void *destination_value, const void "
            "*source_value, RunesCloneContext *context) {\n",
            clone);
    if (type->kind == TY_POINTER) {
      char inner_descriptor[1024];
      if (!descriptor_name(cg, type->as.pointer.inner, inner_descriptor,
                           sizeof(inner_descriptor)))
        return false;
      fprintf(cg->out,
              "  void *source_pointer = NULL;\n"
              "  memcpy(&source_pointer, source_value, sizeof "
              "source_pointer);\n"
              "  void *destination_pointer = runes_clone_pointer(context, "
              "source_pointer, &%s);\n"
              "  memcpy(destination_value, &destination_pointer, sizeof "
              "destination_pointer);\n",
              inner_descriptor);
    } else if (type->kind == TY_SLICE) {
      char slice_name[1024];
      char inner_descriptor[1024];
      if (!slice_type_name(cg, type, slice_name, sizeof(slice_name)) ||
          !descriptor_name(cg, type->as.slice.inner, inner_descriptor,
                           sizeof(inner_descriptor)))
        return false;
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n"
              "  void *destination_pointer = runes_clone_slice(context, "
              "source->ptr, source->len, &%s);\n"
              "  memcpy(&destination->ptr, &destination_pointer, sizeof "
              "destination->ptr);\n",
              slice_name, slice_name, inner_descriptor);
    } else if (type->kind == TY_PRIMITIVE) {
      fputs("  RunesStr source = *(const RunesStr *)source_value;\n"
            "  *(RunesStr *)destination_value = "
            "runes_clone_string(context, source);\n",
            cg->out);
    } else if (type->kind == TY_ARRAY) {
      char inner_descriptor[1024];
      if (!descriptor_name(cg, type->as.array.inner, inner_descriptor,
                           sizeof(inner_descriptor)))
        return false;
      fprintf(cg->out,
              "  for (size_t i = 0; i < %zu; i++)\n"
              "    runes_clone_value(context, (char *)destination_value + "
              "i * %s.size, (const char *)source_value + i * %s.size, "
              "&%s);\n",
              type->as.array.size, inner_descriptor, inner_descriptor,
              inner_descriptor);
    } else if (type->kind == TY_TUPLE) {
      char tuple_name[1024];
      if (!tuple_type_name(cg, type, tuple_name, sizeof(tuple_name)))
        return false;
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n",
              tuple_name, tuple_name);
      for (int i = 0; i < type->as.tuple.count; i++) {
        char destination[64], source[64];
        snprintf(destination, sizeof(destination), "destination->_%d", i);
        snprintf(source, sizeof(source), "source->_%d", i);
        if (!emit_clone_value_call(cg, type->as.tuple.elems[i], destination,
                                   source))
          return false;
      }
    } else if (type->kind == TY_STRUCT) {
      const char *name = registered_type_name(cg, type);
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n",
              name, name);
      for (int i = 0; i < type->as.struct_t.field_count; i++) {
        char destination[512], source[512];
        snprintf(destination, sizeof(destination), "destination->%s",
                 type->as.struct_t.field_names[i]);
        snprintf(source, sizeof(source), "source->%s",
                 type->as.struct_t.field_names[i]);
        if (!emit_clone_value_call(cg, type->as.struct_t.field_types[i],
                                   destination, source))
          return false;
      }
    } else if (type->kind == TY_VARIANT) {
      const char *name = registered_type_name(cg, type);
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n"
              "  switch (source->tag) {\n",
              name, name);
      for (int arm = 0; arm < type->as.variant.arm_count; arm++) {
        Type *payload = type->as.variant.arm_types[arm];
        if (!payload)
          continue;
        const char *arm_name = type->as.variant.arm_names[arm];
        fprintf(cg->out, "  case %s_%s:\n", name, arm_name);
        if (payload->kind == TY_TUPLE) {
          for (int field = 0; field < payload->as.tuple.count; field++) {
            char destination[768], source[768];
            snprintf(destination, sizeof(destination),
                     "destination->data.%s._%d", arm_name, field);
            snprintf(source, sizeof(source), "source->data.%s._%d", arm_name,
                     field);
            if (!emit_clone_value_call(cg, payload->as.tuple.elems[field],
                                       destination, source))
              return false;
          }
        } else {
          char destination[768], source[768];
          snprintf(destination, sizeof(destination),
                   "destination->data.%s._0", arm_name);
          snprintf(source, sizeof(source), "source->data.%s._0", arm_name);
          if (!emit_clone_value_call(cg, payload, destination, source))
            return false;
        }
        fputs("    break;\n", cg->out);
      }
      fputs("  default: break;\n  }\n", cg->out);
    } else if (type->kind == TY_INTERFACE) {
      const char *name = registered_type_name(cg, type);
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n"
              "  if (source->data && source->type)\n"
              "    destination->data = runes_clone_pointer(context, "
              "source->data, source->type);\n",
              name, name);
    } else if (type->kind == TY_FUNCTION) {
      char name[1024];
      if (!closure_type_name(cg, type, name, sizeof(name)))
        return false;
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n"
              "  *destination = *source;\n"
              "  if (source->env && source->env_type)\n"
              "    destination->env = runes_clone_pointer(context, "
              "source->env, source->env_type);\n",
              name, name);
    } else if (type->kind == TY_FALLIBLE) {
      char result_name[1024];
      if (!result_type_name(cg, type, result_name, sizeof(result_name)))
        return false;
      fprintf(cg->out,
              "  %s *destination = destination_value;\n"
              "  const %s *source = source_value;\n"
              "  if (source->ok) {\n",
              result_name, result_name);
      if (!emit_clone_value_call(cg, type->as.fallible.inner,
                                 "destination->value", "source->value"))
        return false;
      fputs("  }\n", cg->out);
    }
    fputs("}\n", cg->out);
  }
  if (has_trace) {
    fprintf(cg->out,
            "static void %s(const void *value, RunesGcVisit visit, "
            "void *context) {\n",
            trace);
    if (type->kind == TY_POINTER) {
      fputs("  void *pointer = NULL;\n"
            "  memcpy(&pointer, value, sizeof pointer);\n"
            "  visit(pointer, context);\n",
            cg->out);
    } else if (type->kind == TY_SLICE) {
      char slice_name[1024], inner_descriptor[1024];
      if (!slice_type_name(cg, type, slice_name, sizeof(slice_name)) ||
          !descriptor_name(cg, type->as.slice.inner, inner_descriptor,
                           sizeof(inner_descriptor)))
        return false;
      fprintf(cg->out,
              "  const %s *slice = value;\n"
              "  visit(slice->ptr, context);\n"
              "  for (size_t i = 0; i < slice->len; i++)\n"
              "    runes_gc_trace_value(&slice->ptr[i], &%s, visit, "
              "context);\n",
              slice_name, inner_descriptor);
    } else if (type->kind == TY_PRIMITIVE) {
      fputs("  visit(((const RunesStr *)value)->ptr, context);\n", cg->out);
    } else if (type->kind == TY_ARRAY) {
      char inner_descriptor[1024];
      if (!descriptor_name(cg, type->as.array.inner, inner_descriptor,
                           sizeof(inner_descriptor)))
        return false;
      fprintf(cg->out,
              "  for (size_t i = 0; i < %zu; i++)\n"
              "    runes_gc_trace_value((const char *)value + i * %s.size, "
              "&%s, visit, context);\n",
              type->as.array.size, inner_descriptor, inner_descriptor);
    } else if (type->kind == TY_TUPLE) {
      char tuple_name[1024];
      if (!tuple_type_name(cg, type, tuple_name, sizeof(tuple_name)))
        return false;
      fprintf(cg->out, "  const %s *tuple = value;\n", tuple_name);
      for (int i = 0; i < type->as.tuple.count; i++) {
        char inner_descriptor[1024];
        if (!descriptor_name(cg, type->as.tuple.elems[i], inner_descriptor,
                             sizeof(inner_descriptor)))
          return false;
        fprintf(cg->out,
                "  runes_gc_trace_value(&tuple->_%d, &%s, visit, context);\n",
                i, inner_descriptor);
      }
    } else if (type->kind == TY_STRUCT) {
      const char *name = registered_type_name(cg, type);
      fprintf(cg->out, "  const %s *object = value;\n", name);
      for (int i = 0; i < type->as.struct_t.field_count; i++) {
        char inner_descriptor[1024];
        if (!descriptor_name(cg, type->as.struct_t.field_types[i],
                             inner_descriptor, sizeof(inner_descriptor)))
          return false;
        fprintf(cg->out,
                "  runes_gc_trace_value(&object->%s, &%s, visit, "
                "context);\n",
                type->as.struct_t.field_names[i], inner_descriptor);
      }
    } else if (type->kind == TY_VARIANT) {
      const char *name = registered_type_name(cg, type);
      fprintf(cg->out,
              "  const %s *object = value;\n  switch (object->tag) {\n",
              name);
      for (int arm = 0; arm < type->as.variant.arm_count; arm++) {
        Type *payload = type->as.variant.arm_types[arm];
        if (!payload || !type_needs_gc_trace(payload))
          continue;
        const char *arm_name = type->as.variant.arm_names[arm];
        fprintf(cg->out, "  case %s_%s:\n", name, arm_name);
        if (payload->kind == TY_TUPLE) {
          for (int field = 0; field < payload->as.tuple.count; field++) {
            char inner_descriptor[1024];
            if (!descriptor_name(cg, payload->as.tuple.elems[field],
                                 inner_descriptor, sizeof(inner_descriptor)))
              return false;
            fprintf(cg->out,
                    "    runes_gc_trace_value(&object->data.%s._%d, &%s, "
                    "visit, context);\n",
                    arm_name, field, inner_descriptor);
          }
        } else {
          char inner_descriptor[1024];
          if (!descriptor_name(cg, payload, inner_descriptor,
                               sizeof(inner_descriptor)))
            return false;
          fprintf(cg->out,
                  "    runes_gc_trace_value(&object->data.%s._0, &%s, visit, "
                  "context);\n",
                  arm_name, inner_descriptor);
        }
        fputs("    break;\n", cg->out);
      }
      fputs("  default: break;\n  }\n", cg->out);
    } else if (type->kind == TY_INTERFACE) {
      const char *name = registered_type_name(cg, type);
      fprintf(cg->out,
              "  const %s *interface_value = value;\n"
              "  visit(interface_value->data, context);\n",
              name);
    } else if (type->kind == TY_FUNCTION) {
      char name[1024];
      if (!closure_type_name(cg, type, name, sizeof(name)))
        return false;
      fprintf(cg->out,
              "  const %s *closure = value;\n"
              "  visit(closure->env, context);\n"
              "  if (closure->env && closure->env_type)\n"
              "    runes_gc_trace_value(closure->env, closure->env_type, "
              "visit, context);\n",
              name);
    } else if (type->kind == TY_FALLIBLE) {
      char result_name[1024], inner_descriptor[1024];
      if (!result_type_name(cg, type, result_name, sizeof(result_name)) ||
          !descriptor_name(cg, type->as.fallible.inner, inner_descriptor,
                           sizeof(inner_descriptor)))
        return false;
      fprintf(cg->out,
              "  const %s *result = value;\n"
              "  if (result->ok) runes_gc_trace_value(&result->value, &%s, "
              "visit, context);\n",
              result_name, inner_descriptor);
    }
    fputs("}\n", cg->out);
  }
  fprintf(cg->out,
          "static const RunesTypeDescriptor %s RUNES_MAYBE_UNUSED = { sizeof(%s), "
          "_Alignof(%s), %s, %s };\n\n",
          descriptor, c_type, c_type, has_clone ? clone : "NULL",
          has_trace ? trace : "NULL");
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
  if (type->kind == TY_SLICE) {
    if (!emit_tuple_dependencies(cg, type->as.slice.inner, error_node))
      return false;
    return emit_slice_definition(cg, type, error_node);
  }
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
    if (type->as.function.ret && type->as.function.ret->kind == TY_FALLIBLE &&
        !emit_result_definition(cg, type->as.function.ret, error_node))
      return false;
    return emit_tuple_dependencies(cg, type->as.function.ret, error_node) &&
           emit_closure_definition(cg, type, error_node);
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
    if (cg->descriptor_phase && node->resolved_type &&
        type_needs_gc_trace(node->resolved_type) &&
        !emit_type_descriptor(cg, node->resolved_type, node))
      return false;
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
      if (cg->descriptor_phase && node->resolved_type &&
          node->resolved_type->kind == TY_FUNCTION &&
          type_needs_gc_trace(node->resolved_type->as.function.ret) &&
          !emit_type_descriptor(cg,
                                node->resolved_type->as.function.ret, node))
        return false;
      if (cg->descriptor_phase)
        for (int i = 0; i < node->as.func_decl.capture_count; i++)
          if ((node->as.func_decl.is_move ||
               type_needs_gc_trace(node->as.func_decl.capture_types[i])) &&
              !emit_type_descriptor(cg,
                                    node->as.func_decl.capture_types[i], node))
            return false;
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
    case AST_DEFER_STMT:
      if (!emit_ast_tuple_dependencies(cg, node->as.defer_stmt.expression))
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
      if (cg->descriptor_phase && node->resolved_type &&
          node->resolved_type->kind == TY_POINTER &&
          node->as.cast.expr && node->as.cast.expr->kind == AST_CALL_EXPR &&
          node->as.cast.expr->as.call.callee->kind == AST_IDENTIFIER &&
          strcmp(node->as.cast.expr->as.call.callee->as.identifier.name,
                 "alloc") == 0 &&
          !emit_type_descriptor(cg, node->resolved_type->as.pointer.inner,
                                node))
        return false;
      break;
    case AST_PROMOTE_EXPR:
      if (!emit_ast_tuple_dependencies(cg, node->as.promote.expr))
        return false;
      if (cg->descriptor_phase) {
        Type *source = node->as.promote.expr->resolved_type;
        if (!source || source->kind != TY_POINTER ||
            !emit_type_descriptor(cg, source->as.pointer.inner, node))
          return false;
      }
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

static bool emit_closure_wrapper_header(Codegen *cg, AstNode *function,
                                        const char *name) {
  if (!function || !function->resolved_type ||
      function->resolved_type->kind != TY_FUNCTION ||
      function->as.func_decl.is_main)
    return true;
  Type *type = function->resolved_type;
  const char *wrapper = format_name(cg, "%s__closure_call", name);
  if (!wrapper)
    return false;
  fputs("static RUNES_MAYBE_UNUSED ", cg->out);
  Type *result = type->as.function.ret;
  if (result->kind == TY_ARRAY) {
    char array_name[1024];
    if (!array_type_name(cg, result, array_name, sizeof(array_name)))
      return false;
    fprintf(cg->out, "%s %s", array_name, wrapper);
  } else if (!emit_semantic_decl(cg, result, wrapper, function)) {
    return false;
  }
  fputs("(void *__runes_env", cg->out);
  int index = 0;
  for (AstNode *parameter = function->as.func_decl.params; parameter;
       parameter = parameter->next, index++) {
    fputs(", ", cg->out);
    if (!emit_semantic_decl(cg, type->as.function.params[index],
                            parameter->as.param.name, parameter))
      return false;
  }
  fputc(')', cg->out);
  return true;
}

static bool emit_closure_wrapper(Codegen *cg, AstNode *function,
                                 const char *name) {
  if (!function || function->as.func_decl.is_main)
    return true;
  if (!emit_closure_wrapper_header(cg, function, name))
    return false;
  fputs(" {\n", cg->out);
  if (function->as.func_decl.capture_count) {
    fprintf(cg->out, "  %s__closure_env *__env = __runes_env;\n", name);
  } else {
    fputs("  (void)__runes_env;\n", cg->out);
  }
  Type *result = function->resolved_type->as.function.ret;
  bool returns_void = result->kind == TY_PRIMITIVE &&
                      strcmp(result->as.primitive.name, "void") == 0;
  fputs(returns_void ? "  " : "  return ", cg->out);
  fprintf(cg->out, "%s(", name);
  bool wrote = false;
  for (AstNode *parameter = function->as.func_decl.params; parameter;
       parameter = parameter->next) {
    if (wrote)
      fputs(", ", cg->out);
    fputs(parameter->as.param.name, cg->out);
    wrote = true;
  }
  for (int i = 0; i < function->as.func_decl.capture_count; i++) {
    if (wrote)
      fputs(", ", cg->out);
    fprintf(cg->out, function->as.func_decl.is_move ? "&__env->capture_%d"
                                                    : "__env->capture_%d",
            i);
    wrote = true;
  }
  fputs(");\n}\n", cg->out);
  return true;
}

static bool emit_closure_environment_definitions(Codegen *cg) {
  for (int index = 0; index < cg->nested_function_count; index++) {
    AstNode *function = cg->nested_functions[index];
    if (!function->as.func_decl.capture_count)
      continue;
    const char *name = registered_decl_name(cg, function);
    fprintf(cg->out, "typedef struct %s__closure_env {\n", name);
    for (int i = 0; i < function->as.func_decl.capture_count; i++) {
      fputs("  ", cg->out);
      char field[64];
      snprintf(field, sizeof(field),
               function->as.func_decl.is_move ? "capture_%d"
                                              : "*capture_%d",
               i);
      if (!emit_semantic_decl(cg, function->as.func_decl.capture_types[i],
                              field, function))
        return false;
      fputs(";\n", cg->out);
    }
    fprintf(cg->out, "} %s__closure_env;\n\n", name);
  }
  return true;
}

static bool emit_move_closure_environment_descriptors(Codegen *cg) {
  for (int index = 0; index < cg->nested_function_count; index++) {
    AstNode *function = cg->nested_functions[index];
    if (!function->as.func_decl.is_move ||
        !function->as.func_decl.capture_count)
      continue;
    const char *name = registered_decl_name(cg, function);
    fprintf(cg->out,
            "static void %s__closure_env_clone(void *destination_value, "
            "const void *source_value, RunesCloneContext *context) {\n"
            "  %s__closure_env *destination = destination_value;\n"
            "  const %s__closure_env *source = source_value;\n",
            name, name, name);
    for (int i = 0; i < function->as.func_decl.capture_count; i++) {
      char descriptor[1024];
      if (!descriptor_name(cg, function->as.func_decl.capture_types[i],
                           descriptor, sizeof(descriptor)))
        return false;
      fprintf(cg->out,
              "  runes_clone_value(context, &destination->capture_%d, "
              "&source->capture_%d, &%s);\n",
              i, i, descriptor);
    }
    fputs("}\n", cg->out);
    fprintf(cg->out,
            "static void %s__closure_env_trace(const void *value, "
            "RunesGcVisit visit, void *context) {\n"
            "  const %s__closure_env *environment = value;\n",
            name, name);
    bool traces = false;
    for (int i = 0; i < function->as.func_decl.capture_count; i++) {
      Type *capture = function->as.func_decl.capture_types[i];
      if (!type_needs_gc_trace(capture))
        continue;
      char descriptor[1024];
      if (!descriptor_name(cg, capture, descriptor, sizeof(descriptor)))
        return false;
      fprintf(cg->out,
              "  runes_gc_trace_value(&environment->capture_%d, &%s, visit, "
              "context);\n",
              i, descriptor);
      traces = true;
    }
    if (!traces)
      fputs("  (void)environment; (void)visit; (void)context;\n", cg->out);
    fputs("}\n", cg->out);
    fprintf(cg->out,
            "static const RunesTypeDescriptor %s__closure_env_type = { "
            "sizeof(%s__closure_env), _Alignof(%s__closure_env), "
            "%s__closure_env_clone, %s__closure_env_trace };\n\n",
            name, name, name, name, name);
  }
  return true;
}

static bool emit_nested_function_phase(Codegen *cg, EmitPhase phase) {
  for (int i = 0; i < cg->nested_function_count; i++) {
    AstNode *function = cg->nested_functions[i];
    if (!function->codegen_reachable)
      continue;
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
      if (!emit_function_abi_suffix(cg, function, true))
        return false;
      fputs(";\n", cg->out);
      if (!emit_closure_wrapper_header(cg, function, name))
        return false;
      fputs(";\n", cg->out);
    } else if (phase == EMIT_FUNCTIONS) {
      if (!emit_func(cg, function, name, false))
        return false;
      if (!emit_closure_wrapper(cg, function, name))
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
      const char *nested_prefix = registered_decl_name(cg, decl);
      if (!nested_prefix)
        return false;
      if (!emit_declaration_phase(cg, decl->as.mod_decl.declarations,
                                  nested_prefix, phase))
        return false;
      cg->current_prefix = prefix;
      continue;
    }

    if ((decl->kind == AST_FUNC_DECL || decl->kind == AST_EXTERN_DECL) &&
        !decl->codegen_reachable)
      continue;

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
      if (!emit_function_abi_suffix(cg, decl, true))
        return false;
      fputs(";\n", cg->out);
      if (!is_main && !emit_closure_wrapper_header(cg, decl, name))
        return false;
      if (!is_main)
        fputs(";\n", cg->out);
    } else if (phase == EMIT_PROTOTYPES && decl->kind == AST_METHOD_DECL) {
      for (AstNode *method = decl->as.method_decl.methods; method;
           method = method->next) {
        const char *name = registered_decl_name(cg, method);
        if (!emit_func_header(cg, method, name, false))
          return false;
        if (!emit_function_abi_suffix(cg, method, true))
          return false;
        fputs(";\n", cg->out);
      }
    } else if (phase == EMIT_FUNCTIONS && decl->kind == AST_FUNC_DECL) {
      const char *name = registered_decl_name(cg, decl);
      bool is_main = !prefix && strcmp(decl->as.func_decl.name, "main") == 0;
      if (!emit_func(cg, decl, name, is_main))
        return false;
      if (!is_main && !emit_closure_wrapper(cg, decl, name))
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

void codegen_init(Codegen *cg, FILE *out, Arena *arena) {
  cg->out = out;
  cg->arena = arena;
  cg->had_error = false;
  cg->error_count = 0;
  cg->temp_id = 0;
  cg->current_fallible = NULL;
  cg->current_return_type = NULL;
  cg->current_result_name = NULL;
  cg->current_result_c_name = NULL;
  cg->current_error_name = NULL;
  cg->current_function = NULL;
  cg->current_arena_scope = false;
  cg->current_gc_frame = false;
  cg->current_gc_scope = false;
  cg->current_cleanup_used = false;
  cg->descriptor_phase = false;
  cg->emitted_result_count = 0;
  cg->emitted_results = NULL;
  cg->emitted_result_capacity = 0;
  cg->emitted_tuple_count = 0;
  cg->emitted_tuples = NULL;
  cg->emitted_tuple_capacity = 0;
  cg->emitted_array_count = 0;
  cg->emitted_arrays = NULL;
  cg->emitted_array_capacity = 0;
  cg->emitted_slice_count = 0;
  cg->emitted_slices = NULL;
  cg->emitted_slice_capacity = 0;
  cg->emitted_closure_count = 0;
  cg->emitted_closures = NULL;
  cg->emitted_closure_capacity = 0;
  cg->emitted_descriptor_count = 0;
  cg->emitted_descriptors = NULL;
  cg->emitted_descriptor_capacity = 0;
  cg->completed_type_count = 0;
  cg->completed_types = NULL;
  cg->completed_type_capacity = 0;
  cg->active_type_count = 0;
  cg->active_types = NULL;
  cg->active_type_capacity = 0;
  cg->named_decl_count = 0;
  cg->named_decls = NULL;
  cg->named_decl_capacity = 0;
  cg->named_type_count = 0;
  cg->named_types = NULL;
  cg->named_type_capacity = 0;
  cg->runtime_initializer_count = 0;
  cg->runtime_initializers = NULL;
  cg->runtime_initializer_capacity = 0;
  cg->nested_function_count = 0;
  cg->nested_functions = NULL;
  cg->nested_function_capacity = 0;
  cg->interface_impl_count = 0;
  cg->interface_impls = NULL;
  cg->interface_impl_capacity = 0;
  cg->gc_globals = NULL;
  cg->gc_global_count = 0;
  cg->gc_global_capacity = 0;
  cg->gc_root_tokens = NULL;
  cg->gc_root_count = 0;
  cg->gc_root_capacity = 0;
  cg->loop_codegen_depth = 0;
  cg->deferred = NULL;
  cg->deferred_count = 0;
  cg->deferred_capacity = 0;
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
  fputs("typedef struct RunesCloneContext RunesCloneContext;\n"
        "typedef struct RunesTypeDescriptor RunesTypeDescriptor;\n"
        "typedef struct RunesStr { const uint8_t *ptr; size_t len; } "
        "RunesStr;\n"
        "#if defined(__GNUC__) || defined(__clang__)\n"
        "#define RUNES_MAYBE_UNUSED __attribute__((unused))\n"
        "#else\n"
        "#define RUNES_MAYBE_UNUSED\n"
        "#endif\n"
        "typedef void (*RunesCloneFunction)(void *, const void *, "
        "RunesCloneContext *);\n"
        "typedef void (*RunesGcVisit)(const void *, void *);\n"
        "typedef void (*RunesTraceFunction)(const void *, RunesGcVisit, "
        "void *);\n"
        "struct RunesTypeDescriptor { size_t size; size_t align; "
        "RunesCloneFunction clone; RunesTraceFunction trace; };\n"
        "extern void *runes_arena_scope_enter(unsigned, unsigned);\n"
        "extern void runes_arena_scope_leave(void *);\n"
        "extern void *runes_alloc(size_t, size_t, unsigned, unsigned);\n"
        "extern void *runes_alloc_typed(size_t, size_t, const "
        "RunesTypeDescriptor *, unsigned, unsigned);\n"
        "extern void *runes_raw_alloc(size_t, unsigned, unsigned);\n"
        "extern void *runes_raw_alloc_aligned(size_t, size_t, unsigned, "
        "unsigned);\n"
        "extern void runes_raw_free(void *);\n"
        "extern void runes_gc_scope_enter(unsigned, unsigned);\n"
        "extern void runes_gc_scope_leave(void);\n"
        "extern void *runes_gc_frame_enter(unsigned, unsigned);\n"
        "extern void *runes_gc_frame_enter_if_active(unsigned, unsigned);\n"
        "extern void runes_gc_frame_leave(void *);\n"
        "extern void *runes_gc_root_push(void *, const "
        "RunesTypeDescriptor *, unsigned, unsigned);\n"
        "extern void runes_gc_root_freeze(void *, unsigned, unsigned);\n"
        "extern void *runes_gc_alloc(size_t, size_t, const "
        "RunesTypeDescriptor *, unsigned, unsigned);\n"
        "extern void runes_gc_collect(void);\n"
        "extern void runes_gc_commit_allocations(void);\n"
        "extern void runes_gc_protect_value(const void *, const "
        "RunesTypeDescriptor *);\n"
        "extern void runes_gc_trace_value(const void *, const "
        "RunesTypeDescriptor *, RunesGcVisit, void *);\n"
        "extern void *runes_promote_dynamic(const void *, const "
        "RunesTypeDescriptor *, unsigned, unsigned);\n"
        "extern void *runes_promote_gc(const void *, const "
        "RunesTypeDescriptor *, unsigned, unsigned);\n"
        "extern void *runes_clone_pointer(RunesCloneContext *, const void *, "
        "const RunesTypeDescriptor *);\n"
        "extern void *runes_clone_slice(RunesCloneContext *, const void *, "
        "size_t, const RunesTypeDescriptor *);\n"
        "extern void runes_clone_value(RunesCloneContext *, void *, const "
        "void *, const RunesTypeDescriptor *);\n"
        "extern RunesStr runes_clone_string(RunesCloneContext *, RunesStr);\n"
        "extern RunesStr runes_str_from_c(const char *);\n"
        "extern bool runes_str_equal(RunesStr, RunesStr);\n"
        "extern int runes_str_compare(RunesStr, RunesStr);\n"
        "extern RunesStr runes_str_concat(RunesStr, RunesStr, unsigned, "
        "unsigned);\n"
        "extern RunesStr runes_str_slice_bytes(RunesStr, size_t, size_t, "
        "bool, unsigned, unsigned);\n"
        "extern uint8_t runes_str_byte_at(RunesStr, size_t, unsigned, "
        "unsigned);\n"
        "extern uint32_t runes_char_from_u64(uint64_t, unsigned, unsigned);\n"
        "extern void runes_str_write_stdout(RunesStr);\n"
        "extern void runes_char_write_stdout(uint32_t, unsigned, unsigned);\n\n",
        cg->out);
  fputs("static inline RUNES_MAYBE_UNUSED size_t runes_checked_index(size_t index, size_t length, "
        "unsigned line, unsigned column) {\n"
        "  if (index >= length) {\n"
        "    fprintf(stderr, \"Runes bounds error at %u:%u: index %zu, length "
        "%zu\\n\", line, column, index, length);\n"
        "    abort();\n"
        "  }\n"
        "  return index;\n"
        "}\n\n",
        cg->out);
  fputs("static inline RUNES_MAYBE_UNUSED void runes_arithmetic_fail(const char *operation, "
        "unsigned line, unsigned column) {\n"
        "  fprintf(stderr, \"Runes arithmetic error at %u:%u: %s\\n\", "
        "line, column, operation);\n"
        "  abort();\n"
        "}\n"
        "static inline RUNES_MAYBE_UNUSED unsigned runes_checked_shift_count_signed(int64_t "
        "count, unsigned width, unsigned line, unsigned column) {\n"
        "  if (count < 0 || (uint64_t)count >= width) "
        "runes_arithmetic_fail(\"invalid shift count\", line, column);\n"
        "  return (unsigned)count;\n"
        "}\n"
        "static inline RUNES_MAYBE_UNUSED unsigned runes_checked_shift_count_unsigned(uint64_t "
        "count, unsigned width, unsigned line, unsigned column) {\n"
        "  if (count >= width) runes_arithmetic_fail(\"invalid shift count\", "
        "line, column);\n"
        "  return (unsigned)count;\n"
        "}\n"
        "#define RUNES_CHECKED_COMMON(type, suffix) \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_add_##suffix(type a, type b, "
        "unsigned l, unsigned c) { type r; if (__builtin_add_overflow(a, b, "
        "&r)) runes_arithmetic_fail(\"overflow in addition\", l, c); return "
        "r; } \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_sub_##suffix(type a, type b, "
        "unsigned l, unsigned c) { type r; if (__builtin_sub_overflow(a, b, "
        "&r)) runes_arithmetic_fail(\"overflow in subtraction\", l, c); "
        "return r; } \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_mul_##suffix(type a, type b, "
        "unsigned l, unsigned c) { type r; if (__builtin_mul_overflow(a, b, "
        "&r)) runes_arithmetic_fail(\"overflow in multiplication\", l, c); "
        "return r; } \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_neg_##suffix(type a, unsigned l, "
        "unsigned c) { type r; if (__builtin_sub_overflow((type)0, a, &r)) "
        "runes_arithmetic_fail(\"overflow in negation\", l, c); return r; } "
        "\\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_shl_##suffix(type a, unsigned b, "
        "unsigned l, unsigned c) { type r = a; for (unsigned i = 0; i < b; "
        "i++) { if (__builtin_mul_overflow(r, (type)2, &r)) "
        "runes_arithmetic_fail(\"overflow in left shift\", l, c); } return r; "
        "}\n"
        "#define RUNES_CHECKED_SIGNED(type, suffix, minimum) \\\n"
        "RUNES_CHECKED_COMMON(type, suffix) \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_div_##suffix(type a, type b, "
        "unsigned l, unsigned c) { if (b == 0) "
        "runes_arithmetic_fail(\"division by zero\", l, c); if (a == "
        "(type)(minimum) && b == (type)-1) "
        "runes_arithmetic_fail(\"overflow in division\", l, c); return a / "
        "b; } \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_rem_##suffix(type a, type b, "
        "unsigned l, unsigned c) { if (b == 0) "
        "runes_arithmetic_fail(\"remainder by zero\", l, c); if (a == "
        "(type)(minimum) && b == (type)-1) return 0; return a % b; } \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_shr_##suffix(type a, unsigned b, "
        "unsigned l, unsigned c) { (void)l; (void)c; for (unsigned i = 0; "
        "i < b; i++) { type q = a / (type)2; type r = a % (type)2; a = q - "
        "(r != 0 && a < 0); } return a; }\n"
        "#define RUNES_CHECKED_UNSIGNED(type, suffix) \\\n"
        "RUNES_CHECKED_COMMON(type, suffix) \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_div_##suffix(type a, type b, "
        "unsigned l, unsigned c) { if (b == 0) "
        "runes_arithmetic_fail(\"division by zero\", l, c); return a / b; } "
        "\\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_rem_##suffix(type a, type b, "
        "unsigned l, unsigned c) { if (b == 0) "
        "runes_arithmetic_fail(\"remainder by zero\", l, c); return a % b; "
        "} \\\n"
        "static inline RUNES_MAYBE_UNUSED type runes_checked_shr_##suffix(type a, unsigned b, "
        "unsigned l, unsigned c) { (void)l; (void)c; return (type)(a >> b); "
        "}\n"
        "RUNES_CHECKED_SIGNED(int8_t, i8, INT8_MIN)\n"
        "RUNES_CHECKED_SIGNED(int16_t, i16, INT16_MIN)\n"
        "RUNES_CHECKED_SIGNED(int32_t, i32, INT32_MIN)\n"
        "RUNES_CHECKED_SIGNED(int64_t, i64, INT64_MIN)\n"
        "RUNES_CHECKED_UNSIGNED(uint8_t, u8)\n"
        "RUNES_CHECKED_UNSIGNED(uint16_t, u16)\n"
        "RUNES_CHECKED_UNSIGNED(uint32_t, u32)\n"
        "RUNES_CHECKED_UNSIGNED(uint64_t, u64)\n"
        "RUNES_CHECKED_UNSIGNED(size_t, usize)\n\n",
        cg->out);
  fputs("static inline RUNES_MAYBE_UNUSED const void *runes_unwrap_ptr(const void *pointer, unsigned line, "
        "unsigned column) {\n"
        "  if (!pointer) {\n"
        "    fprintf(stderr, \"Runes null error at %u:%u: attempted to "
        "unwrap a null pointer\\n\", line, column);\n"
        "    abort();\n"
        "  }\n"
        "  return pointer;\n"
        "}\n\n",
        cg->out);
  AstNode *declarations = program->as.program.declarations;
  if (!register_codegen_decls(cg, declarations, NULL)) {
    cg_error(cg, program, "too many declarations for C name registry");
    return false;
  }
  if (!validate_codegen_declarations(cg, declarations))
    return false;

  fputs("typedef uint32_t RunesError;\n", cg->out);
  fputs("enum {\n  RUNES_ERROR_NONE = 0", cg->out);
  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_ERRORS))
    return false;
  fputs("\n};\n\n", cg->out);

  if (!emit_declaration_phase(cg, declarations, NULL, EMIT_TYPEDEFS))
    return false;
  fputc('\n', cg->out);
  if (!emit_named_type_definitions(cg, declarations))
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
  if (!emit_closure_invoke_definitions(cg, program))
    return false;
  if (!emit_closure_environment_definitions(cg))
    return false;
  cg->descriptor_phase = true;
  if (!emit_ast_tuple_dependencies(cg, declarations))
    return false;
  for (int i = 0; i < cg->interface_impl_count; i++) {
    AstNode *implementation = cg->interface_impls[i];
    AstNode *method = implementation->as.method_decl.methods;
    Type *function = method ? method->resolved_type : NULL;
    Type *concrete = function && function->kind == TY_FUNCTION &&
                             function->as.function.param_count > 0
                         ? function->as.function.params[0]
                         : NULL;
    if (concrete && concrete->kind == TY_POINTER)
      concrete = concrete->as.pointer.inner;
    if (concrete && !emit_type_descriptor(cg, concrete, implementation))
      return false;
  }
  if (!emit_move_closure_environment_descriptors(cg))
    return false;
  cg->descriptor_phase = false;
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
