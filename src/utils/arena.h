#ifndef RUNES_ARENA_H
#define RUNES_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// Tunables
// ─────────────────────────────────────────────────────────────────────────────

#ifndef ARENA_BLOCK_SIZE
#define ARENA_BLOCK_SIZE (64 * 1024) // 64 KiB per block
#endif

#ifndef ARENA_ALIGN
#define ARENA_ALIGN (_Alignof(max_align_t))
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Internal block (not for direct use outside arena.c)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct ArenaBlock {
  struct ArenaBlock *next;
  char *ptr;   // bump cursor
  char *end;   // one past the last usable byte
  char data[]; // FAM — actual memory follows the header
} ArenaBlock;

// ─────────────────────────────────────────────────────────────────────────────
// Arena
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Arena {
  ArenaBlock *first;
  ArenaBlock *current;
  struct Arena *parent;
  struct Arena *first_child;
  struct Arena *next_sibling;
  size_t block_size;
  size_t bytes_allocated;
} Arena;

// ─────────────────────────────────────────────────────────────────────────────
// Snapshot — lightweight save/restore point for parser backtracking
//
// Usage:
//   ArenaSnapshot snap = arena_snapshot(a);
//   ... try to parse something ...
//   if (failed) arena_restore(a, snap);  // rewind, no leak
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
  ArenaBlock *block; // which block was current at snapshot time
  char *ptr;         // where the bump cursor was in that block
} ArenaSnapshot;

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

// Lifecycle
bool arena_init(Arena *a);
bool arena_init_with_block_size(Arena *a, size_t block_size);
Arena *arena_create_child(Arena *parent);
void arena_destroy(Arena *a);
void arena_reset(Arena *a); // rewind everything, keep block chain alive
bool arena_owns(const Arena *a, const void *pointer);

// Allocation
void *arena_alloc(Arena *a, size_t size);
void *arena_alloc_aligned(Arena *a, size_t size, size_t align);

// String helpers — returned pointer lives in the arena
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, size_t n);

// Snapshot / restore
ArenaSnapshot arena_snapshot(const Arena *a);
void arena_restore(Arena *a, ArenaSnapshot snap);

// Convenience macro — allocate and zero a single typed value
#define ARENA_ALLOC(arena, T)                                                  \
  ((T *)arena_alloc_aligned((arena), sizeof(T), _Alignof(T)))

// Convenience macro — allocate and zero a typed array
#define ARENA_ALLOC_N(arena, T, n)                                             \
  ((T *)arena_alloc_aligned((arena), sizeof(T) * (n), _Alignof(T)))

#endif // RUNES_ARENA_H
