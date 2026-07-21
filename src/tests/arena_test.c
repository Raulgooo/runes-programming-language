#include "../utils/arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  Arena root;
  assert(arena_init_with_block_size(&root, 128));

  uint64_t *aligned = arena_alloc_aligned(&root, sizeof(*aligned), 64);
  assert(aligned != NULL);
  assert((uintptr_t)aligned % 64 == 0);
  *aligned = 42;
  assert(arena_owns(&root, aligned));
  assert(arena_alloc_aligned(&root, 8, 3) == NULL);

  ArenaSnapshot snapshot = arena_snapshot(&root);
  char *temporary = arena_strdup(&root, "temporary");
  assert(temporary && strcmp(temporary, "temporary") == 0);
  arena_restore(&root, snapshot);

  Arena *child = arena_create_child(&root);
  assert(child != NULL);
  char *child_value = arena_strdup(child, "child survives return");
  assert(child_value != NULL);
  assert(arena_owns(child, child_value));
  assert(arena_owns(&root, child_value));

  void *large = arena_alloc_aligned(child, 4096, 128);
  assert(large != NULL);
  assert((uintptr_t)large % 128 == 0);
  assert(arena_owns(&root, large));

  arena_destroy(&root);
  assert(root.first == NULL);
  assert(root.first_child == NULL);
  puts("arena tests passed");
  return 0;
}
