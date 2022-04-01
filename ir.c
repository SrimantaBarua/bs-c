// Local variables should all be on the stack. Whenever a new local variable
// is created, allocate a stack slot for it, and add it to a map. When someone
// else (e.g. a lambda) wants to access that function, it'll refer to that
// stack slot.
// Alternatively, we can still keep refering to the variable, and have the
// binding to stack offset be done later


#include "ir.h"

#include <stdlib.h>

#include "ast.h"
#include "log.h"

#define INVALID_TEMP ((struct Temp) { UINT64_MAX })
#define TEMP_IS_INVALID(TEMP) ((TEMP).i == UINT64_MAX)

struct IrGenerator {
  struct IrChunk* current_chunk; // Chunk we're currently writing to
  struct Writer* writer;         // Where to write error message to
  size_t next_temp;              // Next temporary value to generate
  const char* source;            // The original source code (for error messages)
};

// FIXME: This doesn't check for overflow, and assumes temp index will never overflow
static struct Temp new_temp(struct IrGenerator* gen) {
  return (struct Temp) { gen->next_temp++ };
}

static void chunk_push_instr(struct IrChunk* chunk, struct IrInstr instr) {
  if (chunk->length == chunk->capacity) {
    size_t new_capacity = chunk->capacity == 0 ? 8 : chunk->capacity * 2;
    if (!(chunk->instrs = realloc(chunk->instrs, new_capacity * sizeof(struct IrInstr)))) {
      DIE_ERR("realloc()");
    }
    chunk->capacity = new_capacity;
  }
  chunk->instrs[chunk->length++] = instr;
}

static void write_binary_instr(struct IrGenerator* gen, size_t offset, struct Temp dest,
                               enum BinaryOp op, struct Temp lhs, struct Temp rhs) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Binary,
    .binary = (struct IrInstrBinary) {
      .destination = dest,
      .operation = op,
      .lhs = lhs,
      .rhs = rhs,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_unary_instr(struct IrGenerator* gen, size_t offset, struct Temp dest,
                               enum UnaryOp op, struct Temp rhs) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Unary,
    .unary = (struct IrInstrUnary) {
      .destination = dest,
      .operation = op,
      .rhs = rhs,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_variable_access(struct IrGenerator* gen, size_t offset, struct Temp dest,
                                  struct Str identifier) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_VarIntoTemp,
    .var = (struct IrInstrVarIntoTemp) {
      .destination = dest,
      .identifier = identifier,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_integer_literal(struct IrGenerator* gen, size_t offset, struct Temp dest, int64_t i) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_LiteralIntoTemp,
    .literal = (struct IrInstrLiteralIntoTemp) {
      .destination = dest,
      .literal = (struct IrLiteral) {
        .type = IL_Integer,
        .i = i
      }
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

// Forward declaration
static bool emit(struct IrGenerator* gen, const struct Ast* ast, bool is_root_chunk, struct Temp* result);

static bool emit_program(struct IrGenerator* gen, const struct AstProgram* ast, struct Temp* result) {
  struct Temp dummy;
  for (size_t i = 0; i < ast->statements.length; i++) {
    if (!emit(gen, ast->statements.data[i], false, &dummy)) {
      return false;
    }
  }
  *result = INVALID_TEMP;
  return true;
}

static bool emit_binary(struct IrGenerator* gen, const struct AstBinary* ast, struct Temp* result) {
  struct Temp lhs, rhs, dest;
  if (!emit(gen, ast->lhs, false, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  if (!emit(gen, ast->rhs, false, &rhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(rhs)) {
    return false;
  }
  dest = new_temp(gen);
  write_binary_instr(gen, ast->ast.offset, dest, ast->operation, lhs, rhs);
  *result = dest;
  return true;
}

static bool emit_unary(struct IrGenerator* gen, const struct AstUnary* ast, struct Temp* result) {
  struct Temp rhs, dest;
  if (!emit(gen, ast->rhs, false, &rhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(rhs)) {
    return false;
  }
  dest = new_temp(gen);
  write_unary_instr(gen, ast->ast.offset, dest, ast->operation, rhs);
  *result = dest;
  return true;
}

// TODO: Refer to the "environment" to see if we should capture any variables (i.e. is this a closure?)
static bool emit_identifier(struct IrGenerator* gen, const struct AstIdentifier* ast,
                            struct Temp* result) {
  struct Temp dest = new_temp(gen);
  write_variable_access(gen, ast->ast.offset, dest, ast->identifier);
  *result = dest;
  return true;
}

static bool emit_integer(struct IrGenerator* gen, const struct AstInteger* ast, struct Temp* result) {
  struct Temp dest = new_temp(gen);
  write_integer_literal(gen, ast->ast.offset, dest, ast->i);
  *result = dest;
  return true;
}

static bool emit(struct IrGenerator* gen, const struct Ast* ast, bool is_root_chunk,
                 struct Temp* result) {
  switch (ast->type) {
  case AST_Program:
    CHECK(is_root_chunk);
    return emit_program(gen, (const struct AstProgram*) ast, result);
  case AST_Block:
    UNIMPLEMENTED();
    break;
  case AST_Struct:
    UNIMPLEMENTED();
    break;
  case AST_Function:
    UNIMPLEMENTED();
    break;
  case AST_If:
    UNIMPLEMENTED();
    break;
  case AST_While:
    UNIMPLEMENTED();
    break;
  case AST_For:
    UNIMPLEMENTED();
    break;
  case AST_Let:
    UNIMPLEMENTED();
    break;
  case AST_Require:
    UNIMPLEMENTED();
    break;
  case AST_Yield:
    UNIMPLEMENTED();
    break;
  case AST_Break:
    UNIMPLEMENTED();
    break;
  case AST_Continue:
    UNIMPLEMENTED();
    break;
  case AST_Return:
    UNIMPLEMENTED();
    break;
  case AST_Member:
    UNIMPLEMENTED();
    break;
  case AST_Index:
    UNIMPLEMENTED();
    break;
  case AST_Assignment:
    UNIMPLEMENTED();
    break;
  case AST_Binary:
    return emit_binary(gen, (const struct AstBinary*) ast, result);
  case AST_Unary:
    return emit_unary(gen, (const struct AstUnary*) ast, result);
    break;
  case AST_Call:
    UNIMPLEMENTED();
    break;
  case AST_Self:
    UNIMPLEMENTED();
    break;
  case AST_Varargs:
    UNIMPLEMENTED();
    break;
  case AST_Array:
    UNIMPLEMENTED();
    break;
  case AST_Set:
    UNIMPLEMENTED();
    break;
  case AST_Dictionary:
    UNIMPLEMENTED();
    break;
  case AST_String:
    UNIMPLEMENTED();
    break;
  case AST_Identifier:
    return emit_identifier(gen, (const struct AstIdentifier*) ast, result);
    break;
  case AST_Float:
    UNIMPLEMENTED();
    break;
  case AST_Integer:
    return emit_integer(gen, (const struct AstInteger*) ast, result);
  case AST_True:
    UNIMPLEMENTED();
    break;
  case AST_False:
    UNIMPLEMENTED();
    break;
  case AST_Ellipsis:
    UNIMPLEMENTED();
    break;
  case AST_Nil:
    UNIMPLEMENTED();
    break;
  default:
    UNREACHABLE();
    break;
  }
}

void ir_chunk_init(struct IrChunk* chunk) {
  chunk->instrs = NULL;
  chunk->length = chunk->capacity = 0;
}

bool ir_generate(const char *source, const struct Ast *ast, struct IrChunk *root_chunk,
                 struct Writer *err_writer) {
  struct Temp dummy;
  struct IrGenerator generator;
  generator.current_chunk = root_chunk;
  generator.writer = err_writer;
  generator.next_temp = 0;
  generator.source = source;
  return emit(&generator, ast, true, &dummy);
}

static void ir_instr_literal_into_temp_print(const struct IrInstrLiteralIntoTemp* instr,
                                             struct Writer* writer) {
  writer->writef(writer, "  t%llu := ", instr->destination.i);
  switch (instr->literal.type) {
  case IL_Nil:
    writer->writef(writer, "nil\n");
    break;
  case IL_True:
    writer->writef(writer, "true\n");
    break;
  case IL_False:
    writer->writef(writer, "false\n");
    break;
  case IL_Integer:
    writer->writef(writer, "%lld\n", instr->literal.i);
    break;
  case IL_Float:
    writer->writef(writer, "%lf\n", instr->literal.i);
    break;
  case IL_String:
    writer->writef(writer, "%.*s\n", instr->literal.str.length, (const char*) instr->literal.str.data);
    break;
  default:
    UNREACHABLE();
  }
}

static void ir_instr_var_into_temp_print(const struct IrInstrVarIntoTemp* instr,
                                         struct Writer* writer) {
  writer->writef(writer, "  t%llu := %.*s\n", instr->destination.i, instr->identifier.length,
                 (const char*) instr->identifier.data);
}

static void ir_instr_binary_print(const struct IrInstrBinary* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := t%llu %s t%llu\n", instr->destination.i, instr->lhs.i,
                 binary_op_to_str(instr->operation), instr->rhs.i);
}

static void ir_instr_unary_print(const struct IrInstrUnary* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := %s t%llu\n", instr->destination.i,
                 unary_op_to_str(instr->operation), instr->rhs.i);
}

void ir_chunk_print(const struct IrChunk *root_chunk, const char *name, struct Writer *writer) {
  writer->writef(writer, "%s:\n", name);
  for (size_t i = 0; i < root_chunk->length; i++) {
    const struct IrInstr* instr = &root_chunk->instrs[i];
    switch (instr->type) {
    case II_LiteralIntoTemp:
      ir_instr_literal_into_temp_print(&instr->literal, writer);
      break;
    case II_VarIntoTemp:
      ir_instr_var_into_temp_print(&instr->var, writer);
      break;
    case II_Binary:
      ir_instr_binary_print(&instr->binary, writer);
      break;
    case II_Unary:
      ir_instr_unary_print(&instr->unary, writer);
      break;
    default:
      UNREACHABLE();
    }
  }
}

// TODO: Update when we're linking other chunks
void ir_chunk_fini(struct IrChunk* root_chunk) {
  free(root_chunk->instrs);
}
