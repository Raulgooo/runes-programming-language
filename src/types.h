#ifndef RUNES_TYPES_H
#define RUNES_TYPES_H

#include <stddef.h> // size_t

typedef enum {
  STRATEGY_STACK,    // f
  STRATEGY_EXPLICIT, // stack f
  STRATEGY_DYNAMIC,  // dynamic f
  STRATEGY_REGIONAL, // regional f
  STRATEGY_GC,       // gc f
  STRATEGY_FLEX,     // flex f
} MemoryStrategy;

typedef enum {
  TY_PRIMITIVE, // i8, i32, f64, bool, str, char, usize, void...
  TY_POINTER,   // *T
  TY_ARRAY,     // [N]T
  TY_TUPLE,     // (T1, T2, ...)
  TY_FUNCTION,  // funct firm
  TY_FALLIBLE,  // !T
  TY_STRUCT,    // type Point = { x: f32, y: f32 }
  TY_VARIANT,   // type Color = | Red | Green | RGB(u8,u8,u8)
  TY_INTERFACE, // interface Drawable { ... }
  TY_ERROR,     // error MathError = { | DivByZero | Overflow }
  TY_UNKNOWN,   // pending
} SemTypeKind;

typedef struct Type Type;

typedef struct {
  const char *name; // "i32", "f64", "bool", etc
} PrimitiveType;

typedef struct {
  Type *inner;
} PointerType;

typedef struct {
  Type *inner;
  size_t size; // N en [N]T
} ArrayType;

typedef struct {
  Type **elems;
  int count;
} TupleType;

typedef struct {
  Type **params;
  int param_count;
  Type *ret;
  MemoryStrategy strategy; // f, dynamic, regional, gc, flex, stack
} FunctionType;

typedef struct {
  Type *inner; // el T en !T
} FallibleType;

typedef struct {
  const char *name;
  const char **field_names;
  Type **field_types;
  int field_count;
} StructType;

typedef struct {
  const char *name;
  const char **arm_names;
  Type **arm_types; // NULL si el arm no tiene payload
  int arm_count;
} VariantType;

typedef struct {
  const char *name;
  const char **method_names;
  Type **method_types; // FunctionType cada uno
  int method_count;
} InterfaceType;

typedef struct {
  const char *name;
  const char **variants;
  int variant_count;
} ErrorType;

struct Type {
  SemTypeKind kind;
  union {
    PrimitiveType primitive;
    PointerType pointer;
    ArrayType array;
    TupleType tuple;
    FunctionType function;
    FallibleType fallible;
    StructType struct_t;
    VariantType variant;
    InterfaceType interface_t;
    ErrorType error_t;
  } as;
};

#include "utils/arena.h"
#include <stdbool.h>

typedef struct {
  Arena *arena;

  // Singletons primitives
  Type *type_i8;
  Type *type_i16;
  Type *type_i32;
  Type *type_i64;
  Type *type_u8;
  Type *type_u16;
  Type *type_u32;
  Type *type_u64;
  Type *type_f32;
  Type *type_f64;
  Type *type_bool;
  Type *type_str;
  Type *type_char;
  Type *type_usize;
  Type *type_void;

  // Singletons especiales
  Type *type_unknown;
} TypeContext;

void type_context_init(TypeContext *ctx, Arena *arena);
Type *type_new_primitive(TypeContext *ctx, const char *name);
Type *type_new_pointer(TypeContext *ctx, Type *inner);
Type *type_new_array(TypeContext *ctx, Type *inner, size_t size);
Type *type_new_tuple(TypeContext *ctx, Type **elems, int count);
Type *type_new_function(TypeContext *ctx, Type **params, int param_count,
                        Type *ret, MemoryStrategy strategy);
Type *type_new_fallible(TypeContext *ctx, Type *inner);

bool type_equals(Type *a, Type *b);
bool type_is_assignable(Type *target, Type *source);
bool type_is_comparable(Type *a, Type *b);

#endif // RUNES_TYPES_H