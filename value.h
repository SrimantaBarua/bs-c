#ifndef __BS_VALUE_H__
#define __BS_VALUE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "writer.h"

enum ValueType {
  V_Nil,
  V_Boolean,
  V_Integer,
  V_Float,
};

struct Value {
  enum ValueType type;
  union {
    bool b;
    int64_t i;
    double f;
  };
};

// Print a value out to a writer
int value_print(const struct Value value, struct Writer* writer);

#define NIL_VAL()    ((struct Value) { .type = V_Nil, .b = false })
#define BOOL_VAL(B)  ((struct Value) { .type = V_Boolean, .b = B })
#define INT_VAL(I)   ((struct Value) { .type = V_Integer, .i = I })
#define FLOAT_VAL(F) ((struct Value) { .type = V_Float, .f = F })

#define IS_NIL(V)   ((V).type == V_Nil)
#define IS_BOOL(V)  ((V).type == V_Boolean)
#define IS_INT(V)   ((V).type == V_Integer)
#define IS_FLOAT(V) ((V).type == V_Float)

// Check if a value is "false-y"
inline bool value_is_falsey(const struct Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && value.b == false);
}

// A growable array of values
struct ValueVec {
  struct Memory* mem;   // Handle to memory manager
  struct Value* values;
  size_t length;
  size_t capacity;
};

// Initialize an empty value vector
void value_vec_init(struct ValueVec* vec, struct Memory* mem);

// Push a value onto the value vector
void value_vec_push(struct ValueVec* vec, struct Value value);

// Free memory for a value vector
void value_vec_fini(struct ValueVec* vec);

#endif  // __BS_VALUE_H__
