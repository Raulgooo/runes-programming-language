#ifndef RUNES_ARENA_H
#define RUNES_ARENA_H

#include <stddef.h>

/* -----------------------------------------------------------------------
 * Arena allocator
 *
 * Bump-pointer allocator. Alloc is O(1). Free is one call for everything.
 * Blocks are chained — the arena grows automatically when full.
 * ----------------------------------------------------------------------- */

#define ARENA_BLOCK_SIZE (1024 * 1024)  /* 1 MB per block */
#define ARENA_ALIGN      8              /* default alignment */

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    char              *ptr;
    char              *end;
    char               data[];  /* flexible array — block + data in one malloc */
} ArenaBlock;

typedef struct {
    ArenaBlock *current;
    ArenaBlock *first;
} Arena;

/* init / free */
void  arena_init(Arena *a);
void  arena_destroy(Arena *a);

/* reset — keeps allocated memory, rewinds pointer to start */
void  arena_reset(Arena *a);

/* alloc — returns zeroed memory, aligned to ARENA_ALIGN */
void *arena_alloc(Arena *a, size_t size);

/* alloc_aligned — explicit alignment (must be power of 2) */
void *arena_alloc_aligned(Arena *a, size_t size, size_t align);

/* convenience — alloc and copy a string */
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, size_t n);

#endif /* RUNES_ARENA_H */