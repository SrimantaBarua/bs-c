#include "value.h"

#include "log.h"
#include "memory.h"

void value_vec_init(struct ValueVec* vec, struct Memory* mem) {
  vec->values = NULL;
  vec->length = vec->capacity = 0;
  vec->mem = mem;
}

// Push a value onto the value vector
void value_vec_push(struct ValueVec* vec, struct Value value) {
  if (vec->length >= vec->capacity) {
    size_t new_capacity = vec->capacity == 0 ? 8 : vec->capacity * 2;
    vec->values = MEM_REALLOC(vec->mem, vec->values, vec->capacity * sizeof(struct Value),
                              new_capacity * sizeof(struct Value));
    vec->capacity = new_capacity;
  }
  vec->values[vec->length++] = value;
}

// Free memory for a value vector
void value_vec_fini(struct ValueVec* vec) {
  // We don't free the values because the garbage collector will take care of that
  MEM_FREE(vec->mem, vec->values, vec->capacity * sizeof(struct Value));
}

int value_print(const struct Value value, struct Writer* writer) {
  switch (value.type) {
  case V_Nil:     return writer->writef(writer, "nil");
  case V_Boolean: return writer->writef(writer, value.b ? "true" : "false");
  case V_Integer: return writer->writef(writer, "%lld", value.i);
  case V_Float:   return writer->writef(writer, "%lf", value.f);
  default:
    UNREACHABLE();
  }
}
