#include "arena.h"

#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static size_t align_up(size_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}

static ArenaBlock *block_new(size_t min_size) {
  size_t size = min_size > ARENA_BLOCK_SIZE ? min_size : ARENA_BLOCK_SIZE;
  ArenaBlock *b = malloc(sizeof(ArenaBlock) + size);
  if (!b)
    return NULL;
  b->next = NULL;
  b->ptr = b->data;
  b->end = b->data + size;
  return b;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void arena_init(Arena *a) {
  a->first = block_new(ARENA_BLOCK_SIZE);
  a->current = a->first;
}

void arena_destroy(Arena *a) {
  ArenaBlock *b = a->first;
  while (b) {
    ArenaBlock *next = b->next;
    free(b);
    b = next;
  }
  a->first = NULL;
  a->current = NULL;
}

void arena_reset(Arena *a) {
  // Rewind every block in the chain so they are all available for reuse.
  // a->current goes back to the first block.
  ArenaBlock *b = a->first;
  while (b) {
    b->ptr = b->data;
    b = b->next;
  }
  a->current = a->first;
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation
// ─────────────────────────────────────────────────────────────────────────────

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
  if (size == 0)
    size = 1;

  // Walk the chain from current forward looking for a block with enough
  // space. This is O(k) where k is the number of blocks skipped — in
  // practice almost always 0 or 1 after a reset, never pathological during
  // normal forward allocation.
  ArenaBlock *b = a->current;
  while (b) {
    char *aligned = (char *)align_up((uintptr_t)b->ptr, align);
    if (aligned + size <= b->end) {
      a->current = b;
      b->ptr = aligned + size;
      memset(aligned, 0, size);
      return aligned;
    }
    b = b->next;
  }

  // No existing block fits. Allocate a fresh one and splice it in after
  // current so we don't lose the rest of the chain.
  ArenaBlock *fresh = block_new(size + align);
  if (!fresh)
    return NULL;

  fresh->next = a->current->next;
  a->current->next = fresh;
  a->current = fresh;

  char *aligned = (char *)align_up((uintptr_t)fresh->ptr, align);
  fresh->ptr = aligned + size;
  memset(aligned, 0, size);
  return aligned;
}

void *arena_alloc(Arena *a, size_t size) {
  return arena_alloc_aligned(a, size, ARENA_ALIGN);
}

// ─────────────────────────────────────────────────────────────────────────────
// String helpers
// ─────────────────────────────────────────────────────────────────────────────

char *arena_strdup(Arena *a, const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s) + 1;
  char *dst = arena_alloc(a, len);
  memcpy(dst, s, len);
  return dst;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
  if (!s)
    return NULL;
  char *dst = arena_alloc(a, n + 1);
  memcpy(dst, s, n);
  dst[n] = '\0';
  return dst;
}

// ─────────────────────────────────────────────────────────────────────────────
// Snapshot / restore
// ─────────────────────────────────────────────────────────────────────────────

ArenaSnapshot arena_snapshot(const Arena *a) {
  return (ArenaSnapshot){
      .block = a->current,
      .ptr = a->current->ptr,
  };
}

void arena_restore(Arena *a, ArenaSnapshot snap) {
  // Rewind the saved block to its saved cursor position.
  snap.block->ptr = snap.ptr;

  // Zero-rewind every block that came after snap.block in the chain.
  // This ensures no stale data is left and that the chain is fully reusable,
  // which matters for correctness after nested snapshot/restore pairs.
  ArenaBlock *b = snap.block->next;
  while (b) {
    b->ptr = b->data;
    b = b->next;
  }

  // Restore current.
  a->current = snap.block;
}