#ifndef RUNES_SYMBOL_TABLE_H
#define RUNES_SYMBOL_TABLE_H

#include "ast.h"
#include "utils/arena.h"
#include <stdbool.h>

typedef enum {
  SYM_VAR,
  SYM_FUNC,
  SYM_TYPE,
  SYM_SCHEMA,
  SYM_IFACE,
  SYM_MOD,
  SYM_ERROR,
} SymbolKind;

typedef struct Symbol {
  const char *name; // interned
  SymbolKind kind;
  AstNode *node; // the declaration node
  void *type;    // placeholder for type checking phase
  bool is_pub;
} Symbol;

typedef struct ScopeEntry {
  Symbol symbol;
  struct ScopeEntry *next; // for collisions (chaining)
} ScopeEntry;

typedef struct Scope {
  struct Scope *parent;
  ScopeEntry **buckets;
  uint32_t capacity;
  uint32_t count;
} Scope;

typedef struct {
  Scope *current;
  Arena *arena;
} SymbolTable;

void symbol_table_init(SymbolTable *st, Arena *arena);
void symbol_table_push(SymbolTable *st);
void symbol_table_pop(SymbolTable *st);

bool symbol_table_define(SymbolTable *st, Symbol sym);
Symbol *symbol_table_lookup(SymbolTable *st, const char *name);
Symbol *symbol_table_lookup_local(SymbolTable *st, const char *name);

#endif // RUNES_SYMBOL_TABLE_H