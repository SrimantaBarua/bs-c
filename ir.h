#ifndef __BS_IR_H__
#define __BS_IR_H__

#include <stdint.h>

#include "ast.h"
#include "writer.h"

// A temporary generated for the IR
struct Temp {
  uint64_t i; // Index of the temporary variable. The variable is named t0, t1, ...
};

enum IrLiteralType {
  IL_Nil,
  IL_True,
  IL_False,
  IL_Integer,
  IL_Float,
  IL_String,
};

struct IrLiteral {
  enum IrLiteralType type;
  union {
    int64_t i;
    double f;
    struct Str str;
  };
};

// Assign a literal to a temporary
struct IrInstrLiteralIntoTemp {
  struct Temp destination;
  struct IrLiteral literal;
};

// Copy the value of a variable to a temporary
struct IrInstrVarIntoTemp {
  struct Temp destination;
  struct Str identifier;
};

// A binary operation, assigning to a temporary
struct IrInstrBinary {
  struct Temp destination;
  enum BinaryOp operation;
  struct Temp lhs;
  struct Temp rhs;
};

struct IrInstrUnary {
  struct Temp destination;
  enum UnaryOp operation;
  struct Temp rhs;
};

// Type of IR instruction
enum IrInstrType {
  II_LiteralIntoTemp, // Copy the value of a literal to a temporary
  II_VarIntoTemp,     // Copy the value of a variable to a temporary
  II_Binary,          // A binary operation assigning to a temporary
  II_Unary,           // A unary operation assigning to a temporary
};

// A single "instruction" in the (mostly) 3-address-code IR
struct IrInstr {
  size_t offset;          // Offset into source code
  enum IrInstrType type;  // Type of instruction
  union {
    struct IrInstrLiteralIntoTemp literal;
    struct IrInstrVarIntoTemp var;
    struct IrInstrBinary binary;
    struct IrInstrUnary unary;
  };
};

struct IrChunk {
  struct IrInstr *instrs;
  size_t length;
  size_t capacity;
};

// Initialize an IR chunk
void ir_chunk_init(struct IrChunk* chunk);

// Generates IR from and AST and writes to an IrChunk (which can link to other
// chunks). Returns `true` on success, and `false` on failure.
bool ir_generate(const char *source, const struct Ast *ast, struct IrChunk *root_chunk,
                 struct Writer *err_writer);

// Prints an IR chunk (recursively prints all linked chunks after)
void ir_chunk_print(const struct IrChunk *root_chunk, const char *name, struct Writer *writer);

// Recursively free an IR chunk (frees all linked chunks)
void ir_chunk_fini(struct IrChunk* root_chunk);

#endif  // __BS_IR_H__
