#ifndef RUNES_TYPECHECK_H
#define RUNES_TYPECHECK_H

#include "symbol_table.h"
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  Arena *arena;
  TypeContext *tctx;
  SymbolTable *st;

  uint32_t error_count;
  bool had_error;

  Type *expected_ret;
  MemoryRealm current_realm;
  int loop_depth;
} TypeChecker;

void typechecker_init(TypeChecker *tc, Arena *arena, TypeContext *tctx,
                      SymbolTable *st);
void typechecker_error(TypeChecker *tc, uint32_t line, uint32_t col,
                       const char *fmt, ...);

Type *typechecker_resolve_type_expr(TypeChecker *tc, AstNode *node);
Type *typechecker_infer_expr(TypeChecker *tc, AstNode *expr);
void typechecker_check(TypeChecker *tc, AstNode *program);

#endif // RUNES_TYPECHECK_H
