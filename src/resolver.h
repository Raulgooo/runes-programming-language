#ifndef RUNES_RESOLVER_H
#define RUNES_RESOLVER_H

#include "symbol_table.h"

typedef struct {
  SymbolTable *st;
  int error_count;
  bool had_error;
} Resolver;

void resolver_init(Resolver *r, SymbolTable *st);
void resolver_resolve(Resolver *r, AstNode *node);

#endif // RUNES_RESOLVER_H
