#ifndef __BS_BYTECODE_H__
#define __BS_BYTECODE_H__

#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "value.h"
#include "writer.h"

enum OpCode {
  // Literals
  OP_Nil = 1, // Push nil value
  OP_True,    // Push true value
  OP_False,   // Push false value
  OP_Const1B, // Push 1-byte offset to constant value
  OP_Const2B, // Push 2-byte offset to constant value
  OP_Const4B, // Push 4-byte offset to constant value
  // Binary operations
  OP_Equal,
  OP_NotEqual,
  OP_LessEqual,
  OP_LessThan,
  OP_GreaterEqual,
  OP_GreaterThan,
  OP_ShiftLeft,
  OP_ShiftRight,
  OP_Add,
  OP_Subtract,
  OP_Multiply,
  OP_Divide,
  OP_Modulo,
  OP_BitOr,
  OP_BitAnd,
  OP_BitXor,
  // Unary operations
  OP_Minus,
  OP_BitNot,
  OP_LogicalNot,
};

struct CodeVec {
  struct Memory* mem;
  uint8_t* code;
  size_t length;
  size_t capacity;
};

struct Chunk {
  struct CodeVec code;
  struct ValueVec values;
};

// Initialize an empty chunk of bytecode
void chunk_init(struct Chunk* chunk, struct Memory* mem);

// Free memory for a chunk of bytecode
void chunk_fini(struct Chunk* chunk);

// Push a byte to the chunk
void chunk_push_byte(struct Chunk* chunk, uint8_t byte);

// Push a little-endian uint16_t to the chunk
void chunk_push_word(struct Chunk* chunk, uint16_t word);

// Push a little-endian uint32_t to the chunk
void chunk_push_dword(struct Chunk* chunk, uint32_t dword);

// Push a value to the chunk array and return its index
size_t chunk_push_value(struct Chunk* chunk, struct Value value);

// Disassemble a chunk of bytecode
void chunk_disassemble(const struct Chunk* chunk, const char* name, struct Writer* writer);

#endif  // __BS_BYTECODE_H__
