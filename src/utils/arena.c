#include "arena.h"

#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

static size_t align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

static ArenaBlock *block_new(size_t min_size) {
    size_t size = min_size > ARENA_BLOCK_SIZE ? min_size : ARENA_BLOCK_SIZE;
    ArenaBlock *b = malloc(sizeof(ArenaBlock) + size);
    if (!b) return NULL;
    b->next = NULL;
    b->ptr  = b->data;
    b->end  = b->data + size;
    return b;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void arena_init(Arena *a) {
    a->first   = block_new(ARENA_BLOCK_SIZE);
    a->current = a->first;
}

void arena_destroy(Arena *a) {
    ArenaBlock *b = a->first;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->first   = NULL;
    a->current = NULL;
}

void arena_reset(Arena *a) {
    /* rewind all blocks, keep the chain alive for reuse */
    ArenaBlock *b = a->first;
    while (b) {
        b->ptr = b->data;
        b = b->next;
    }
    a->current = a->first;
}

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    if (size == 0) size = 1;

    /* try current block first */
    ArenaBlock *b = a->current;
    char *aligned = (char *)align_up((size_t)b->ptr, align);

    if (aligned + size <= b->end) {
        b->ptr = aligned + size;
        memset(aligned, 0, size);
        return aligned;
    }

    /* current block is full — walk chain for a block with enough space */
    b = a->first;
    while (b) {
        aligned = (char *)align_up((size_t)b->ptr, align);
        if (aligned + size <= b->end) {
            a->current = b;
            b->ptr = aligned + size;
            memset(aligned, 0, size);
            return aligned;
        }
        b = b->next;
    }

    /* no existing block fits — allocate a new one */
    ArenaBlock *fresh = block_new(size + align);
    if (!fresh) return NULL;

    /* append to chain */
    b = a->first;
    while (b->next) b = b->next;
    b->next = fresh;
    a->current = fresh;

    aligned = (char *)align_up((size_t)fresh->ptr, align);
    fresh->ptr = aligned + size;
    memset(aligned, 0, size);
    return aligned;
}

void *arena_alloc(Arena *a, size_t size) {
    return arena_alloc_aligned(a, size, ARENA_ALIGN);
}

char *arena_strdup(Arena *a, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char  *dst = arena_alloc(a, len);
    memcpy(dst, s, len);
    return dst;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    if (!s) return NULL;
    char *dst = arena_alloc(a, n + 1);
    memcpy(dst, s, n);
    dst[n] = '\0';
    return dst;
}