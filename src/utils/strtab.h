#ifndef RUNES_STRTAB_H
#define RUNES_STRTAB_H

#include "arena.h"
#include <stddef.h>

// ─────────────────────────────────────────────────────────────────────────────
// String Table — fast interning backed by an Arena
//
// Every unique string is stored exactly once in the arena.  Interning returns
// a stable, NUL-terminated `const char *` valid for the lifetime of the arena.
//
// Collision resolution: open addressing with FNV-1a hashing.
// ─────────────────────────────────────────────────────────────────────────────

#ifndef STRTAB_INIT_BUCKETS
#define STRTAB_INIT_BUCKETS 256 // must be a power of two
#endif

typedef struct {
  const char *ptr; // interned NUL-terminated string in arena (NULL = empty slot)
  size_t      len; // byte length (not counting NUL)
} StrTabEntry;

typedef struct {
  StrTabEntry *buckets;
  size_t       count;    // number of occupied entries
  size_t       capacity; // number of buckets (always power of two)
  Arena       *arena;    // strings and bucket arrays live here
} StrTab;

// Initialize the table.  `arena` must outlive the StrTab.
void strtab_init(StrTab *st, Arena *arena);

// Intern the byte string [ptr, ptr+len).
// Returns a stable, NUL-terminated pointer into the arena.
// Two calls with equal content return the SAME pointer.
const char *strtab_intern(StrTab *st, const char *ptr, size_t len);

#endif // RUNES_STRTAB_H
