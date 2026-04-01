#include "types.h"
#include <stdio.h>
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
  ctx->type_unknown->kind = TY_UNKNOWN;
}

Type *type_new_primitive(TypeContext *ctx, const char *name) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TY_PRIMITIVE;
  t->as.primitive.name = name;
  return t;
}

Type *type_new_pointer(TypeContext *ctx, Type *inner) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TY_POINTER;
  t->as.pointer.inner = inner;
  return t;
}

Type *type_new_array(TypeContext *ctx, Type *inner, size_t size) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TY_ARRAY;
  t->as.array.inner = inner;
  t->as.array.size = size;
  return t;
}

Type *type_new_tuple(TypeContext *ctx, Type **elems, int count) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TY_TUPLE;
  t->as.tuple.elems = elems;
  t->as.tuple.count = count;
  return t;
}

Type *type_new_function(TypeContext *ctx, Type **params, int param_count,
                        Type *ret, MemoryStrategy strategy) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TY_FUNCTION;
  t->as.function.params = params;
  t->as.function.param_count = param_count;
  t->as.function.ret = ret;
  t->as.function.strategy = strategy;
  return t;
}

Type *type_new_fallible(TypeContext *ctx, Type *inner) {
  Type *t = arena_alloc(ctx->arena, sizeof(Type));
  t->kind = TY_FALLIBLE;
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
  case TY_PRIMITIVE:
    return strcmp(a->as.primitive.name, b->as.primitive.name) == 0;

  case TY_POINTER:
    return type_equals(a->as.pointer.inner, b->as.pointer.inner);

  case TY_ARRAY:
    return a->as.array.size == b->as.array.size &&
           type_equals(a->as.array.inner, b->as.array.inner);

  case TY_TUPLE:
    if (a->as.tuple.count != b->as.tuple.count)
      return false;
    for (int i = 0; i < a->as.tuple.count; i++) {
      if (!type_equals(a->as.tuple.elems[i], b->as.tuple.elems[i]))
        return false;
    }
    return true;

  case TY_FUNCTION:
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

  case TY_FALLIBLE:
    return type_equals(a->as.fallible.inner, b->as.fallible.inner);

  case TY_STRUCT:
  case TY_VARIANT:
  case TY_INTERFACE:
  case TY_ERROR:
    if (a->kind == TY_STRUCT)
      return strcmp(a->as.struct_t.name, b->as.struct_t.name) == 0;
    if (a->kind == TY_VARIANT)
      return strcmp(a->as.variant.name, b->as.variant.name) == 0;
    if (a->kind == TY_INTERFACE)
      return strcmp(a->as.interface_t.name, b->as.interface_t.name) == 0;
    if (a->kind == TY_ERROR)
      return strcmp(a->as.error_t.name, b->as.error_t.name) == 0;
    break;

  case TY_UNKNOWN:
    return true;
  }

  return false;
}

bool type_is_assignable(Type *target, Type *source) {
  if (type_equals(target, source))
    return true;
  if (!target || !source)
    return false;

  if (target->kind == TY_UNKNOWN || source->kind == TY_UNKNOWN) {
    return true;
  }

  // Permissive integer literal assignment: allow i32 (default literal type)
  // to be assigned to other integer types.
  // TODO: Implement range checking.
  if (target->kind == TY_PRIMITIVE && source->kind == TY_PRIMITIVE) {
    const char *tn = target->as.primitive.name;
    const char *sn = source->as.primitive.name;
    if (strcmp(sn, "i32") == 0) {
      if (strcmp(tn, "i8") == 0 || strcmp(tn, "i16") == 0 ||
          strcmp(tn, "i64") == 0 || strcmp(tn, "u8") == 0 ||
          strcmp(tn, "u16") == 0 || strcmp(tn, "u32") == 0 ||
          strcmp(tn, "u64") == 0 || strcmp(tn, "usize") == 0) {
        return true;
      }
    }
    if (strcmp(sn, "f64") == 0) {
      if (strcmp(tn, "f32") == 0) {
        return true;
      }
    }
    // Allow any numeric type to be assigned from its larger counterparts for
    // literals (simplification)
  }

  // Allow T to be assigned to !T
  if (target->kind == TY_FALLIBLE) {
    if (type_is_assignable(target->as.fallible.inner, source)) {
      return true;
    }
  }

  // Allow assignment of error variants (TODO: proper error set checking)
  if (source->kind == TY_PRIMITIVE &&
      strcmp(source->as.primitive.name, "Error") == 0) {
    return true;
  }

  return false;
}

bool type_is_comparable(Type *a, Type *b) {
  if (type_equals(a, b))
    return true;
  if (!a || !b)
    return false;

  if (a->kind == TY_UNKNOWN || b->kind == TY_UNKNOWN) {
    return true;
  }

  // Permissive literal comparison
  if (a->kind == TY_PRIMITIVE && b->kind == TY_PRIMITIVE) {
    const char *an = a->as.primitive.name;
    const char *bn = b->as.primitive.name;

    // Allow i32 (literal) to be compared with any integer/usize
    bool a_is_lit = strcmp(an, "i32") == 0;
    bool b_is_lit = strcmp(bn, "i32") == 0;

    if (a_is_lit || b_is_lit) {
      const char *other = a_is_lit ? bn : an;
      if (other[0] == 'i' || (other[0] == 'u' && strcmp(other, "usize") == 0) ||
          (other[0] == 'u' && (other[1] == '8' || other[1] == '1' ||
                               other[1] == '3' || other[1] == '6'))) {
        return true;
      }
    }

    // Allow f64 (literal) to be compared with f32
    bool a_is_flit = strcmp(an, "f64") == 0;
    bool b_is_flit = strcmp(bn, "f64") == 0;
    if (a_is_flit || b_is_flit) {
      const char *other = a_is_flit ? bn : an;
      if (strcmp(other, "f32") == 0) {
        return true;
      }
    }
  }

  return false;
}
