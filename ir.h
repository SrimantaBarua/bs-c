#ifndef __BS_IR_H__
#define __BS_IR_H__

#include <stdbool.h>
#include <stdint.h>

#include "ast.h"
#include "string.h"
#include "writer.h"

// A temporary generated for the IR
struct Temp {
  uint64_t i; // Index of the temporary variable. The variable is named t0, t1, ...
};

// A placeholder for an offset in the chunk
struct Label {
  uint64_t i; // Index of the label
};

enum IrLiteralType {
  IL_Nil,
  IL_Boolean,
  IL_Integer,
  IL_Float,
  IL_String,
};

struct IrLiteral {
  enum IrLiteralType type;
  union {
    bool b;
    int64_t i;
    double f;
    struct Str str;
  };
};

// Assign a literal to a temporary
struct IrInstrLiteral {
  struct Temp destination;
  struct IrLiteral literal;
};

// Move the value of a variable to a temporary
struct IrInstrVar {
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

struct IrInstrMember {
  struct Temp destination;
  struct Temp lhs;
  struct Str member;
};

struct IrInstrIndex {
  struct Temp destination;
  struct Temp lhs;
  struct Temp index;
};

struct IrInstrAssignment {
  struct Temp destination;
  struct Temp source;
};

struct IrInstrMove {
  struct Temp destination;
  struct Temp source;
};

struct IrInstrPush {
  struct Temp source;
};

struct IrInstrCall {
  struct Temp destination;
  struct Temp function;
  size_t num_args;
};

struct IrInstrJumpIfFalse {
  struct Temp condition;
  struct Label destination;
};

struct IrInstrJump {
  struct Label destination;
};

// Type of IR instruction
enum IrInstrType {
  II_Literal,      // Copy the value of a literal to a temporary
  II_VarRvalue,    // Copy the value of a variable to a temporary
  II_Binary,       // A binary operation assigning to a temporary
  II_Unary,        // A unary operation assigning to a temporary
  II_VarLvalue,    // Accessing the address of a variable to use as an lvalue
  II_Member,       // Accessing a struct member
  II_MemberLvalue, // Accessing a struct member as an lvalue
  II_Index,        // Indexing into a collection
  II_IndexLvalue,  // Indexing into a collection as an lvalue
  II_Assignment,   // Assign to an lvalue
  II_Push,         // Push temporary onto the stack
  II_Call,         // Call a function (temporary) and give number of arguments
  II_Label,        // A placeholder for an offset in the chunk
  II_JumpIfFalse,  // Jump to a label if the condition is false
  II_Jump,         // Unconditionally jump to a label
  II_Move,         // Move the value of a temp to another temp
};

// A single "instruction" in the (mostly) 3-address-code IR
struct IrInstr {
  size_t offset;          // Offset into source code
  enum IrInstrType type;  // Type of instruction
  union {
    struct IrInstrLiteral literal;        // Storing a literal in a temporry
    struct IrInstrVar var;                // Accessing a variable
    struct IrInstrBinary binary;          // Binary operation
    struct IrInstrUnary unary;            // Unary operation
    struct IrInstrMember member;          // Getting a struct member
    struct IrInstrIndex index;            // Indexing into a collection
    struct IrInstrAssignment assign;      // Assigning to the lvalue in a temporary
    struct IrInstrPush push;              // Pushing a value on the stack
    struct IrInstrCall call;              // Function call
    struct Label label;                   // Marker for where in the IR the label is
    struct IrInstrJumpIfFalse cond_jump;  // Jump to the label if the condition is "false-y"
    struct IrInstrJump jump;              // Unconditionally jump to the label
    struct IrInstrMove move;              // Move the value of one temp to another
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
