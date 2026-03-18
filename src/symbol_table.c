#include "symbol_table.h"
#include <stdint.h>
#include <string.h>

#define INITIAL_CAPACITY 64

// Simple hash for pointers
static uint32_t hash_ptr(const void *ptr) {
  uintptr_t val = (uintptr_t)ptr;
  return (uint32_t)((val >> 3) ^ (val >> 16));
}

static Scope *scope_new(Arena *arena, Scope *parent) {
  Scope *s = arena_alloc(arena, sizeof(Scope));
  s->parent = parent;
  s->capacity = INITIAL_CAPACITY;
  s->count = 0;
  s->buckets = arena_alloc_aligned(arena, sizeof(ScopeEntry *) * s->capacity,
                                   _Alignof(ScopeEntry *));
  memset(s->buckets, 0, sizeof(ScopeEntry *) * s->capacity);
  return s;
}

void symbol_table_init(SymbolTable *st, Arena *arena) {
  st->arena = arena;
  st->current = scope_new(arena, NULL);
}

void symbol_table_push(SymbolTable *st) {
  st->current = scope_new(st->arena, st->current);
}

void symbol_table_pop(SymbolTable *st) {
  if (st->current->parent) {
    st->current = st->current->parent;
  }
}

bool symbol_table_define(SymbolTable *st, Symbol sym) {
  Scope *s = st->current;

  // Check if already defined in CURRENT scope
  uint32_t h = hash_ptr(sym.name);
  uint32_t slot = h % s->capacity;

  ScopeEntry *e = s->buckets[slot];
  while (e) {
    if (e->symbol.name == sym.name) {
      return false; // Already defined
    }
    e = e->next;
  }

  // Create new entry
  ScopeEntry *new_entry = arena_alloc(st->arena, sizeof(ScopeEntry));
  new_entry->symbol = sym;
  new_entry->next = s->buckets[slot];
  s->buckets[slot] = new_entry;
  s->count++;

  return true;
}

Symbol *symbol_table_lookup(SymbolTable *st, const char *name) {
  Scope *s = st->current;
  while (s) {
    uint32_t h = hash_ptr(name);
    uint32_t slot = h % s->capacity;

    ScopeEntry *e = s->buckets[slot];
    while (e) {
      if (e->symbol.name == name) {
        return &e->symbol;
      }
      e = e->next;
    }
    s = s->parent;
  }
  return NULL;
}

Symbol *symbol_table_lookup_local(SymbolTable *st, const char *name) {
  Scope *s = st->current;
  uint32_t h = hash_ptr(name);
  uint32_t slot = h % s->capacity;

  ScopeEntry *e = s->buckets[slot];
  while (e) {
    if (e->symbol.name == name) {
      return &e->symbol;
    }
    e = e->next;
  }
  return NULL;
}
