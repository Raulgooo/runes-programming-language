#define _POSIX_C_SOURCE 200809L

#include "../runtime.h"

#include <assert.h>
#if defined(__linux__) && !defined(RUNES_FREESTANDING)
#include <signal.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static RunesStr bytes(const uint8_t *data, size_t length) {
  return (RunesStr){.ptr = data, .len = length};
}

static void test_runtime_initialization(void) {
  runes_runtime_init();
#if defined(__linux__) && !defined(RUNES_FREESTANDING)
  struct sigaction action;
  assert(sigaction(SIGPIPE, NULL, &action) == 0);
  assert(action.sa_handler == SIG_IGN);
#endif
}

static void test_small_alignment_allocation(void) {
  void *raw = runes_raw_alloc_aligned(1, 1, 1, 1);
  assert(raw);
  assert((uintptr_t)raw % sizeof(void *) == 0);
  runes_raw_free(raw);

  void *scope = runes_arena_scope_enter(1, 1);
  void *regional = runes_regional_alloc(1, 1, 1, 1);
  assert(regional);
  assert((uintptr_t)regional % sizeof(void *) == 0);
  runes_arena_scope_leave(scope);
}

typedef struct TestGcNode {
  struct TestGcNode *next;
  uint32_t value;
} TestGcNode;

static void trace_gc_node(const void *value, RunesGcVisit visit,
                          void *context) {
  const TestGcNode *node = value;
  visit(node->next, context);
}

static void trace_gc_node_pointer(const void *value, RunesGcVisit visit,
                                  void *context) {
  TestGcNode *pointer = NULL;
  memcpy(&pointer, value, sizeof(pointer));
  visit(pointer, context);
}

static const RunesTypeDescriptor gc_node_descriptor = {
    sizeof(TestGcNode), _Alignof(TestGcNode), NULL, trace_gc_node};
static const RunesTypeDescriptor gc_node_pointer_descriptor = {
    sizeof(TestGcNode *), _Alignof(TestGcNode *), NULL,
    trace_gc_node_pointer};
static const RunesTypeDescriptor i32_descriptor = {
    sizeof(int32_t), _Alignof(int32_t), NULL, NULL};

static void test_gc(void) {
  runes_gc_set_threshold(SIZE_MAX);
  void *frame = runes_gc_frame_enter(1, 1);
  runes_gc_scope_enter(1, 1);
  TestGcNode *root = runes_gc_alloc(sizeof(*root), _Alignof(TestGcNode),
                                    &gc_node_descriptor, 1, 1);
  void *root_handle = runes_gc_root_push(&root, &gc_node_pointer_descriptor,
                                         1, 1);
  TestGcNode *second = runes_gc_alloc(sizeof(*second), _Alignof(TestGcNode),
                                      &gc_node_descriptor, 1, 1);
  TestGcNode *third = runes_gc_alloc(sizeof(*third), _Alignof(TestGcNode),
                                     &gc_node_descriptor, 1, 1);
  (void)runes_gc_alloc(sizeof(TestGcNode), _Alignof(TestGcNode),
                       &gc_node_descriptor, 1, 1);
  root->next = second;
  second->next = third;
  third->next = root;
  runes_gc_commit_allocations();
  runes_gc_collect();
  RunesGcStats stats = runes_gc_stats();
  assert(stats.object_count == 3 && stats.collections == 1);

  runes_gc_root_freeze(root_handle, 1, 1);
  root = NULL;
  runes_gc_collect();
  assert(runes_gc_stats().object_count == 3);
  runes_gc_scope_leave();
  runes_gc_frame_leave(frame);
  runes_gc_collect();
  assert(runes_gc_stats().object_count == 0);
}

static void test_realm_storage(void) {
  RunesStorageStats before = runes_storage_stats();
  int32_t *dynamic =
      runes_storage_try_allocate_dynamic(2, &i32_descriptor, 1, 1);
  assert(dynamic && runes_storage_last_error() == RUNES_STORAGE_OK);
  dynamic[0] = 10;
  dynamic[1] = 20;
  int32_t *grown = runes_storage_try_resize_dynamic(
      dynamic, 2, 2, 4, &i32_descriptor, 1, 1);
  assert(grown && grown[0] == 10 && grown[1] == 20);
  assert(!runes_storage_try_resize_dynamic(
      grown, 5, 4, 8, &i32_descriptor, 1, 1));
  assert(runes_storage_last_error() == RUNES_STORAGE_CAPACITY_OVERFLOW);
  assert(grown[0] == 10 && grown[1] == 20);

  runes_storage_debug_fail_after(0);
  assert(!runes_storage_try_resize_dynamic(
      grown, 2, 4, 8, &i32_descriptor, 1, 1));
  assert(runes_storage_last_error() == RUNES_STORAGE_OUT_OF_MEMORY);
  assert(grown[0] == 10 && grown[1] == 20);
  runes_storage_debug_clear_failure();

  assert(!runes_storage_try_allocate_dynamic(
      SIZE_MAX, &i32_descriptor, 1, 1));
  assert(runes_storage_last_error() == RUNES_STORAGE_CAPACITY_OVERFLOW);
  runes_storage_release_dynamic(grown);

  void *outer = runes_arena_scope_enter(1, 1);
  int32_t *regional =
      runes_storage_try_allocate_regional(1, &i32_descriptor, 1, 1);
  assert(regional);
  regional[0] = 7;
  void *child = runes_arena_scope_enter(1, 1);
  assert(!runes_storage_try_resize_regional(
      regional, 1, 1, 2, &i32_descriptor, 1, 1));
  assert(runes_storage_last_error() == RUNES_STORAGE_OWNER_UNAVAILABLE);
  assert(regional[0] == 7);
  runes_arena_scope_leave(child);
  regional = runes_storage_try_resize_regional(
      regional, 1, 1, 2, &i32_descriptor, 1, 1);
  assert(regional && regional[0] == 7);
  runes_storage_release_regional(regional, 1, 1);
  assert(runes_storage_last_error() == RUNES_STORAGE_OK);
  runes_arena_scope_leave(outer);

  RunesStorageStats after = runes_storage_stats();
  assert(after.dynamic_allocations >= before.dynamic_allocations + 2);
  assert(after.dynamic_releases >= before.dynamic_releases + 2);
  assert(after.regional_allocations >= before.regional_allocations + 2);
}

static void test_gc_typed_storage(void) {
  runes_gc_set_threshold(SIZE_MAX);
  void *frame = runes_gc_frame_enter(1, 1);
  runes_gc_scope_enter(1, 1);
  TestGcNode *node = runes_gc_alloc(
      sizeof(*node), _Alignof(TestGcNode), &gc_node_descriptor, 1, 1);
  TestGcNode **values = runes_storage_try_allocate_gc(
      4, &gc_node_pointer_descriptor, 1, 1);
  assert(values);
  values[0] = node;
  values = runes_storage_try_resize_gc(
      values, 1, 4, 8, &gc_node_pointer_descriptor, 1, 1);
  assert(values && values[0] == node);
  void *root = runes_gc_root_push(
      &values, &gc_node_pointer_descriptor, 1, 1);
  (void)root;
  runes_gc_commit_allocations();
  runes_gc_collect();
  assert(runes_gc_stats().object_count == 2);
  runes_gc_scope_leave();
  runes_gc_frame_leave(frame);
  runes_gc_collect();
  assert(runes_gc_stats().object_count == 0);
}

static void test_string_bytes(void) {
  static const uint8_t left_data[] = {'a', 0, 'b'};
  static const uint8_t right_data[] = {'a', 0, 'c'};
  RunesStr left = bytes(left_data, sizeof(left_data));
  RunesStr same = bytes(left_data, sizeof(left_data));
  RunesStr right = bytes(right_data, sizeof(right_data));

  assert(runes_str_equal(left, same));
  assert(!runes_str_equal(left, right));
  assert(runes_str_compare(left, right) < 0);
  assert(runes_str_byte_at(left, 1, 1, 1) == 0);
  RunesStr tail = runes_str_slice_bytes(left, 1, 2, true, 1, 1);
  assert(tail.len == 2 && tail.ptr[0] == 0 && tail.ptr[1] == 'b');
  assert(runes_str_hash(left) == runes_str_hash(same));

  RunesStr joined = runes_str_concat(left, right, 1, 1);
  assert(joined.len == 6 && memcmp(joined.ptr, left_data, 3) == 0 &&
         memcmp(joined.ptr + 3, right_data, 3) == 0);
  runes_raw_free((void *)joined.ptr);

  char *c_value = runes_str_to_c(left, 1, 1);
  assert(c_value[0] == 'a' && c_value[1] == 0 && c_value[2] == 'b' &&
         c_value[3] == 0);
  runes_raw_free(c_value);
  assert(runes_str_from_c("hello").len == 5);
  RunesStr checked = {0};
  assert(runes_str_from_bytes(left_data, sizeof(left_data), &checked));
  assert(runes_str_equal(left, checked));
  static const uint8_t invalid[] = {0xff};
  assert(!runes_str_from_bytes(invalid, sizeof(invalid), &checked));
  assert(runes_str_from_c_checked("hello", &checked) && checked.len == 5);
}

static void test_utf8(void) {
  static const uint8_t valid[] = {0x68, 0xc3, 0xa9, 0xf0, 0x9f, 0x8c, 0x8d};
  static const uint8_t overlong[] = {0xc0, 0x80};
  static const uint8_t surrogate[] = {0xed, 0xa0, 0x80};
  static const uint8_t truncated[] = {0xf0, 0x9f, 0x8c};
  RunesStr text = bytes(valid, sizeof(valid));

  assert(runes_str_is_utf8(text));
  assert(!runes_str_is_utf8(bytes(overlong, sizeof(overlong))));
  assert(!runes_str_is_utf8(bytes(surrogate, sizeof(surrogate))));
  assert(!runes_str_is_utf8(bytes(truncated, sizeof(truncated))));
  assert(runes_str_is_scalar_boundary(text, 0));
  assert(runes_str_is_scalar_boundary(text, 1));
  assert(!runes_str_is_scalar_boundary(text, 2));
  assert(runes_str_is_scalar_boundary(text, 3));

  size_t index = 0;
  uint32_t scalar = 0;
  assert(runes_str_decode_next(text, &index, &scalar) && scalar == 'h');
  assert(runes_str_decode_next(text, &index, &scalar) && scalar == 0xe9);
  assert(runes_str_decode_next(text, &index, &scalar) && scalar == 0x1f30d);
  assert(index == text.len && !runes_str_decode_next(text, &index, &scalar));

  uint8_t encoded[4];
  size_t length = 0;
  assert(runes_utf8_encode(0x1f30d, encoded, &length));
  assert(length == 4 && memcmp(encoded, valid + 3, 4) == 0);
  assert(!runes_utf8_encode(0xd800, encoded, &length));
  assert(!runes_utf8_encode(0x110000, encoded, &length));
  assert(runes_char_from_u64(0x1f30d, 1, 1) == 0x1f30d);
}

static uint32_t property_random(uint32_t *state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

static int reference_compare(RunesStr left, RunesStr right) {
  size_t shared = left.len < right.len ? left.len : right.len;
  int compared = shared ? memcmp(left.ptr, right.ptr, shared) : 0;
  if (compared != 0)
    return compared < 0 ? -1 : 1;
  return (left.len > right.len) - (left.len < right.len);
}

static void test_string_properties(void) {
  uint32_t state = 0x5eed1234u;
  uint8_t left_data[64];
  uint8_t right_data[64];

  for (size_t iteration = 0; iteration < 5000; iteration++) {
    size_t left_length = property_random(&state) % sizeof(left_data);
    size_t right_length = property_random(&state) % sizeof(right_data);
    for (size_t i = 0; i < left_length; i++)
      left_data[i] = (uint8_t)property_random(&state);
    for (size_t i = 0; i < right_length; i++)
      right_data[i] = (uint8_t)property_random(&state);

    RunesStr left = bytes(left_data, left_length);
    RunesStr right = bytes(right_data, right_length);
    int compared = runes_str_compare(left, right);
    int reverse = runes_str_compare(right, left);
    assert(compared == reference_compare(left, right));
    assert(compared == -reverse);
    assert(runes_str_equal(left, right) == (compared == 0));
    if (runes_str_equal(left, right))
      assert(runes_str_hash(left) == runes_str_hash(right));
  }
}

static void test_unicode_roundtrip_properties(void) {
  uint32_t state = 0xc0decafeu;
  for (size_t iteration = 0; iteration < 10000; iteration++) {
    uint32_t scalar = property_random(&state) % 0x110000u;
    if (scalar >= 0xd800u && scalar <= 0xdfffu)
      continue;

    uint8_t encoded[4];
    size_t length = 0;
    assert(runes_utf8_encode(scalar, encoded, &length));
    RunesStr value = bytes(encoded, length);
    size_t index = 0;
    uint32_t decoded = 0;
    assert(runes_str_is_utf8(value));
    assert(runes_str_decode_next(value, &index, &decoded));
    assert(decoded == scalar && index == length);
    assert(!runes_str_decode_next(value, &index, &decoded));
  }
}

int main(void) {
  test_runtime_initialization();
  test_small_alignment_allocation();
  test_string_bytes();
  test_utf8();
  test_string_properties();
  test_unicode_roundtrip_properties();
  test_gc();
  test_realm_storage();
  test_gc_typed_storage();
  puts("runtime tests passed");
  return 0;
}
