#include "code-gen.h"

#include "bytecode.h"
#include "log.h"
#include "value.h"

// State for the code generator
struct State {
  struct Chunk* chunk;   // Chunk that we're writing to
  struct Writer* writer; // Sink for error messages
};

static void state_init(struct State* state, struct Chunk* chunk, struct Writer* writer) {
  state->chunk = chunk;
  state->writer = writer;
}

// Forward declaration
static bool emit(struct State* state, const struct Ast* ast);

static bool emit_program(struct State* state, const struct AstProgram* ast) {
  for (size_t i = 0; i < ast->statements.length; i++) {
    if (!emit(state, ast->statements.data[i])) {
      return false;
    }
  }
  return true;
}

static void emit_const(struct Chunk* chunk, size_t index) {
  if (index <= 0xff) {
    chunk_push_byte(chunk, OP_Const1B);
    chunk_push_byte(chunk, index & 0xff);
  } else if (index <= 0xffff) {
    chunk_push_byte(chunk, OP_Const2B);
    chunk_push_word(chunk, index & 0xffff);
  } else if (index <= 0xffffffff) {
    chunk_push_byte(chunk, OP_Const4B);
    chunk_push_dword(chunk, index & 0xffffffff);
  } else {
    UNIMPLEMENTED();
  }
}

static bool emit_binary(struct State* state, const struct AstBinary* ast) {
  if (!emit(state, ast->lhs)) {
    return false;
  }
  if (!emit(state, ast->rhs)) {
    return false;
  }
  switch (ast->operation) {
  case BO_Equal:        chunk_push_byte(state->chunk, OP_Equal); break;
  case BO_NotEqual:     chunk_push_byte(state->chunk, OP_NotEqual); break;
  case BO_LessEqual:    chunk_push_byte(state->chunk, OP_LessEqual); break;
  case BO_LessThan:     chunk_push_byte(state->chunk, OP_LessThan); break;
  case BO_GreaterEqual: chunk_push_byte(state->chunk, OP_GreaterEqual); break;
  case BO_GreaterThan:  chunk_push_byte(state->chunk, OP_GreaterThan); break;
  case BO_ShiftLeft:    chunk_push_byte(state->chunk, OP_ShiftLeft); break;
  case BO_ShiftRight:   chunk_push_byte(state->chunk, OP_ShiftRight); break;
  case BO_Add:          chunk_push_byte(state->chunk, OP_Add); break;
  case BO_Subtract:     chunk_push_byte(state->chunk, OP_Subtract); break;
  case BO_Multiply:     chunk_push_byte(state->chunk, OP_Multiply); break;
  case BO_Divide:       chunk_push_byte(state->chunk, OP_Divide); break;
  case BO_Modulo:       chunk_push_byte(state->chunk, OP_Modulo); break;
  case BO_BitOr:        chunk_push_byte(state->chunk, OP_BitOr); break;
  case BO_BitAnd:       chunk_push_byte(state->chunk, OP_BitAnd); break;
  case BO_BitXor:       chunk_push_byte(state->chunk, OP_BitXor); break;
  case BO_LogicalAnd:
    // TODO: Short-circuiting
    UNIMPLEMENTED();
  case BO_LogicalOr:
    // TODO: Short-circuiting
    UNIMPLEMENTED();
  }
  return true;
}

static bool emit_unary(struct State* state, const struct AstUnary* ast) {
  if (!emit(state, ast->rhs)) {
    return false;
  }
  switch (ast->operation) {
  case UO_Minus:      chunk_push_byte(state->chunk, OP_Minus); break;
  case UO_BitNot:     chunk_push_byte(state->chunk, OP_BitNot); break;
  case UO_LogicalNot: chunk_push_byte(state->chunk, OP_LogicalNot); break;
  }
  return true;
}

static bool emit_float(struct State* state, const struct AstFloat* ast) {
  size_t index = chunk_push_value(state->chunk, FLOAT_VAL(ast->f));
  emit_const(state->chunk, index);
  return true;
}

static bool emit_integer(struct State* state, const struct AstInteger* ast) {
  size_t index = chunk_push_value(state->chunk, INT_VAL(ast->i));
  emit_const(state->chunk, index);
  return true;
}

static bool emit_boolean(struct State* state, const struct AstBoolean* ast) {
  chunk_push_byte(state->chunk, ast->b ? OP_True : OP_False);
  return true;
}

static bool emit_nil(struct State* state, const struct AstNil* ast) {
  chunk_push_byte(state->chunk, OP_Nil);
  return true;
}

static bool emit(struct State* state, const struct Ast* ast) {
  switch (ast->type) {
  case AST_Program:    return emit_program(state, (const struct AstProgram*) ast);
  case AST_Block:      UNIMPLEMENTED();
  case AST_Struct:     UNIMPLEMENTED();
  case AST_Function:   UNIMPLEMENTED();
  case AST_If:         UNIMPLEMENTED();
  case AST_While:      UNIMPLEMENTED();
  case AST_Let:        UNIMPLEMENTED();
  case AST_Require:    UNIMPLEMENTED();
  case AST_Yield:      UNIMPLEMENTED();
  case AST_Break:      UNIMPLEMENTED();
  case AST_Continue:   UNIMPLEMENTED();
  case AST_Return:     UNIMPLEMENTED();
  case AST_Member:     UNIMPLEMENTED();
  case AST_Index:      UNIMPLEMENTED();
  case AST_Assignment: UNIMPLEMENTED();
  case AST_Binary:     return emit_binary(state, (const struct AstBinary*) ast);
  case AST_Unary:      return emit_unary(state, (const struct AstUnary*) ast);
  case AST_Call:       UNIMPLEMENTED();
  case AST_Self:       UNIMPLEMENTED();
  case AST_Varargs:    UNIMPLEMENTED();
  case AST_Array:      UNIMPLEMENTED();
  case AST_Set:        UNIMPLEMENTED();
  case AST_Dictionary: UNIMPLEMENTED();
  case AST_String:     UNIMPLEMENTED();
  case AST_Identifier: UNIMPLEMENTED();
  case AST_Float:      return emit_float(state, (const struct AstFloat*) ast);
  case AST_Integer:    return emit_integer(state, (const struct AstInteger*) ast);
  case AST_Boolean:    return emit_boolean(state, (const struct AstBoolean*) ast);
  case AST_Ellipsis:   UNIMPLEMENTED();
  case AST_Nil:        return emit_nil(state, (const struct AstNil*) ast);
  default:
    UNIMPLEMENTED();
  }
}

bool generate_bytecode(const struct Ast* ast, struct Chunk* chunk, struct Writer* writer) {
  struct State state;
  state_init(&state, chunk, writer);
  return emit(&state, ast);
}
