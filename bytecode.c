#include "bytecode.h"

#include <stdint.h>

#include "log.h"
#include "value.h"

static size_t read_u16(const uint8_t* ptr) {
  return ((size_t) ptr[0]) | (((size_t) ptr[1]) << 8);
}

static size_t read_u32(const uint8_t* ptr) {
  return read_u16(ptr) | (read_u16(ptr + 2) << 16);
}

static void code_vec_init(struct CodeVec* code, struct Memory* mem) {
  code->code = NULL;
  code->length = code->capacity = 0;
  code->mem = mem;
}

static void code_vec_push(struct CodeVec* code, uint8_t byte) {
  if (code->length >= code->capacity) {
    size_t new_capacity = code->capacity == 0 ? 8 : code->capacity * 2;
    code->code = MEM_REALLOC(code->mem, code->code, code->capacity, new_capacity);
    code->capacity = new_capacity;
  }
  code->code[code->length++] = byte;
}

static void code_vec_fini(struct CodeVec* code) {
  MEM_FREE(code->mem, code->code, code->capacity);
}

void chunk_init(struct Chunk* chunk, struct Memory* mem) {
  code_vec_init(&chunk->code, mem);
  value_vec_init(&chunk->values, mem);
}

void chunk_fini(struct Chunk* chunk) {
  code_vec_fini(&chunk->code);
  value_vec_fini(&chunk->values);
}

void chunk_push_byte(struct Chunk* chunk, uint8_t byte) {
  code_vec_push(&chunk->code, byte);
}

void chunk_push_word(struct Chunk* chunk, uint16_t word) {
  code_vec_push(&chunk->code, word & 0xff);
  code_vec_push(&chunk->code, (word >> 8) & 0xff);
}

void chunk_push_dword(struct Chunk* chunk, uint32_t dword) {
  code_vec_push(&chunk->code, dword & 0xff);
  code_vec_push(&chunk->code, (dword >> 8) & 0xff);
  code_vec_push(&chunk->code, (dword >> 16) & 0xff);
  code_vec_push(&chunk->code, (dword >> 24) & 0xff);
}

size_t chunk_push_value(struct Chunk* chunk, struct Value value) {
  size_t ret = chunk->values.length;
  value_vec_push(&chunk->values, value);
  return ret;
}

static size_t disassemble_simple_instruction(const char *name, struct Writer* writer) {
  writer->writef(writer, "  %s\n", name);
  return 1;
}

static void disassemble_const_instruction(const char* name, size_t index, struct Value value,
                                          struct Writer* writer) {
  writer->writef(writer, "  %-16s (%lu) ", name, index);
  value_print(value, writer);
  writer->writef(writer, "\n");
}

static size_t disassemble_const1_instruction(const struct Chunk* chunk, size_t offset,
                                             struct Writer* writer) {
  CHECK(offset + 1 < chunk->code.length);
  size_t index = chunk->code.code[offset + 1];
  CHECK(index < chunk->values.length);
  disassemble_const_instruction("OP_Const1B", index, chunk->values.values[index], writer);
  return 2;
}

static size_t disassemble_const2_instruction(const struct Chunk* chunk, size_t offset,
                                             struct Writer* writer) {
  CHECK(offset + 2 < chunk->code.length);
  size_t index = read_u16(chunk->code.code + offset + 1);
  CHECK(index < chunk->values.length);
  disassemble_const_instruction("OP_Const3B", index, chunk->values.values[index], writer);
  return 3;
}

static size_t disassemble_const4_instruction(const struct Chunk* chunk, size_t offset,
                                             struct Writer* writer) {
  CHECK(offset + 4 < chunk->code.length);
  size_t index = read_u32(chunk->code.code + offset + 1);
  CHECK(index < chunk->values.length);
  disassemble_const_instruction("OP_Const4B", index, chunk->values.values[index], writer);
  return 5;
}

static size_t disassemble_instruction(const struct Chunk* chunk, size_t offset,
                                      struct Writer* writer) {
  uint8_t b = chunk->code.code[offset];
  switch (b) {
  case OP_Nil:          return disassemble_simple_instruction("OP_Nil", writer);
  case OP_True:         return disassemble_simple_instruction("OP_True", writer);
  case OP_False:        return disassemble_simple_instruction("OP_False", writer);
  case OP_Const1B:      return disassemble_const1_instruction(chunk, offset, writer);
  case OP_Const2B:      return disassemble_const2_instruction(chunk, offset, writer);
  case OP_Const4B:      return disassemble_const4_instruction(chunk, offset, writer);
  case OP_Equal:        return disassemble_simple_instruction("OP_Equal", writer);
  case OP_NotEqual:     return disassemble_simple_instruction("OP_NotEqual", writer);
  case OP_LessEqual:    return disassemble_simple_instruction("OP_LessEqual", writer);
  case OP_LessThan:     return disassemble_simple_instruction("OP_LessThan", writer);
  case OP_GreaterEqual: return disassemble_simple_instruction("OP_GreaterEqual", writer);
  case OP_GreaterThan:  return disassemble_simple_instruction("OP_GreaterThan", writer);
  case OP_ShiftLeft:    return disassemble_simple_instruction("OP_ShiftLeft", writer);
  case OP_ShiftRight:   return disassemble_simple_instruction("OP_ShiftRight", writer);
  case OP_Add:          return disassemble_simple_instruction("OP_Add", writer);
  case OP_Subtract:     return disassemble_simple_instruction("OP_Subtract", writer);
  case OP_Multiply:     return disassemble_simple_instruction("OP_Multiply", writer);
  case OP_Divide:       return disassemble_simple_instruction("OP_Divide", writer);
  case OP_Modulo:       return disassemble_simple_instruction("OP_Modulo", writer);
  case OP_BitOr:        return disassemble_simple_instruction("OP_BitOr", writer);
  case OP_BitAnd:       return disassemble_simple_instruction("OP_BitAnd", writer);
  case OP_BitXor:       return disassemble_simple_instruction("OP_BitXor", writer);
  case OP_Minus:        return disassemble_simple_instruction("OP_Minus", writer);
  case OP_BitNot:       return disassemble_simple_instruction("OP_BitNot", writer);
  case OP_LogicalNot:   return disassemble_simple_instruction("OP_LogicalNot", writer);
  default:
    DIE("unexpected byte: %u", b);
  }
}

void chunk_disassemble(const struct Chunk* chunk, const char* name, struct Writer* writer) {
  writer->writef(writer, "%s:\n", name);
  size_t offset = 0;
  while (offset < chunk->code.length) {
    offset += disassemble_instruction(chunk, offset, writer);
  }
}
