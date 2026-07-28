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

typedef struct FlexTemplate {
  AstNode *node;
  const char *module;
  struct FlexTemplate *next;
} FlexTemplate;

typedef struct FlexInstance {
  AstNode *template;
  EffectiveRealm realm;
  const char *name;
  AstNode *declaration;
  struct FlexInstance *next;
} FlexInstance;

typedef struct RealmFunctionFamily {
  const char *name;
  const char *module;
  AstNode *fallback;
  AstNode *variants[4];
  uint8_t exclusions;
  struct RealmFunctionFamily *next;
} RealmFunctionFamily;

typedef struct RealmFunctionInstance {
  RealmFunctionFamily *family;
  EffectiveRealm realm;
  const char *name;
  AstNode *declaration;
  struct RealmFunctionInstance *next;
} RealmFunctionInstance;

typedef struct RealmTypeFamily {
  const char *name;
  const char *module;
  AstNode *fallback;
  AstNode *variants[4];
  uint8_t exclusions;
  struct RealmTypeFamily *next;
} RealmTypeFamily;

typedef struct RealmTypeInstance {
  RealmTypeFamily *family;
  AstNode *selected;
  EffectiveRealm realm;
  const char *name;
  AstNode *arguments;
  AstNode *declaration;
  struct RealmTypeInstance *next;
} RealmTypeInstance;

typedef struct RealmSignatureTemplate {
  AstNode *node;
  const char *module;
  struct RealmSignatureTemplate *next;
} RealmSignatureTemplate;

typedef struct RealmSignaturePlan {
  AstNode *call;
  RealmSignatureTemplate *template;
  Binding *bindings;
  struct RealmSignaturePlan *next;
} RealmSignaturePlan;

typedef struct RealmSignatureInstance {
  AstNode *template;
  const char *name;
  struct RealmSignatureInstance *next;
} RealmSignatureInstance;

typedef struct RealmMethodFamily {
  const char *owner_name;
  const char *interface_name;
  const char *name;
  const char *module;
  AstNode *fallback_block;
  AstNode *fallback;
  AstNode *variant_blocks[4];
  AstNode *variants[4];
  uint8_t exclusions;
  struct RealmMethodFamily *next;
} RealmMethodFamily;

typedef struct RealmMethodPlan {
  AstNode *call;
  RealmMethodFamily *family;
  AstNode *owner_type;
  AstNode *method_arguments;
  struct RealmMethodPlan *next;
} RealmMethodPlan;

typedef struct RealmMethodInstance {
  RealmMethodFamily *family;
  EffectiveRealm realm;
  const char *name;
  struct RealmMethodInstance *next;
} RealmMethodInstance;

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

typedef struct ImportBinding {
  const char *scope_module;
  const char *local_name;
  AstNode *use_decl;
  AstNode *target_decl;
  const char *target_module;
  struct ImportBinding *next;
} ImportBinding;

typedef struct {
  Arena *arena;
  AstNode *program;
  GenericTemplate *templates;
  FlexTemplate *flex_templates;
  RealmFunctionFamily *realm_function_families;
  RealmFunctionInstance *realm_function_instances;
  RealmTypeFamily *realm_type_families;
  RealmTypeInstance *realm_type_instances;
  RealmSignatureTemplate *realm_signature_templates;
  RealmSignaturePlan *realm_signature_plans;
  RealmSignatureInstance *realm_signature_instances;
  RealmMethodFamily *realm_method_families;
  RealmMethodPlan *realm_method_plans;
  RealmMethodInstance *realm_method_instances;
  ImportBinding *imports;
  GenericInstance *instances;
  FlexInstance *flex_instances;
  GenericMethodPlan *method_plans;
  GenericMethodInstance *method_instances;
  AstNode *generated_head;
  AstNode *generated_tail;
  const char *current_module;
  bool has_current_effective_realm;
  EffectiveRealm current_effective_realm;
  bool cloning_callee;
  bool cloning_realm_overload;
  bool had_error;
  int error_count;
} Monomorphizer;

static AstNode *clone_node(Monomorphizer *mono, AstNode *node,
                           Binding *bindings);
static AstNode *alloc_node(Monomorphizer *mono, AstNode *source);
static bool is_template_declaration(AstNode *node);
static GenericTemplate *template_for_node(Monomorphizer *mono,
                                          AstNode *template);
static void append_generated(Monomorphizer *mono, AstNode *declaration);
static FlexTemplate *find_flex_template(Monomorphizer *mono,
                                        const char *name);
static FlexTemplate *find_qualified_flex_template(Monomorphizer *mono,
                                                  const char *module,
                                                  const char *name);
static RealmFunctionFamily *find_realm_function_family(Monomorphizer *mono,
                                                       const char *name);
static RealmFunctionFamily *
find_qualified_realm_function_family(Monomorphizer *mono, const char *module,
                                     const char *name);
static bool is_realm_function_member(Monomorphizer *mono, AstNode *node);
static bool is_realm_method_member(Monomorphizer *mono, AstNode *node);
static bool is_realm_type_member(Monomorphizer *mono, AstNode *node);
static bool is_realm_type_method_block(Monomorphizer *mono, AstNode *node);
static bool is_realm_signature_template(Monomorphizer *mono, AstNode *node);
static bool type_contains_realm_family(Monomorphizer *mono, AstNode *type);
static const char *instantiate_realm_type(
    Monomorphizer *mono, RealmTypeFamily *family, EffectiveRealm realm,
    AstNode *arguments, const AstNode *site);

static bool node_requires_static_realm(Monomorphizer *mono, AstNode *node,
                                       int depth) {
  if (!node || depth > 64)
    return false;
  switch (node->kind) {
  case AST_REALM_BLOCK:
    return true;
  case AST_CALL_EXPR: {
    FlexTemplate *template = NULL;
    AstNode *callee = node->as.call.callee;
    if (callee && callee->kind == AST_IDENTIFIER)
      template = find_flex_template(mono, callee->as.identifier.name);
    else if (callee && callee->kind == AST_FIELD_EXPR &&
             callee->as.field.target &&
             callee->as.field.target->kind == AST_IDENTIFIER)
      template = find_qualified_flex_template(
          mono, callee->as.field.target->as.identifier.name,
          callee->as.field.field);
    if (template &&
        node_requires_static_realm(
            mono, template->node->as.func_decl.body, depth + 1))
      return true;
    for (AstNode *argument = node->as.call.args; argument;
         argument = argument->next)
      if (node_requires_static_realm(mono, argument, depth + 1))
        return true;
    return false;
  }
  case AST_FUNC_DECL:
    return node_requires_static_realm(mono, node->as.func_decl.body,
                                      depth + 1);
  case AST_BLOCK:
    for (AstNode *statement = node->as.block.statements; statement;
         statement = statement->next)
      if (node_requires_static_realm(mono, statement, depth + 1))
        return true;
    return false;
  case AST_VAR_DECL:
    return node_requires_static_realm(mono, node->as.var_decl.init,
                                      depth + 1);
  case AST_RETURN_STMT:
    return node_requires_static_realm(mono, node->as.return_stmt.value,
                                      depth + 1);
  case AST_DEFER_STMT:
    return node_requires_static_realm(mono, node->as.defer_stmt.expression,
                                      depth + 1);
  case AST_ASSIGN:
    return node_requires_static_realm(mono, node->as.assign.target,
                                      depth + 1) ||
           node_requires_static_realm(mono, node->as.assign.value,
                                      depth + 1);
  case AST_IF_STMT:
    return node_requires_static_realm(mono, node->as.if_stmt.then_branch,
                                      depth + 1) ||
           node_requires_static_realm(mono, node->as.if_stmt.else_branch,
                                      depth + 1);
  case AST_WHILE_STMT:
    return node_requires_static_realm(mono, node->as.while_stmt.body,
                                      depth + 1);
  case AST_FOR_STMT:
    return node_requires_static_realm(mono, node->as.for_stmt.body,
                                      depth + 1);
  case AST_LOOP_STMT:
    return node_requires_static_realm(mono, node->as.loop_stmt.body,
                                      depth + 1);
  case AST_UNSAFE_BLOCK:
    return node_requires_static_realm(mono, node->as.unsafe_block.body,
                                      depth + 1);
  default:
    return false;
  }
}

static ImportBinding *find_import(Monomorphizer *mono, const char *name) {
  for (ImportBinding *binding = mono->imports; binding;
       binding = binding->next) {
    bool same_scope =
        (!binding->scope_module && !mono->current_module) ||
        (binding->scope_module && mono->current_module &&
         strcmp(binding->scope_module, mono->current_module) == 0);
    if (same_scope && strcmp(binding->local_name, name) == 0)
      return binding;
  }
  return NULL;
}

static bool template_matches_kind(AstNode *node, bool functions, bool types) {
  return (functions && node->kind == AST_FUNC_DECL) ||
         (types &&
          (node->kind == AST_TYPE_DECL || node->kind == AST_VARIANT_DECL));
}

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
    if (is_template_declaration(node) ||
        is_realm_function_member(mono, node) ||
        is_realm_type_member(mono, node) ||
        is_realm_type_method_block(mono, node) ||
        is_realm_signature_template(mono, node))
      continue;
    bool saved_has_realm = mono->has_current_effective_realm;
    if (node->kind == AST_FUNC_DECL &&
        node->as.func_decl.realm == REALM_FLEX)
      mono->has_current_effective_realm = false;
    *tail = clone_node(mono, node, bindings);
    mono->has_current_effective_realm = saved_has_realm;
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
  ImportBinding *import = find_import(mono, name);
  if (import && template_matches_kind(import->target_decl, functions, types))
    return template_for_node(mono, import->target_decl);

  for (GenericTemplate *entry = mono->templates; entry; entry = entry->next) {
    AstNode *node = entry->node;
    bool visible_here =
        (!entry->module && !mono->current_module) ||
        (entry->module && mono->current_module &&
         strcmp(entry->module, mono->current_module) == 0);
    if (template_matches_kind(node, functions, types) &&
        visible_here && strcmp(declaration_name(node), name) == 0)
      return entry;
  }
  return NULL;
}

static GenericTemplate *find_qualified_template(Monomorphizer *mono,
                                                const char *module,
                                                const char *name,
                                                bool functions, bool types) {
  ImportBinding *module_import = find_import(mono, module);
  if (module_import && module_import->target_decl &&
      module_import->target_decl->kind == AST_MOD_DECL)
    module = module_import->target_decl->as.mod_decl.name;

  for (GenericTemplate *entry = mono->templates; entry; entry = entry->next) {
    AstNode *node = entry->node;
    if (entry->module && strcmp(entry->module, module) == 0 &&
        template_matches_kind(node, functions, types) &&
        strcmp(declaration_name(node), name) == 0)
      return entry;
  }
  return NULL;
}

static FlexTemplate *flex_template_for_node(Monomorphizer *mono,
                                            AstNode *template) {
  for (FlexTemplate *entry = mono->flex_templates; entry;
       entry = entry->next)
    if (entry->node == template)
      return entry;
  return NULL;
}

static FlexTemplate *find_flex_template(Monomorphizer *mono,
                                        const char *name) {
  ImportBinding *import = find_import(mono, name);
  if (import && import->target_decl &&
      import->target_decl->kind == AST_FUNC_DECL)
    return flex_template_for_node(mono, import->target_decl);

  for (FlexTemplate *entry = mono->flex_templates; entry;
       entry = entry->next) {
    bool visible_here =
        (!entry->module && !mono->current_module) ||
        (entry->module && mono->current_module &&
         strcmp(entry->module, mono->current_module) == 0);
    if (visible_here &&
        strcmp(entry->node->as.func_decl.name, name) == 0)
      return entry;
  }
  return NULL;
}

static FlexTemplate *find_qualified_flex_template(Monomorphizer *mono,
                                                  const char *module,
                                                  const char *name) {
  ImportBinding *module_import = find_import(mono, module);
  if (module_import && module_import->target_decl &&
      module_import->target_decl->kind == AST_MOD_DECL)
    module = module_import->target_decl->as.mod_decl.name;

  for (FlexTemplate *entry = mono->flex_templates; entry;
       entry = entry->next)
    if (entry->module && strcmp(entry->module, module) == 0 &&
        strcmp(entry->node->as.func_decl.name, name) == 0)
      return entry;
  return NULL;
}

static bool realm_family_contains(RealmFunctionFamily *family,
                                  AstNode *declaration) {
  if (family->fallback == declaration)
    return true;
  for (unsigned realm = 0; realm < 4; realm++)
    if (family->variants[realm] == declaration)
      return true;
  return false;
}

static RealmFunctionFamily *realm_family_for_node(Monomorphizer *mono,
                                                  AstNode *declaration) {
  for (RealmFunctionFamily *family = mono->realm_function_families; family;
       family = family->next)
    if (realm_family_contains(family, declaration))
      return family;
  return NULL;
}

static RealmFunctionFamily *find_realm_function_family(Monomorphizer *mono,
                                                       const char *name) {
  ImportBinding *import = find_import(mono, name);
  if (import && import->target_decl &&
      import->target_decl->kind == AST_FUNC_DECL) {
    RealmFunctionFamily *family =
        realm_family_for_node(mono, import->target_decl);
    if (family)
      return family;
  }
  for (RealmFunctionFamily *family = mono->realm_function_families; family;
       family = family->next) {
    bool visible_here =
        (!family->module && !mono->current_module) ||
        (family->module && mono->current_module &&
         strcmp(family->module, mono->current_module) == 0);
    if (visible_here && strcmp(family->name, name) == 0)
      return family;
  }
  return NULL;
}

static RealmFunctionFamily *
find_qualified_realm_function_family(Monomorphizer *mono, const char *module,
                                     const char *name) {
  ImportBinding *module_import = find_import(mono, module);
  if (module_import && module_import->target_decl &&
      module_import->target_decl->kind == AST_MOD_DECL)
    module = module_import->target_decl->as.mod_decl.name;
  for (RealmFunctionFamily *family = mono->realm_function_families; family;
       family = family->next)
    if (family->module && strcmp(family->module, module) == 0 &&
        strcmp(family->name, name) == 0)
      return family;
  return NULL;
}

static bool realm_type_family_contains(RealmTypeFamily *family,
                                       AstNode *declaration) {
  if (family->fallback == declaration)
    return true;
  for (unsigned realm = 0; realm < 4; realm++)
    if (family->variants[realm] == declaration)
      return true;
  return false;
}

static RealmTypeFamily *realm_type_family_for_node(Monomorphizer *mono,
                                                   AstNode *declaration) {
  for (RealmTypeFamily *family = mono->realm_type_families; family;
       family = family->next)
    if (realm_type_family_contains(family, declaration))
      return family;
  return NULL;
}

static RealmTypeFamily *find_realm_type_family(Monomorphizer *mono,
                                               const char *name) {
  ImportBinding *import = find_import(mono, name);
  if (import && import->target_decl &&
      (import->target_decl->kind == AST_TYPE_DECL ||
       import->target_decl->kind == AST_VARIANT_DECL)) {
    RealmTypeFamily *family =
        realm_type_family_for_node(mono, import->target_decl);
    if (family)
      return family;
  }
  for (RealmTypeFamily *family = mono->realm_type_families; family;
       family = family->next) {
    bool visible_here =
        (!family->module && !mono->current_module) ||
        (family->module && mono->current_module &&
         strcmp(family->module, mono->current_module) == 0);
    if (visible_here && strcmp(family->name, name) == 0)
      return family;
  }
  return NULL;
}

static RealmTypeFamily *
find_qualified_realm_type_family(Monomorphizer *mono, const char *module,
                                 const char *name) {
  ImportBinding *module_import = find_import(mono, module);
  if (module_import && module_import->target_decl &&
      module_import->target_decl->kind == AST_MOD_DECL)
    module = module_import->target_decl->as.mod_decl.name;
  for (RealmTypeFamily *family = mono->realm_type_families; family;
       family = family->next)
    if (family->module && strcmp(family->module, module) == 0 &&
        strcmp(family->name, name) == 0)
      return family;
  return NULL;
}

static RealmTypeInstance *realm_type_instance_for_name(Monomorphizer *mono,
                                                       const char *name) {
  for (RealmTypeInstance *instance = mono->realm_type_instances; instance;
       instance = instance->next)
    if (strcmp(instance->name, name) == 0)
      return instance;
  return NULL;
}

static RealmSignatureTemplate *
find_realm_signature_template(Monomorphizer *mono, const char *name) {
  ImportBinding *import = find_import(mono, name);
  if (import && import->target_decl &&
      import->target_decl->kind == AST_FUNC_DECL)
    for (RealmSignatureTemplate *entry = mono->realm_signature_templates;
         entry; entry = entry->next)
      if (entry->node == import->target_decl)
        return entry;
  for (RealmSignatureTemplate *entry = mono->realm_signature_templates; entry;
       entry = entry->next) {
    bool visible_here =
        (!entry->module && !mono->current_module) ||
        (entry->module && mono->current_module &&
         strcmp(entry->module, mono->current_module) == 0);
    if (visible_here &&
        strcmp(entry->node->as.func_decl.name, name) == 0)
      return entry;
  }
  return NULL;
}

static RealmSignatureTemplate *find_qualified_realm_signature_template(
    Monomorphizer *mono, const char *module, const char *name) {
  ImportBinding *module_import = find_import(mono, module);
  if (module_import && module_import->target_decl &&
      module_import->target_decl->kind == AST_MOD_DECL)
    module = module_import->target_decl->as.mod_decl.name;
  for (RealmSignatureTemplate *entry = mono->realm_signature_templates; entry;
       entry = entry->next)
    if (entry->module && strcmp(entry->module, module) == 0 &&
        strcmp(entry->node->as.func_decl.name, name) == 0)
      return entry;
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

static const char *importable_name(AstNode *node) {
  if (!node)
    return NULL;
  switch (node->kind) {
  case AST_FUNC_DECL:
    return node->as.func_decl.name;
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
  case AST_VAR_DECL:
    return node->as.var_decl.name;
  default:
    return NULL;
  }
}

static bool importable_is_public(AstNode *node) {
  if (!node)
    return false;
  switch (node->kind) {
  case AST_FUNC_DECL:
    return node->as.func_decl.is_pub;
  case AST_TYPE_DECL:
    return node->as.type_decl.is_pub;
  case AST_VARIANT_DECL:
    return node->as.variant_decl.is_pub;
  case AST_INTERFACE_DECL:
    return node->as.interface_decl.is_pub;
  case AST_ERROR_DECL:
    return node->as.error_decl.is_pub;
  case AST_MOD_DECL:
    return node->as.mod_decl.is_pub;
  case AST_EXTERN_DECL:
    return true;
  case AST_VAR_DECL:
    return node->as.var_decl.is_pub;
  default:
    return false;
  }
}

static AstNode *find_importable(AstNode *declarations, const char *name,
                                bool require_public) {
  for (AstNode *decl = declarations; decl; decl = decl->next) {
    const char *candidate = importable_name(decl);
    if (candidate && strcmp(candidate, name) == 0 &&
        (!require_public || importable_is_public(decl)))
      return decl;
  }
  return NULL;
}

static AstNode *resolve_import_target(Monomorphizer *mono, AstNode *path,
                                      const char *scope_module) {
  if (!path || path->kind != AST_IDENTIFIER)
    return NULL;
  AstNode *current = find_importable(
      module_declarations(mono, scope_module), path->as.identifier.name,
      false);
  if (!current && scope_module)
    current = find_importable(module_declarations(mono, NULL),
                              path->as.identifier.name, false);
  if (!current)
    return NULL;

  for (AstNode *segment = path->next; segment; segment = segment->next) {
    if (current->kind != AST_MOD_DECL ||
        segment->kind != AST_IDENTIFIER)
      return NULL;
    current = find_importable(current->as.mod_decl.declarations,
                              segment->as.identifier.name, true);
    if (!current)
      return NULL;
  }
  return current;
}

static void collect_imports(Monomorphizer *mono, AstNode *declarations,
                            const char *scope_module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (node->kind == AST_USE_DECL) {
      AstNode *target =
          resolve_import_target(mono, node->as.use_decl.path, scope_module);
      if (!target)
        continue;
      AstNode *last = node->as.use_decl.path;
      while (last->next)
        last = last->next;
      ImportBinding *binding =
          arena_alloc(mono->arena, sizeof(*binding));
      binding->scope_module = scope_module;
      binding->local_name =
          node->as.use_decl.alias ? node->as.use_decl.alias
                                  : last->as.identifier.name;
      binding->use_decl = node;
      binding->target_decl = target;
      binding->target_module =
          target->kind == AST_MOD_DECL ? target->as.mod_decl.name : NULL;
      GenericTemplate *template = template_for_node(mono, target);
      if (template)
        binding->target_module = template->module;
      binding->next = mono->imports;
      mono->imports = binding;
      node->as.use_decl.target_decl = target;
    }
    if (node->kind == AST_MOD_DECL)
      collect_imports(mono, node->as.mod_decl.declarations,
                      node->as.mod_decl.name);
  }
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
                     type->as.type_expr.nullable
                         ? (type->as.type_expr.readonly ? "k" : "o")
                         : (type->as.type_expr.readonly ? "c" : "p")) ||
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
  if (template->kind == AST_FUNC_DECL &&
      template->as.func_decl.realm == REALM_FLEX &&
      mono->has_current_effective_realm &&
      !append_text(buffer, sizeof(buffer), &used, "__realm_%s",
                   effective_realm_name(mono->current_effective_realm)))
    return NULL;
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  return name;
}

static const char *flex_specialization_name(Monomorphizer *mono,
                                            FlexTemplate *template,
                                            EffectiveRealm realm) {
  char buffer[4096];
  size_t used = 0;
  const char *base = template->node->as.func_decl.name;
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_realm_"))
    return NULL;
  if (template->module &&
      !append_text(buffer, sizeof(buffer), &used, "%zu_%s_",
                   strlen(template->module), template->module))
    return NULL;
  if (!append_text(buffer, sizeof(buffer), &used, "%zu_%s__%s",
                   strlen(base), base, effective_realm_name(realm)))
    return NULL;
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  return name;
}

static AstNode *realm_family_representative(RealmFunctionFamily *family) {
  if (family->fallback)
    return family->fallback;
  for (unsigned realm = 0; realm < 4; realm++)
    if (family->variants[realm])
      return family->variants[realm];
  return NULL;
}

static AstNode *select_realm_function(Monomorphizer *mono,
                                      RealmFunctionFamily *family,
                                      EffectiveRealm realm,
                                      const AstNode *site) {
  if (family->exclusions & (uint8_t)(1u << (unsigned)realm)) {
    mono_error(mono, site, "function '%s' excludes the %s realm", family->name,
               effective_realm_name(realm));
    return NULL;
  }
  AstNode *selected = family->variants[realm];
  if (!selected)
    selected = family->fallback;
  if (!selected) {
    char available[96] = {0};
    size_t used = 0;
    for (unsigned candidate = 0; candidate < 4; candidate++) {
      if (!family->variants[candidate])
        continue;
      append_text(available, sizeof(available), &used, "%s%s",
                  used ? ", " : "",
                  effective_realm_name((EffectiveRealm)candidate));
    }
    mono_error(mono, site, "function '%s' has no %s definition; available: %s",
               family->name, effective_realm_name(realm),
               used ? available : "none");
    return NULL;
  }
  return selected;
}

static const char *realm_function_specialization_name(
    Monomorphizer *mono, RealmFunctionFamily *family, EffectiveRealm realm,
    AstNode *arguments) {
  char buffer[4096];
  size_t used = 0;
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_overload_"))
    return NULL;
  if (family->module &&
      !append_text(buffer, sizeof(buffer), &used, "%zu_%s_",
                   strlen(family->module), family->module))
    return NULL;
  if (!append_text(buffer, sizeof(buffer), &used, "%zu_%s__%s",
                   strlen(family->name), family->name,
                   effective_realm_name(realm)))
    return NULL;
  if (arguments &&
      !append_text(buffer, sizeof(buffer), &used, "__"))
    return NULL;
  for (AstNode *argument = arguments; argument; argument = argument->next)
    if (!encode_type(argument, buffer, sizeof(buffer), &used))
      return NULL;
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  return name;
}

static AstNode *realm_type_representative(RealmTypeFamily *family) {
  if (family->fallback)
    return family->fallback;
  for (unsigned realm = 0; realm < 4; realm++)
    if (family->variants[realm])
      return family->variants[realm];
  return NULL;
}

static AstNode *select_realm_type(Monomorphizer *mono,
                                  RealmTypeFamily *family,
                                  EffectiveRealm realm,
                                  const AstNode *site) {
  if (family->exclusions & (uint8_t)(1u << (unsigned)realm)) {
    mono_error(mono, site, "type '%s' excludes the %s realm", family->name,
               effective_realm_name(realm));
    return NULL;
  }
  AstNode *selected = family->variants[realm];
  if (!selected)
    selected = family->fallback;
  if (!selected) {
    char available[96] = {0};
    size_t used = 0;
    for (unsigned candidate = 0; candidate < 4; candidate++) {
      if (!family->variants[candidate])
        continue;
      append_text(available, sizeof(available), &used, "%s%s",
                  used ? ", " : "",
                  effective_realm_name((EffectiveRealm)candidate));
    }
    mono_error(mono, site, "type '%s' has no %s definition; available: %s",
               family->name, effective_realm_name(realm),
               used ? available : "none");
    return NULL;
  }
  return selected;
}

static const char *realm_type_specialization_name(
    Monomorphizer *mono, RealmTypeFamily *family, EffectiveRealm realm,
    AstNode *arguments) {
  char buffer[4096];
  size_t used = 0;
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_realm_type_"))
    return NULL;
  if (family->module &&
      !append_text(buffer, sizeof(buffer), &used, "%zu_%s_",
                   strlen(family->module), family->module))
    return NULL;
  if (!append_text(buffer, sizeof(buffer), &used, "%zu_%s__%s",
                   strlen(family->name), family->name,
                   effective_realm_name(realm)))
    return NULL;
  if (arguments && !append_text(buffer, sizeof(buffer), &used, "__"))
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
    if (formal->as.type_expr.nullable != actual->as.type_expr.nullable ||
        formal->as.type_expr.readonly != actual->as.type_expr.readonly)
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
  case AST_UNARY_EXPR: {
    AstNode *inner =
        infer_expression_type(mono, expression->as.unary.expr, locals);
    if (!inner || inner->kind != AST_TYPE_EXPR)
      return inner;
    if (expression->as.unary.op == TOKEN_AMP) {
      AstNode *pointer = ast_new_type_ptr(mono->arena, inner);
      pointer->line = expression->line;
      pointer->col = expression->col;
      return pointer;
    }
    if (expression->as.unary.op == TOKEN_STAR &&
        inner->as.type_expr.kind == TYPE_PTR)
      return inner->as.type_expr.inner;
    return inner;
  }
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
    RealmTypeFamily *type_family = NULL;
    if (callee->kind == AST_IDENTIFIER) {
      type_family =
          find_realm_type_family(mono, callee->as.identifier.name);
    } else if (callee->kind == AST_FIELD_EXPR && callee->as.field.target &&
               callee->as.field.target->kind == AST_IDENTIFIER) {
      const char *target =
          callee->as.field.target->as.identifier.name;
      type_family = find_qualified_realm_type_family(
          mono, target, callee->as.field.field);
      if (!type_family)
        type_family = find_realm_type_family(mono, target);
    } else if (callee->kind == AST_FIELD_EXPR && callee->as.field.target &&
               callee->as.field.target->kind == AST_FIELD_EXPR &&
               callee->as.field.target->as.field.target &&
               callee->as.field.target->as.field.target->kind ==
                   AST_IDENTIFIER) {
      AstNode *qualified = callee->as.field.target;
      type_family = find_qualified_realm_type_family(
          mono, qualified->as.field.target->as.identifier.name,
          qualified->as.field.field);
    }
    if (type_family && mono->has_current_effective_realm) {
      const char *name = instantiate_realm_type(
          mono, type_family, mono->current_effective_realm,
          expression->as.call.type_args, expression);
      if (!name)
        return NULL;
      AstNode *type = ast_new_type_named(mono->arena, name);
      type->line = expression->line;
      type->col = expression->col;
      return type;
    }
    for (RealmSignaturePlan *plan = mono->realm_signature_plans; plan;
         plan = plan->next)
      if (plan->call == expression)
        return clone_node(
            mono, plan->template->node->as.func_decl.ret_type,
            plan->bindings);
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
                 callee->as.identifier.name) == 0) {
        EffectiveRealm return_realm = EFFECTIVE_REALM_DYNAMIC;
        bool has_return_realm =
            declaration->as.func_decl.realm == REALM_FLEX
                ? mono->has_current_effective_realm
                : resolve_effective_realm(declaration->as.func_decl.realm,
                                          EFFECTIVE_REALM_DYNAMIC,
                                          &return_realm);
        if (declaration->as.func_decl.realm == REALM_FLEX)
          return_realm = mono->current_effective_realm;
        if (!has_return_realm)
          return declaration->as.func_decl.ret_type;
        bool saved_has_realm = mono->has_current_effective_realm;
        EffectiveRealm saved_realm = mono->current_effective_realm;
        mono->has_current_effective_realm = true;
        mono->current_effective_realm = return_realm;
        AstNode *result =
            clone_node(mono, declaration->as.func_decl.ret_type, NULL);
        mono->has_current_effective_realm = saved_has_realm;
        mono->current_effective_realm = saved_realm;
        return result;
      }
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
  RealmFunctionFamily *realm_family = NULL;
  AstNode *template_node = NULL;
  if (call->as.call.callee->kind == AST_IDENTIFIER) {
    realm_family = find_realm_function_family(
        mono, call->as.call.callee->as.identifier.name);
    template = find_template(
        mono, call->as.call.callee->as.identifier.name, true, false);
  } else if (call->as.call.callee->kind == AST_FIELD_EXPR &&
             call->as.call.callee->as.field.target->kind == AST_IDENTIFIER) {
    realm_family = find_qualified_realm_function_family(
        mono,
        call->as.call.callee->as.field.target->as.identifier.name,
        call->as.call.callee->as.field.field);
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
  if (realm_family) {
    template_node = realm_family_representative(realm_family);
    if (call->as.call.callee->kind == AST_FIELD_EXPR && template_node &&
        !template_node->as.func_decl.is_pub) {
      mono_error(mono, call, "generic function '%s' is private",
                 realm_family->name);
      return;
    }
  } else if (template) {
    template_node = template->node;
  }
  if (!template_node || !generic_params(template_node))
    return;
  Binding *bindings = NULL;
  AstNode *formal = template_node->as.func_decl.params;
  AstNode *actual = call->as.call.args;
  for (; formal && actual; formal = formal->next, actual = actual->next) {
    AstNode *actual_value = actual->kind == AST_NAMED_ARG
                                ? actual->as.named_arg.value
                                : actual;
    AstNode *actual_type = infer_expression_type(mono, actual_value, locals);
    if (actual_type)
      infer_unify(mono, generic_params(template_node), formal->as.param.type,
                  actual_type, &bindings, call);
  }
  AstNode **tail = &call->as.call.type_args;
  for (AstNode *parameter = generic_params(template_node); parameter;
       parameter = parameter->next) {
    Binding *binding = find_binding(bindings, parameter->as.param.name);
    if (!binding) {
      mono_error(mono, call, "cannot infer type argument '%s' for generic '%s'; "
                             "provide explicit type arguments",
                 parameter->as.param.name, declaration_name(template_node));
      call->as.call.type_args = NULL;
      return;
    }
    *tail = copy_inferred_type(mono, binding->type);
    tail = &(*tail)->next;
  }
}

static bool bind_realm_signature_type(Monomorphizer *mono, AstNode *formal,
                                      AstNode *actual, Binding **bindings,
                                      const AstNode *site) {
  if (!formal || !actual || formal->kind != AST_TYPE_EXPR ||
      actual->kind != AST_TYPE_EXPR)
    return false;
  RealmTypeFamily *family = NULL;
  if (formal->as.type_expr.kind == TYPE_NAMED)
    family = find_realm_type_family(mono, formal->as.type_expr.name);
  else if (formal->as.type_expr.kind == TYPE_QUALIFIED)
    family = find_qualified_realm_type_family(
        mono, formal->as.type_expr.module, formal->as.type_expr.name);
  if (family) {
    RealmTypeInstance *actual_instance =
        actual->as.type_expr.kind == TYPE_NAMED
            ? realm_type_instance_for_name(
                  mono, actual->as.type_expr.name)
            : NULL;
    AstNode *concrete = actual;
    if (!actual_instance) {
      RealmTypeFamily *actual_family = NULL;
      if (actual->as.type_expr.kind == TYPE_NAMED)
        actual_family =
            find_realm_type_family(mono, actual->as.type_expr.name);
      else if (actual->as.type_expr.kind == TYPE_QUALIFIED)
        actual_family = find_qualified_realm_type_family(
            mono, actual->as.type_expr.module, actual->as.type_expr.name);
      if (actual_family == family && mono->has_current_effective_realm)
        concrete = clone_node(mono, actual, NULL);
      actual_instance =
          concrete && concrete->as.type_expr.kind == TYPE_NAMED
              ? realm_type_instance_for_name(
                    mono, concrete->as.type_expr.name)
              : NULL;
    }
    if (!actual_instance || actual_instance->family != family)
      return false;
    Binding *existing = find_binding(*bindings, family->name);
    if (existing) {
      if (same_inferred_type(existing->type, concrete))
        return true;
      mono_error(mono, site,
                 "realm type '%s' is inferred from incompatible owner realms",
                 family->name);
      return false;
    }
    Binding *binding = arena_alloc(mono->arena, sizeof(*binding));
    binding->name = family->name;
    binding->type = copy_inferred_type(mono, concrete);
    binding->next = *bindings;
    *bindings = binding;
    return true;
  }
  if ((formal->as.type_expr.kind == TYPE_PTR ||
       formal->as.type_expr.kind == TYPE_SLICE ||
       formal->as.type_expr.kind == TYPE_ARRAY ||
       formal->as.type_expr.kind == TYPE_FALLIBLE) &&
      formal->as.type_expr.kind == actual->as.type_expr.kind)
    return bind_realm_signature_type(mono, formal->as.type_expr.inner,
                                     actual->as.type_expr.inner, bindings,
                                     site);
  if (formal->as.type_expr.kind != actual->as.type_expr.kind)
    return false;
  bool matched = false;
  AstNode *formal_element = formal->as.type_expr.elems;
  AstNode *actual_element = actual->as.type_expr.elems;
  for (; formal_element && actual_element;
       formal_element = formal_element->next,
       actual_element = actual_element->next)
    if (bind_realm_signature_type(mono, formal_element, actual_element,
                                  bindings, site))
      matched = true;
  AstNode *formal_argument = formal->as.type_expr.type_args;
  AstNode *actual_argument = actual->as.type_expr.type_args;
  for (; formal_argument && actual_argument;
       formal_argument = formal_argument->next,
       actual_argument = actual_argument->next)
    if (bind_realm_signature_type(mono, formal_argument, actual_argument,
                                  bindings, site))
      matched = true;
  return matched;
}

static void infer_realm_signature_call(Monomorphizer *mono, AstNode *call,
                                       LocalType *locals) {
  RealmSignatureTemplate *template = NULL;
  AstNode *callee = call->as.call.callee;
  if (callee->kind == AST_IDENTIFIER)
    template =
        find_realm_signature_template(mono, callee->as.identifier.name);
  else if (callee->kind == AST_FIELD_EXPR && callee->as.field.target &&
           callee->as.field.target->kind == AST_IDENTIFIER)
    template = find_qualified_realm_signature_template(
        mono, callee->as.field.target->as.identifier.name,
        callee->as.field.field);
  if (!template)
    return;
  Binding *bindings = NULL;
  AstNode *formal = template->node->as.func_decl.params;
  AstNode *actual = call->as.call.args;
  for (; formal && actual; formal = formal->next, actual = actual->next) {
    AstNode *actual_value = actual->kind == AST_NAMED_ARG
                                ? actual->as.named_arg.value
                                : actual;
    AstNode *actual_type =
        infer_expression_type(mono, actual_value, locals);
    if (actual_type)
      bind_realm_signature_type(mono, formal->as.param.type, actual_type,
                                &bindings, call);
  }
  if (!bindings) {
    mono_error(mono, call,
               "cannot infer hidden realm type for function '%s'",
               template->node->as.func_decl.name);
    return;
  }
  RealmSignaturePlan *plan = arena_alloc(mono->arena, sizeof(*plan));
  plan->call = call;
  plan->template = template;
  plan->bindings = bindings;
  plan->next = mono->realm_signature_plans;
  mono->realm_signature_plans = plan;
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
  RealmTypeInstance *owner_instance =
      owner_type->as.type_expr.kind == TYPE_NAMED
          ? realm_type_instance_for_name(mono, owner_type->as.type_expr.name)
          : NULL;
  const char *owner_name =
      owner_instance ? owner_instance->family->name
                     : owner_type->as.type_expr.name;
  const char *module =
      owner_instance
          ? owner_instance->family->module
          : (owner_type->as.type_expr.kind == TYPE_QUALIFIED
                 ? owner_type->as.type_expr.module
                 : mono->current_module);
  if (!owner_instance && owner_type->as.type_expr.kind == TYPE_NAMED) {
    GenericTemplate *owner_template =
        find_template(mono, owner_type->as.type_expr.name, false, true);
    if (owner_template)
      module = owner_template->module;
  }
  for (AstNode *declaration = module_declarations(mono, module);
       declaration; declaration = declaration->next) {
    if (declaration->kind != AST_METHOD_DECL ||
        strcmp(declaration->as.method_decl.type_name, owner_name) != 0)
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

static RealmMethodFamily *find_realm_method_family(
    Monomorphizer *mono, const char *module, const char *owner,
    const char *name) {
  RealmMethodFamily *interface_match = NULL;
  for (RealmMethodFamily *family = mono->realm_method_families; family;
       family = family->next) {
    bool same_module = (!module && !family->module) ||
                       (module && family->module &&
                        strcmp(module, family->module) == 0);
    if (!same_module || strcmp(owner, family->owner_name) != 0 ||
        strcmp(name, family->name) != 0)
      continue;
    if (!family->interface_name)
      return family;
    if (interface_match)
      return NULL;
    interface_match = family;
  }
  return interface_match;
}

static AstNode *realm_method_representative(RealmMethodFamily *family) {
  if (family->fallback)
    return family->fallback;
  for (unsigned realm = 0; realm < 4; realm++)
    if (family->variants[realm])
      return family->variants[realm];
  return NULL;
}

static bool infer_realm_method_call(Monomorphizer *mono, AstNode *call,
                                    LocalType *locals) {
  AstNode *callee = call->as.call.callee;
  if (!callee || callee->kind != AST_FIELD_EXPR)
    return false;
  AstNode *owner_type =
      infer_expression_type(mono, callee->as.field.target, locals);
  while (owner_type && owner_type->kind == AST_TYPE_EXPR &&
         owner_type->as.type_expr.kind == TYPE_PTR)
    owner_type = owner_type->as.type_expr.inner;
  if (!owner_type || owner_type->kind != AST_TYPE_EXPR ||
      (owner_type->as.type_expr.kind != TYPE_NAMED &&
       owner_type->as.type_expr.kind != TYPE_QUALIFIED))
    return false;
  RealmTypeInstance *owner_instance =
      owner_type->as.type_expr.kind == TYPE_NAMED
          ? realm_type_instance_for_name(
                mono, owner_type->as.type_expr.name)
          : NULL;
  const char *owner_name = owner_instance
                               ? owner_instance->family->name
                               : owner_type->as.type_expr.name;
  const char *module = owner_instance
                           ? owner_instance->family->module
                           : (owner_type->as.type_expr.kind == TYPE_QUALIFIED
                                  ? owner_type->as.type_expr.module
                                  : mono->current_module);
  if (!owner_instance && owner_type->as.type_expr.kind == TYPE_NAMED) {
    GenericTemplate *owner_template =
        find_template(mono, owner_type->as.type_expr.name, false, true);
    if (owner_template)
      module = owner_template->module;
  }
  RealmMethodFamily *family = find_realm_method_family(
      mono, module, owner_name, callee->as.field.field);
  if (!family)
    return false;

  AstNode *method = realm_method_representative(family);
  AstNode *arguments = call->as.call.type_args;
  if (method && method->as.func_decl.generic_params && !arguments) {
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
                   "cannot infer type argument '%s' for realm method '%s'",
                   parameter->as.param.name, family->name);
        return true;
      }
      *tail = copy_inferred_type(mono, binding->type);
      tail = &(*tail)->next;
    }
    call->as.call.type_args = arguments;
  }

  RealmMethodPlan *plan = arena_alloc(mono->arena, sizeof(*plan));
  plan->call = call;
  plan->family = family;
  plan->owner_type = copy_inferred_type(mono, owner_type);
  plan->method_arguments = arguments;
  plan->next = mono->realm_method_plans;
  mono->realm_method_plans = plan;
  return true;
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
    bool saved_has_realm = mono->has_current_effective_realm;
    EffectiveRealm saved_realm = mono->current_effective_realm;
    EffectiveRealm body_realm = EFFECTIVE_REALM_DYNAMIC;
    bool has_body_realm = false;
    if (node->as.func_decl.is_main) {
      body_realm = EFFECTIVE_REALM_DYNAMIC;
      has_body_realm = true;
    } else if (node->as.func_decl.realm == REALM_FLEX) {
      has_body_realm = mono->has_current_effective_realm;
      body_realm = mono->current_effective_realm;
    } else {
      has_body_realm =
          resolve_effective_realm(node->as.func_decl.realm,
                                  EFFECTIVE_REALM_DYNAMIC, &body_realm);
    }
    mono->has_current_effective_realm = has_body_realm;
    mono->current_effective_realm = body_realm;
    for (AstNode *parameter = node->as.func_decl.params; parameter;
         parameter = parameter->next)
      add_local_type(mono, locals, parameter->as.param.name,
                     parameter->as.param.type);
    infer_calls_in_node(mono, node->as.func_decl.body, locals);
    *locals = saved;
    mono->has_current_effective_realm = saved_has_realm;
    mono->current_effective_realm = saved_realm;
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
    AstNode *type = NULL;
    if (node->as.var_decl.type && mono->has_current_effective_realm &&
        type_contains_realm_family(mono, node->as.var_decl.type))
      type = clone_node(mono, node->as.var_decl.type, NULL);
    else
      type = node->as.var_decl.type
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
  case AST_DEFER_STMT:
    infer_calls_in_node(mono, node->as.defer_stmt.expression, locals);
    break;
  case AST_IF_STMT:
    infer_calls_in_node(mono, node->as.if_stmt.condition, locals);
    infer_calls_in_node(mono, node->as.if_stmt.then_branch, locals);
    infer_calls_in_node(mono, node->as.if_stmt.else_branch, locals);
    break;
  case AST_REALM_BLOCK:
    infer_calls_in_node(mono, node->as.realm_block.body, locals);
    infer_calls_in_node(mono, node->as.realm_block.else_branch, locals);
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
    infer_realm_signature_call(mono, node, *locals);
    if (!infer_realm_method_call(mono, node, *locals))
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

static const char *ensure_generated_import(Monomorphizer *mono,
                                           ImportBinding *import) {
  char buffer[512];
  size_t used = 0;
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_import"))
    return NULL;
  for (AstNode *segment = import->use_decl->as.use_decl.path; segment;
       segment = segment->next) {
    if (segment->kind != AST_IDENTIFIER ||
        !append_text(buffer, sizeof(buffer), &used, "_%s",
                     segment->as.identifier.name))
      return NULL;
  }

  for (AstNode *generated = mono->generated_head; generated;
       generated = generated->next)
    if (generated->kind == AST_USE_DECL &&
        generated->as.use_decl.alias &&
        strcmp(generated->as.use_decl.alias, buffer) == 0)
      return generated->as.use_decl.alias;

  char *alias = arena_alloc(mono->arena, used + 1);
  memcpy(alias, buffer, used + 1);
  AstNode *generated_use = clone_node(mono, import->use_decl, NULL);
  generated_use->as.use_decl.alias = alias;
  append_generated(mono, generated_use);
  return alias;
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
    if (argument->kind == AST_TYPE_EXPR &&
        argument->as.type_expr.kind == TYPE_NAMED &&
        !argument->as.type_expr.type_args) {
      ImportBinding *import =
          find_import(mono, argument->as.type_expr.name);
      if (import &&
          template_matches_kind(import->target_decl, false, true)) {
        const char *alias = ensure_generated_import(mono, import);
        if (!alias) {
          mono_error(mono, site,
                     "imported generic type argument name is too complex");
          return NULL;
        }
        binding->type = clone_node(mono, argument, NULL);
        binding->type->as.type_expr.name = alias;
      }
    }
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

static const char *instantiate_realm_type(
    Monomorphizer *mono, RealmTypeFamily *family, EffectiveRealm realm,
    AstNode *arguments, const AstNode *site) {
  AstNode *selected = select_realm_type(mono, family, realm, site);
  if (!selected)
    return NULL;
  bool is_generic = generic_params(selected) != NULL;
  if (is_generic && !arguments) {
    mono_error(mono, site, "generic realm type '%s' requires type arguments",
               family->name);
    return NULL;
  }
  if (!is_generic && arguments) {
    mono_error(mono, site, "realm type '%s' is not generic", family->name);
    return NULL;
  }
  const char *name =
      realm_type_specialization_name(mono, family, realm, arguments);
  if (!name) {
    mono_error(mono, site, "realm type specialization name is too complex");
    return NULL;
  }
  for (RealmTypeInstance *instance = mono->realm_type_instances; instance;
       instance = instance->next)
    if (instance->family == family && instance->realm == realm &&
        strcmp(instance->name, name) == 0)
      return instance->name;

  Binding *bindings = build_bindings(mono, selected, arguments, site);
  if (!bindings && is_generic)
    return NULL;
  RealmTypeInstance *instance =
      arena_alloc(mono->arena, sizeof(*instance));
  instance->family = family;
  instance->selected = selected;
  instance->realm = realm;
  instance->name = name;
  instance->arguments = arguments;
  instance->declaration = NULL;
  instance->next = mono->realm_type_instances;
  mono->realm_type_instances = instance;

  const char *saved_module = mono->current_module;
  bool saved_has_realm = mono->has_current_effective_realm;
  EffectiveRealm saved_realm = mono->current_effective_realm;
  mono->current_module = family->module;
  mono->has_current_effective_realm = true;
  mono->current_effective_realm = realm;
  AstNode *declaration = clone_node(mono, selected, bindings);
  mono->current_module = saved_module;
  mono->has_current_effective_realm = saved_has_realm;
  mono->current_effective_realm = saved_realm;
  if (!declaration)
    return NULL;
  if (declaration->kind == AST_TYPE_DECL) {
    declaration->as.type_decl.name = name;
    declaration->as.type_decl.generic_params = NULL;
  } else {
    declaration->as.variant_decl.name = name;
    declaration->as.variant_decl.generic_params = NULL;
  }
  declaration->has_overload_realm = false;
  declaration->excluded_realms = 0;
  declaration->realm_family_root = NULL;
  declaration->realm_family_fallback = NULL;
  memset(declaration->realm_family_variants, 0,
         sizeof(declaration->realm_family_variants));
  instance->declaration = declaration;
  append_generated(mono, declaration);

  for (AstNode *method = module_declarations(mono, family->module); method;
       method = method->next) {
    if (method->kind != AST_METHOD_DECL ||
        strcmp(method->as.method_decl.type_name, family->name) != 0 ||
        method->has_overload_realm || method->realm_family_root ||
        method->excluded_realms)
      continue;
    AstNode *specialized_method = alloc_node(mono, method);
    specialized_method->as.method_decl.type_name = name;
    specialized_method->as.method_decl.type_args = NULL;
    specialized_method->as.method_decl.methods = NULL;
    AstNode **method_tail = &specialized_method->as.method_decl.methods;
    for (AstNode *function = method->as.method_decl.methods; function;
         function = function->next) {
      if (function->as.func_decl.generic_params ||
          is_realm_method_member(mono, function))
        continue;
      const char *saved_method_module = mono->current_module;
      bool saved_method_has_realm = mono->has_current_effective_realm;
      EffectiveRealm saved_method_realm = mono->current_effective_realm;
      mono->current_module = family->module;
      mono->has_current_effective_realm = true;
      mono->current_effective_realm = realm;
      *method_tail = clone_node(mono, function, bindings);
      mono->current_module = saved_method_module;
      mono->has_current_effective_realm = saved_method_has_realm;
      mono->current_effective_realm = saved_method_realm;
      method_tail = &(*method_tail)->next;
    }
    if (specialized_method->as.method_decl.methods)
      append_generated(mono, specialized_method);
  }
  return name;
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
        if (function->as.func_decl.generic_params ||
            is_realm_method_member(mono, function))
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
  RealmTypeInstance *owner_instance =
      owner_type->as.type_expr.kind == TYPE_NAMED
          ? realm_type_instance_for_name(mono, owner_name)
          : NULL;
  GenericTemplate *owner_template = NULL;
  if (owner_instance) {
    owner_bindings = build_bindings(mono, owner_instance->selected,
                                    owner_instance->arguments, site);
    if (!owner_bindings && generic_params(owner_instance->selected))
      return NULL;
  } else {
    owner_template =
        owner_type->as.type_expr.kind == TYPE_QUALIFIED
            ? find_qualified_template(mono, owner_type->as.type_expr.module,
                                      owner_name, false, true)
            : find_template(mono, owner_name, false, true);
  }
  if (!owner_instance && owner_template) {
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
  if (node->as.type_expr.kind == TYPE_NAMED) {
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

  if (bindings && copy->as.type_expr.kind == TYPE_NAMED) {
    ImportBinding *import =
        find_import(mono, copy->as.type_expr.name);
    if (import && !generic_params(import->target_decl) &&
        !realm_type_family_for_node(mono, import->target_decl)) {
      const char *alias = ensure_generated_import(mono, import);
      if (alias)
        copy->as.type_expr.name = alias;
    }
  }

  RealmTypeFamily *realm_family = NULL;
  if (copy->as.type_expr.kind == TYPE_NAMED)
    realm_family =
        find_realm_type_family(mono, copy->as.type_expr.name);
  else if (copy->as.type_expr.kind == TYPE_QUALIFIED)
    realm_family = find_qualified_realm_type_family(
        mono, copy->as.type_expr.module, copy->as.type_expr.name);
  if (realm_family) {
    if (!mono->has_current_effective_realm) {
      mono_error(mono, node,
                 "realm-specific type requires a statically known realm");
      return copy;
    }
    AstNode *representative = realm_type_representative(realm_family);
    if (copy->as.type_expr.kind == TYPE_QUALIFIED && representative &&
        !generic_declaration_is_public(representative)) {
      mono_error(mono, node, "realm type '%s.%s' is private",
                 copy->as.type_expr.module, copy->as.type_expr.name);
      return copy;
    }
    const char *name = instantiate_realm_type(
        mono, realm_family, mono->current_effective_realm,
        copy->as.type_expr.type_args, node);
    if (name) {
      copy->as.type_expr.kind = TYPE_NAMED;
      copy->as.type_expr.name = name;
      copy->as.type_expr.module = NULL;
      copy->as.type_expr.type_args = NULL;
    }
    return copy;
  }

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

static const char *instantiate_flex(Monomorphizer *mono,
                                    FlexTemplate *template,
                                    EffectiveRealm realm,
                                    const AstNode *site) {
  const char *name = flex_specialization_name(mono, template, realm);
  if (!name) {
    mono_error(mono, site, "realm specialization name is too complex");
    return NULL;
  }
  for (FlexInstance *instance = mono->flex_instances; instance;
       instance = instance->next)
    if (instance->template == template->node && instance->realm == realm)
      return instance->name;

  FlexInstance *instance = arena_alloc(mono->arena, sizeof(*instance));
  instance->template = template->node;
  instance->realm = realm;
  instance->name = name;
  instance->declaration = NULL;
  instance->next = mono->flex_instances;
  mono->flex_instances = instance;

  const char *saved_module = mono->current_module;
  bool saved_has_realm = mono->has_current_effective_realm;
  EffectiveRealm saved_realm = mono->current_effective_realm;
  mono->current_module = template->module;
  mono->has_current_effective_realm = true;
  mono->current_effective_realm = realm;
  AstNode *declaration = clone_node(mono, template->node, NULL);
  mono->current_module = saved_module;
  mono->has_current_effective_realm = saved_has_realm;
  mono->current_effective_realm = saved_realm;
  if (!declaration)
    return NULL;

  declaration->as.func_decl.name = name;
  declaration->as.func_decl.has_effective_realm = true;
  declaration->as.func_decl.effective_realm = realm;
  instance->declaration = declaration;
  append_generated(mono, declaration);
  return name;
}

static const char *instantiate_realm_function(
    Monomorphizer *mono, RealmFunctionFamily *family, EffectiveRealm realm,
    AstNode *arguments, const AstNode *site) {
  AstNode *selected = select_realm_function(mono, family, realm, site);
  if (!selected)
    return NULL;
  bool is_generic = generic_params(selected) != NULL;
  if (is_generic && !arguments) {
    mono_error(mono, site,
               "generic realm-overloaded function '%s' requires type arguments",
               family->name);
    return NULL;
  }
  if (!is_generic && arguments) {
    mono_error(mono, site, "function '%s' is not generic", family->name);
    return NULL;
  }
  const char *name =
      realm_function_specialization_name(mono, family, realm, arguments);
  if (!name) {
    mono_error(mono, site, "realm overload specialization name is too complex");
    return NULL;
  }
  for (RealmFunctionInstance *instance = mono->realm_function_instances;
       instance; instance = instance->next)
    if (instance->family == family && instance->realm == realm &&
        strcmp(instance->name, name) == 0)
      return instance->name;

  Binding *bindings = build_bindings(mono, selected, arguments, site);
  if (!bindings && is_generic)
    return NULL;
  RealmFunctionInstance *instance =
      arena_alloc(mono->arena, sizeof(*instance));
  instance->family = family;
  instance->realm = realm;
  instance->name = name;
  instance->declaration = NULL;
  instance->next = mono->realm_function_instances;
  mono->realm_function_instances = instance;

  const char *saved_module = mono->current_module;
  bool saved_has_realm = mono->has_current_effective_realm;
  bool saved_overload = mono->cloning_realm_overload;
  EffectiveRealm saved_realm = mono->current_effective_realm;
  mono->current_module = family->module;
  mono->has_current_effective_realm = true;
  mono->current_effective_realm = realm;
  mono->cloning_realm_overload = true;
  AstNode *declaration = clone_node(mono, selected, bindings);
  mono->current_module = saved_module;
  mono->has_current_effective_realm = saved_has_realm;
  mono->current_effective_realm = saved_realm;
  mono->cloning_realm_overload = saved_overload;
  if (!declaration)
    return NULL;
  declaration->as.func_decl.name = name;
  declaration->as.func_decl.generic_params = NULL;
  if (!selected->as.func_decl.has_declared_realm ||
      selected->as.func_decl.realm == REALM_FLEX) {
    declaration->as.func_decl.realm =
        effective_realm_as_memory_realm(
            declaration->as.func_decl.effective_realm);
    declaration->as.func_decl.has_declared_realm = true;
  }
  declaration->has_overload_realm = false;
  declaration->excluded_realms = 0;
  declaration->realm_family_root = NULL;
  declaration->realm_family_fallback = NULL;
  memset(declaration->realm_family_variants, 0,
         sizeof(declaration->realm_family_variants));
  instance->declaration = declaration;
  append_generated(mono, declaration);
  return name;
}

static AstNode *select_realm_method(Monomorphizer *mono,
                                    RealmMethodFamily *family,
                                    EffectiveRealm realm,
                                    const AstNode *site,
                                    AstNode **selected_block) {
  if (family->exclusions & (uint8_t)(1u << (unsigned)realm)) {
    mono_error(mono, site, "method '%s' excludes the %s realm", family->name,
               effective_realm_name(realm));
    return NULL;
  }
  AstNode *method = family->variants[realm];
  *selected_block = family->variant_blocks[realm];
  if (!method) {
    method = family->fallback;
    *selected_block = family->fallback_block;
  }
  if (!method) {
    char available[96] = {0};
    size_t used = 0;
    for (unsigned candidate = 0; candidate < 4; candidate++) {
      if (!family->variants[candidate])
        continue;
      append_text(available, sizeof(available), &used, "%s%s",
                  used ? ", " : "",
                  effective_realm_name((EffectiveRealm)candidate));
    }
    mono_error(mono, site, "method '%s' has no %s definition; available: %s",
               family->name, effective_realm_name(realm),
               used ? available : "none");
    return NULL;
  }
  return method;
}

static const char *realm_method_instance_name(
    Monomorphizer *mono, RealmMethodFamily *family, EffectiveRealm realm,
    AstNode *owner_type, AstNode *arguments) {
  char buffer[4096];
  size_t used = 0;
  if (!append_text(buffer, sizeof(buffer), &used, "__runes_realm_method_%zu_%s__",
                   strlen(family->name), family->name) ||
      !encode_type(owner_type, buffer, sizeof(buffer), &used) ||
      !append_text(buffer, sizeof(buffer), &used, "__%s",
                   effective_realm_name(realm)))
    return NULL;
  if (arguments &&
      !append_text(buffer, sizeof(buffer), &used, "__"))
    return NULL;
  for (AstNode *argument = arguments; argument; argument = argument->next)
    if (!encode_type(argument, buffer, sizeof(buffer), &used))
      return NULL;
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  return name;
}

static const char *instantiate_realm_method(Monomorphizer *mono,
                                            RealmMethodPlan *plan,
                                            const AstNode *site,
                                            Binding *outer_bindings) {
  AstNode *owner_type =
      substitute_inferred_type(mono, plan->owner_type, outer_bindings);
  if (owner_type && owner_type->kind == AST_TYPE_EXPR &&
      owner_type->as.type_expr.kind == TYPE_NAMED &&
      find_realm_type_family(mono, owner_type->as.type_expr.name))
    owner_type = clone_node(mono, owner_type, NULL);
  EffectiveRealm realm = mono->current_effective_realm;
  RealmTypeInstance *owner_instance =
      owner_type && owner_type->kind == AST_TYPE_EXPR &&
              owner_type->as.type_expr.kind == TYPE_NAMED
          ? realm_type_instance_for_name(
                mono, owner_type->as.type_expr.name)
          : NULL;
  if (owner_instance)
    realm = owner_instance->realm;
  AstNode *selected_block = NULL;
  AstNode *selected =
      select_realm_method(mono, plan->family, realm, site, &selected_block);
  if (!selected)
    return NULL;
  AstNode *arguments = NULL;
  AstNode **argument_tail = &arguments;
  for (AstNode *argument = plan->method_arguments; argument;
       argument = argument->next) {
    *argument_tail =
        substitute_inferred_type(mono, argument, outer_bindings);
    argument_tail = &(*argument_tail)->next;
  }
  bool method_generic = generic_params(selected) != NULL;
  if (method_generic && !arguments) {
    mono_error(mono, site, "generic realm method '%s' requires type arguments",
               plan->family->name);
    return NULL;
  }
  if (!method_generic && arguments) {
    mono_error(mono, site, "realm method '%s' is not generic",
               plan->family->name);
    return NULL;
  }
  const char *instance_name = realm_method_instance_name(
      mono, plan->family, realm, owner_type, arguments);
  if (!instance_name) {
    mono_error(mono, site, "realm method specialization name is too complex");
    return NULL;
  }
  for (RealmMethodInstance *instance = mono->realm_method_instances; instance;
       instance = instance->next)
    if (instance->family == plan->family && instance->realm == realm &&
        strcmp(instance->name, instance_name) == 0)
      return instance->name;

  Binding *owner_bindings = NULL;
  const char *owner_name = owner_type->as.type_expr.name;
  GenericTemplate *owner_template = NULL;
  if (owner_instance) {
    owner_bindings = build_bindings(mono, owner_instance->selected,
                                    owner_instance->arguments, site);
    if (!owner_bindings && generic_params(owner_instance->selected))
      return NULL;
  } else {
    owner_template =
        owner_type->as.type_expr.kind == TYPE_QUALIFIED
            ? find_qualified_template(mono, owner_type->as.type_expr.module,
                                      owner_name, false, true)
            : find_template(mono, owner_name, false, true);
  }
  if (!owner_instance && owner_template) {
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
      build_bindings(mono, selected, arguments, site);
  if (!method_bindings && method_generic)
    return NULL;
  Binding *combined = method_bindings;
  Binding **binding_tail = &combined;
  while (*binding_tail)
    binding_tail = &(*binding_tail)->next;
  *binding_tail = owner_bindings;

  RealmMethodInstance *instance =
      arena_alloc(mono->arena, sizeof(*instance));
  instance->family = plan->family;
  instance->realm = realm;
  instance->name = instance_name;
  instance->next = mono->realm_method_instances;
  mono->realm_method_instances = instance;

  const char *saved_module = mono->current_module;
  bool saved_has_realm = mono->has_current_effective_realm;
  bool saved_overload = mono->cloning_realm_overload;
  EffectiveRealm saved_realm = mono->current_effective_realm;
  mono->current_module = plan->family->module;
  mono->has_current_effective_realm = true;
  mono->current_effective_realm = realm;
  mono->cloning_realm_overload = true;
  AstNode *function = clone_node(mono, selected, combined);
  mono->current_module = saved_module;
  mono->has_current_effective_realm = saved_has_realm;
  mono->current_effective_realm = saved_realm;
  mono->cloning_realm_overload = saved_overload;
  if (!function)
    return NULL;
  function->as.func_decl.name = instance_name;
  function->as.func_decl.generic_params = NULL;
  if (!selected->as.func_decl.has_declared_realm ||
      selected->as.func_decl.realm == REALM_FLEX) {
    function->as.func_decl.realm =
        effective_realm_as_memory_realm(function->as.func_decl.effective_realm);
    function->as.func_decl.has_declared_realm = true;
  }
  function->has_overload_realm = false;
  function->excluded_realms = 0;
  function->realm_family_root = NULL;

  AstNode *method_block = alloc_node(mono, selected_block);
  method_block->as.method_decl.type_name = owner_name;
  method_block->as.method_decl.type_args = NULL;
  method_block->as.method_decl.methods = function;
  method_block->has_overload_realm = false;
  method_block->excluded_realms = 0;
  method_block->realm_family_root = NULL;
  append_generated(mono, method_block);
  return instance_name;
}

static const char *instantiate_realm_signature(
    Monomorphizer *mono, RealmSignaturePlan *plan, const AstNode *site,
    Binding *outer_bindings) {
  (void)site;
  char buffer[4096];
  size_t used = 0;
  const char *base = plan->template->node->as.func_decl.name;
  if (!append_text(buffer, sizeof(buffer), &used,
                   "__runes_owner_signature_"))
    return NULL;
  if (plan->template->module &&
      !append_text(buffer, sizeof(buffer), &used, "%zu_%s_",
                   strlen(plan->template->module),
                   plan->template->module))
    return NULL;
  if (!append_text(buffer, sizeof(buffer), &used, "%zu_%s__",
                   strlen(base), base))
    return NULL;

  Binding *bindings = NULL;
  Binding **tail = &bindings;
  for (Binding *binding = plan->bindings; binding; binding = binding->next) {
    AstNode *type =
        substitute_inferred_type(mono, binding->type, outer_bindings);
    if (!append_text(buffer, sizeof(buffer), &used, "%zu_%s_",
                     strlen(binding->name), binding->name) ||
        !encode_type(type, buffer, sizeof(buffer), &used))
      return NULL;
    Binding *copy = arena_alloc(mono->arena, sizeof(*copy));
    copy->name = binding->name;
    copy->type = type;
    copy->next = NULL;
    *tail = copy;
    tail = &copy->next;
  }
  char *name = arena_alloc(mono->arena, used + 1);
  memcpy(name, buffer, used + 1);
  for (RealmSignatureInstance *instance = mono->realm_signature_instances;
       instance; instance = instance->next)
    if (instance->template == plan->template->node &&
        strcmp(instance->name, name) == 0)
      return instance->name;

  RealmSignatureInstance *instance =
      arena_alloc(mono->arena, sizeof(*instance));
  instance->template = plan->template->node;
  instance->name = name;
  instance->next = mono->realm_signature_instances;
  mono->realm_signature_instances = instance;

  const char *saved_module = mono->current_module;
  mono->current_module = plan->template->module;
  AstNode *declaration =
      clone_node(mono, plan->template->node, bindings);
  mono->current_module = saved_module;
  if (!declaration)
    return NULL;
  declaration->as.func_decl.name = name;
  append_generated(mono, declaration);
  return name;
}

static AstNode *clone_call(Monomorphizer *mono, AstNode *node,
                           Binding *bindings) {
  AstNode *copy = alloc_node(mono, node);
  bool saved_cloning_callee = mono->cloning_callee;
  mono->cloning_callee = true;
  copy->as.call.callee = clone_node(mono, node->as.call.callee, bindings);
  mono->cloning_callee = saved_cloning_callee;
  copy->as.call.args = clone_list(mono, node->as.call.args, bindings);
  copy->as.call.type_args =
      clone_list(mono, node->as.call.type_args, bindings);

  for (RealmSignaturePlan *plan = mono->realm_signature_plans; plan;
       plan = plan->next) {
    if (plan->call != node)
      continue;
    const char *specialized =
        instantiate_realm_signature(mono, plan, node, bindings);
    if (specialized) {
      AstNode *identifier = ast_new_identifier(mono->arena, specialized);
      identifier->line = copy->as.call.callee->line;
      identifier->col = copy->as.call.callee->col;
      copy->as.call.callee = identifier;
    }
    return copy;
  }

  RealmTypeFamily *type_family = NULL;
  enum {
    REALM_CTOR_NONE,
    REALM_CTOR_DIRECT,
    REALM_CTOR_QUALIFIED,
    REALM_CTOR_VARIANT,
    REALM_CTOR_QUALIFIED_VARIANT,
  } type_ctor_kind = REALM_CTOR_NONE;
  if (copy->as.call.callee->kind == AST_IDENTIFIER) {
    type_family = find_realm_type_family(
        mono, copy->as.call.callee->as.identifier.name);
    if (type_family)
      type_ctor_kind = REALM_CTOR_DIRECT;
  } else if (copy->as.call.callee->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target &&
             copy->as.call.callee->as.field.target->kind == AST_IDENTIFIER) {
    const char *target =
        copy->as.call.callee->as.field.target->as.identifier.name;
    type_family = find_qualified_realm_type_family(
        mono, target, copy->as.call.callee->as.field.field);
    if (type_family)
      type_ctor_kind = REALM_CTOR_QUALIFIED;
    else {
      type_family = find_realm_type_family(mono, target);
      if (type_family)
        type_ctor_kind = REALM_CTOR_VARIANT;
    }
  } else if (copy->as.call.callee->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target &&
             copy->as.call.callee->as.field.target->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target->as.field.target &&
             copy->as.call.callee->as.field.target->as.field.target->kind ==
                 AST_IDENTIFIER) {
    AstNode *qualified = copy->as.call.callee->as.field.target;
    type_family = find_qualified_realm_type_family(
        mono, qualified->as.field.target->as.identifier.name,
        qualified->as.field.field);
    if (type_family)
      type_ctor_kind = REALM_CTOR_QUALIFIED_VARIANT;
  }
  if (type_family) {
    if (!mono->has_current_effective_realm) {
      mono_error(mono, node,
                 "realm-specific constructor requires a statically known realm");
      return copy;
    }
    AstNode *representative = realm_type_representative(type_family);
    if ((type_ctor_kind == REALM_CTOR_QUALIFIED ||
         type_ctor_kind == REALM_CTOR_QUALIFIED_VARIANT) &&
        representative && !generic_declaration_is_public(representative)) {
      mono_error(mono, node, "realm type '%s' is private",
                 type_family->name);
      return copy;
    }
    const char *specialized = instantiate_realm_type(
        mono, type_family, mono->current_effective_realm,
        copy->as.call.type_args, node);
    if (!specialized)
      return copy;
    AstNode *identifier = ast_new_identifier(mono->arena, specialized);
    identifier->line = copy->as.call.callee->line;
    identifier->col = copy->as.call.callee->col;
    if (type_ctor_kind == REALM_CTOR_DIRECT ||
        type_ctor_kind == REALM_CTOR_QUALIFIED)
      copy->as.call.callee = identifier;
    else
      copy->as.call.callee->as.field.target = identifier;
    copy->as.call.type_args = NULL;
    return copy;
  }

  for (RealmMethodPlan *plan = mono->realm_method_plans; plan;
       plan = plan->next) {
    if (plan->call != node)
      continue;
    if (!mono->has_current_effective_realm) {
      mono_error(mono, node,
                 "realm-overloaded method requires a statically known realm");
      return copy;
    }
    const char *specialized =
        instantiate_realm_method(mono, plan, node, bindings);
    if (specialized && copy->as.call.callee->kind == AST_FIELD_EXPR) {
      copy->as.call.callee->as.field.field = specialized;
      copy->as.call.type_args = NULL;
    }
    return copy;
  }
  RealmFunctionFamily *realm_family = NULL;
  bool realm_family_qualified = false;
  if (copy->as.call.callee->kind == AST_IDENTIFIER) {
    realm_family = find_realm_function_family(
        mono, copy->as.call.callee->as.identifier.name);
  } else if (copy->as.call.callee->kind == AST_FIELD_EXPR &&
             copy->as.call.callee->as.field.target &&
             copy->as.call.callee->as.field.target->kind == AST_IDENTIFIER) {
    realm_family = find_qualified_realm_function_family(
        mono, copy->as.call.callee->as.field.target->as.identifier.name,
        copy->as.call.callee->as.field.field);
    realm_family_qualified = realm_family != NULL;
  }
  if (realm_family) {
    if (!mono->has_current_effective_realm) {
      mono_error(mono, node,
                 "realm-overloaded function requires a statically known realm");
      return copy;
    }
    AstNode *representative = realm_family_representative(realm_family);
    if (realm_family_qualified &&
        representative && !representative->as.func_decl.is_pub) {
      mono_error(mono, node, "realm-overloaded function '%s' is private",
                 realm_family->name);
      return copy;
    }
    const char *specialized = instantiate_realm_function(
        mono, realm_family, mono->current_effective_realm,
        copy->as.call.type_args, node);
    if (!specialized)
      return copy;
    AstNode *identifier = ast_new_identifier(mono->arena, specialized);
    identifier->line = copy->as.call.callee->line;
    identifier->col = copy->as.call.callee->col;
    copy->as.call.callee = identifier;
    copy->as.call.type_args = NULL;
    return copy;
  }
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
  if (!copy->as.call.type_args) {
    if (!mono->has_current_effective_realm)
      return copy;

    FlexTemplate *flex_template = NULL;
    bool qualified = false;
    if (copy->as.call.callee->kind == AST_IDENTIFIER) {
      flex_template =
          find_flex_template(mono, copy->as.call.callee->as.identifier.name);
    } else if (copy->as.call.callee->kind == AST_FIELD_EXPR &&
               copy->as.call.callee->as.field.target->kind ==
                   AST_IDENTIFIER) {
      const char *module =
          copy->as.call.callee->as.field.target->as.identifier.name;
      const char *name = copy->as.call.callee->as.field.field;
      flex_template =
          find_qualified_flex_template(mono, module, name);
      qualified = flex_template != NULL;
    }
    if (!flex_template)
      return copy;

    const char *specialized =
        instantiate_flex(mono, flex_template,
                         mono->current_effective_realm, node);
    if (!specialized)
      return copy;
    if (!qualified) {
      copy->as.call.callee->as.identifier.name = specialized;
    } else {
      AstNode *identifier = ast_new_identifier(mono->arena, specialized);
      identifier->line = copy->as.call.callee->line;
      identifier->col = copy->as.call.callee->col;
      copy->as.call.callee = identifier;
    }
    return copy;
  }

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
  if (node->kind == AST_REALM_BLOCK) {
    if (!mono->has_current_effective_realm) {
      return alloc_node(mono, node);
    }
    AstNode *selected =
        node->as.realm_block.realm == mono->current_effective_realm
            ? node->as.realm_block.body
            : node->as.realm_block.else_branch;
    return clone_node(mono, selected, bindings);
  }
  if (node->kind == AST_TYPE_EXPR)
    return clone_type(mono, node, bindings);
  if (node->kind == AST_CALL_EXPR)
    return clone_call(mono, node, bindings);
  if (node->kind == AST_IDENTIFIER && !mono->cloning_callee) {
    RealmFunctionFamily *realm_family =
        find_realm_function_family(mono, node->as.identifier.name);
    if (realm_family) {
      mono_error(mono, node,
                 "realm-overloaded function requires a direct call");
      return alloc_node(mono, node);
    }
    FlexTemplate *template =
        find_flex_template(mono, node->as.identifier.name);
    if (template && node_requires_static_realm(
                        mono, template->node->as.func_decl.body, 0)) {
      mono_error(mono, node,
                 "'when realm' requires a statically specialized direct call");
      return alloc_node(mono, node);
    }
  }
  if (node->kind == AST_FIELD_EXPR && !mono->cloning_callee &&
      node->as.field.target &&
      node->as.field.target->kind == AST_IDENTIFIER) {
    RealmFunctionFamily *realm_family =
        find_qualified_realm_function_family(
            mono, node->as.field.target->as.identifier.name,
            node->as.field.field);
    if (realm_family) {
      mono_error(mono, node,
                 "realm-overloaded function requires a direct call");
      return alloc_node(mono, node);
    }
    FlexTemplate *template = find_qualified_flex_template(
        mono, node->as.field.target->as.identifier.name,
        node->as.field.field);
    if (template && node_requires_static_realm(
                        mono, template->node->as.func_decl.body, 0)) {
      mono_error(mono, node,
                 "'when realm' requires a statically specialized direct call");
      return alloc_node(mono, node);
    }
  }

  AstNode *copy = alloc_node(mono, node);
  switch (node->kind) {
  case AST_PROGRAM:
    copy->as.program.declarations =
        clone_list(mono, node->as.program.declarations, bindings);
    break;
  case AST_FUNC_DECL: {
    bool saved_has_realm = mono->has_current_effective_realm;
    EffectiveRealm saved_realm = mono->current_effective_realm;
    EffectiveRealm body_realm = EFFECTIVE_REALM_DYNAMIC;
    bool has_body_realm = false;
    if ((mono->cloning_realm_overload || node->has_overload_realm ||
         node->realm_family_root || node->excluded_realms) &&
        mono->has_current_effective_realm) {
      if (!node->as.func_decl.has_declared_realm ||
          node->as.func_decl.realm == REALM_FLEX) {
        body_realm = mono->current_effective_realm;
        has_body_realm = true;
      } else {
        has_body_realm = resolve_effective_realm(
            node->as.func_decl.realm, mono->current_effective_realm,
            &body_realm);
      }
    } else if (node->as.func_decl.is_main) {
      body_realm = EFFECTIVE_REALM_DYNAMIC;
      has_body_realm = true;
    } else if (node->as.func_decl.realm == REALM_FLEX) {
      has_body_realm = mono->has_current_effective_realm;
      body_realm = mono->current_effective_realm;
    } else {
      has_body_realm =
          resolve_effective_realm(node->as.func_decl.realm,
                                  EFFECTIVE_REALM_DYNAMIC, &body_realm);
    }
    mono->has_current_effective_realm = has_body_realm;
    mono->current_effective_realm = body_realm;
    copy->as.func_decl.has_effective_realm = has_body_realm;
    copy->as.func_decl.effective_realm = body_realm;
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
    mono->has_current_effective_realm = saved_has_realm;
    mono->current_effective_realm = saved_realm;
    break;
  }
  case AST_VAR_DECL:
    copy->as.var_decl.is_pub = node->as.var_decl.is_pub;
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
      if (method->as.func_decl.generic_params ||
          is_realm_method_member(mono, method))
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
    copy->as.use_decl.alias = node->as.use_decl.alias;
    copy->as.use_decl.target_decl = node->as.use_decl.target_decl;
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
  case AST_DEFER_STMT:
    copy->as.defer_stmt.expression =
        clone_node(mono, node->as.defer_stmt.expression, bindings);
    break;
  case AST_IF_STMT:
    copy->as.if_stmt.condition =
        clone_node(mono, node->as.if_stmt.condition, bindings);
    copy->as.if_stmt.then_branch =
        clone_node(mono, node->as.if_stmt.then_branch, bindings);
    copy->as.if_stmt.else_branch =
        clone_node(mono, node->as.if_stmt.else_branch, bindings);
    break;
  case AST_REALM_BLOCK:
    break; // Pruned before allocating the clone.
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
    if (bindings && copy->as.field.target &&
        copy->as.field.target->kind == AST_IDENTIFIER) {
      ImportBinding *import =
          find_import(mono, copy->as.field.target->as.identifier.name);
      if (import && !generic_params(import->target_decl) &&
          (import->target_decl->kind == AST_TYPE_DECL ||
           import->target_decl->kind == AST_VARIANT_DECL ||
           import->target_decl->kind == AST_ERROR_DECL)) {
        const char *alias = ensure_generated_import(mono, import);
        if (alias)
          copy->as.field.target->as.identifier.name = alias;
      }
    }
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

static bool is_realm_function_member(Monomorphizer *mono, AstNode *node) {
  return node && node->kind == AST_FUNC_DECL &&
         realm_family_for_node(mono, node) != NULL;
}

static bool is_realm_type_member(Monomorphizer *mono, AstNode *node) {
  return node &&
         (node->kind == AST_TYPE_DECL || node->kind == AST_VARIANT_DECL) &&
         realm_type_family_for_node(mono, node) != NULL;
}

static bool is_realm_type_method_block(Monomorphizer *mono, AstNode *node) {
  return node && node->kind == AST_METHOD_DECL &&
         find_realm_type_family(mono, node->as.method_decl.type_name) != NULL;
}

static bool type_contains_realm_family(Monomorphizer *mono, AstNode *type) {
  if (!type || type->kind != AST_TYPE_EXPR)
    return false;
  if (type->as.type_expr.kind == TYPE_NAMED &&
      find_realm_type_family(mono, type->as.type_expr.name))
    return true;
  if (type->as.type_expr.kind == TYPE_QUALIFIED &&
      find_qualified_realm_type_family(
          mono, type->as.type_expr.module, type->as.type_expr.name))
    return true;
  if (type_contains_realm_family(mono, type->as.type_expr.inner))
    return true;
  for (AstNode *element = type->as.type_expr.elems; element;
       element = element->next)
    if (type_contains_realm_family(mono, element))
      return true;
  for (AstNode *argument = type->as.type_expr.type_args; argument;
       argument = argument->next)
    if (type_contains_realm_family(mono, argument))
      return true;
  return false;
}

static bool is_realm_signature_template_node(Monomorphizer *mono,
                                             AstNode *node) {
  if (!node || node->kind != AST_FUNC_DECL ||
      node->as.func_decl.generic_params ||
      is_realm_function_member(mono, node))
    return false;
  for (AstNode *parameter = node->as.func_decl.params; parameter;
       parameter = parameter->next)
    if (type_contains_realm_family(mono, parameter->as.param.type))
      return true;
  return false;
}

static void collect_realm_signature_templates(Monomorphizer *mono,
                                              AstNode *declarations,
                                              const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (is_realm_signature_template_node(mono, node)) {
      RealmSignatureTemplate *entry =
          arena_alloc(mono->arena, sizeof(*entry));
      entry->node = node;
      entry->module = module;
      entry->next = mono->realm_signature_templates;
      mono->realm_signature_templates = entry;
    }
    if (node->kind == AST_MOD_DECL)
      collect_realm_signature_templates(
          mono, node->as.mod_decl.declarations, node->as.mod_decl.name);
  }
}

static bool is_realm_signature_template(Monomorphizer *mono, AstNode *node) {
  for (RealmSignatureTemplate *entry = mono->realm_signature_templates; entry;
       entry = entry->next)
    if (entry->node == node)
      return true;
  return false;
}

static void collect_realm_type_families(Monomorphizer *mono,
                                        AstNode *declarations,
                                        const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if ((node->kind == AST_TYPE_DECL || node->kind == AST_VARIANT_DECL) &&
        (node->realm_family_root || node->has_overload_realm ||
         node->excluded_realms)) {
      AstNode *root = node->realm_family_root ? node->realm_family_root : node;
      if (!realm_type_family_for_node(mono, root)) {
        RealmTypeFamily *family =
            arena_alloc(mono->arena, sizeof(*family));
        memset(family, 0, sizeof(*family));
        family->name = declaration_name(root);
        family->module = module;
        family->fallback = root->realm_family_root
                               ? root->realm_family_fallback
                               : (root->has_overload_realm ? NULL : root);
        family->exclusions = root->excluded_realms;
        if (root->realm_family_root) {
          for (unsigned realm = 0; realm < 4; realm++)
            family->variants[realm] = root->realm_family_variants[realm];
        } else if (root->has_overload_realm) {
          family->variants[root->overload_realm] = root;
        }
        family->next = mono->realm_type_families;
        mono->realm_type_families = family;
      }
    }
    if (node->kind == AST_MOD_DECL)
      collect_realm_type_families(
          mono, node->as.mod_decl.declarations, node->as.mod_decl.name);
  }
}

static bool declaration_contains_realm_type(Monomorphizer *mono,
                                            AstNode *node) {
  if (node->kind == AST_TYPE_DECL) {
    for (AstNode *field = node->as.type_decl.fields; field;
         field = field->next)
      if (type_contains_realm_family(mono, field->as.field_decl.type))
        return true;
  } else if (node->kind == AST_VARIANT_DECL) {
    for (AstNode *arm = node->as.variant_decl.arms; arm; arm = arm->next)
      for (AstNode *field = arm->as.variant_arm.fields; field;
           field = field->next)
        if (type_contains_realm_family(mono, field))
          return true;
  }
  return false;
}

static bool collect_derived_realm_type_families_pass(
    Monomorphizer *mono, AstNode *declarations, const char *module) {
  bool changed = false;
  for (AstNode *node = declarations; node; node = node->next) {
    if ((node->kind == AST_TYPE_DECL || node->kind == AST_VARIANT_DECL) &&
        !realm_type_family_for_node(mono, node) &&
        declaration_contains_realm_type(mono, node)) {
      RealmTypeFamily *family =
          arena_alloc(mono->arena, sizeof(*family));
      memset(family, 0, sizeof(*family));
      family->name = declaration_name(node);
      family->module = module;
      family->fallback = node;
      family->next = mono->realm_type_families;
      mono->realm_type_families = family;
      changed = true;
    }
    if (node->kind == AST_MOD_DECL &&
        collect_derived_realm_type_families_pass(
            mono, node->as.mod_decl.declarations,
            node->as.mod_decl.name))
      changed = true;
  }
  return changed;
}

static void collect_derived_realm_type_families(Monomorphizer *mono) {
  while (collect_derived_realm_type_families_pass(
      mono, mono->program->as.program.declarations, NULL)) {
  }
}

static void collect_realm_function_families(Monomorphizer *mono,
                                            AstNode *declarations,
                                            const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (node->kind == AST_FUNC_DECL &&
        (node->realm_family_root || node->has_overload_realm ||
         node->excluded_realms)) {
      AstNode *root = node->realm_family_root ? node->realm_family_root : node;
      if (!realm_family_for_node(mono, root)) {
        RealmFunctionFamily *family =
            arena_alloc(mono->arena, sizeof(*family));
        memset(family, 0, sizeof(*family));
        family->name = root->as.func_decl.name;
        family->module = module;
        family->fallback = root->realm_family_root
                               ? root->realm_family_fallback
                               : (root->has_overload_realm ? NULL : root);
        family->exclusions = root->excluded_realms;
        if (root->realm_family_root) {
          for (unsigned realm = 0; realm < 4; realm++)
            family->variants[realm] = root->realm_family_variants[realm];
        } else if (root->has_overload_realm) {
          family->variants[root->overload_realm] = root;
        }
        family->next = mono->realm_function_families;
        mono->realm_function_families = family;
      }
    }
    if (node->kind == AST_MOD_DECL)
      collect_realm_function_families(
          mono, node->as.mod_decl.declarations, node->as.mod_decl.name);
  }
}

static RealmMethodFamily *get_realm_method_family(
    Monomorphizer *mono, const char *module, const char *owner,
    const char *interface_name, const char *name) {
  for (RealmMethodFamily *family = mono->realm_method_families; family;
       family = family->next) {
    bool same_module = (!module && !family->module) ||
                       (module && family->module &&
                        strcmp(module, family->module) == 0);
    bool same_interface =
        (!interface_name && !family->interface_name) ||
        (interface_name && family->interface_name &&
         strcmp(interface_name, family->interface_name) == 0);
    if (same_module && same_interface &&
        strcmp(owner, family->owner_name) == 0 &&
        strcmp(name, family->name) == 0)
      return family;
  }
  RealmMethodFamily *family = arena_alloc(mono->arena, sizeof(*family));
  memset(family, 0, sizeof(*family));
  family->owner_name = owner;
  family->interface_name = interface_name;
  family->name = name;
  family->module = module;
  family->next = mono->realm_method_families;
  mono->realm_method_families = family;
  return family;
}

static void add_realm_method_case(Monomorphizer *mono, AstNode *block,
                                  AstNode *method, const char *module,
                                  bool exact, EffectiveRealm realm,
                                  uint8_t exclusions) {
  RealmMethodFamily *family = get_realm_method_family(
      mono, module, block->as.method_decl.type_name,
      block->as.method_decl.iface_name, method->as.func_decl.name);
  if (family->exclusions && family->exclusions != exclusions)
    mono_error(mono, method,
               "method '%s' has inconsistent realm exclusions",
               method->as.func_decl.name);
  family->exclusions = exclusions;
  if (exact) {
    if (family->variants[realm] && family->variants[realm] != method)
      mono_error(mono, method, "method '%s' has duplicate %s definitions",
                 method->as.func_decl.name, effective_realm_name(realm));
    family->variants[realm] = method;
    family->variant_blocks[realm] = block;
  } else {
    if (family->fallback && family->fallback != method)
      mono_error(mono, method, "method '%s' has multiple shared fallbacks",
                 method->as.func_decl.name);
    family->fallback = method;
    family->fallback_block = block;
  }
}

static void collect_realm_method_families(Monomorphizer *mono,
                                          AstNode *declarations,
                                          const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (node->kind == AST_METHOD_DECL) {
      bool block_special = node->realm_family_root ||
                           node->has_overload_realm ||
                           node->excluded_realms;
      for (AstNode *method = node->as.method_decl.methods; method;
           method = method->next) {
        if (block_special) {
          add_realm_method_case(
              mono, node, method, module, node->has_overload_realm,
              node->overload_realm, node->excluded_realms);
          continue;
        }
        if (!(method->realm_family_root || method->has_overload_realm ||
              method->excluded_realms))
          continue;
        AstNode *root =
            method->realm_family_root ? method->realm_family_root : method;
        if (method != root)
          continue;
        uint8_t exclusions = root->excluded_realms;
        if (root->realm_family_root) {
          if (root->realm_family_fallback)
            add_realm_method_case(mono, node, root->realm_family_fallback,
                                  module, false, EFFECTIVE_REALM_STACK,
                                  exclusions);
          for (unsigned realm = 0; realm < 4; realm++)
            if (root->realm_family_variants[realm])
              add_realm_method_case(
                  mono, node, root->realm_family_variants[realm], module, true,
                  (EffectiveRealm)realm, exclusions);
        } else {
          add_realm_method_case(mono, node, root, module,
                                root->has_overload_realm,
                                root->overload_realm, exclusions);
        }
      }
    }
    if (node->kind == AST_MOD_DECL)
      collect_realm_method_families(
          mono, node->as.mod_decl.declarations, node->as.mod_decl.name);
  }
}

static bool is_realm_method_member(Monomorphizer *mono, AstNode *method) {
  for (RealmMethodFamily *family = mono->realm_method_families; family;
       family = family->next) {
    if (family->fallback == method)
      return true;
    for (unsigned realm = 0; realm < 4; realm++)
      if (family->variants[realm] == method)
        return true;
  }
  return false;
}

static void collect_templates(Monomorphizer *mono, AstNode *declarations,
                              const char *module) {
  for (AstNode *node = declarations; node; node = node->next) {
    if (generic_params(node) && !is_realm_function_member(mono, node)) {
      GenericTemplate *entry = arena_alloc(mono->arena, sizeof(*entry));
      entry->node = node;
      entry->module = module;
      entry->next = mono->templates;
      mono->templates = entry;
    }
    if (node->kind == AST_FUNC_DECL &&
        node->as.func_decl.realm == REALM_FLEX &&
        !node->as.func_decl.generic_params &&
        !is_realm_function_member(mono, node)) {
      FlexTemplate *entry = arena_alloc(mono->arena, sizeof(*entry));
      entry->node = node;
      entry->module = module;
      entry->next = mono->flex_templates;
      mono->flex_templates = entry;
    }
    if (node->kind == AST_MOD_DECL)
      collect_templates(mono, node->as.mod_decl.declarations,
                        node->as.mod_decl.name);
  }
}

static AstNode *find_unsupported_realm_declaration(AstNode *node) {
  for (; node; node = node->next) {
    if (node->kind == AST_INTERFACE_DECL &&
        (node->realm_family_root || node->has_overload_realm ||
         node->excluded_realms))
      return node;
    if (node->kind == AST_INTERFACE_DECL) {
      for (AstNode *method = node->as.interface_decl.methods; method;
           method = method->next)
        if (method->realm_family_root || method->has_overload_realm ||
            method->excluded_realms)
          return method;
    }
    if (node->kind == AST_MOD_DECL) {
      AstNode *nested = find_unsupported_realm_declaration(
          node->as.mod_decl.declarations);
      if (nested)
        return nested;
    }
  }
  return NULL;
}

bool monomorphize_program(Arena *arena, AstNode *program) {
  if (!program || program->kind != AST_PROGRAM)
    return false;
  AstNode *unsupported =
      find_unsupported_realm_declaration(program->as.program.declarations);
  if (unsupported) {
    fprintf(stderr,
            "Error at %u:%u: realm-specific interface selection is "
            "not implemented yet\n",
            unsupported->line, unsupported->col);
    return false;
  }
  Monomorphizer mono = {
      .arena = arena,
      .program = program,
      .has_current_effective_realm = true,
      .current_effective_realm = EFFECTIVE_REALM_DYNAMIC,
  };
  collect_realm_type_families(
      &mono, program->as.program.declarations, NULL);
  collect_derived_realm_type_families(&mono);
  collect_realm_function_families(
      &mono, program->as.program.declarations, NULL);
  collect_realm_method_families(
      &mono, program->as.program.declarations, NULL);
  collect_realm_signature_templates(
      &mono, program->as.program.declarations, NULL);
  collect_templates(&mono, program->as.program.declarations, NULL);
  collect_imports(&mono, program->as.program.declarations, NULL);

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
    if (is_template_declaration(node) ||
        is_realm_function_member(&mono, node) ||
        is_realm_type_member(&mono, node) ||
        is_realm_type_method_block(&mono, node) ||
        is_realm_signature_template(&mono, node))
      continue;
    bool saved_has_realm = mono.has_current_effective_realm;
    if (node->kind == AST_FUNC_DECL &&
        node->as.func_decl.realm == REALM_FLEX)
      mono.has_current_effective_realm = false;
    AstNode *copy = clone_node(&mono, node, NULL);
    mono.has_current_effective_realm = saved_has_realm;
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
