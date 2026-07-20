#ifndef RUNES_CODEGEN_H
#define RUNES_CODEGEN_H

#include "ast.h"
#include "types.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  FILE *out;
  bool had_error;
  int error_count;
  unsigned temp_id;
  Type *current_fallible;
  const char *current_result_name;
  const char *current_result_c_name;
  const char *current_error_name;
  Type *emitted_results[64];
  int emitted_result_count;
  Type *emitted_tuples[64];
  int emitted_tuple_count;
  Type *emitted_arrays[64];
  int emitted_array_count;
  AstNode *named_decls[256];
  char named_decl_names[256][256];
  int named_decl_count;
  Type *named_types[128];
  char named_type_names[128][256];
  int named_type_count;
  AstNode *runtime_initializers[128];
  int runtime_initializer_count;
  AstNode *nested_functions[128];
  int nested_function_count;
  AstNode *interface_impls[64];
  int interface_impl_count;
  const char *current_prefix;
} Codegen;

void codegen_init(Codegen *cg, FILE *out);
bool codegen_emit_c(Codegen *cg, AstNode *program);

#endif // RUNES_CODEGEN_H
