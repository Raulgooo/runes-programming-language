#include "monomorphize.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct GenericTemplate {
  AstNode *node;
  const char *module;
  struct GenericTemplate *next;
} GenericTemplate;

typedef struct Binding {
  const char *name;
  AstNode *type;
  struct Binding *next;
} Binding;

typedef struct GenericInstance {
  AstNode *template;
  const char *name;
  AstNode *declaration;
  struct GenericInstance *next;
} GenericInstance;

typedef struct LocalType {
  const char *name;
  AstNode *type;
  struct LocalType *next;
} LocalType;

typedef struct GenericMethodPlan {
  AstNode *call;
  AstNode *method_block;
  AstNode *method;
  const char *module;
  AstNode *owner_type;
  AstNode *method_arguments;
  const char *name;
  bool emitted;
  struct GenericMethodPlan *next;
} GenericMethodPlan;

typedef struct GenericMethodInstance {
  AstNode *method;
  const char *name;
  struct GenericMethodInstance *next;
} GenericMethodInstance;

typedef struct {
  Arena *arena;
  AstNode *program;
  GenericTemplate *templates;
  GenericInstance *instances;
  GenericMethodPlan *method_plans;
  GenericMethodInstance *method_instances;
  AstNode *generated_head;
  AstNode *generated_tail;
  const char *current_module;
  bool had_error;
  int error_count;
} Monomorphizer;

static AstNode *clone_node(Monomorphizer *mono, AstNode *node,
                           Binding *bindings);
static AstNode *alloc_node(Monomorphizer *mono, AstNode *source);
static bool is_template_declaration(AstNode *node);

static AstNode *copy_inferred_type(Monomorphizer *mono, AstNode *type) {
  if (!type || type->kind != AST_TYPE_EXPR)
    return NULL;
  AstNode *copy = alloc_node(mono, type);
  copy->as.type_expr.inner = copy_inferred_type(mono, type->as.type_expr.inner);
  copy->as.type_expr.size = type->as.type_expr.size;
  AstNode **tail = &copy->as.type_expr.elems;
  *tail = NULL;
  for (AstNode *element = type->as.type_expr.elems; element;
       element = element->next) {
    *tail = copy_inferred_type(mono, element);
    tail = &(*tail)->next;
  }
  tail = &copy->as.type_expr.type_args;
  *tail = NULL;
  for (AstNode *argument = type->as.type_expr.type_args; argument;
       argument = argument->next) {
    *tail = copy_inferred_type(mono, argument);
    tail = &(*tail)->next;
  }
  return copy;
}

static void mono_error(Monomorphizer *mono, const AstNode *node,
                       const char *format, ...) {
  fprintf(stderr, "[Generic Error] %u:%u: ", node ? node->line : 0,
          node ? node->col : 0);
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputc('\n', stderr);
  mono->had_error = true;
  mono->error_count++;
}

static AstNode *alloc_node(Monomorphizer *mono, AstNode *source) {
  AstNode *node = arena_alloc(mono->arena, sizeof(*node));
  *node = *source;
  node->next = NULL;
  node->resolved_type = NULL;
  node->resolved_decl = NULL;
  node->memory_provenance = 0;
  node->enclosing_function = NULL;
  return node;
}

static AstNode *clone_list(Monomorphizer *mono, AstNode *node,
                           Binding *bindings) {
  AstNode *head = NULL;
  AstNode **tail = &head;
  for (; node; node = node->next) {
    AstNode *copy = clone_node(mono, node, bindings);
    if (!copy)
      continue;
    *tail = copy;
    tail = &copy->next;
  }
  return head;
}

static AstNode *clone_concrete_declarations(Monomorphizer *mono,
                                            AstNode *node,
                                            Binding *bindings) {
  AstNode *head = NULL;
  AstNode **tail = &head;
  for (; node; node = node->next) {
    if (is_template_declaration(node))
      continue;
    *tail = clone_node(mono, node, bindings);
    tail = &(*tail)->next;
  }
  return head;
}

static Attr *clone_attrs(Monomorphizer *mono, Attr *attr, Binding *bindings) {
  Attr *head = NULL;
  Attr **tail = &head;
  for (; attr; attr = attr->next) {
    Attr *copy = arena_alloc(mono->arena, sizeof(*copy));
    copy->name = attr->name;
    copy->arg = clone_node(mono, attr->arg, bindings);
    copy->next = NULL;
    *tail = copy;
    tail = &copy->next;
  }
  return head;
}

static Binding *find_binding(Binding *binding, const char *name) {
  for (; binding; binding = binding->next)
    if (strcmp(binding->name, name) == 0)
      return binding;
  return NULL;
}

static AstNode *generic_params(AstNode *node) {
  if (!node)
    return NULL;
  if (node->kind == AST_FUNC_DECL)
    return node->as.func_decl.generic_params;
  if (node->kind == AST_TYPE_DECL)
    return node->as.type_decl.generic_params;
  if (node->kind == AST_VARIANT_DECL)
    return node->as.variant_decl.generic_params;
  return NULL;
}

static const char *declaration_name(AstNode *node) {
  if (node->kind == AST_FUNC_DECL)
    return node->as.func_decl.name;
  if (node->kind == AST_TYPE_DECL)
    return node->as.type_decl.name;
  if (node->kind == AST_VARIANT_DECL)
    return node->as.variant_decl.name;
  return NULL;
}

static bool generic_declaration_is_public(AstNode *node) {
  if (node->kind == AST_FUNC_DECL)
    return node->as.func_decl.is_pub;
  if (node->kind == AST_TYPE_DECL)
    return node->as.type_decl.is_pub;
  if (node->kind == AST_VARIANT_DECL)
    return node->as.variant_decl.is_pub;
  return false;
}

static GenericTemplate *find_template(Monomorphizer *mono, const char *name,
                                      bool functions, bool types) {
  for (GenericTemplate *entry = mono->templates; entry; entry = entry->next) {
    AstNode *node = entry->node;
    bool visible_here =
        (!entry->module && !mono->current_module) ||
        (entry->module && mono->current_module &&
         strcmp(entry->module, mono->current_module) == 0);
    if (((functions && node->kind == AST_FUNC_DECL) ||
         (types && (node->kind == AST_TYPE_DECL ||
                    node->kind == AST_VARIANT_DECL))) &&
        visible_here && strcmp(declaration_name(node), name) == 0)
      return entry;
  }
  return NULL;
}

static GenericTemplate *find_qualified_template(Monomorphizer *mono,
                                                const char *module,
                                                const char *name,
                                                bool functions, bool types) {
  for (GenericTemplate *entry = mono->templates; entry; entry = entry->next) {
    AstNode *node = entry->node;
    if (entry->module && strcmp(entry->module, module) == 0 &&
        ((functions && node->kind == AST_FUNC_DECL) ||
         (types && (node->kind == AST_TYPE_DECL ||
                    node->kind == AST_VARIANT_DECL))) &&
        strcmp(declaration_name(node), name) == 0)
      return entry;
  }
  return NULL;
}

static GenericTemplate *template_for_node(Monomorphizer *mono,
                                          AstNode *template) {
  for (GenericTemplate *entry = mono->templates; entry; entry = entry->next)
    if (entry->node == template)
      return entry;
  return NULL;
}

static AstNode *find_module_declarations(AstNode *declarations,
                                         const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (node->kind != AST_MOD_DECL)
      continue;
    if (strcmp(node->as.mod_decl.name, module) == 0)
      return node->as.mod_decl.declarations;
    AstNode *nested =
        find_module_declarations(node->as.mod_decl.declarations, module);
    if (nested)
      return nested;
  }
  return NULL;
}

static AstNode *module_declarations(Monomorphizer *mono, const char *module) {
  if (!module)
    return mono->program->as.program.declarations;
  return find_module_declarations(mono->program->as.program.declarations,
                                  module);
}

static bool append_text(char *buffer, size_t capacity, size_t *used,
                        const char *format, ...) {
  if (*used >= capacity)
    return false;
  va_list args;
  va_start(args, format);
  int written = vsnprintf(buffer + *used, capacity - *used, format, args);
  va_end(args);
  if (written < 0 || (size_t)written >= capacity - *used)
    return false;
  *used += (size_t)written;
  return true;
}

static bool encode_type(AstNode *type, char *buffer, size_t capacity,
                        size_t *used) {
  if (!type || type->kind != AST_TYPE_EXPR)
    return false;
  switch (type->as.type_expr.kind) {
  case TYPE_NAMED:
    if (!append_text(buffer, capacity, used, "n%zu_%s",
                     strlen(type->as.type_expr.name),
                     type->as.type_expr.name))
      return false;
    break;
  case TYPE_QUALIFIED:
    if (!append_text(buffer, capacity, used, "q%zu_%s%zu_%s",
                     strlen(type->as.type_expr.module),
                     type->as.type_expr.module,
                     strlen(type->as.type_expr.name), type->as.type_expr.name))
      return false;
    break;
  case TYPE_PTR:
    if (!append_text(buffer, capacity, used,
                     type->as.type_expr.nullable ? "o" : "p") ||
        !encode_type(type->as.type_expr.inner, buffer, capacity, used))
      return false;
    break;
  case TYPE_ARRAY:
    if (!type->as.type_expr.size ||
        type->as.type_expr.size->kind != AST_INT_LITERAL ||
        !append_text(buffer, capacity, used, "a%llu_",
                     type->as.type_expr.size->as.int_literal.value) ||
        !encode_type(type->as.type_expr.inner, buffer, capacity, used))
      return false;
    break;
  case TYPE_SLICE:
    if (!append_text(buffer, capacity, used,
                     type->as.type_expr.readonly ? "r" : "s") ||
        !encode_type(type->as.type_expr.inner, buffer, capacity, used))
      return false;
    break;
  case TYPE_FALLIBLE:
    if (!append_text(buffer, capacity, used, "f") ||
        (type->as.type_expr.inner &&
         !encode_type(type->as.type_expr.inner, buffer, capacity, used)))
      return false;
    break;
  case TYPE_TUPLE: {
    int count = 0;
    for (AstNode *element = type->as.type_expr.elems; element;
         element = element->next)
      count++;
    if (!append_text(buffer, capacity, used, "t%d_", count))
      return false;
    for (AstNode *element = type->as.type_expr.elems; element;
         element = element->next)
      if (!encode_type(element, buffer, capacity, used))
        return false;
    break;
  }
  case TYPE_FUNCTION: {
    int count = 0;
    for (AstNode *parameter = type->as.type_expr.elems; parameter;
         parameter = parameter->next)
      count++;
    if (!append_text(buffer, capacity, used, "c%d_%d_", count,
                     (int)type->as.type_expr.realm))
      return false;
    for (AstNode *parameter = type->as.type_expr.elems; parameter;
         parameter = parameter->next)
      if (!encode_type(parameter, buffer, capacity, used))
        return false;
    if (!append_text(buffer, capacity, used, "r") ||
        !encode_type(type->as.type_expr.inner, buffer, capacity, used))
      return false;
    break;
  }
  }
  if (type->as.type_expr.type_args) {
    if (!append_text(buffer, capacity, used, "g"))
      return false;
    for (AstNode *argument = type->as.type_expr.type_args; argument;
         argument = argument->next)
      if (!encode_type(argument, buffer, capacity, used))
        return false;
    if (!append_text(buffer, capacity, used, "e"))
      return false;
  }
  return true;
}

static const char *specialization_name(Monomorphizer *mono, AstNode *template,
                                       AstNode *arguments) {
  char buffer[4096];
  size_t used = 0;
  const char *base = declaration_name(template);
  GenericTemplate *entry = template_for_node(mono, template);
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_gen_"))
    return NULL;
  if (entry && entry->module &&
      !append_text(buffer, sizeof(buffer), &used, "%zu_%s_",
                   strlen(entry->module), entry->module))
    return NULL;
  if (!append_text(buffer, sizeof(buffer), &used, "%zu_%s__", strlen(base),
                   base))
    return NULL;
  for (AstNode *argument = arguments; argument; argument = argument->next)
    if (!encode_type(argument, buffer, sizeof(buffer), &used))
      return NULL;
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  return name;
}

static int list_count(AstNode *node) {
  int count = 0;
  for (; node; node = node->next)
    count++;
  return count;
}

static AstNode *local_type(LocalType *locals, const char *name) {
  for (; locals; locals = locals->next)
    if (strcmp(locals->name, name) == 0)
      return locals->type;
  return NULL;
}

static bool is_generic_parameter(AstNode *parameters, const char *name) {
  for (; parameters; parameters = parameters->next)
    if (strcmp(parameters->as.param.name, name) == 0)
      return true;
  return false;
}

static bool same_inferred_type(AstNode *left, AstNode *right) {
  char left_name[4096], right_name[4096];
  size_t left_used = 0, right_used = 0;
  return encode_type(left, left_name, sizeof(left_name), &left_used) &&
         encode_type(right, right_name, sizeof(right_name), &right_used) &&
         left_used == right_used &&
         memcmp(left_name, right_name, left_used) == 0;
}

static bool infer_unify(Monomorphizer *mono, AstNode *parameters,
                        AstNode *formal, AstNode *actual, Binding **bindings,
                        const AstNode *site) {
  if (!formal || !actual || formal->kind != AST_TYPE_EXPR ||
      actual->kind != AST_TYPE_EXPR)
    return false;
  if (formal->as.type_expr.kind == TYPE_NAMED &&
      !formal->as.type_expr.type_args &&
      is_generic_parameter(parameters, formal->as.type_expr.name)) {
    Binding *existing = find_binding(*bindings, formal->as.type_expr.name);
    if (existing) {
      if (same_inferred_type(existing->type, actual))
        return true;
      mono_error(mono, site, "conflicting inferred types for '%s'",
                 formal->as.type_expr.name);
      return false;
    }
    Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
    binding->name = formal->as.type_expr.name;
    binding->type = actual;
    binding->next = *bindings;
    *bindings = binding;
    return true;
  }
  if (formal->as.type_expr.kind == TYPE_SLICE &&
      actual->as.type_expr.kind == TYPE_ARRAY)
    return infer_unify(mono, parameters, formal->as.type_expr.inner,
                       actual->as.type_expr.inner, bindings, site);
  if (formal->as.type_expr.kind != actual->as.type_expr.kind)
    return false;
  switch (formal->as.type_expr.kind) {
  case TYPE_NAMED:
    if (strcmp(formal->as.type_expr.name, actual->as.type_expr.name) != 0)
      return false;
    break;
  case TYPE_QUALIFIED:
    if (strcmp(formal->as.type_expr.module, actual->as.type_expr.module) != 0 ||
        strcmp(formal->as.type_expr.name, actual->as.type_expr.name) != 0)
      return false;
    break;
  case TYPE_PTR:
    if (formal->as.type_expr.nullable != actual->as.type_expr.nullable)
      return false;
    return infer_unify(mono, parameters, formal->as.type_expr.inner,
                       actual->as.type_expr.inner, bindings, site);
  case TYPE_SLICE:
    if (formal->as.type_expr.readonly != actual->as.type_expr.readonly)
      return false;
    return infer_unify(mono, parameters, formal->as.type_expr.inner,
                       actual->as.type_expr.inner, bindings, site);
  case TYPE_ARRAY:
    if (!formal->as.type_expr.size || !actual->as.type_expr.size ||
        formal->as.type_expr.size->kind != AST_INT_LITERAL ||
        actual->as.type_expr.size->kind != AST_INT_LITERAL ||
        formal->as.type_expr.size->as.int_literal.value !=
            actual->as.type_expr.size->as.int_literal.value)
      return false;
    return infer_unify(mono, parameters, formal->as.type_expr.inner,
                       actual->as.type_expr.inner, bindings, site);
  case TYPE_FALLIBLE:
    if (!formal->as.type_expr.inner || !actual->as.type_expr.inner)
      return formal->as.type_expr.inner == actual->as.type_expr.inner;
    return infer_unify(mono, parameters, formal->as.type_expr.inner,
                       actual->as.type_expr.inner, bindings, site);
  case TYPE_TUPLE:
    break;
  case TYPE_FUNCTION:
    if (formal->as.type_expr.realm != actual->as.type_expr.realm ||
        !infer_unify(mono, parameters, formal->as.type_expr.inner,
                     actual->as.type_expr.inner, bindings, site))
      return false;
    break;
  }
  AstNode *formal_element = formal->as.type_expr.elems;
  AstNode *actual_element = actual->as.type_expr.elems;
  while (formal_element && actual_element) {
    if (!infer_unify(mono, parameters, formal_element, actual_element,
                     bindings, site))
      return false;
    formal_element = formal_element->next;
    actual_element = actual_element->next;
  }
  if (formal_element || actual_element)
    return false;
  AstNode *formal_argument = formal->as.type_expr.type_args;
  AstNode *actual_argument = actual->as.type_expr.type_args;
  while (formal_argument && actual_argument) {
    if (!infer_unify(mono, parameters, formal_argument, actual_argument,
                     bindings, site))
      return false;
    formal_argument = formal_argument->next;
    actual_argument = actual_argument->next;
  }
  return !formal_argument && !actual_argument;
}

static AstNode *substitute_inferred_type(Monomorphizer *mono, AstNode *type,
                                         Binding *bindings) {
  if (!type)
    return NULL;
  if (type->as.type_expr.kind == TYPE_NAMED &&
      !type->as.type_expr.type_args) {
    Binding *binding = find_binding(bindings, type->as.type_expr.name);
    if (binding)
      return copy_inferred_type(mono, binding->type);
  }
  AstNode *copy = copy_inferred_type(mono, type);
  copy->as.type_expr.inner =
      substitute_inferred_type(mono, type->as.type_expr.inner, bindings);
  AstNode **tail = &copy->as.type_expr.elems;
  *tail = NULL;
  for (AstNode *element = type->as.type_expr.elems; element;
       element = element->next) {
    *tail = substitute_inferred_type(mono, element, bindings);
    tail = &(*tail)->next;
  }
  tail = &copy->as.type_expr.type_args;
  *tail = NULL;
  for (AstNode *argument = type->as.type_expr.type_args; argument;
       argument = argument->next) {
    *tail = substitute_inferred_type(mono, argument, bindings);
    tail = &(*tail)->next;
  }
  return copy;
}

static AstNode *inferred_primitive(Monomorphizer *mono, const AstNode *site,
                                   const char *name) {
  AstNode *type = ast_new_type_named(mono->arena, name);
  type->line = site->line;
  type->col = site->col;
  return type;
}

static AstNode *infer_expression_type(Monomorphizer *mono, AstNode *expression,
                                      LocalType *locals) {
  if (!expression)
    return NULL;
  switch (expression->kind) {
  case AST_IDENTIFIER:
    return local_type(locals, expression->as.identifier.name);
  case AST_INT_LITERAL:
    return inferred_primitive(mono, expression, "i32");
  case AST_FLOAT_LITERAL:
    return inferred_primitive(mono, expression, "f64");
  case AST_STRING_LITERAL:
    return inferred_primitive(mono, expression, "str");
  case AST_BOOL_LITERAL:
    return inferred_primitive(mono, expression, "bool");
  case AST_CHAR_LITERAL:
    return inferred_primitive(mono, expression, "char");
  case AST_CAST_EXPR:
    return expression->as.cast.target_type;
  case AST_NAMED_ARG:
    return infer_expression_type(mono, expression->as.named_arg.value, locals);
  case AST_UNARY_EXPR:
    return infer_expression_type(mono, expression->as.unary.expr, locals);
  case AST_FIELD_EXPR: {
    AstNode *target_type =
        infer_expression_type(mono, expression->as.field.target, locals);
    while (target_type && target_type->kind == AST_TYPE_EXPR &&
           target_type->as.type_expr.kind == TYPE_PTR)
      target_type = target_type->as.type_expr.inner;
    if (!target_type || target_type->kind != AST_TYPE_EXPR)
      return NULL;
    GenericTemplate *template = NULL;
    if (target_type->as.type_expr.kind == TYPE_QUALIFIED)
      template = find_qualified_template(
          mono, target_type->as.type_expr.module,
          target_type->as.type_expr.name, false, true);
    else if (target_type->as.type_expr.kind == TYPE_NAMED)
      template = find_template(mono, target_type->as.type_expr.name, false,
                               true);
    if (template && template->node->kind == AST_TYPE_DECL) {
      Binding *bindings = NULL;
      AstNode *parameter = generic_params(template->node);
      AstNode *argument = target_type->as.type_expr.type_args;
      for (; parameter && argument;
           parameter = parameter->next, argument = argument->next) {
        Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
        binding->name = parameter->as.param.name;
        binding->type = argument;
        binding->next = bindings;
        bindings = binding;
      }
      for (AstNode *field = template->node->as.type_decl.fields; field;
           field = field->next)
        if (strcmp(field->as.field_decl.name,
                   expression->as.field.field) == 0)
          return substitute_inferred_type(mono, field->as.field_decl.type,
                                          bindings);
    }
    return NULL;
  }
  case AST_CALL_EXPR: {
    AstNode *callee = expression->as.call.callee;
    if (callee->kind == AST_FIELD_EXPR) {
      for (GenericMethodPlan *plan = mono->method_plans; plan;
           plan = plan->next) {
        if (plan->call != expression)
          continue;
        Binding *bindings = NULL;
        GenericTemplate *owner_template =
            plan->owner_type->as.type_expr.kind == TYPE_QUALIFIED
                ? find_qualified_template(
                      mono, plan->owner_type->as.type_expr.module,
                      plan->owner_type->as.type_expr.name, false, true)
                : find_template(mono, plan->owner_type->as.type_expr.name,
                                false, true);
        if (owner_template) {
          AstNode *parameter = generic_params(owner_template->node);
          AstNode *argument = plan->owner_type->as.type_expr.type_args;
          for (; parameter && argument;
               parameter = parameter->next, argument = argument->next) {
            Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
            binding->name = parameter->as.param.name;
            binding->type = argument;
            binding->next = bindings;
            bindings = binding;
          }
        }
        AstNode *parameter = plan->method->as.func_decl.generic_params;
        AstNode *argument = plan->method_arguments;
        for (; parameter && argument;
             parameter = parameter->next, argument = argument->next) {
          Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
          binding->name = parameter->as.param.name;
          binding->type = argument;
          binding->next = bindings;
          bindings = binding;
        }
        return substitute_inferred_type(
            mono, plan->method->as.func_decl.ret_type, bindings);
      }
      return NULL;
    }
    if (callee->kind != AST_IDENTIFIER)
      return NULL;
    GenericTemplate *template =
        find_template(mono, callee->as.identifier.name, true, false);
    if (template && expression->as.call.type_args) {
      Binding *bindings = NULL;
      AstNode *parameter = generic_params(template->node);
      AstNode *argument = expression->as.call.type_args;
      for (; parameter && argument;
           parameter = parameter->next, argument = argument->next) {
        Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
        binding->name = parameter->as.param.name;
        binding->type = argument;
        binding->next = bindings;
        bindings = binding;
      }
      return substitute_inferred_type(
          mono, template->node->as.func_decl.ret_type, bindings);
    }
    for (AstNode *declaration = mono->program->as.program.declarations;
         declaration; declaration = declaration->next)
      if (declaration->kind == AST_FUNC_DECL &&
          strcmp(declaration->as.func_decl.name,
                 callee->as.identifier.name) == 0)
        return declaration->as.func_decl.ret_type;
    return NULL;
  }
  default:
    return NULL;
  }
}

static void infer_calls_in_node(Monomorphizer *mono, AstNode *node,
                                LocalType **locals);

static void infer_calls_in_list(Monomorphizer *mono, AstNode *node,
                                LocalType **locals) {
  for (; node; node = node->next)
    infer_calls_in_node(mono, node, locals);
}

static void infer_generic_call(Monomorphizer *mono, AstNode *call,
                               LocalType *locals) {
  if (call->as.call.type_args)
    return;
  GenericTemplate *template = NULL;
  if (call->as.call.callee->kind == AST_IDENTIFIER) {
    template = find_template(
        mono, call->as.call.callee->as.identifier.name, true, false);
  } else if (call->as.call.callee->kind == AST_FIELD_EXPR &&
             call->as.call.callee->as.field.target->kind == AST_IDENTIFIER) {
    template = find_qualified_template(
        mono,
        call->as.call.callee->as.field.target->as.identifier.name,
        call->as.call.callee->as.field.field, true, false);
    if (template && !generic_declaration_is_public(template->node)) {
      mono_error(mono, call, "generic function '%s' is private",
                 call->as.call.callee->as.field.field);
      return;
    }
  }
  if (!template)
    return;
  Binding *bindings = NULL;
  AstNode *formal = template->node->as.func_decl.params;
  AstNode *actual = call->as.call.args;
  for (; formal && actual; formal = formal->next, actual = actual->next) {
    AstNode *actual_value = actual->kind == AST_NAMED_ARG
                                ? actual->as.named_arg.value
                                : actual;
    AstNode *actual_type = infer_expression_type(mono, actual_value, locals);
    if (actual_type)
      infer_unify(mono, generic_params(template->node), formal->as.param.type,
                  actual_type, &bindings, call);
  }
  AstNode **tail = &call->as.call.type_args;
  for (AstNode *parameter = generic_params(template->node); parameter;
       parameter = parameter->next) {
    Binding *binding = find_binding(bindings, parameter->as.param.name);
    if (!binding) {
      mono_error(mono, call, "cannot infer type argument '%s' for generic '%s'; "
                             "provide explicit type arguments",
                 parameter->as.param.name, declaration_name(template->node));
      call->as.call.type_args = NULL;
      return;
    }
    *tail = copy_inferred_type(mono, binding->type);
    tail = &(*tail)->next;
  }
}

static const char *method_specialization_name(Monomorphizer *mono,
                                              AstNode *method,
                                              AstNode *owner_type,
                                              AstNode *arguments) {
  char buffer[4096];
  size_t used = 0;
  const char *base = method->as.func_decl.name;
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_method_%zu_%s__",
                   strlen(base), base) ||
      !encode_type(owner_type, buffer, sizeof(buffer), &used) ||
      !append_text(buffer, sizeof(buffer), &used, "__"))
    return NULL;
  for (AstNode *argument = arguments; argument; argument = argument->next)
    if (!encode_type(argument, buffer, sizeof(buffer), &used))
      return NULL;
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  return name;
}

static void infer_generic_method_call(Monomorphizer *mono, AstNode *call,
                                      LocalType *locals) {
  AstNode *callee = call->as.call.callee;
  if (callee->kind != AST_FIELD_EXPR)
    return;
  AstNode *owner_type =
      infer_expression_type(mono, callee->as.field.target, locals);
  while (owner_type && owner_type->kind == AST_TYPE_EXPR &&
         owner_type->as.type_expr.kind == TYPE_PTR)
    owner_type = owner_type->as.type_expr.inner;
  if (!owner_type || owner_type->kind != AST_TYPE_EXPR ||
      (owner_type->as.type_expr.kind != TYPE_NAMED &&
       owner_type->as.type_expr.kind != TYPE_QUALIFIED))
    return;

  AstNode *method_block = NULL;
  AstNode *method = NULL;
  const char *module = owner_type->as.type_expr.kind == TYPE_QUALIFIED
                           ? owner_type->as.type_expr.module
                           : mono->current_module;
  for (AstNode *declaration = module_declarations(mono, module);
       declaration; declaration = declaration->next) {
    if (declaration->kind != AST_METHOD_DECL ||
        strcmp(declaration->as.method_decl.type_name,
               owner_type->as.type_expr.name) != 0)
      continue;
    for (AstNode *candidate = declaration->as.method_decl.methods; candidate;
         candidate = candidate->next) {
      if (strcmp(candidate->as.func_decl.name, callee->as.field.field) == 0 &&
          candidate->as.func_decl.generic_params) {
        if (method) {
          mono_error(mono, call, "generic method '%s' is ambiguous",
                     callee->as.field.field);
          return;
        }
        method_block = declaration;
        method = candidate;
      }
    }
  }
  if (!method)
    return;

  AstNode *arguments = call->as.call.type_args;
  if (!arguments) {
    Binding *bindings = NULL;
    AstNode *formal = method->as.func_decl.params;
    if (formal && strcmp(formal->as.param.name, "self") == 0)
      formal = formal->next;
    AstNode *actual = call->as.call.args;
    for (; formal && actual; formal = formal->next, actual = actual->next) {
      AstNode *actual_value = actual->kind == AST_NAMED_ARG
                                  ? actual->as.named_arg.value
                                  : actual;
      AstNode *actual_type =
          infer_expression_type(mono, actual_value, locals);
      if (actual_type)
        infer_unify(mono, method->as.func_decl.generic_params,
                    formal->as.param.type, actual_type, &bindings, call);
    }
    AstNode **tail = &arguments;
    for (AstNode *parameter = method->as.func_decl.generic_params; parameter;
         parameter = parameter->next) {
      Binding *binding = find_binding(bindings, parameter->as.param.name);
      if (!binding) {
        mono_error(mono, call,
                   "cannot infer type argument '%s' for generic method '%s'; "
                   "provide explicit type arguments",
                   parameter->as.param.name, method->as.func_decl.name);
        return;
      }
      *tail = copy_inferred_type(mono, binding->type);
      tail = &(*tail)->next;
    }
  }

  const char *name =
      method_specialization_name(mono, method, owner_type, arguments);
  if (!name) {
    mono_error(mono, call, "generic method specialization is too complex");
    return;
  }
  GenericMethodPlan *plan = arena_alloc(mono->arena, sizeof(*plan));
  plan->call = call;
  plan->method_block = method_block;
  plan->method = method;
  plan->module = module;
  plan->owner_type = copy_inferred_type(mono, owner_type);
  plan->method_arguments = arguments;
  plan->name = name;
  plan->emitted = false;
  plan->next = mono->method_plans;
  mono->method_plans = plan;
}

static void infer_generic_constructor_call(Monomorphizer *mono, AstNode *call,
                                           LocalType *locals) {
  if (call->as.call.type_args)
    return;
  AstNode *callee = call->as.call.callee;
  GenericTemplate *template = NULL;
  const char *arm_name = NULL;
  if (callee->kind == AST_IDENTIFIER) {
    template = find_template(mono, callee->as.identifier.name, false, true);
  } else if (callee->kind == AST_FIELD_EXPR &&
             callee->as.field.target->kind == AST_IDENTIFIER) {
    const char *target = callee->as.field.target->as.identifier.name;
    template = find_qualified_template(mono, target, callee->as.field.field,
                                       false, true);
    if (!template) {
      template = find_template(mono, target, false, true);
      arm_name = callee->as.field.field;
    }
  } else if (callee->kind == AST_FIELD_EXPR &&
             callee->as.field.target->kind == AST_FIELD_EXPR &&
             callee->as.field.target->as.field.target->kind ==
                 AST_IDENTIFIER) {
    AstNode *qualified = callee->as.field.target;
    template = find_qualified_template(
        mono, qualified->as.field.target->as.identifier.name,
        qualified->as.field.field, false, true);
    arm_name = callee->as.field.field;
  }
  if (!template)
    return;

  Binding *bindings = NULL;
  AstNode *formal = NULL;
  if (template->node->kind == AST_TYPE_DECL) {
    formal = template->node->as.type_decl.fields;
  } else {
    for (AstNode *arm = template->node->as.variant_decl.arms; arm;
         arm = arm->next)
      if (arm_name && strcmp(arm->as.variant_arm.name, arm_name) == 0) {
        formal = arm->as.variant_arm.fields;
        break;
      }
  }

  AstNode *actual = call->as.call.args;
  if (template->node->kind == AST_TYPE_DECL) {
    for (; actual; actual = actual->next) {
      AstNode *field = formal;
      AstNode *actual_value = actual;
      if (actual->kind == AST_NAMED_ARG) {
        for (; field; field = field->next)
          if (strcmp(field->as.field_decl.name,
                     actual->as.named_arg.name) == 0)
            break;
        actual_value = actual->as.named_arg.value;
      } else if (formal) {
        formal = formal->next;
      }
      if (!field)
        continue;
      AstNode *actual_type =
          infer_expression_type(mono, actual_value, locals);
      if (actual_type)
        infer_unify(mono, generic_params(template->node),
                    field->as.field_decl.type, actual_type, &bindings, call);
    }
  } else {
    for (; formal && actual; formal = formal->next, actual = actual->next) {
      AstNode *actual_value = actual->kind == AST_NAMED_ARG
                                  ? actual->as.named_arg.value
                                  : actual;
      AstNode *actual_type =
          infer_expression_type(mono, actual_value, locals);
      if (actual_type)
        infer_unify(mono, generic_params(template->node), formal, actual_type,
                    &bindings, call);
    }
  }

  AstNode **tail = &call->as.call.type_args;
  for (AstNode *parameter = generic_params(template->node); parameter;
       parameter = parameter->next) {
    Binding *binding = find_binding(bindings, parameter->as.param.name);
    if (!binding) {
      mono_error(mono, call,
                 "cannot infer type argument '%s' for generic type '%s'; "
                 "provide explicit type arguments",
                 parameter->as.param.name, declaration_name(template->node));
      call->as.call.type_args = NULL;
      return;
    }
    *tail = copy_inferred_type(mono, binding->type);
    tail = &(*tail)->next;
  }
}

static void add_local_type(Monomorphizer *mono, LocalType **locals,
                           const char *name, AstNode *type) {
  if (!name || !type)
    return;
  LocalType *local = arena_alloc(mono->arena, sizeof(*local));
  local->name = name;
  local->type = type;
  local->next = *locals;
  *locals = local;
}

static void infer_calls_in_node(Monomorphizer *mono, AstNode *node,
                                LocalType **locals) {
  if (!node)
    return;
  switch (node->kind) {
  case AST_PROGRAM:
    infer_calls_in_list(mono, node->as.program.declarations, locals);
    break;
  case AST_FUNC_DECL: {
    LocalType *saved = *locals;
    for (AstNode *parameter = node->as.func_decl.params; parameter;
         parameter = parameter->next)
      add_local_type(mono, locals, parameter->as.param.name,
                     parameter->as.param.type);
    infer_calls_in_node(mono, node->as.func_decl.body, locals);
    *locals = saved;
    break;
  }
  case AST_METHOD_DECL:
    infer_calls_in_list(mono, node->as.method_decl.methods, locals);
    break;
  case AST_MOD_DECL:
    {
    const char *saved_module = mono->current_module;
    mono->current_module = node->as.mod_decl.name;
    infer_calls_in_list(mono, node->as.mod_decl.declarations, locals);
    mono->current_module = saved_module;
    }
    break;
  case AST_VAR_DECL: {
    infer_calls_in_node(mono, node->as.var_decl.init, locals);
    AstNode *type = node->as.var_decl.type
                        ? node->as.var_decl.type
                        : infer_expression_type(mono, node->as.var_decl.init,
                                                *locals);
    add_local_type(mono, locals, node->as.var_decl.name, type);
    break;
  }
  case AST_BLOCK: {
    LocalType *saved = *locals;
    infer_calls_in_list(mono, node->as.block.statements, locals);
    *locals = saved;
    break;
  }
  case AST_RETURN_STMT:
    infer_calls_in_node(mono, node->as.return_stmt.value, locals);
    break;
  case AST_IF_STMT:
    infer_calls_in_node(mono, node->as.if_stmt.condition, locals);
    infer_calls_in_node(mono, node->as.if_stmt.then_branch, locals);
    infer_calls_in_node(mono, node->as.if_stmt.else_branch, locals);
    break;
  case AST_WHILE_STMT:
    infer_calls_in_node(mono, node->as.while_stmt.condition, locals);
    infer_calls_in_node(mono, node->as.while_stmt.body, locals);
    break;
  case AST_FOR_STMT:
    infer_calls_in_node(mono, node->as.for_stmt.iter, locals);
    infer_calls_in_node(mono, node->as.for_stmt.body, locals);
    break;
  case AST_LOOP_STMT:
    infer_calls_in_node(mono, node->as.loop_stmt.body, locals);
    break;
  case AST_MATCH_STMT:
    infer_calls_in_node(mono, node->as.match_stmt.subject, locals);
    infer_calls_in_list(mono, node->as.match_stmt.arms, locals);
    break;
  case AST_MATCH_ARM:
    infer_calls_in_node(mono, node->as.match_arm.guard, locals);
    infer_calls_in_node(mono, node->as.match_arm.body, locals);
    break;
  case AST_UNSAFE_BLOCK:
    infer_calls_in_node(mono, node->as.unsafe_block.body, locals);
    break;
  case AST_ARRAY_LITERAL:
    infer_calls_in_list(mono, node->as.array_literal.elems, locals);
    break;
  case AST_TUPLE_EXPR:
    infer_calls_in_list(mono, node->as.tuple_expr.elems, locals);
    break;
  case AST_RANGE_EXPR:
    infer_calls_in_node(mono, node->as.range_expr.start, locals);
    infer_calls_in_node(mono, node->as.range_expr.end, locals);
    break;
  case AST_BINARY_EXPR:
    infer_calls_in_node(mono, node->as.binary.left, locals);
    infer_calls_in_node(mono, node->as.binary.right, locals);
    break;
  case AST_UNARY_EXPR:
    infer_calls_in_node(mono, node->as.unary.expr, locals);
    break;
  case AST_ASSIGN:
    infer_calls_in_node(mono, node->as.assign.target, locals);
    infer_calls_in_node(mono, node->as.assign.value, locals);
    break;
  case AST_CALL_EXPR:
    infer_calls_in_node(mono, node->as.call.callee, locals);
    infer_calls_in_list(mono, node->as.call.args, locals);
    infer_generic_call(mono, node, *locals);
    infer_generic_constructor_call(mono, node, *locals);
    infer_generic_method_call(mono, node, *locals);
    break;
  case AST_INDEX_EXPR:
    infer_calls_in_node(mono, node->as.index.target, locals);
    infer_calls_in_node(mono, node->as.index.index, locals);
    break;
  case AST_FIELD_EXPR:
    infer_calls_in_node(mono, node->as.field.target, locals);
    break;
  case AST_CAST_EXPR:
    infer_calls_in_node(mono, node->as.cast.expr, locals);
    break;
  case AST_PROMOTE_EXPR:
    infer_calls_in_node(mono, node->as.promote.expr, locals);
    break;
  case AST_TRY_EXPR:
    infer_calls_in_node(mono, node->as.try_expr.expr, locals);
    break;
  case AST_CATCH_EXPR:
    infer_calls_in_node(mono, node->as.catch_expr.expr, locals);
    infer_calls_in_node(mono, node->as.catch_expr.handler, locals);
    break;
  case AST_VOLATILE_EXPR:
    infer_calls_in_node(mono, node->as.volatile_expr.expr, locals);
    break;
  case AST_NAMED_ARG:
    infer_calls_in_node(mono, node->as.named_arg.value, locals);
    break;
  case AST_TUPLE_DESTRUCTURE:
    infer_calls_in_node(mono, node->as.tuple_destructure.init, locals);
    break;
  case AST_FIELD_PATTERN:
    infer_calls_in_node(mono, node->as.field_pattern.pattern, locals);
    break;
  case AST_TYPE_DECL:
  case AST_VARIANT_DECL:
  case AST_VARIANT_ARM:
  case AST_FIELD_DECL:
  case AST_INTERFACE_DECL:
  case AST_ERROR_DECL:
  case AST_USE_DECL:
  case AST_EXTERN_DECL:
  case AST_PARAM:
  case AST_BREAK_STMT:
  case AST_CONTINUE_STMT:
  case AST_INT_LITERAL:
  case AST_FLOAT_LITERAL:
  case AST_STRING_LITERAL:
  case AST_BOOL_LITERAL:
  case AST_CHAR_LITERAL:
  case AST_NULL_LITERAL:
  case AST_IDENTIFIER:
  case AST_SIZEOF_EXPR:
  case AST_ALIGNOF_EXPR:
  case AST_TYPE_EXPR:
  case AST_ERROR_EXPR:
  case AST_ASM_EXPR:
  case AST_STRUCT_PATTERN:
    break;
  }
}

static bool interface_exists_in(AstNode *declarations, const char *name) {
  for (AstNode *node = declarations; node;
       node = node->next)
    if (node->kind == AST_INTERFACE_DECL &&
        strcmp(node->as.interface_decl.name, name) == 0)
      return true;
  return false;
}

static bool interface_exists(Monomorphizer *mono, AstNode *constraint) {
  if (constraint->as.type_expr.kind == TYPE_QUALIFIED)
    return interface_exists_in(
        module_declarations(mono, constraint->as.type_expr.module),
        constraint->as.type_expr.name);
  return interface_exists_in(
             module_declarations(mono, mono->current_module),
             constraint->as.type_expr.name) ||
         (mono->current_module &&
          interface_exists_in(mono->program->as.program.declarations,
                              constraint->as.type_expr.name));
}

static bool implementation_exists_in(AstNode *declarations,
                                     const char *interface_name,
                                     const char *actual_name) {
  for (AstNode *node = declarations; node; node = node->next)
    if (node->kind == AST_METHOD_DECL && node->as.method_decl.iface_name &&
        strcmp(node->as.method_decl.iface_name, interface_name) == 0 &&
        strcmp(node->as.method_decl.type_name, actual_name) == 0)
      return true;
  return false;
}

static bool constraint_satisfied(Monomorphizer *mono, AstNode *constraint,
                                 AstNode *actual) {
  if (!constraint)
    return true;
  if (constraint->kind != AST_TYPE_EXPR ||
      (constraint->as.type_expr.kind != TYPE_NAMED &&
       constraint->as.type_expr.kind != TYPE_QUALIFIED) ||
      constraint->as.type_expr.type_args ||
      !interface_exists(mono, constraint))
    return false;
  if (!actual || actual->kind != AST_TYPE_EXPR ||
      (actual->as.type_expr.kind != TYPE_NAMED &&
       actual->as.type_expr.kind != TYPE_QUALIFIED))
    return false;
  const char *actual_name = actual->as.type_expr.name;
  const char *interface_name = constraint->as.type_expr.name;
  const char *actual_module = actual->as.type_expr.kind == TYPE_QUALIFIED
                                  ? actual->as.type_expr.module
                                  : mono->current_module;
  return implementation_exists_in(
             module_declarations(mono, actual_module), interface_name,
             actual_name) ||
         (actual_module &&
          implementation_exists_in(mono->program->as.program.declarations,
                                   interface_name, actual_name));
}

static Binding *build_bindings(Monomorphizer *mono, AstNode *template,
                               AstNode *arguments, const AstNode *site) {
  AstNode *parameter = generic_params(template);
  AstNode *argument = arguments;
  Binding *head = NULL;
  Binding **tail = &head;
  while (parameter && argument) {
    if (!constraint_satisfied(mono, parameter->as.param.type, argument)) {
      mono_error(mono, site, "type argument for '%s' does not satisfy exact "
                             "interface constraint",
                 parameter->as.param.name);
      return NULL;
    }
    Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
    binding->name = parameter->as.param.name;
    binding->type = argument;
    binding->next = NULL;
    *tail = binding;
    tail = &binding->next;
    parameter = parameter->next;
    argument = argument->next;
  }
  if (parameter || argument) {
    mono_error(mono, site, "generic '%s' expects %d type argument(s), got %d",
               declaration_name(template), list_count(generic_params(template)),
               list_count(arguments));
    return NULL;
  }
  return head;
}

static void append_generated(Monomorphizer *mono, AstNode *declaration) {
  declaration->next = NULL;
  if (!mono->generated_head)
    mono->generated_head = mono->generated_tail = declaration;
  else {
    mono->generated_tail->next = declaration;
    mono->generated_tail = declaration;
  }
}

static const char *instantiate(Monomorphizer *mono, AstNode *template,
                               AstNode *arguments, const AstNode *site) {
  const char *name = specialization_name(mono, template, arguments);
  if (!name) {
    mono_error(mono, site, "generic specialization name is too complex");
    return NULL;
  }
  for (GenericInstance *instance = mono->instances; instance;
       instance = instance->next)
    if (instance->template == template && strcmp(instance->name, name) == 0)
      return instance->name;

  Binding *bindings = build_bindings(mono, template, arguments, site);
  if (!bindings && generic_params(template))
    return NULL;
  GenericInstance *instance = arena_alloc(mono->arena, sizeof(*instance));
  instance->template = template;
  instance->name = name;
  instance->declaration = NULL;
  instance->next = mono->instances;
  mono->instances = instance;

  GenericTemplate *template_entry = template_for_node(mono, template);
  const char *saved_module = mono->current_module;
  if (template_entry && template_entry->module)
    mono->current_module = template_entry->module;
  AstNode *declaration = clone_node(mono, template, bindings);
  mono->current_module = saved_module;
  if (!declaration)
    return NULL;
  if (declaration->kind == AST_FUNC_DECL) {
    declaration->as.func_decl.name = name;
    declaration->as.func_decl.generic_params = NULL;
  } else if (declaration->kind == AST_TYPE_DECL) {
    declaration->as.type_decl.name = name;
    declaration->as.type_decl.generic_params = NULL;
  } else {
    declaration->as.variant_decl.name = name;
    declaration->as.variant_decl.generic_params = NULL;
  }
  instance->declaration = declaration;
  append_generated(mono, declaration);

  if (template->kind == AST_TYPE_DECL || template->kind == AST_VARIANT_DECL) {
    const char *template_module =
        template_entry ? template_entry->module : NULL;
    for (AstNode *method = module_declarations(mono, template_module); method;
         method = method->next) {
      if (method->kind != AST_METHOD_DECL ||
          strcmp(method->as.method_decl.type_name,
                 declaration_name(template)) != 0 ||
          !method->as.method_decl.type_args)
        continue;
      AstNode *specialized_method = alloc_node(mono, method);
      specialized_method->as.method_decl.type_name = name;
      specialized_method->as.method_decl.type_args = NULL;
      specialized_method->as.method_decl.methods = NULL;
      AstNode **method_tail = &specialized_method->as.method_decl.methods;
      for (AstNode *function = method->as.method_decl.methods; function;
           function = function->next) {
        if (function->as.func_decl.generic_params)
          continue;
        const char *saved_method_module = mono->current_module;
        mono->current_module = template_module;
        *method_tail = clone_node(mono, function, bindings);
        mono->current_module = saved_method_module;
        method_tail = &(*method_tail)->next;
      }
      if (specialized_method->as.method_decl.methods)
        append_generated(mono, specialized_method);
    }
  }
  return name;
}

static const char *instantiate_method(Monomorphizer *mono,
                                      GenericMethodPlan *plan,
                                      const AstNode *site,
                                      Binding *outer_bindings) {
  AstNode *owner_type =
      substitute_inferred_type(mono, plan->owner_type, outer_bindings);
  AstNode *method_arguments = NULL;
  AstNode **argument_tail = &method_arguments;
  for (AstNode *argument = plan->method_arguments; argument;
       argument = argument->next) {
    *argument_tail =
        substitute_inferred_type(mono, argument, outer_bindings);
    argument_tail = &(*argument_tail)->next;
  }
  const char *instance_name = method_specialization_name(
      mono, plan->method, owner_type, method_arguments);
  if (!instance_name) {
    mono_error(mono, site, "generic method specialization is too complex");
    return NULL;
  }
  for (GenericMethodInstance *existing = mono->method_instances; existing;
       existing = existing->next)
    if (existing->method == plan->method &&
        strcmp(existing->name, instance_name) == 0)
      return existing->name;

  Binding *owner_bindings = NULL;
  const char *owner_name = owner_type->as.type_expr.name;
  GenericTemplate *owner_template =
      owner_type->as.type_expr.kind == TYPE_QUALIFIED
          ? find_qualified_template(mono, owner_type->as.type_expr.module,
                                    owner_name, false, true)
          : find_template(mono, owner_name, false, true);
  if (owner_template) {
    if (!owner_type->as.type_expr.type_args) {
      mono_error(mono, site, "generic method owner '%s' requires type arguments",
                 owner_name);
      return NULL;
    }
    owner_bindings = build_bindings(
        mono, owner_template->node, owner_type->as.type_expr.type_args, site);
    if (!owner_bindings)
      return NULL;
    owner_name = instantiate(mono, owner_template->node,
                             owner_type->as.type_expr.type_args, site);
    if (!owner_name)
      return NULL;
  }

  Binding *method_bindings =
      build_bindings(mono, plan->method, method_arguments, site);
  if (!method_bindings)
    return NULL;
  Binding *combined = method_bindings;
  Binding **tail = &combined;
  while (*tail)
    tail = &(*tail)->next;
  *tail = owner_bindings;

  const char *saved_module = mono->current_module;
  mono->current_module = plan->module;
  AstNode *function = clone_node(mono, plan->method, combined);
  mono->current_module = saved_module;
  function->as.func_decl.name = instance_name;
  function->as.func_decl.generic_params = NULL;

  AstNode *method_block = alloc_node(mono, plan->method_block);
  method_block->as.method_decl.type_name = owner_name;
  method_block->as.method_decl.type_args = NULL;
  method_block->as.method_decl.methods = function;
  append_generated(mono, method_block);
  GenericMethodInstance *instance =
      arena_alloc(mono->arena, sizeof(*instance));
  instance->method = plan->method;
  instance->name = instance_name;
  instance->next = mono->method_instances;
  mono->method_instances = instance;
  plan->emitted = true;
  return instance_name;
}

static AstNode *clone_type(Monomorphizer *mono, AstNode *node,
                           Binding *bindings) {
  if (node->as.type_expr.kind == TYPE_NAMED &&
      !node->as.type_expr.type_args) {
    Binding *binding = find_binding(bindings, node->as.type_expr.name);
    if (binding)
      return clone_node(mono, binding->type, NULL);
  }

  AstNode *copy = alloc_node(mono, node);
  copy->as.type_expr.inner =
      clone_node(mono, node->as.type_expr.inner, bindings);
  copy->as.type_expr.size = clone_node(mono, node->as.type_expr.size, bindings);
  copy->as.type_expr.elems = clone_list(mono, node->as.type_expr.elems, bindings);
  copy->as.type_expr.type_args =
      clone_list(mono, node->as.type_expr.type_args, bindings);

  if ((copy->as.type_expr.kind == TYPE_NAMED ||
       copy->as.type_expr.kind == TYPE_QUALIFIED) &&
      copy->as.type_expr.type_args) {
    GenericTemplate *template = copy->as.type_expr.kind == TYPE_QUALIFIED
                                    ? find_qualified_template(
                                          mono, copy->as.type_expr.module,
                                          copy->as.type_expr.name, false, true)
                                    : find_template(mono,
                                                    copy->as.type_expr.name,
                                                    false, true);
    if (!template) {
      mono_error(mono, node, "'%s' is not a generic type",
                 copy->as.type_expr.name);
      return copy;
    }
    if (copy->as.type_expr.kind == TYPE_QUALIFIED &&
        !generic_declaration_is_public(template->node)) {
      mono_error(mono, node, "generic type '%s.%s' is private",
                 copy->as.type_expr.module, copy->as.type_expr.name);
      return copy;
    }
    const char *name =
        instantiate(mono, template->node, copy->as.type_expr.type_args, node);
    if (name) {
      copy->as.type_expr.kind = TYPE_NAMED;
      copy->as.type_expr.name = name;
      copy->as.type_expr.module = NULL;
      copy->as.type_expr.type_args = NULL;
    }
  } else if (copy->as.type_expr.kind == TYPE_NAMED &&
             find_template(mono, copy->as.type_expr.name, false, true)) {
    mono_error(mono, node, "generic type '%s' requires type arguments",
               copy->as.type_expr.name);
  }
  return copy;
}

static AstNode *clone_call(Monomorphizer *mono, AstNode *node,
                           Binding *bindings) {
  AstNode *copy = alloc_node(mono, node);
  copy->as.call.callee = clone_node(mono, node->as.call.callee, bindings);
  copy->as.call.args = clone_list(mono, node->as.call.args, bindings);
  copy->as.call.type_args =
      clone_list(mono, node->as.call.type_args, bindings);
  for (GenericMethodPlan *plan = mono->method_plans; plan; plan = plan->next) {
    if (plan->call != node)
      continue;
    const char *specialized = instantiate_method(mono, plan, node, bindings);
    if (specialized && copy->as.call.callee->kind == AST_FIELD_EXPR) {
      copy->as.call.callee->as.field.field = specialized;
      copy->as.call.type_args = NULL;
    }
    return copy;
  }
  if (!copy->as.call.type_args)
    return copy;

  const char *name = NULL;
  GenericTemplate *template = NULL;
  bool qualified_member = false;
  bool qualified_variant = false;
  if (copy->as.call.callee->kind == AST_IDENTIFIER) {
    name = copy->as.call.callee->as.identifier.name;
    template = find_template(mono, name, true, true);
  } else if (copy->as.call.callee->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target->kind == AST_IDENTIFIER) {
    const char *target =
        copy->as.call.callee->as.field.target->as.identifier.name;
    const char *field = copy->as.call.callee->as.field.field;
    template = find_qualified_template(mono, target, field, true, true);
    if (template) {
      name = field;
      qualified_member = true;
    } else {
      name = target;
      template = find_template(mono, name, false, true);
    }
  } else if (copy->as.call.callee->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target->as.field.target->kind ==
                 AST_IDENTIFIER) {
    AstNode *qualified_type = copy->as.call.callee->as.field.target;
    const char *module =
        qualified_type->as.field.target->as.identifier.name;
    name = qualified_type->as.field.field;
    template =
        find_qualified_template(mono, module, name, false, true);
    qualified_variant = template != NULL;
  }
  if (!template) {
    mono_error(mono, node,
               "explicit type arguments require a generic declaration");
    return copy;
  }
  if ((qualified_member || qualified_variant) &&
      !generic_declaration_is_public(template->node)) {
    mono_error(mono, node, "generic declaration '%s' is private", name);
    return copy;
  }
  const char *specialized =
      instantiate(mono, template->node, copy->as.call.type_args, node);
  if (specialized) {
    if (copy->as.call.callee->kind == AST_IDENTIFIER)
      copy->as.call.callee->as.identifier.name = specialized;
    else if (qualified_member) {
      AstNode *identifier =
          ast_new_identifier(mono->arena, specialized);
      identifier->line = copy->as.call.callee->line;
      identifier->col = copy->as.call.callee->col;
      copy->as.call.callee = identifier;
    } else if (qualified_variant) {
      AstNode *identifier = ast_new_identifier(mono->arena, specialized);
      identifier->line = copy->as.call.callee->as.field.target->line;
      identifier->col = copy->as.call.callee->as.field.target->col;
      copy->as.call.callee->as.field.target = identifier;
    }
    else
      copy->as.call.callee->as.field.target->as.identifier.name = specialized;
    copy->as.call.type_args = NULL;
  }
  return copy;
}

static AstNode *clone_node(Monomorphizer *mono, AstNode *node,
                           Binding *bindings) {
  if (!node)
    return NULL;
  if (node->kind == AST_TYPE_EXPR)
    return clone_type(mono, node, bindings);
  if (node->kind == AST_CALL_EXPR)
    return clone_call(mono, node, bindings);

  AstNode *copy = alloc_node(mono, node);
  switch (node->kind) {
  case AST_PROGRAM:
    copy->as.program.declarations =
        clone_list(mono, node->as.program.declarations, bindings);
    break;
  case AST_FUNC_DECL:
    copy->as.func_decl.generic_params =
        clone_list(mono, node->as.func_decl.generic_params, bindings);
    copy->as.func_decl.params =
        clone_list(mono, node->as.func_decl.params, bindings);
    copy->as.func_decl.ret_type =
        clone_node(mono, node->as.func_decl.ret_type, bindings);
    copy->as.func_decl.body =
        clone_node(mono, node->as.func_decl.body, bindings);
    copy->as.func_decl.attrs =
        clone_attrs(mono, node->as.func_decl.attrs, bindings);
    break;
  case AST_VAR_DECL:
    copy->as.var_decl.type =
        clone_node(mono, node->as.var_decl.type, bindings);
    copy->as.var_decl.init =
        clone_node(mono, node->as.var_decl.init, bindings);
    copy->as.var_decl.attrs =
        clone_attrs(mono, node->as.var_decl.attrs, bindings);
    break;
  case AST_TYPE_DECL:
    copy->as.type_decl.generic_params =
        clone_list(mono, node->as.type_decl.generic_params, bindings);
    copy->as.type_decl.fields =
        clone_list(mono, node->as.type_decl.fields, bindings);
    copy->as.type_decl.attrs =
        clone_attrs(mono, node->as.type_decl.attrs, bindings);
    break;
  case AST_VARIANT_DECL:
    copy->as.variant_decl.generic_params =
        clone_list(mono, node->as.variant_decl.generic_params, bindings);
    copy->as.variant_decl.arms =
        clone_list(mono, node->as.variant_decl.arms, bindings);
    break;
  case AST_VARIANT_ARM:
    copy->as.variant_arm.fields =
        clone_list(mono, node->as.variant_arm.fields, bindings);
    break;
  case AST_FIELD_DECL:
    copy->as.field_decl.type =
        clone_node(mono, node->as.field_decl.type, bindings);
    copy->as.field_decl.default_val =
        clone_node(mono, node->as.field_decl.default_val, bindings);
    copy->as.field_decl.attrs =
        clone_attrs(mono, node->as.field_decl.attrs, bindings);
    break;
  case AST_METHOD_DECL:
    copy->as.method_decl.type_args =
        clone_list(mono, node->as.method_decl.type_args, bindings);
    copy->as.method_decl.methods = NULL;
    AstNode **method_tail = &copy->as.method_decl.methods;
    for (AstNode *method = node->as.method_decl.methods; method;
         method = method->next) {
      if (method->as.func_decl.generic_params)
        continue;
      *method_tail = clone_node(mono, method, bindings);
      method_tail = &(*method_tail)->next;
    }
    break;
  case AST_INTERFACE_DECL:
    copy->as.interface_decl.methods =
        clone_list(mono, node->as.interface_decl.methods, bindings);
    break;
  case AST_ERROR_DECL:
    copy->as.error_decl.variants =
        clone_list(mono, node->as.error_decl.variants, bindings);
    break;
  case AST_MOD_DECL:
    {
      const char *saved_module = mono->current_module;
      mono->current_module = node->as.mod_decl.name;
      copy->as.mod_decl.declarations = clone_concrete_declarations(
          mono, node->as.mod_decl.declarations, bindings);
      mono->current_module = saved_module;
    }
    break;
  case AST_USE_DECL:
    copy->as.use_decl.path = clone_list(mono, node->as.use_decl.path, bindings);
    break;
  case AST_EXTERN_DECL:
    copy->as.extern_decl.params =
        clone_list(mono, node->as.extern_decl.params, bindings);
    copy->as.extern_decl.ret_type =
        clone_node(mono, node->as.extern_decl.ret_type, bindings);
    copy->as.extern_decl.var_type =
        clone_node(mono, node->as.extern_decl.var_type, bindings);
    break;
  case AST_PARAM:
    copy->as.param.type = clone_node(mono, node->as.param.type, bindings);
    break;
  case AST_BLOCK:
    copy->as.block.statements =
        clone_list(mono, node->as.block.statements, bindings);
    break;
  case AST_RETURN_STMT:
    copy->as.return_stmt.value =
        clone_node(mono, node->as.return_stmt.value, bindings);
    break;
  case AST_IF_STMT:
    copy->as.if_stmt.condition =
        clone_node(mono, node->as.if_stmt.condition, bindings);
    copy->as.if_stmt.then_branch =
        clone_node(mono, node->as.if_stmt.then_branch, bindings);
    copy->as.if_stmt.else_branch =
        clone_node(mono, node->as.if_stmt.else_branch, bindings);
    break;
  case AST_WHILE_STMT:
    copy->as.while_stmt.condition =
        clone_node(mono, node->as.while_stmt.condition, bindings);
    copy->as.while_stmt.body =
        clone_node(mono, node->as.while_stmt.body, bindings);
    break;
  case AST_FOR_STMT:
    copy->as.for_stmt.iter = clone_node(mono, node->as.for_stmt.iter, bindings);
    copy->as.for_stmt.body = clone_node(mono, node->as.for_stmt.body, bindings);
    break;
  case AST_LOOP_STMT:
    copy->as.loop_stmt.body =
        clone_node(mono, node->as.loop_stmt.body, bindings);
    break;
  case AST_MATCH_STMT:
    copy->as.match_stmt.subject =
        clone_node(mono, node->as.match_stmt.subject, bindings);
    copy->as.match_stmt.arms =
        clone_list(mono, node->as.match_stmt.arms, bindings);
    break;
  case AST_MATCH_ARM:
    copy->as.match_arm.pattern =
        clone_node(mono, node->as.match_arm.pattern, bindings);
    copy->as.match_arm.guard =
        clone_node(mono, node->as.match_arm.guard, bindings);
    copy->as.match_arm.body =
        clone_node(mono, node->as.match_arm.body, bindings);
    break;
  case AST_UNSAFE_BLOCK:
    copy->as.unsafe_block.body =
        clone_node(mono, node->as.unsafe_block.body, bindings);
    break;
  case AST_ARRAY_LITERAL:
    copy->as.array_literal.elems =
        clone_list(mono, node->as.array_literal.elems, bindings);
    break;
  case AST_TUPLE_EXPR:
    copy->as.tuple_expr.elems =
        clone_list(mono, node->as.tuple_expr.elems, bindings);
    break;
  case AST_RANGE_EXPR:
    copy->as.range_expr.start =
        clone_node(mono, node->as.range_expr.start, bindings);
    copy->as.range_expr.end =
        clone_node(mono, node->as.range_expr.end, bindings);
    break;
  case AST_BINARY_EXPR:
    copy->as.binary.left = clone_node(mono, node->as.binary.left, bindings);
    copy->as.binary.right = clone_node(mono, node->as.binary.right, bindings);
    break;
  case AST_UNARY_EXPR:
    copy->as.unary.expr = clone_node(mono, node->as.unary.expr, bindings);
    break;
  case AST_ASSIGN:
    copy->as.assign.target = clone_node(mono, node->as.assign.target, bindings);
    copy->as.assign.value = clone_node(mono, node->as.assign.value, bindings);
    break;
  case AST_INDEX_EXPR:
    copy->as.index.target = clone_node(mono, node->as.index.target, bindings);
    copy->as.index.index = clone_node(mono, node->as.index.index, bindings);
    break;
  case AST_FIELD_EXPR:
    copy->as.field.target = clone_node(mono, node->as.field.target, bindings);
    break;
  case AST_CAST_EXPR:
    copy->as.cast.expr = clone_node(mono, node->as.cast.expr, bindings);
    copy->as.cast.target_type =
        clone_node(mono, node->as.cast.target_type, bindings);
    break;
  case AST_PROMOTE_EXPR:
    copy->as.promote.expr = clone_node(mono, node->as.promote.expr, bindings);
    break;
  case AST_SIZEOF_EXPR:
    copy->as.sizeof_expr.type =
        clone_node(mono, node->as.sizeof_expr.type, bindings);
    break;
  case AST_ALIGNOF_EXPR:
    copy->as.alignof_expr.type =
        clone_node(mono, node->as.alignof_expr.type, bindings);
    break;
  case AST_TRY_EXPR:
    copy->as.try_expr.expr = clone_node(mono, node->as.try_expr.expr, bindings);
    break;
  case AST_CATCH_EXPR:
    copy->as.catch_expr.expr =
        clone_node(mono, node->as.catch_expr.expr, bindings);
    copy->as.catch_expr.handler =
        clone_node(mono, node->as.catch_expr.handler, bindings);
    break;
  case AST_ERROR_EXPR:
    copy->as.error_expr.path =
        clone_list(mono, node->as.error_expr.path, bindings);
    copy->as.error_expr.module = mono->current_module;
    if (mono->current_module && copy->as.error_expr.path) {
      const char *set_name = copy->as.error_expr.path->as.identifier.name;
      for (AstNode *decl =
               module_declarations(mono, mono->current_module);
           decl; decl = decl->next) {
        if (decl->kind == AST_ERROR_DECL &&
            strcmp(decl->as.error_decl.name, set_name) == 0) {
          copy->resolved_decl = decl;
          break;
        }
      }
    }
    break;
  case AST_VOLATILE_EXPR:
    copy->as.volatile_expr.expr =
        clone_node(mono, node->as.volatile_expr.expr, bindings);
    break;
  case AST_NAMED_ARG:
    copy->as.named_arg.value =
        clone_node(mono, node->as.named_arg.value, bindings);
    break;
  case AST_TUPLE_DESTRUCTURE:
    copy->as.tuple_destructure.targets =
        clone_list(mono, node->as.tuple_destructure.targets, bindings);
    copy->as.tuple_destructure.init =
        clone_node(mono, node->as.tuple_destructure.init, bindings);
    break;
  case AST_STRUCT_PATTERN:
    copy->as.struct_pattern.fields =
        clone_list(mono, node->as.struct_pattern.fields, bindings);
    break;
  case AST_FIELD_PATTERN:
    copy->as.field_pattern.pattern =
        clone_node(mono, node->as.field_pattern.pattern, bindings);
    break;
  case AST_CALL_EXPR:
  case AST_TYPE_EXPR:
  case AST_INT_LITERAL:
  case AST_FLOAT_LITERAL:
  case AST_STRING_LITERAL:
  case AST_BOOL_LITERAL:
  case AST_CHAR_LITERAL:
  case AST_NULL_LITERAL:
  case AST_IDENTIFIER:
  case AST_BREAK_STMT:
  case AST_CONTINUE_STMT:
  case AST_ASM_EXPR:
    break;
  }
  return copy;
}

static bool is_template_declaration(AstNode *node) {
  return generic_params(node) != NULL ||
         (node->kind == AST_METHOD_DECL && node->as.method_decl.type_args);
}

static void collect_templates(Monomorphizer *mono, AstNode *declarations,
                              const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (generic_params(node)) {
      GenericTemplate *entry = arena_alloc(mono->arena, sizeof(*entry));
      entry->node = node;
      entry->module = module;
      entry->next = mono->templates;
      mono->templates = entry;
    }
    if (node->kind == AST_MOD_DECL)
      collect_templates(mono, node->as.mod_decl.declarations,
                        node->as.mod_decl.name);
  }
}

bool monomorphize_program(Arena *arena, AstNode *program) {
  if (!program || program->kind != AST_PROGRAM)
    return false;
  Monomorphizer mono = {.arena = arena, .program = program};
  collect_templates(&mono, program->as.program.declarations, NULL);

  LocalType *locals = NULL;
  infer_calls_in_node(&mono, program, &locals);
  if (mono.had_error) {
    fprintf(stderr, "Generic inference failed with %d error(s)\n",
            mono.error_count);
    return false;
  }

  AstNode *concrete_head = NULL;
  AstNode **tail = &concrete_head;
  for (AstNode *node = program->as.program.declarations; node;
       node = node->next) {
    if (is_template_declaration(node))
      continue;
    AstNode *copy = clone_node(&mono, node, NULL);
    *tail = copy;
    tail = &copy->next;
  }
  *tail = mono.generated_head;
  program->as.program.declarations = concrete_head;

  if (mono.had_error)
    fprintf(stderr, "Generic specialization failed with %d error(s)\n",
            mono.error_count);
  return !mono.had_error;
}
