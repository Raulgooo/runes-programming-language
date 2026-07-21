#ifndef RUNES_RUNTIME_H
#define RUNES_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RunesStr {
  const uint8_t *ptr;
  size_t len;
} RunesStr;

typedef struct RunesCloneContext RunesCloneContext;
typedef struct RunesTypeDescriptor RunesTypeDescriptor;
typedef void (*RunesGcVisit)(const void *pointer, void *context);
typedef void (*RunesCloneFunction)(void *destination, const void *source,
                                   RunesCloneContext *context);
typedef void (*RunesTraceFunction)(const void *value, RunesGcVisit visit,
                                   void *context);

struct RunesTypeDescriptor {
  size_t size;
  size_t align;
  RunesCloneFunction clone;
  RunesTraceFunction trace;
};

typedef struct {
  size_t object_count;
  size_t allocated_bytes;
  size_t collections;
} RunesGcStats;

void *runes_arena_scope_enter(unsigned line, unsigned column);
void runes_arena_scope_leave(void *scope);
size_t runes_debug_live_arena_scopes(void);
size_t runes_debug_live_arena_roots(void);

void *runes_alloc(size_t size, size_t align, unsigned line, unsigned column);
void *runes_alloc_typed(size_t size, size_t align,
                        const RunesTypeDescriptor *descriptor, unsigned line,
                        unsigned column);
void *runes_raw_alloc(size_t size, unsigned line, unsigned column);
void *runes_raw_alloc_aligned(size_t size, size_t align, unsigned line,
                              unsigned column);
void runes_raw_free(void *pointer);

void runes_gc_scope_enter(unsigned line, unsigned column);
void runes_gc_scope_leave(void);
void *runes_gc_frame_enter(unsigned line, unsigned column);
void *runes_gc_frame_enter_if_active(unsigned line, unsigned column);
void runes_gc_frame_leave(void *frame);
void *runes_gc_root_push(void *value, const RunesTypeDescriptor *descriptor,
                         unsigned line, unsigned column);
void runes_gc_root_freeze(void *root, unsigned line, unsigned column);
void *runes_gc_alloc(size_t size, size_t align,
                     const RunesTypeDescriptor *descriptor, unsigned line,
                     unsigned column);
void runes_gc_collect(void);
RunesGcStats runes_gc_stats(void);
void runes_gc_set_threshold(size_t bytes);
size_t runes_gc_debug_object_count(void);
size_t runes_gc_debug_collection_count(void);
void runes_gc_commit_allocations(void);
void runes_gc_protect_value(const void *value,
                            const RunesTypeDescriptor *descriptor);
void runes_gc_trace_value(const void *value,
                          const RunesTypeDescriptor *descriptor,
                          RunesGcVisit visit, void *context);

void *runes_promote_dynamic(const void *source,
                            const RunesTypeDescriptor *descriptor,
                            unsigned line, unsigned column);
void *runes_promote_gc(const void *source,
                       const RunesTypeDescriptor *descriptor, unsigned line,
                       unsigned column);
void *runes_clone_pointer(RunesCloneContext *context, const void *source,
                          const RunesTypeDescriptor *descriptor);
void *runes_clone_slice(RunesCloneContext *context, const void *source,
                        size_t length,
                        const RunesTypeDescriptor *element_descriptor);
void runes_clone_value(RunesCloneContext *context, void *destination,
                       const void *source,
                       const RunesTypeDescriptor *descriptor);
RunesStr runes_clone_string(RunesCloneContext *context, RunesStr source);

bool runes_str_equal(RunesStr left, RunesStr right);
int runes_str_compare(RunesStr left, RunesStr right);
RunesStr runes_str_concat(RunesStr left, RunesStr right, unsigned line,
                          unsigned column);
RunesStr runes_str_slice_bytes(RunesStr value, size_t start, size_t end,
                               bool inclusive, unsigned line,
                               unsigned column);
uint8_t runes_str_byte_at(RunesStr value, size_t index, unsigned line,
                          unsigned column);
bool runes_str_is_utf8(RunesStr value);
bool runes_str_is_scalar_boundary(RunesStr value, size_t index);
bool runes_str_decode_next(RunesStr value, size_t *index, uint32_t *scalar);
bool runes_utf8_encode(uint32_t scalar, uint8_t output[4], size_t *length);
uint32_t runes_char_from_u64(uint64_t value, unsigned line, unsigned column);
uint64_t runes_str_hash(RunesStr value);
RunesStr runes_str_from_c(const char *value);
bool runes_str_from_bytes(const uint8_t *data, size_t length,
                          RunesStr *result);
bool runes_str_from_c_checked(const char *value, RunesStr *result);
char *runes_str_to_c(RunesStr value, unsigned line, unsigned column);
void runes_str_write_stdout(RunesStr value);
void runes_char_write_stdout(uint32_t scalar, unsigned line, unsigned column);

#endif
