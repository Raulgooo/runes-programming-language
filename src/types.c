#include "types.h"
#include <string.h>

void type_context_init(TypeContext *ctx, Arena *arena) {
  ctx->arena = arena;
  ctx->type_i8 = type_new_primitive(ctx, "i8");
  ctx->type_i16 = type_new_primitive(ctx, "i16");
  ctx->type_i32 = type_new_primitive(ctx, "i32");
  ctx->type_i64 = type_new_primitive(ctx, "i64");
  ctx->type_u8 = type_new_primitive(ctx, "u8");
  ctx->type_u16 = type_new_primitive(ctx, "u16");
  ctx->type_u32 = type_new_primitive(ctx, "u32");
  ctx->type_u64 = type_new_primitive(ctx, "u64");
  ctx->type_f32 = type_new_primitive(ctx, "f32");
  ctx->type_f64 = type_new_primitive(ctx, "f64");
  ctx->type_bool = type_new_primitive(ctx, "bool");
  ctx->type_str = type_new_primitive(ctx, "str");
  ctx->type_char = type_new_primitive(ctx, "char");
  ctx->type_usize = type_new_primitive(ctx, "usize");
  ctx->type_void = type_new_primitive(ctx, "void");

  ctx->type_unknown = arena_alloc(arena, sizeof(Type));
  ctx->type_unknown->kind = TYPE_UNKNOWN;

  // Note: J, SL, DL would be initialized with new kinds or explicit primitives
  // if mapped.
}

Type *type_new_primitive(TypeContext *ctx, const char *name) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TYPE_PRIMITIVE;
  t->as.primitive.name = name; // Assuming name is already interned
  return t;
}

Type *type_new_pointer(TypeContext *ctx, Type *inner) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TYPE_POINTER;
  t->as.pointer.inner = inner;
  return t;
}

Type *type_new_array(TypeContext *ctx, Type *inner, size_t size) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TYPE_ARRAY;
  t->as.array.inner = inner;
  t->as.array.size = size;
  return t;
}

Type *type_new_tuple(TypeContext *ctx, Type **elems, int count) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TYPE_TUPLE;
  t->as.tuple.elems = elems;
  t->as.tuple.count = count;
  return t;
}

Type *type_new_function(TypeContext *ctx, Type **params, int param_count,
                        Type *ret, MemoryStrategy strategy) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TYPE_FUNCTION;
  t->as.function.params = params;
  t->as.function.param_count = param_count;
  t->as.function.ret = ret;
  t->as.function.strategy = strategy;
  return t;
}

Type *type_new_fallible(TypeContext *ctx, Type *inner) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TYPE_FALLIBLE;
  t->as.fallible.inner = inner;
  return t;
}

bool type_equals(Type *a, Type *b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  if (a->kind != b->kind)
    return false;

  switch (a->kind) {
  case TYPE_PRIMITIVE:
    // Since they are strings (conceptually interned from parser), we can
    // compare char ptrs, but to be safe we'll use strcmp.
    return strcmp(a->as.primitive.name, b->as.primitive.name) == 0;

  case TYPE_POINTER:
    return type_equals(a->as.pointer.inner, b->as.pointer.inner);

  case TYPE_ARRAY:
    return a->as.array.size == b->as.array.size &&
           type_equals(a->as.array.inner, b->as.array.inner);

  case TYPE_TUPLE:
    if (a->as.tuple.count != b->as.tuple.count)
      return false;
    for (int i = 0; i < a->as.tuple.count; i++) {
      if (!type_equals(a->as.tuple.elems[i], b->as.tuple.elems[i]))
        return false;
    }
    return true;

  case TYPE_FUNCTION:
    if (a->as.function.strategy != b->as.function.strategy)
      return false;
    if (a->as.function.param_count != b->as.function.param_count)
      return false;
    if (!type_equals(a->as.function.ret, b->as.function.ret))
      return false;
    for (int i = 0; i < a->as.function.param_count; i++) {
      if (!type_equals(a->as.function.params[i], b->as.function.params[i]))
        return false;
    }
    return true;

  case TYPE_FALLIBLE:
    // It handles !void (inner == NULL) gracefully
    return type_equals(a->as.fallible.inner, b->as.fallible.inner);

  case TYPE_STRUCT:
  case TYPE_VARIANT:
  case TYPE_INTERFACE:
  case TYPE_ERROR:
    // Named types are equal if they refer to the exact same definition
    // (which we represent via pointer equality, or name checking). Name check:
    if (a->kind == TYPE_STRUCT)
      return strcmp(a->as.struct_t.name, b->as.struct_t.name) == 0;
    if (a->kind == TYPE_VARIANT)
      return strcmp(a->as.variant.name, b->as.variant.name) == 0;
    if (a->kind == TYPE_INTERFACE)
      return strcmp(a->as.interface_t.name, b->as.interface_t.name) == 0;
    if (a->kind == TYPE_ERROR)
      return strcmp(a->as.error_t.name, b->as.error_t.name) == 0;
    break;

  case TYPE_UNKNOWN:
    return true; // Or false depending on rigorousness, true enables ignoring
                 // some cascade errors
  }

  return false;
}

bool type_is_assignable(Type *target, Type *source) {
  if (type_equals(target, source))
    return true;
  if (!target || !source)
    return false;

  // Unknown can be assigned anything or assigned to anything during error
  // recovery
  if (target->kind == TYPE_UNKNOWN || source->kind == TYPE_UNKNOWN) {
    return true;
  }

  // Struct/Interface subtyping could go here. For now, strict:
  return false;
}
