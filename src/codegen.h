#ifndef RUNES_CODEGEN_H
#define RUNES_CODEGEN_H

#include "ast.h"
#include "types.h"
#include "utils/arena.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  AstNode *decl;
  const char *name;
} CodegenNamedDecl;

typedef struct {
  Type *type;
  const char *name;
} CodegenNamedType;

typedef struct {
  FILE *out;
  Arena *arena;
  bool had_error;
  int error_count;
  unsigned temp_id;
  Type *current_fallible;
  Type *current_return_type;
  const char *current_result_name;
  const char *current_result_c_name;
  const char *current_error_name;
  AstNode *current_function;
  bool current_arena_scope;
  bool current_gc_frame;
  bool current_gc_scope;
  bool current_cleanup_used;
  bool descriptor_phase;
  Type **emitted_results;
  int emitted_result_count;
  int emitted_result_capacity;
  Type **emitted_tuples;
  int emitted_tuple_count;
  int emitted_tuple_capacity;
  Type **emitted_arrays;
  int emitted_array_count;
  int emitted_array_capacity;
  Type **emitted_slices;
  int emitted_slice_count;
  int emitted_slice_capacity;
  Type **emitted_closures;
  int emitted_closure_count;
  int emitted_closure_capacity;
  Type **emitted_descriptors;
  int emitted_descriptor_count;
  int emitted_descriptor_capacity;
  Type **completed_types;
  int completed_type_count;
  int completed_type_capacity;
  Type **active_types;
  int active_type_count;
  int active_type_capacity;
  CodegenNamedDecl *named_decls;
  int named_decl_count;
  int named_decl_capacity;
  CodegenNamedType *named_types;
  int named_type_count;
  int named_type_capacity;
  AstNode **runtime_initializers;
  int runtime_initializer_count;
  int runtime_initializer_capacity;
  AstNode **nested_functions;
  int nested_function_count;
  int nested_function_capacity;
  AstNode **interface_impls;
  int interface_impl_count;
  int interface_impl_capacity;
  AstNode **gc_globals;
  int gc_global_count;
  int gc_global_capacity;
  const char **gc_root_tokens;
  int gc_root_count;
  int gc_root_capacity;
  int loop_gc_root_starts[64];
  int loop_codegen_depth;
  AstNode **deferred;
  int deferred_count;
  int deferred_capacity;
  int loop_defer_starts[64];
  const char *current_prefix;
} Codegen;

void codegen_init(Codegen *cg, FILE *out, Arena *arena);
bool codegen_emit_c(Codegen *cg, AstNode *program);

#endif // RUNES_CODEGEN_H
