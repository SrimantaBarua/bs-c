#include "ir.h"

#include <stdlib.h>
#include <stdarg.h>

#include "ast.h"
#include "log.h"
#include "writer.h"

#define INVALID_TEMP ((struct Temp) { UINT64_MAX })
#define TEMP_IS_INVALID(TEMP) ((TEMP).i == UINT64_MAX)

struct IrGenerator {
  struct IrChunk* current_chunk; // Chunk we're currently writing to
  struct Writer* writer;         // Where to write error message to
  size_t next_temp;              // Next temporary value to generate
  size_t next_label;             // Next label to generate
  const char* source;            // The original source code (for error messages)
};

static void error(struct IrGenerator* gen, const struct Ast* ast, const char *fmt, ...) {
  va_list ap;
  gen->writer->writef(gen->writer, "\x1b[1;31mERROR\x1b[0m: ");
  va_start(ap, fmt);
  gen->writer->vwritef(gen->writer, fmt, ap);
  va_end(ap);
  ast_print(ast, gen->writer);
  struct SourceLine line;
  const char *source = gen->source;
  if (get_source_line(source, ast->offset, 1, &line)) {
    gen->writer->writef(gen->writer, "\n[%lu]: %.*s\x1b[1;4m%.*s\x1b[0m%.*s\n",
                        line.line_number, line.range_start, line.start,
                        line.range_end - line.range_start, line.start + line.range_start,
                        line.length - line.range_end, line.start + line.range_end);
  } else {
    gen->writer->writef(gen->writer, "\n");
  }
}

// FIXME: This doesn't check for overflow, and assumes temp index will never overflow
static struct Temp new_temp(struct IrGenerator* gen) {
  return (struct Temp) { gen->next_temp++ };
}

// FIXME: This doesn't check for overflow, and assumes label index will never overflow
static struct Label new_label(struct IrGenerator* gen) {
  return (struct Label) { gen->next_label++ };
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

static void write_label(struct IrGenerator* gen, size_t offset, struct Label label) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Label,
    .label = label,
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_jump(struct IrGenerator* gen, size_t offset, struct Label label) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Jump,
    .jump = (struct IrInstrJump) {
      .destination = label,
    },
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_jump_if_false(struct IrGenerator* gen, size_t offset, struct Temp condition,
                                struct Label label) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_JumpIfFalse,
    .cond_jump = (struct IrInstrJumpIfFalse) {
      .condition = condition,
      .destination = label,
    },
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_member_instr(struct IrGenerator* gen, size_t offset, struct Temp dest,
                               struct Temp lhs, struct Str member) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Member,
    .member = (struct IrInstrMember) {
      .destination = dest,
      .lhs = lhs,
      .member = member,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_member_lvalue_instr(struct IrGenerator* gen, size_t offset, struct Temp dest,
                                      struct Temp lhs, struct Str member) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_MemberLvalue,
    .member = (struct IrInstrMember) {
      .destination = dest,
      .lhs = lhs,
      .member = member,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_index_instr(struct IrGenerator* gen, size_t offset, struct Temp dest,
                              struct Temp lhs, struct Temp index) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Index,
    .index = (struct IrInstrIndex) {
      .destination = dest,
      .lhs = lhs,
      .index = index,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_index_lvalue_instr(struct IrGenerator* gen, size_t offset, struct Temp dest,
                                     struct Temp lhs, struct Temp index) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_IndexLvalue,
    .index = (struct IrInstrIndex) {
      .destination = dest,
      .lhs = lhs,
      .index = index,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
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
    .type = II_VarRvalue,
    .var = (struct IrInstrVar) {
      .destination = dest,
      .identifier = identifier,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_variable_lvalue(struct IrGenerator* gen, size_t offset, struct Temp dest,
                                  struct Str identifier) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_VarLvalue,
    .var = (struct IrInstrVar) {
      .destination = dest,
      .identifier = identifier,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_nil_literal(struct IrGenerator* gen, size_t offset, struct Temp dest) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Literal,
    .literal = (struct IrInstrLiteral) {
      .destination = dest,
      .literal = (struct IrLiteral) {
        .type = IL_Nil,
        .b = false
      }
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_boolean_literal(struct IrGenerator* gen, size_t offset, struct Temp dest, bool b) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Literal,
    .literal = (struct IrInstrLiteral) {
      .destination = dest,
      .literal = (struct IrLiteral) {
        .type = IL_Boolean,
        .b = b
      }
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_integer_literal(struct IrGenerator* gen, size_t offset, struct Temp dest, int64_t i) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Literal,
    .literal = (struct IrInstrLiteral) {
      .destination = dest,
      .literal = (struct IrLiteral) {
        .type = IL_Integer,
        .i = i
      }
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_assignment_instr(struct IrGenerator* gen, size_t offset, struct Temp lhs,
                                   struct Temp rhs) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Assignment,
    .assign = (struct IrInstrAssignment) {
      .destination = lhs,
      .source = rhs,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_push(struct IrGenerator* gen, size_t offset, struct Temp source) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Push,
    .push = (struct IrInstrPush) {
      .source = source,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

static void write_call(struct IrGenerator* gen, size_t offset, struct Temp dest, struct Temp func,
                       size_t num_args) {
  struct IrInstr instruction = {
    .offset = offset,
    .type = II_Call,
    .call = (struct IrInstrCall) {
      .destination = dest,
      .function = func,
      .num_args = num_args,
    }
  };
  chunk_push_instr(gen->current_chunk, instruction);
}

// Forward declaration
static bool emit(struct IrGenerator* gen, const struct Ast* ast, struct Temp* result);

static bool emit_program(struct IrGenerator* gen, const struct AstProgram* ast, struct Temp* result) {
  struct Temp dummy;
  for (size_t i = 0; i < ast->statements.length; i++) {
    if (!emit(gen, ast->statements.data[i], &dummy)) {
      return false;
    }
  }
  *result = INVALID_TEMP;
  return true;
}

// TODO: Use the environment to resolve locals, (or upvalues for closures)
static bool emit_block(struct IrGenerator* gen, const struct AstBlock* ast, struct Temp* result) {
  struct Temp block_result;
  for (size_t i = 0; i < ast->statements.length; i++) {
    if (!emit(gen, ast->statements.data[i], &block_result)) {
      return false;
    }
  }
  if (ast->last_had_semicolon) {
    *result = INVALID_TEMP;
  } else {
    *result = block_result;
  }
  return true;
}

static bool emit_member(struct IrGenerator* gen, const struct AstMember* ast, struct Temp* result) {
  struct Temp lhs, dest;
  if (!emit(gen, ast->lhs, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  dest = new_temp(gen);
  write_member_instr(gen, ast->ast.offset, dest, lhs, ast->member);
  *result = dest;
  return true;
}

static bool emit_member_lvalue(struct IrGenerator* gen, const struct AstMember* ast, struct Temp* result) {
  struct Temp lhs, dest;
  if (!emit(gen, ast->lhs, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  dest = new_temp(gen);
  write_member_lvalue_instr(gen, ast->ast.offset, dest, lhs, ast->member);
  *result = dest;
  return true;
}

static bool emit_index(struct IrGenerator* gen, const struct AstIndex* ast, struct Temp* result) {
  struct Temp lhs, index, dest;
  if (!emit(gen, ast->lhs, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  if (!emit(gen, ast->index, &index)) {
    return false;
  }
  if (TEMP_IS_INVALID(index)) {
    return false;
  }
  dest = new_temp(gen);
  write_index_instr(gen, ast->ast.offset, dest, lhs, index);
  *result = dest;
  return true;
}

static bool emit_index_lvalue(struct IrGenerator* gen, const struct AstIndex* ast, struct Temp* result) {
  struct Temp lhs, index, dest;
  if (!emit(gen, ast->lhs, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  if (!emit(gen, ast->index, &index)) {
    return false;
  }
  if (TEMP_IS_INVALID(index)) {
    return false;
  }
  dest = new_temp(gen);
  write_index_lvalue_instr(gen, ast->ast.offset, dest, lhs, index);
  *result = dest;
  return true;
}

static bool emit_binary(struct IrGenerator* gen, const struct AstBinary* ast, struct Temp* result) {
  struct Temp lhs, rhs, dest;
  if (!emit(gen, ast->lhs, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  if (!emit(gen, ast->rhs, &rhs)) {
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
  if (!emit(gen, ast->rhs, &rhs)) {
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

// TODO: Refer to the "environment" to see if we should capture any variables (i.e. is this a closure?)
static bool emit_identifier_lvalue(struct IrGenerator* gen, const struct AstIdentifier* ast,
                                   struct Temp* result) {
  struct Temp dest = new_temp(gen);
  write_variable_lvalue(gen, ast->ast.offset, dest, ast->identifier);
  *result = dest;
  return true;
}

static bool emit_nil(struct IrGenerator* gen, const struct AstNil* ast, struct Temp* result) {
  struct Temp dest = new_temp(gen);
  write_nil_literal(gen, ast->ast.offset, dest);
  *result = dest;
  return true;
}

static bool emit_boolean(struct IrGenerator* gen, const struct AstBoolean* ast, struct Temp* result) {
  struct Temp dest = new_temp(gen);
  write_boolean_literal(gen, ast->ast.offset, dest, true);
  *result = dest;
  return true;
}

static bool emit_integer(struct IrGenerator* gen, const struct AstInteger* ast, struct Temp* result) {
  struct Temp dest = new_temp(gen);
  write_integer_literal(gen, ast->ast.offset, dest, ast->i);
  *result = dest;
  return true;
}

static bool emit_call(struct IrGenerator* gen, const struct AstCall* ast, struct Temp* result) {
  struct Temp dest, func;
  for (size_t i = 0; i < ast->arguments.length; i++) {
    struct Temp arg;
    if (!emit(gen, ast->arguments.data[i], &arg)) {
      return false;
    }
    if (TEMP_IS_INVALID(arg)) {
      return false;
    }
    write_push(gen, ast->ast.offset, arg);
  }
  if (!emit(gen, ast->function, &func)) {
    return false;
  }
  if (TEMP_IS_INVALID(func)) {
    return false;
  }
  dest = new_temp(gen);
  write_call(gen, ast->ast.offset, dest, func, ast->arguments.length);
  *result = dest;
  return true;
}

static bool emit_while(struct IrGenerator* gen, const struct AstWhile* ast, struct Temp* result) {
  struct Label loop_start = new_label(gen);
  struct Label loop_end = new_label(gen);
  struct Temp condition, dummy;
  write_label(gen, ast->ast.offset, loop_start);
  if (!emit(gen, ast->condition, &condition)) {
    return false;
  }
  if (TEMP_IS_INVALID(condition)) {
    return false;
  }
  write_jump_if_false(gen, ast->ast.offset, condition, loop_end);
  if (!emit(gen, ast->body, &dummy)) {
    return false;
  }
  write_jump(gen, ast->ast.offset, loop_start);
  write_label(gen, ast->ast.offset, loop_end);
  *result = INVALID_TEMP;
  return true;
}

static bool emit_for(struct IrGenerator* gen, const struct AstFor* ast, struct Temp* result) {
  struct Temp generator, iterator_ref, iterator, condition, nil, dummy;
  if (!emit(gen, ast->generator, &generator)) {
    return false;
  }
  if (TEMP_IS_INVALID(generator)) {
    return false;
  }
  struct Label loop_start = new_label(gen);
  struct Label loop_end = new_label(gen);
  write_label(gen, ast->ast.offset, loop_start);
  // Temporary to store just "nil"
  nil = new_temp(gen);
  write_nil_literal(gen, ast->generator->offset, nil);
  // Call the generator
  iterator = new_temp(gen);
  write_call(gen, ast->generator->offset, iterator, generator, 0);
  // If the result is nil, jump to the end of the loop
  condition = new_temp(gen);
  write_binary_instr(gen, ast->generator->offset, condition, BO_NotEqual, nil, iterator);
  write_jump_if_false(gen, ast->generator->offset, condition, loop_end);
  // Store the iterator in the variable.
  // TODO: this variable needs to be declared, and should only be part of the body's environment.
  iterator_ref = new_temp(gen);
  write_variable_lvalue(gen, ast->ast.offset, iterator_ref, ast->identifier);
  write_assignment_instr(gen, ast->ast.offset, iterator_ref, iterator);
  // Generate the loop body
  if (!emit(gen, ast->body, &dummy)) {
    return false;
  }
  write_jump(gen, ast->ast.offset, loop_start);
  write_label(gen, ast->ast.offset, loop_end);
  *result = INVALID_TEMP;
  return true;
}

static bool emit_lvalue(struct IrGenerator* gen, const struct Ast* ast, struct Temp* result) {
  switch (ast->type) {
  case AST_Identifier:
    return emit_identifier_lvalue(gen, (const struct AstIdentifier*) ast, result);
  case AST_Member:
    return emit_member_lvalue(gen, (const struct AstMember*) ast, result);
  case AST_Index:
    return emit_index_lvalue(gen, (const struct AstIndex*) ast, result);
  default:
    error(gen, ast, "expected lvalue, found: ");
    *result = INVALID_TEMP;
    return false;
  }
}

static bool emit_assignment(struct IrGenerator* gen, const struct AstAssignment* ast, struct Temp* result) {
  struct Temp lhs, rhs;
  if (!emit_lvalue(gen, ast->lhs, &lhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(lhs)) {
    return false;
  }
  if (!emit(gen, ast->rhs, &rhs)) {
    return false;
  }
  if (TEMP_IS_INVALID(rhs)) {
    return false;
  }
  write_assignment_instr(gen, ast->ast.offset, lhs, rhs);
  *result = INVALID_TEMP;
  return true;
}

static bool emit(struct IrGenerator* gen, const struct Ast* ast, struct Temp* result) {
  switch (ast->type) {
  case AST_Program:    return emit_program(gen, (const struct AstProgram*) ast, result);
  case AST_Block:      return emit_block(gen, (const struct AstBlock*) ast, result);
  case AST_Struct:
    UNIMPLEMENTED();
    break;
  case AST_Function:
    UNIMPLEMENTED();
    break;
  case AST_If:
    UNIMPLEMENTED();
    break;
  case AST_While:      return emit_while(gen, (const struct AstWhile*) ast, result);
  case AST_For:        return emit_for(gen, (const struct AstFor*) ast, result);
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
  case AST_Member:     return emit_member(gen, (const struct AstMember*) ast, result);
  case AST_Index:      return emit_index(gen, (const struct AstIndex*) ast, result);
  case AST_Assignment: return emit_assignment(gen, (const struct AstAssignment*) ast, result);
  case AST_Binary:     return emit_binary(gen, (const struct AstBinary*) ast, result);
  case AST_Unary:      return emit_unary(gen, (const struct AstUnary*) ast, result);
  case AST_Call:       return emit_call(gen, (const struct AstCall*) ast, result);
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
  case AST_Identifier: return emit_identifier(gen, (const struct AstIdentifier*) ast, result);
  case AST_Float:
    UNIMPLEMENTED();
    break;
  case AST_Integer:    return emit_integer(gen, (const struct AstInteger*) ast, result);
  case AST_Boolean:    return emit_boolean(gen, (const struct AstBoolean*) ast, result);
  case AST_Ellipsis:
    UNIMPLEMENTED();
    break;
  case AST_Nil:        return emit_nil(gen, (const struct AstNil*) ast, result);
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
  generator.next_label = 0;
  generator.source = source;
  return emit(&generator, ast, &dummy);
}

static void ir_instr_literal_print(const struct IrInstrLiteral* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := ", instr->destination.i);
  switch (instr->literal.type) {
  case IL_Nil:
    writer->writef(writer, "nil\n");
    break;
  case IL_Boolean:
    writer->writef(writer, instr->literal.b ? "true\n" : "false\n");
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

static void ir_instr_var_rvalue_print(const struct IrInstrVar* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := %.*s\n", instr->destination.i, instr->identifier.length,
                 (const char*) instr->identifier.data);
}

static void ir_instr_var_lvalue_print(const struct IrInstrVar* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := &%.*s\n", instr->destination.i, instr->identifier.length,
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

static void ir_instr_member_rvalue_print(const struct IrInstrMember* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := t%llu.%.*s\n", instr->destination.i, instr->lhs.i,
                 instr->member.length, (const char*) instr->member.data);
}

static void ir_instr_member_lvalue_print(const struct IrInstrMember* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := &t%llu.%.*s\n", instr->destination.i, instr->lhs.i,
                 instr->member.length, (const char*) instr->member.data);
}

static void ir_instr_index_rvalue_print(const struct IrInstrIndex* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := t%llu[t%llu]\n", instr->destination.i, instr->lhs.i, instr->index.i);
}

static void ir_instr_index_lvalue_print(const struct IrInstrIndex* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := &t%llu[t%llu]\n", instr->destination.i, instr->lhs.i, instr->index.i);
}

static void ir_instr_assignment_print(const struct IrInstrAssignment* instr, struct Writer* writer) {
  writer->writef(writer, "  *t%llu := t%llu\n", instr->destination.i, instr->source.i);
}

static void ir_instr_push_print(const struct IrInstrPush* instr, struct Writer* writer) {
  writer->writef(writer, "  push t%llu\n", instr->source.i);
}

static void ir_instr_call_print(const struct IrInstrCall* instr, struct Writer* writer) {
  writer->writef(writer, "  t%llu := t%llu(%lu)\n", instr->destination.i, instr->function.i,
                 instr->num_args);
}

static void ir_instr_label_print(const struct Label* label, struct Writer* writer) {
  writer->writef(writer, "L%llu:\n", label->i);
}

static void ir_instr_jump_if_false_print(const struct IrInstrJumpIfFalse* instr,
                                         struct Writer* writer) {
  writer->writef(writer, "  goto L%llu if not t%llu\n", instr->destination.i, instr->condition.i);
}

static void ir_instr_jump_print(const struct IrInstrJump* instr, struct Writer* writer) {
  writer->writef(writer, "  goto L%llu\n", instr->destination.i);
}

void ir_chunk_print(const struct IrChunk *root_chunk, const char *name, struct Writer *writer) {
  writer->writef(writer, "%s:\n", name);
  for (size_t i = 0; i < root_chunk->length; i++) {
    const struct IrInstr* instr = &root_chunk->instrs[i];
    switch (instr->type) {
    case II_Literal:
      ir_instr_literal_print(&instr->literal, writer);
      break;
    case II_VarRvalue:
      ir_instr_var_rvalue_print(&instr->var, writer);
      break;
    case II_Binary:
      ir_instr_binary_print(&instr->binary, writer);
      break;
    case II_Unary:
      ir_instr_unary_print(&instr->unary, writer);
      break;
    case II_VarLvalue:
      ir_instr_var_lvalue_print(&instr->var, writer);
      break;
    case II_Member:
      ir_instr_member_rvalue_print(&instr->member, writer);
      break;
    case II_MemberLvalue:
      ir_instr_member_lvalue_print(&instr->member, writer);
      break;
    case II_Index:
      ir_instr_index_rvalue_print(&instr->index, writer);
      break;
    case II_IndexLvalue:
      ir_instr_index_lvalue_print(&instr->index, writer);
      break;
    case II_Assignment:
      ir_instr_assignment_print(&instr->assign, writer);
      break;
    case II_Push:
      ir_instr_push_print(&instr->push, writer);
      break;
    case II_Call:
      ir_instr_call_print(&instr->call, writer);
      break;
    case II_Label:
      ir_instr_label_print(&instr->label, writer);
      break;
    case II_JumpIfFalse:
      ir_instr_jump_if_false_print(&instr->cond_jump, writer);
      break;
    case II_Jump:
      ir_instr_jump_print(&instr->jump, writer);
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
