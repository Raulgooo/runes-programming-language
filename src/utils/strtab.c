#include "strtab.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// FNV-1a 64-bit hash
// ─────────────────────────────────────────────────────────────────────────────
static size_t fnv1a(const char *ptr, size_t len) {
  size_t h = (size_t)14695981039346656037ULL;
  for (size_t i = 0; i < len; i++) {
    h ^= (unsigned char)ptr[i];
    h *= 1099511628211ULL;
  }
  return h;
}

// ─────────────────────────────────────────────────────────────────────────────
// Grow: allocate a fresh bucket array of size new_cap from the arena and
// rehash all existing entries into it.
// Note: we never free the old bucket array — it stays in the arena.
// ─────────────────────────────────────────────────────────────────────────────
static void strtab_grow(StrTab *st) {
  size_t new_cap = st->capacity * 2;
  StrTabEntry *new_buckets =
      (StrTabEntry *)arena_alloc_aligned(st->arena,
                                         sizeof(StrTabEntry) * new_cap,
                                         _Alignof(StrTabEntry));
  memset(new_buckets, 0, sizeof(StrTabEntry) * new_cap);

  // Rehash
  for (size_t i = 0; i < st->capacity; i++) {
    if (!st->buckets[i].ptr)
      continue;
    size_t h = fnv1a(st->buckets[i].ptr, st->buckets[i].len);
    size_t slot = h & (new_cap - 1);
    while (new_buckets[slot].ptr)
      slot = (slot + 1) & (new_cap - 1);
    new_buckets[slot] = st->buckets[i];
  }

  st->buckets  = new_buckets;
  st->capacity = new_cap;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void strtab_init(StrTab *st, Arena *arena) {
  st->arena    = arena;
  st->count    = 0;
  st->capacity = STRTAB_INIT_BUCKETS;
  st->buckets  = (StrTabEntry *)arena_alloc_aligned(
      arena,
      sizeof(StrTabEntry) * STRTAB_INIT_BUCKETS,
      _Alignof(StrTabEntry));
  memset(st->buckets, 0, sizeof(StrTabEntry) * STRTAB_INIT_BUCKETS);
}

const char *strtab_intern(StrTab *st, const char *ptr, size_t len) {
  // Grow if load factor >= 75%
  if (st->count * 4 >= st->capacity * 3)
    strtab_grow(st);

  size_t h    = fnv1a(ptr, len);
  size_t slot = h & (st->capacity - 1);

  // Probe
  while (st->buckets[slot].ptr) {
    StrTabEntry *e = &st->buckets[slot];
    if (e->len == len && memcmp(e->ptr, ptr, len) == 0)
      return e->ptr; // already interned
    slot = (slot + 1) & (st->capacity - 1);
  }

  // New entry — copy into arena (len + 1 for NUL terminator)
  char *copy = (char *)arena_alloc(st->arena, len + 1);
  memcpy(copy, ptr, len);
  copy[len] = '\0';

  st->buckets[slot].ptr = copy;
  st->buckets[slot].len = len;
  st->count++;

  return copy;
}
