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
  TYPE_PRIMITIVE, // i8, i32, f64, bool, str, char, usize, void...
  TYPE_POINTER,   // *T
  TYPE_ARRAY,     // [N]T
  TYPE_TUPLE,     // (T1, T2, ...)
  TYPE_FUNCTION,  // funct firm
  TYPE_FALLIBLE,  // !T
  TYPE_STRUCT,    // type Point = { x: f32, y: f32 }
  TYPE_VARIANT,   // type Color = | Red | Green | RGB(u8,u8,u8)
  TYPE_INTERFACE, // interface Drawable { ... }
  TYPE_ERROR,     // error MathError = { | DivByZero | Overflow }
  TYPE_UNKNOWN,   // pending
} TypeKind;

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
  TypeKind kind;
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