#include "arena.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

static bool align_up(uintptr_t value, size_t align, uintptr_t *result) {
  if (!is_power_of_two(align) || value > UINTPTR_MAX - (align - 1))
    return false;
  *result = (value + align - 1) & ~(uintptr_t)(align - 1);
  return true;
}

static ArenaBlock *block_new(size_t min_size, size_t block_size) {
  size_t size = min_size > block_size ? min_size : block_size;
  if (size > SIZE_MAX - sizeof(ArenaBlock))
    return NULL;
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

bool arena_init_with_block_size(Arena *a, size_t block_size) {
  if (!a || block_size == 0)
    return false;
  memset(a, 0, sizeof(*a));
  a->block_size = block_size;
  a->first = block_new(block_size, block_size);
  a->current = a->first;
  return a->first != NULL;
}

bool arena_init(Arena *a) {
  return arena_init_with_block_size(a, ARENA_BLOCK_SIZE);
}

Arena *arena_create_child(Arena *parent) {
  if (!parent || !parent->first)
    return NULL;
  Arena *child = malloc(sizeof(*child));
  if (!child || !arena_init_with_block_size(child, parent->block_size)) {
    free(child);
    return NULL;
  }
  child->parent = parent;
  child->next_sibling = parent->first_child;
  parent->first_child = child;
  return child;
}

void arena_destroy(Arena *a) {
  if (!a)
    return;
  Arena *child = a->first_child;
  while (child) {
    Arena *next = child->next_sibling;
    child->parent = NULL;
    arena_destroy(child);
    free(child);
    child = next;
  }
  ArenaBlock *b = a->first;
  while (b) {
    ArenaBlock *next = b->next;
    free(b);
    b = next;
  }
  a->first = NULL;
  a->current = NULL;
  a->first_child = NULL;
  a->bytes_allocated = 0;

  if (a->parent) {
    Arena **link = &a->parent->first_child;
    while (*link && *link != a)
      link = &(*link)->next_sibling;
    if (*link == a)
      *link = a->next_sibling;
  }
  a->parent = NULL;
  a->next_sibling = NULL;
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
  a->bytes_allocated = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation
// ─────────────────────────────────────────────────────────────────────────────

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
  if (!a || !a->current || !is_power_of_two(align))
    return NULL;
  if (size == 0)
    size = 1;

  // Walk the chain from current forward looking for a block with enough
  // space. This is O(k) where k is the number of blocks skipped — in
  // practice almost always 0 or 1 after a reset, never pathological during
  // normal forward allocation.
  ArenaBlock *b = a->current;
  while (b) {
    uintptr_t aligned_value;
    if (!align_up((uintptr_t)b->ptr, align, &aligned_value))
      return NULL;
    char *aligned = (char *)aligned_value;
    if (aligned <= b->end && size <= (size_t)(b->end - aligned)) {
      a->current = b;
      b->ptr = aligned + size;
      if (a->bytes_allocated <= SIZE_MAX - size)
        a->bytes_allocated += size;
      memset(aligned, 0, size);
      return aligned;
    }
    b = b->next;
  }

  // No existing block fits. Allocate a fresh one and splice it in after
  // current so we don't lose the rest of the chain.
  if (size > SIZE_MAX - align)
    return NULL;
  ArenaBlock *fresh = block_new(size + align, a->block_size);
  if (!fresh)
    return NULL;

  fresh->next = a->current->next;
  a->current->next = fresh;
  a->current = fresh;

  uintptr_t aligned_value;
  if (!align_up((uintptr_t)fresh->ptr, align, &aligned_value)) {
    a->current = a->first;
    return NULL;
  }
  char *aligned = (char *)aligned_value;
  fresh->ptr = aligned + size;
  a->bytes_allocated += size;
  memset(aligned, 0, size);
  return aligned;
}

bool arena_owns_direct(const Arena *a, const void *pointer) {
  if (!a || !pointer)
    return false;
  uintptr_t address = (uintptr_t)pointer;
  for (const ArenaBlock *block = a->first; block; block = block->next) {
    uintptr_t begin = (uintptr_t)block->data;
    uintptr_t end = (uintptr_t)block->ptr;
    if (address >= begin && address < end)
      return true;
  }
  return false;
}

bool arena_owns(const Arena *a, const void *pointer) {
  if (arena_owns_direct(a, pointer))
    return true;
  for (const Arena *child = a->first_child; child;
       child = child->next_sibling) {
    if (arena_owns(child, pointer))
      return true;
  }
  return false;
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
  if (!a || !a->current)
    return (ArenaSnapshot){0};
  return (ArenaSnapshot){
      .block = a->current,
      .ptr = a->current->ptr,
  };
}

void arena_restore(Arena *a, ArenaSnapshot snap) {
  if (!a || !snap.block)
    return;
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
