#if defined(__linux__) && !defined(RUNES_FREESTANDING)
#define _POSIX_C_SOURCE 200809L
#endif

#include "runtime.h"
#include "utils/arena.h"

#if defined(__linux__) && !defined(RUNES_FREESTANDING)
#include <signal.h>
#endif
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Arena *arena;
  Arena *previous;
  bool owns_root;
} RunesArenaScope;

static _Thread_local Arena *runes_active_arena;
static _Thread_local size_t runes_live_arena_scopes;
static _Thread_local size_t runes_live_arena_roots;

typedef struct RunesGcObject {
  struct RunesGcObject *next;
  struct RunesGcObject *transient_next;
  const RunesTypeDescriptor *descriptor;
  void *payload;
  void *allocation;
  size_t size;
  const RunesTypeDescriptor *sequence_element;
  size_t sequence_length;
  size_t sequence_capacity;
  bool marked;
  bool transient;
  void *transient_owner;
} RunesGcObject;

typedef struct RunesGcRoot {
  struct RunesGcRoot *next;
  void *value;
  const RunesTypeDescriptor *descriptor;
  void *frozen;
} RunesGcRoot;

typedef struct RunesGcFrame {
  struct RunesGcFrame *previous;
  RunesGcRoot *root_marker;
} RunesGcFrame;

static RunesGcObject *runes_gc_objects;
static RunesGcObject *runes_gc_transient_objects;
static size_t runes_gc_object_count;
static size_t runes_gc_allocated_bytes;
static size_t runes_gc_collections;
static size_t runes_gc_threshold = 1024 * 1024;
static atomic_uintptr_t runes_gc_owner;
static _Thread_local unsigned char runes_gc_thread_token;
static _Thread_local size_t runes_gc_scope_depth;
static _Thread_local RunesGcRoot *runes_gc_roots;
static _Thread_local RunesGcFrame *runes_gc_frames;
static _Thread_local size_t runes_gc_no_collect_depth;
static _Thread_local RunesStorageError runes_storage_error;
static _Thread_local size_t runes_storage_failure_countdown = SIZE_MAX;
static RunesStorageStats runes_storage_counters;

typedef struct {
  const void *source;
  void *destination;
} RunesCloneEntry;

struct RunesCloneContext {
  RunesCloneEntry *entries;
  size_t count;
  size_t capacity;
  unsigned line;
  unsigned column;
  bool to_gc;
};

static void runes_runtime_fail(const char *message, unsigned line,
                               unsigned column) {
  fprintf(stderr, "Runes runtime error at %u:%u: %s\n", line, column,
          message);
  abort();
}

void runes_runtime_init(void) {
#if defined(__linux__) && !defined(RUNES_FREESTANDING)
  struct sigaction action = {0};
  action.sa_handler = SIG_IGN;
  if (sigemptyset(&action.sa_mask) != 0 ||
      sigaction(SIGPIPE, &action, NULL) != 0)
    runes_runtime_fail("could not establish the SIGPIPE policy", 0, 0);
#endif
}

static bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

static bool storage_layout(size_t count,
                           const RunesTypeDescriptor *element,
                           size_t *bytes) {
  if (!element || !element->size || !is_power_of_two(element->align)) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    return false;
  }
  if (count > SIZE_MAX / element->size) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    return false;
  }
  *bytes = count * element->size;
  return true;
}

static bool storage_forced_failure(void) {
  if (runes_storage_failure_countdown == SIZE_MAX)
    return false;
  if (runes_storage_failure_countdown) {
    runes_storage_failure_countdown--;
    return false;
  }
  runes_storage_error = RUNES_STORAGE_OUT_OF_MEMORY;
  return true;
}

static bool storage_validate_initialized(void *pointer, size_t expected_old,
                                         size_t new_initialized,
                                         size_t capacity,
                                         const RunesTypeDescriptor *element) {
  size_t ignored;
  if (!pointer || expected_old > capacity || new_initialized > capacity ||
      !storage_layout(capacity, element, &ignored)) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    runes_storage_counters.rejected_transitions++;
    return false;
  }
  return true;
}

static void storage_record_initialized(size_t expected_old,
                                       size_t new_initialized) {
  runes_storage_error = RUNES_STORAGE_OK;
  runes_storage_counters.initialized_publications++;
  if (new_initialized < expected_old)
    runes_storage_counters.initialized_shrinks++;
}

static Arena *active_arena_root(void) {
  Arena *arena = runes_active_arena;
  while (arena && arena->parent)
    arena = arena->parent;
  return arena;
}

static bool pointer_is_arena_owned(const void *pointer) {
  Arena *root = active_arena_root();
  return root && arena_owns(root, pointer);
}

static void gc_claim_owner(unsigned line, unsigned column) {
  uintptr_t thread = (uintptr_t)&runes_gc_thread_token;
  uintptr_t expected = 0;
  if (!atomic_compare_exchange_strong(&runes_gc_owner, &expected, thread) &&
      expected != thread)
    runes_runtime_fail("GC heap is owned by another thread", line, column);
}

static RunesGcObject *gc_find_object(const void *pointer) {
  uintptr_t address = (uintptr_t)pointer;
  for (RunesGcObject *object = runes_gc_objects; object;
       object = object->next) {
    uintptr_t start = (uintptr_t)object->payload;
    if (address >= start && address - start < object->size)
      return object;
  }
  return NULL;
}

typedef struct {
  RunesGcObject **items;
  size_t count;
  size_t capacity;
} RunesGcWorklist;

static void gc_mark_visit(const void *pointer, void *opaque) {
  if (!pointer)
    return;
  RunesGcObject *object = gc_find_object(pointer);
  if (!object || object->marked)
    return;
  object->marked = true;
  RunesGcWorklist *worklist = opaque;
  if (worklist->count == worklist->capacity) {
    size_t next = worklist->capacity ? worklist->capacity * 2 : 64;
    if (next < worklist->capacity ||
        next > SIZE_MAX / sizeof(*worklist->items))
      runes_runtime_fail("GC mark worklist overflow", 0, 0);
    void *grown = realloc(worklist->items, next * sizeof(*worklist->items));
    if (!grown)
      runes_runtime_fail("GC mark worklist allocation failed", 0, 0);
    worklist->items = grown;
    worklist->capacity = next;
  }
  worklist->items[worklist->count++] = object;
}

static void gc_trace_object(const RunesGcObject *object, RunesGcVisit visit,
                            void *context) {
  if (object->sequence_element) {
    size_t stride = object->sequence_element->size;
    for (size_t i = 0; i < object->sequence_length; i++)
      runes_gc_trace_value((const char *)object->payload + i * stride,
                           object->sequence_element, visit, context);
    return;
  }
  runes_gc_trace_value(object->payload, object->descriptor, visit, context);
}

static void gc_make_transient(RunesGcObject *object, void *owner) {
  if (!object)
    return;
  if (object->transient) {
    object->transient_owner = owner;
    return;
  }
  object->transient = true;
  object->transient_owner = owner;
  object->transient_next = runes_gc_transient_objects;
  runes_gc_transient_objects = object;
}

static void *clone_lookup(RunesCloneContext *context, const void *source) {
  for (size_t i = 0; i < context->count; i++)
    if (context->entries[i].source == source)
      return context->entries[i].destination;
  return NULL;
}

static void clone_record(RunesCloneContext *context, const void *source,
                         void *destination) {
  if (context->count == context->capacity) {
    size_t next = context->capacity ? context->capacity * 2 : 16;
    if (next < context->capacity ||
        next > SIZE_MAX / sizeof(*context->entries))
      runes_runtime_fail("promotion map size overflow", context->line,
                         context->column);
    void *grown = realloc(context->entries,
                          next * sizeof(*context->entries));
    if (!grown)
      runes_runtime_fail("promotion map allocation failed", context->line,
                         context->column);
    context->entries = grown;
    context->capacity = next;
  }
  context->entries[context->count++] =
      (RunesCloneEntry){.source = source, .destination = destination};
}

static void *clone_allocate(RunesCloneContext *context, size_t size,
                            size_t align,
                            const RunesTypeDescriptor *descriptor) {
  if (!is_power_of_two(align))
    runes_runtime_fail("invalid promotion alignment", context->line,
                       context->column);
  if (align < sizeof(void *))
    align = sizeof(void *);
  return context->to_gc
             ? runes_gc_alloc(size, align, descriptor, context->line,
                              context->column)
             : runes_raw_alloc_aligned(size, align, context->line,
                                       context->column);
}

void *runes_arena_scope_enter(unsigned line, unsigned column) {
  RunesArenaScope *scope = malloc(sizeof(*scope));
  if (!scope)
    runes_runtime_fail("arena scope allocation failed", line, column);

  scope->previous = runes_active_arena;
  scope->owns_root = scope->previous == NULL;
  if (scope->owns_root) {
    scope->arena = malloc(sizeof(*scope->arena));
    if (!scope->arena || !arena_init(scope->arena)) {
      free(scope->arena);
      free(scope);
      runes_runtime_fail("arena initialization failed", line, column);
    }
  } else {
    scope->arena = arena_create_child(scope->previous);
    if (!scope->arena) {
      free(scope);
      runes_runtime_fail("child arena initialization failed", line, column);
    }
  }
  runes_active_arena = scope->arena;
  runes_live_arena_scopes++;
  if (scope->owns_root)
    runes_live_arena_roots++;
  return scope;
}

void runes_arena_scope_leave(void *opaque_scope) {
  RunesArenaScope *scope = opaque_scope;
  if (!scope)
    return;
  runes_active_arena = scope->previous;
  if (scope->owns_root) {
    arena_destroy(scope->arena);
    free(scope->arena);
    runes_live_arena_roots--;
  }
  runes_live_arena_scopes--;
  free(scope);
}

void *runes_regional_alloc(size_t size, size_t align, unsigned line,
                           unsigned column) {
  if (!runes_active_arena)
    runes_runtime_fail("regional allocation requires an active arena", line,
                       column);
  if (!is_power_of_two(align))
    runes_runtime_fail("allocation alignment must be a power of two", line,
                       column);
  if (align < sizeof(void *))
    align = sizeof(void *);
  void *result =
      arena_alloc_aligned(runes_active_arena, size, align);
  if (!result)
    runes_runtime_fail("regional allocation failed", line, column);
  return result;
}

size_t runes_debug_live_arena_scopes(void) {
  return runes_live_arena_scopes;
}

size_t runes_debug_live_arena_roots(void) {
  return runes_live_arena_roots;
}

void *runes_alloc(size_t size, size_t align, unsigned line, unsigned column) {
  if (!is_power_of_two(align))
    runes_runtime_fail("allocation alignment must be a power of two", line,
                       column);
  if (align < sizeof(void *))
    align = sizeof(void *);
  void *result = runes_gc_scope_depth
                     ? runes_gc_alloc(size, align, NULL, line, column)
                 : runes_active_arena
                     ? arena_alloc_aligned(runes_active_arena, size, align)
                     : runes_raw_alloc_aligned(size, align, line, column);
  if (!result)
    runes_runtime_fail("allocation failed", line, column);
  return result;
}

void *runes_alloc_typed(size_t size, size_t align,
                        const RunesTypeDescriptor *descriptor, unsigned line,
                        unsigned column) {
  if (runes_gc_scope_depth)
    return runes_gc_alloc(size, align, descriptor, line, column);
  return runes_alloc(size, align, line, column);
}

void *runes_raw_alloc(size_t size, unsigned line, unsigned column) {
  if (size == 0)
    size = 1;
  void *result = malloc(size);
  if (!result)
    runes_runtime_fail("raw allocation failed", line, column);
  return result;
}

void *runes_raw_alloc_aligned(size_t size, size_t align, unsigned line,
                              unsigned column) {
  if (!is_power_of_two(align))
    runes_runtime_fail("invalid raw allocation alignment", line, column);
  if (align < sizeof(void *))
    align = sizeof(void *);
  if (size == 0)
    size = 1;
  if (size > SIZE_MAX - (align - 1))
    runes_runtime_fail("raw allocation size overflow", line, column);
  size_t padded = (size + align - 1) & ~(align - 1);
  void *result = aligned_alloc(align, padded);
  if (!result)
    runes_runtime_fail("raw allocation failed", line, column);
  return result;
}

void runes_raw_free(void *pointer) { free(pointer); }

static void *storage_try_allocate_dynamic_bytes(
    size_t bytes, size_t align) {
  if (storage_forced_failure())
    return NULL;
  if (align < sizeof(void *))
    align = sizeof(void *);
  size_t physical = bytes ? bytes : 1;
  if (physical > SIZE_MAX - (align - 1)) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    return NULL;
  }
  size_t padded = (physical + align - 1) & ~(align - 1);
  void *result = aligned_alloc(align, padded);
  if (!result) {
    runes_storage_error = RUNES_STORAGE_OUT_OF_MEMORY;
    return NULL;
  }
  memset(result, 0, physical);
  runes_storage_error = RUNES_STORAGE_OK;
  runes_storage_counters.dynamic_allocations++;
  return result;
}

void *runes_storage_try_allocate_dynamic(
    size_t count, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  (void)line;
  (void)column;
  size_t bytes;
  if (!storage_layout(count, element, &bytes))
    return NULL;
  return storage_try_allocate_dynamic_bytes(bytes, element->align);
}

void *runes_storage_try_allocate_regional(
    size_t count, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  (void)line;
  (void)column;
  size_t bytes;
  if (!storage_layout(count, element, &bytes))
    return NULL;
  if (!runes_active_arena) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    return NULL;
  }
  if (storage_forced_failure())
    return NULL;
  void *result =
      arena_alloc_aligned(runes_active_arena, bytes, element->align);
  if (!result) {
    runes_storage_error = RUNES_STORAGE_OUT_OF_MEMORY;
    return NULL;
  }
  runes_storage_error = RUNES_STORAGE_OK;
  runes_storage_counters.regional_allocations++;
  return result;
}

void *runes_storage_try_resize_dynamic(
    void *pointer, size_t initialized, size_t old_capacity,
    size_t new_capacity, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  (void)line;
  (void)column;
  size_t old_bytes, new_bytes, initialized_bytes;
  if (initialized > old_capacity || initialized > new_capacity) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    return NULL;
  }
  if (!storage_layout(old_capacity, element, &old_bytes) ||
      !storage_layout(new_capacity, element, &new_bytes) ||
      !storage_layout(initialized, element, &initialized_bytes))
    return NULL;
  (void)old_bytes;
  void *replacement =
      storage_try_allocate_dynamic_bytes(new_bytes, element->align);
  if (!replacement)
    return NULL;
  if (pointer && initialized_bytes)
    memcpy(replacement, pointer, initialized_bytes);
  free(pointer);
  if (pointer)
    runes_storage_counters.dynamic_releases++;
  return replacement;
}

void *runes_storage_try_resize_regional(
    void *pointer, size_t initialized, size_t old_capacity,
    size_t new_capacity, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  size_t old_bytes, new_bytes, initialized_bytes;
  if (initialized > old_capacity || initialized > new_capacity) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    return NULL;
  }
  if (!storage_layout(old_capacity, element, &old_bytes) ||
      !storage_layout(new_capacity, element, &new_bytes) ||
      !storage_layout(initialized, element, &initialized_bytes))
    return NULL;
  (void)old_bytes;
  (void)new_bytes;
  if (!runes_active_arena ||
      (pointer && !arena_owns_direct(runes_active_arena, pointer))) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    return NULL;
  }
  void *replacement = runes_storage_try_allocate_regional(
      new_capacity, element, line, column);
  if (!replacement)
    return NULL;
  if (pointer && initialized_bytes)
    memcpy(replacement, pointer, initialized_bytes);
  return replacement;
}

bool runes_storage_set_initialized_dynamic(
    void *pointer, size_t expected_old, size_t new_initialized,
    size_t capacity, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  (void)line;
  (void)column;
  if (!storage_validate_initialized(pointer, expected_old, new_initialized,
                                    capacity, element))
    return false;
  storage_record_initialized(expected_old, new_initialized);
  return true;
}

bool runes_storage_set_initialized_regional(
    void *pointer, size_t expected_old, size_t new_initialized,
    size_t capacity, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  (void)line;
  (void)column;
  if (!storage_validate_initialized(pointer, expected_old, new_initialized,
                                    capacity, element))
    return false;
  if (!runes_active_arena ||
      !arena_owns_direct(runes_active_arena, pointer)) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    runes_storage_counters.rejected_transitions++;
    return false;
  }
  storage_record_initialized(expected_old, new_initialized);
  return true;
}

void runes_storage_release_dynamic(void *pointer) {
  if (pointer) {
    free(pointer);
    runes_storage_counters.dynamic_releases++;
  }
}

void runes_storage_release_regional(void *pointer, unsigned line,
                                    unsigned column) {
  (void)line;
  (void)column;
  if (pointer && (!runes_active_arena ||
                  !arena_owns_direct(runes_active_arena, pointer))) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    return;
  }
  runes_storage_error = RUNES_STORAGE_OK;
}

void runes_storage_release_gc(void *pointer) {
  if (!pointer) {
    runes_storage_error = RUNES_STORAGE_OK;
    return;
  }
  RunesGcObject *object = gc_find_object(pointer);
  if (!object || object->payload != pointer) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    runes_storage_counters.rejected_transitions++;
    return;
  }
  if (object->sequence_element) {
    if (object->sequence_length)
      runes_storage_counters.initialized_shrinks++;
    object->sequence_length = 0;
  }
  runes_storage_error = RUNES_STORAGE_OK;
}

RunesStorageError runes_storage_last_error(void) {
  return runes_storage_error;
}

void runes_storage_fail(RunesStorageError error, unsigned line,
                        unsigned column) {
  const char *message = "storage operation failed";
  switch (error) {
  case RUNES_STORAGE_OUT_OF_MEMORY:
    message = "storage operation failed: out of memory";
    break;
  case RUNES_STORAGE_CAPACITY_OVERFLOW:
    message = "storage operation failed: capacity overflow";
    break;
  case RUNES_STORAGE_OWNER_UNAVAILABLE:
    message = "storage operation failed: owner unavailable";
    break;
  case RUNES_STORAGE_OK:
    message = "storage operation failed without an error";
    break;
  }
  runes_runtime_fail(message, line, column);
}

RunesStorageStats runes_storage_stats(void) {
  return runes_storage_counters;
}

void runes_storage_debug_fail_after(size_t successful_allocations) {
  runes_storage_failure_countdown = successful_allocations;
}

void runes_storage_debug_clear_failure(void) {
  runes_storage_failure_countdown = SIZE_MAX;
}

void runes_gc_scope_enter(unsigned line, unsigned column) {
  gc_claim_owner(line, column);
  runes_gc_scope_depth++;
}

void runes_gc_scope_leave(void) {
  if (!runes_gc_scope_depth)
    runes_runtime_fail("GC scope stack underflow", 0, 0);
  runes_gc_scope_depth--;
}

void *runes_gc_frame_enter(unsigned line, unsigned column) {
  gc_claim_owner(line, column);
  RunesGcFrame *frame = malloc(sizeof(*frame));
  if (!frame)
    runes_runtime_fail("GC root frame allocation failed", line, column);
  frame->previous = runes_gc_frames;
  frame->root_marker = runes_gc_roots;
  runes_gc_frames = frame;
  return frame;
}

void *runes_gc_frame_enter_if_active(unsigned line, unsigned column) {
  return runes_gc_scope_depth ? runes_gc_frame_enter(line, column) : NULL;
}

void runes_gc_frame_leave(void *opaque_frame) {
  RunesGcFrame *frame = opaque_frame;
  if (!frame)
    return;
  if (!frame || frame != runes_gc_frames)
    runes_runtime_fail("invalid GC root frame leave", 0, 0);
  while (runes_gc_roots != frame->root_marker) {
    RunesGcRoot *root = runes_gc_roots;
    runes_gc_roots = root->next;
    free(root->frozen);
    free(root);
  }
  runes_gc_frames = frame->previous;
  free(frame);
}

void *runes_gc_root_push(void *value, const RunesTypeDescriptor *descriptor,
                         unsigned line, unsigned column) {
  if (!runes_gc_frames)
    return NULL;
  if (!value || !descriptor)
    runes_runtime_fail("invalid GC root", line, column);
  RunesGcRoot *root = malloc(sizeof(*root));
  if (!root)
    runes_runtime_fail("GC root allocation failed", line, column);
  *root = (RunesGcRoot){.next = runes_gc_roots,
                       .value = value,
                       .descriptor = descriptor,
                       .frozen = NULL};
  runes_gc_roots = root;
  return root;
}

void runes_gc_root_freeze(void *opaque_root, unsigned line,
                          unsigned column) {
  RunesGcRoot *root = opaque_root;
  if (!root || root->frozen)
    return;
  size_t size = root->descriptor->size ? root->descriptor->size : 1;
  root->frozen = malloc(size);
  if (!root->frozen)
    runes_runtime_fail("GC frozen root allocation failed", line, column);
  memcpy(root->frozen, root->value, root->descriptor->size);
  root->value = root->frozen;
}

void runes_gc_trace_value(const void *value,
                          const RunesTypeDescriptor *descriptor,
                          RunesGcVisit visit, void *context) {
  if (value && descriptor && descriptor->trace)
    descriptor->trace(value, visit, context);
}

void runes_gc_collect(void) {
  gc_claim_owner(0, 0);
  RunesGcWorklist worklist = {0};
  for (RunesGcObject *object = runes_gc_transient_objects; object;
       object = object->transient_next)
    gc_mark_visit(object->payload, &worklist);
  for (RunesGcRoot *root = runes_gc_roots; root; root = root->next)
    runes_gc_trace_value(root->value, root->descriptor, gc_mark_visit,
                         &worklist);
  while (worklist.count) {
    RunesGcObject *object = worklist.items[--worklist.count];
    gc_trace_object(object, gc_mark_visit, &worklist);
  }
  free(worklist.items);

  RunesGcObject **link = &runes_gc_objects;
  while (*link) {
    RunesGcObject *object = *link;
    if (!object->marked) {
      *link = object->next;
      runes_gc_object_count--;
      runes_gc_allocated_bytes -= object->size;
      free(object->allocation);
      free(object);
    } else {
      object->marked = false;
      link = &object->next;
    }
  }
  runes_gc_collections++;
  size_t next = runes_gc_allocated_bytes > SIZE_MAX / 2
                    ? SIZE_MAX
                    : runes_gc_allocated_bytes * 2;
  runes_gc_threshold = next > 1024 * 1024 ? next : 1024 * 1024;
}

static void *gc_allocate(size_t size, size_t align,
                         const RunesTypeDescriptor *descriptor,
                         size_t initialized_sequence,
                         size_t sequence_capacity, bool fallible,
                         unsigned line, unsigned column) {
  gc_claim_owner(line, column);
  if (!is_power_of_two(align)) {
    if (fallible) {
      runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
      return NULL;
    }
    runes_runtime_fail("invalid GC allocation alignment", line, column);
  }
  if (align < sizeof(void *))
    align = sizeof(void *);
  if (size == 0)
    size = 1;
  if (fallible && storage_forced_failure())
    return NULL;
  if (!runes_gc_no_collect_depth &&
      (runes_gc_allocated_bytes > runes_gc_threshold ||
       size > runes_gc_threshold - runes_gc_allocated_bytes))
    runes_gc_collect();
  if (size > SIZE_MAX - (align - 1)) {
    if (fallible) {
      runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
      return NULL;
    }
    runes_runtime_fail("GC allocation size overflow", line, column);
  }
  void *allocation = malloc(size + align - 1);
  RunesGcObject *object = malloc(sizeof(*object));
  if (!allocation || !object) {
    free(allocation);
    free(object);
    allocation = NULL;
    object = NULL;
    if (!runes_gc_no_collect_depth) {
      runes_gc_collect();
      allocation = malloc(size + align - 1);
      object = malloc(sizeof(*object));
    }
  }
  if (!allocation || !object) {
    free(allocation);
    free(object);
    if (fallible) {
      runes_storage_error = RUNES_STORAGE_OUT_OF_MEMORY;
      return NULL;
    }
    runes_runtime_fail("GC allocation failed", line, column);
  }
  uintptr_t address = ((uintptr_t)allocation + align - 1) & ~(align - 1);
  void *payload = (void *)address;
  memset(payload, 0, size);
  bool explicit_sequence = initialized_sequence != SIZE_MAX;
  bool descriptor_sequence =
      explicit_sequence ||
      (descriptor && descriptor->size && size > descriptor->size &&
       size % descriptor->size == 0);
  *object = (RunesGcObject){.next = runes_gc_objects,
                           .transient_next = NULL,
                           .descriptor = descriptor,
                           .payload = payload,
                           .allocation = allocation,
                           .size = size,
                           .sequence_element =
                               descriptor_sequence ? descriptor : NULL,
                           .sequence_length =
                               explicit_sequence
                                   ? initialized_sequence
                                   : (descriptor_sequence
                                          ? size / descriptor->size
                                          : 0),
                           .sequence_capacity =
                               explicit_sequence
                                   ? sequence_capacity
                                   : (descriptor_sequence
                                          ? size / descriptor->size
                                          : 0),
                           .marked = false,
                           .transient = false,
                           .transient_owner = NULL};
  runes_gc_objects = object;
  gc_make_transient(object, runes_gc_frames);
  runes_gc_object_count++;
  runes_gc_allocated_bytes += size;
  if (fallible) {
    runes_storage_error = RUNES_STORAGE_OK;
    runes_storage_counters.gc_allocations++;
  }
  return payload;
}

void *runes_gc_alloc(size_t size, size_t align,
                     const RunesTypeDescriptor *descriptor, unsigned line,
                     unsigned column) {
  return gc_allocate(size, align, descriptor, SIZE_MAX, SIZE_MAX, false, line,
                     column);
}

void *runes_storage_try_allocate_gc(
    size_t count, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  size_t bytes;
  if (!storage_layout(count, element, &bytes))
    return NULL;
  return gc_allocate(bytes, element->align, element, 0, count, true, line,
                     column);
}

void *runes_storage_try_resize_gc(
    void *pointer, size_t initialized, size_t old_capacity,
    size_t new_capacity, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  size_t old_bytes, new_bytes, initialized_bytes;
  if (initialized > old_capacity || initialized > new_capacity) {
    runes_storage_error = RUNES_STORAGE_CAPACITY_OVERFLOW;
    return NULL;
  }
  if (!storage_layout(old_capacity, element, &old_bytes) ||
      !storage_layout(new_capacity, element, &new_bytes) ||
      !storage_layout(initialized, element, &initialized_bytes))
    return NULL;
  (void)old_bytes;
  RunesGcObject *old_object = pointer ? gc_find_object(pointer) : NULL;
  if (pointer &&
      (!old_object || old_object->payload != pointer ||
       old_object->sequence_element != element ||
       old_object->sequence_capacity != old_capacity ||
       old_object->sequence_length != initialized)) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    runes_storage_counters.rejected_transitions++;
    return NULL;
  }
  runes_gc_no_collect_depth++;
  void *replacement =
      gc_allocate(new_bytes, element->align, element, initialized,
                  new_capacity, true, line, column);
  runes_gc_no_collect_depth--;
  if (!replacement)
    return NULL;
  if (pointer && initialized_bytes)
    memcpy(replacement, pointer, initialized_bytes);
  return replacement;
}

bool runes_storage_set_initialized_gc(
    void *pointer, size_t expected_old, size_t new_initialized,
    size_t capacity, const RunesTypeDescriptor *element, unsigned line,
    unsigned column) {
  (void)line;
  (void)column;
  if (!storage_validate_initialized(pointer, expected_old, new_initialized,
                                    capacity, element))
    return false;
  RunesGcObject *object = gc_find_object(pointer);
  if (!object || object->payload != pointer ||
      object->sequence_element != element ||
      object->sequence_capacity != capacity ||
      object->sequence_length != expected_old) {
    runes_storage_error = RUNES_STORAGE_OWNER_UNAVAILABLE;
    runes_storage_counters.rejected_transitions++;
    return false;
  }
  object->sequence_length = new_initialized;
  storage_record_initialized(expected_old, new_initialized);
  return true;
}

RunesGcStats runes_gc_stats(void) {
  return (RunesGcStats){.object_count = runes_gc_object_count,
                        .allocated_bytes = runes_gc_allocated_bytes,
                        .collections = runes_gc_collections};
}

void runes_gc_set_threshold(size_t bytes) {
  runes_gc_threshold = bytes ? bytes : 1;
}

size_t runes_gc_debug_object_count(void) { return runes_gc_object_count; }

size_t runes_gc_debug_collection_count(void) {
  return runes_gc_collections;
}

void runes_gc_commit_allocations(void) {
  RunesGcObject **link = &runes_gc_transient_objects;
  while (*link) {
    RunesGcObject *object = *link;
    if (object->transient_owner == runes_gc_frames) {
      *link = object->transient_next;
      object->transient = false;
      object->transient_next = NULL;
      object->transient_owner = NULL;
    } else {
      link = &object->transient_next;
    }
  }
}

static void gc_protect_visit(const void *pointer, void *opaque) {
  RunesGcWorklist *worklist = opaque;
  RunesGcObject *object = gc_find_object(pointer);
  if (!object || object->marked)
    return;
  object->marked = true;
  if (worklist->count == worklist->capacity) {
    size_t next = worklist->capacity ? worklist->capacity * 2 : 16;
    if (next < worklist->capacity ||
        next > SIZE_MAX / sizeof(*worklist->items))
      runes_runtime_fail("GC return worklist overflow", 0, 0);
    void *grown = realloc(worklist->items, next * sizeof(*worklist->items));
    if (!grown)
      runes_runtime_fail("GC return worklist allocation failed", 0, 0);
    worklist->items = grown;
    worklist->capacity = next;
  }
  worklist->items[worklist->count++] = object;
}

void runes_gc_protect_value(const void *value,
                            const RunesTypeDescriptor *descriptor) {
  RunesGcWorklist worklist = {0};
  runes_gc_trace_value(value, descriptor, gc_protect_visit, &worklist);
  while (worklist.count) {
    RunesGcObject *object = worklist.items[--worklist.count];
    void *owner = runes_gc_frames ? runes_gc_frames->previous : NULL;
    gc_make_transient(object, owner);
    gc_trace_object(object, gc_protect_visit, &worklist);
  }
  for (RunesGcObject *object = runes_gc_objects; object; object = object->next)
    object->marked = false;
  free(worklist.items);
}

void runes_clone_value(RunesCloneContext *context, void *destination,
                       const void *source,
                       const RunesTypeDescriptor *descriptor) {
  if (!descriptor || descriptor->size == 0)
    runes_runtime_fail("invalid promotion type descriptor", context->line,
                       context->column);
  memcpy(destination, source, descriptor->size);
  if (descriptor->clone)
    descriptor->clone(destination, source, context);
}

void *runes_clone_pointer(RunesCloneContext *context, const void *source,
                          const RunesTypeDescriptor *descriptor) {
  if (!source)
    return NULL;
  if (!pointer_is_arena_owned(source))
    return (void *)source;
  void *existing = clone_lookup(context, source);
  if (existing)
    return existing;
  void *destination =
      clone_allocate(context, descriptor->size, descriptor->align,
                     descriptor);
  clone_record(context, source, destination);
  runes_clone_value(context, destination, source, descriptor);
  return destination;
}

void *runes_clone_slice(RunesCloneContext *context, const void *source,
                        size_t length,
                        const RunesTypeDescriptor *element_descriptor) {
  if (!source || !pointer_is_arena_owned(source))
    return (void *)source;
  void *existing = clone_lookup(context, source);
  if (existing)
    return existing;
  if (!element_descriptor || element_descriptor->size == 0)
    runes_runtime_fail("invalid slice element descriptor", context->line,
                       context->column);
  if (length > SIZE_MAX / element_descriptor->size)
    runes_runtime_fail("slice promotion size overflow", context->line,
                       context->column);

  size_t size = length * element_descriptor->size;
  void *destination =
      clone_allocate(context, size, element_descriptor->align, NULL);
  clone_record(context, source, destination);
  if (context->to_gc) {
    RunesGcObject *object = gc_find_object(destination);
    if (!object)
      runes_runtime_fail("promoted slice backing is not GC-owned",
                         context->line, context->column);
    object->sequence_element = element_descriptor;
    object->sequence_length = length;
  }
  for (size_t i = 0; i < length; i++)
    runes_clone_value(context,
                      (char *)destination + i * element_descriptor->size,
                      (const char *)source + i * element_descriptor->size,
                      element_descriptor);
  return destination;
}

RunesStr runes_clone_string(RunesCloneContext *context, RunesStr source) {
  if (!source.ptr || !pointer_is_arena_owned(source.ptr))
    return source;
  void *existing = clone_lookup(context, source.ptr);
  if (existing)
    return (RunesStr){.ptr = existing, .len = source.len};
  uint8_t *destination = clone_allocate(context, source.len, 1, NULL);
  clone_record(context, source.ptr, destination);
  memcpy(destination, source.ptr, source.len);
  return (RunesStr){.ptr = destination, .len = source.len};
}

bool runes_str_equal(RunesStr left, RunesStr right) {
  return left.len == right.len &&
         (left.len == 0 || memcmp(left.ptr, right.ptr, left.len) == 0);
}

int runes_str_compare(RunesStr left, RunesStr right) {
  size_t common = left.len < right.len ? left.len : right.len;
  int compared = common ? memcmp(left.ptr, right.ptr, common) : 0;
  if (compared)
    return compared < 0 ? -1 : 1;
  return left.len < right.len ? -1 : left.len > right.len ? 1 : 0;
}

RunesStr runes_str_concat(RunesStr left, RunesStr right, unsigned line,
                          unsigned column) {
  if (left.len > SIZE_MAX - right.len)
    runes_runtime_fail("string concatenation size overflow", line, column);
  size_t length = left.len + right.len;
  if (length == 0)
    return (RunesStr){0};
  uint8_t *data = runes_alloc(length, sizeof(void *), line, column);
  if (left.len)
    memcpy(data, left.ptr, left.len);
  if (right.len)
    memcpy(data + left.len, right.ptr, right.len);
  return (RunesStr){.ptr = data, .len = length};
}

RunesStr runes_str_slice_bytes(RunesStr value, size_t start, size_t end,
                               bool inclusive, unsigned line,
                               unsigned column) {
  if (inclusive) {
    if (end == SIZE_MAX)
      runes_runtime_fail("string range end overflow", line, column);
    end++;
  }
  if (start > end || end > value.len)
    runes_runtime_fail("invalid string byte range", line, column);
  if (!runes_str_is_scalar_boundary(value, start) ||
      !runes_str_is_scalar_boundary(value, end))
    runes_runtime_fail("string byte range splits a UTF-8 scalar", line,
                       column);
  return (RunesStr){.ptr = value.ptr ? value.ptr + start : NULL,
                    .len = end - start};
}

uint8_t runes_str_byte_at(RunesStr value, size_t index, unsigned line,
                          unsigned column) {
  if (index >= value.len)
    runes_runtime_fail("string byte index out of bounds", line, column);
  return value.ptr[index];
}

static bool decode_utf8(const uint8_t *data, size_t length, size_t *width,
                        uint32_t *scalar) {
  if (!length)
    return false;
  uint8_t first = data[0];
  if (first <= 0x7f) {
    *width = 1;
    *scalar = first;
    return true;
  }
  size_t count;
  uint32_t value;
  uint32_t minimum;
  if (first >= 0xc2 && first <= 0xdf) {
    count = 2;
    value = first & 0x1f;
    minimum = 0x80;
  } else if (first >= 0xe0 && first <= 0xef) {
    count = 3;
    value = first & 0x0f;
    minimum = 0x800;
  } else if (first >= 0xf0 && first <= 0xf4) {
    count = 4;
    value = first & 0x07;
    minimum = 0x10000;
  } else {
    return false;
  }
  if (length < count)
    return false;
  for (size_t i = 1; i < count; i++) {
    if ((data[i] & 0xc0) != 0x80)
      return false;
    value = (value << 6) | (data[i] & 0x3f);
  }
  if (value < minimum || value > 0x10ffff ||
      (value >= 0xd800 && value <= 0xdfff))
    return false;
  *width = count;
  *scalar = value;
  return true;
}

bool runes_str_is_utf8(RunesStr value) {
  size_t index = 0;
  while (index < value.len) {
    size_t width;
    uint32_t scalar;
    if (!decode_utf8(value.ptr + index, value.len - index, &width, &scalar))
      return false;
    index += width;
  }
  return true;
}

bool runes_str_is_scalar_boundary(RunesStr value, size_t index) {
  return index <= value.len &&
         (index == 0 || index == value.len ||
          (value.ptr[index] & 0xc0) != 0x80);
}

bool runes_str_decode_next(RunesStr value, size_t *index, uint32_t *scalar) {
  if (!index || !scalar || *index >= value.len)
    return false;
  size_t width;
  if (!decode_utf8(value.ptr + *index, value.len - *index, &width, scalar))
    return false;
  *index += width;
  return true;
}

bool runes_utf8_encode(uint32_t scalar, uint8_t output[4], size_t *length) {
  if (!output || !length || scalar > 0x10ffff ||
      (scalar >= 0xd800 && scalar <= 0xdfff))
    return false;
  if (scalar <= 0x7f) {
    output[0] = (uint8_t)scalar;
    *length = 1;
  } else if (scalar <= 0x7ff) {
    output[0] = (uint8_t)(0xc0 | (scalar >> 6));
    output[1] = (uint8_t)(0x80 | (scalar & 0x3f));
    *length = 2;
  } else if (scalar <= 0xffff) {
    output[0] = (uint8_t)(0xe0 | (scalar >> 12));
    output[1] = (uint8_t)(0x80 | ((scalar >> 6) & 0x3f));
    output[2] = (uint8_t)(0x80 | (scalar & 0x3f));
    *length = 3;
  } else {
    output[0] = (uint8_t)(0xf0 | (scalar >> 18));
    output[1] = (uint8_t)(0x80 | ((scalar >> 12) & 0x3f));
    output[2] = (uint8_t)(0x80 | ((scalar >> 6) & 0x3f));
    output[3] = (uint8_t)(0x80 | (scalar & 0x3f));
    *length = 4;
  }
  return true;
}

uint32_t runes_char_from_u64(uint64_t value, unsigned line, unsigned column) {
  if (value > UINT32_MAX || (value >= 0xd800 && value <= 0xdfff) ||
      value > 0x10ffff)
    runes_runtime_fail("integer is not a Unicode scalar", line, column);
  return (uint32_t)value;
}

uint64_t runes_str_hash(RunesStr value) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < value.len; i++) {
    hash ^= value.ptr[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

RunesStr runes_str_view(const uint8_t *data, size_t length) {
  return (RunesStr){.ptr = data, .len = length};
}

RunesStr runes_str_from_c(const char *value) {
  return value ? (RunesStr){.ptr = (const uint8_t *)value,
                            .len = strlen(value)}
               : (RunesStr){0};
}

bool runes_str_from_bytes(const uint8_t *data, size_t length,
                          RunesStr *result) {
  if (!result || (length && !data))
    return false;
  RunesStr value = {.ptr = data, .len = length};
  if (!runes_str_is_utf8(value))
    return false;
  *result = value;
  return true;
}

bool runes_str_from_c_checked(const char *value, RunesStr *result) {
  if (!result)
    return false;
  return runes_str_from_bytes((const uint8_t *)value,
                              value ? strlen(value) : 0, result);
}

char *runes_str_to_c(RunesStr value, unsigned line, unsigned column) {
  if (value.len == SIZE_MAX)
    runes_runtime_fail("C string conversion size overflow", line, column);
  char *result = runes_alloc(value.len + 1, sizeof(void *), line, column);
  if (value.len)
    memcpy(result, value.ptr, value.len);
  result[value.len] = '\0';
  return result;
}

void runes_str_write_stdout(RunesStr value) {
  if (value.len && fwrite(value.ptr, 1, value.len, stdout) != value.len)
    runes_runtime_fail("string output failed", 0, 0);
}

void runes_char_write_stdout(uint32_t scalar, unsigned line, unsigned column) {
  uint8_t encoded[4];
  size_t length;
  if (!runes_utf8_encode(scalar, encoded, &length))
    runes_runtime_fail("invalid Unicode scalar", line, column);
  if (fwrite(encoded, 1, length, stdout) != length)
    runes_runtime_fail("character output failed", line, column);
}

void *runes_promote_dynamic(const void *source,
                            const RunesTypeDescriptor *descriptor,
                            unsigned line, unsigned column) {
  if (!source)
    runes_runtime_fail("cannot promote a null root", line, column);
  RunesCloneContext context = {
      .entries = NULL,
      .count = 0,
      .capacity = 0,
      .line = line,
      .column = column,
      .to_gc = false,
  };
  void *destination =
      clone_allocate(&context, descriptor->size, descriptor->align,
                     descriptor);
  clone_record(&context, source, destination);
  runes_clone_value(&context, destination, source, descriptor);
  free(context.entries);
  return destination;
}

void *runes_promote_gc(const void *source,
                       const RunesTypeDescriptor *descriptor, unsigned line,
                       unsigned column) {
  if (!source)
    runes_runtime_fail("cannot promote a null root", line, column);
  gc_claim_owner(line, column);
  RunesCloneContext context = {
      .entries = NULL,
      .count = 0,
      .capacity = 0,
      .line = line,
      .column = column,
      .to_gc = true,
  };
  runes_gc_no_collect_depth++;
  void *destination =
      clone_allocate(&context, descriptor->size, descriptor->align,
                     descriptor);
  clone_record(&context, source, destination);
  runes_clone_value(&context, destination, source, descriptor);
  runes_gc_no_collect_depth--;
  free(context.entries);
  return destination;
}
