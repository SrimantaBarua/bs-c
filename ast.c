#include "ast.h"

#include <stdlib.h>

#include "log.h"
#include "string.h"
#include "util.h"

#define REALLOC(PTR, SIZE) do {        \
    if (!(PTR = realloc(PTR, SIZE))) { \
      DIE_ERR("realloc()");            \
    }                                  \
  } while (0)

#define TRY_ACCUM(ACCUM, ACTION) do { \
    int __tmp = ACTION;               \
    if (__tmp < 0) {                  \
      return __tmp;                   \
    }                                 \
    ACCUM += __tmp;                   \
} while (0)

const char* binary_op_to_str(enum BinaryOp op) {
  switch (op) {
  case BO_Equal:        return "==";
  case BO_NotEqual:     return "!=";
  case BO_LessEqual:    return "<=";
  case BO_LessThan:     return "<";
  case BO_GreaterEqual: return ">=";
  case BO_GreaterThan:  return ">";
  case BO_ShiftLeft:    return "<<";
  case BO_ShiftRight:   return ">>";
  case BO_Add:          return "+";
  case BO_Subtract:     return "-";
  case BO_Multiply:     return "*";
  case BO_Divide:       return "/";
  case BO_Modulo:       return "%";
  case BO_BitOr:        return "|";
  case BO_BitAnd:       return "&";
  case BO_BitXor:       return "^";
  case BO_LogicalAnd:   return "and";
  case BO_LogicalOr:    return "or";
  }
}

const char* unary_op_to_str(enum UnaryOp op) {
  switch (op) {
  case UO_Minus:      return "-";
  case UO_BitNot:     return "!";
  case UO_LogicalNot: return "not";
  }
}

void ast_vec_init(struct AstVec* vec) {
  vec->data = NULL;
  vec->length = vec->capacity = 0;
}

void ast_vec_fini(struct AstVec* vec) {
  for (size_t i = 0; i < vec->length; i++) {
    ast_free(vec->data[i]);
  }
  free(vec->data);
}

void ast_vec_push(struct AstVec* vec, struct Ast* ast) {
  if (vec->length == vec->capacity) {
    size_t new_capacity = vec->capacity == 0 ? 8 : vec->capacity * 2;
    REALLOC(vec->data, new_capacity * sizeof(struct Ast*));
    vec->capacity = new_capacity;
  }
  vec->data[vec->length++] = ast;
}

static int ast_vec_print(const struct AstVec* vec, struct Writer* writer) {
  int ret = 0;
  for (size_t i = 0; i < vec->length; i++) {
    TRY_ACCUM(ret, writer->writef(writer, " "));
    TRY_ACCUM(ret, ast_print(vec->data[i], writer));
  }
  return ret;
}

static struct AstVec ast_vec_clone(const struct AstVec* vec) {
  struct AstVec ret;
  ret.length = vec->length;
  ret.capacity = vec->capacity;
  ret.data = NULL;
  REALLOC(ret.data, ret.capacity * sizeof(struct Ast*));
  for (size_t i = 0; i < ret.length; i++) {
    ret.data[i] = ast_clone(vec->data[i]);
  }
  return ret;
}

void ast_pair_vec_init(struct AstPairVec* vec) {
  vec->data = NULL;
  vec->length = vec->capacity = 0;
}

void ast_pair_vec_fini(struct AstPairVec* vec) {
  for (size_t i = 0; i < vec->length; i++) {
    ast_free(vec->data[i].key);
    ast_free(vec->data[i].value);
  }
  free(vec->data);
}

void ast_pair_vec_push(struct AstPairVec* vec, struct Ast* key, struct Ast* value) {
  if (vec->length == vec->capacity) {
    size_t new_capacity = vec->capacity == 0 ? 8 : vec->capacity * 2;
    REALLOC(vec->data, new_capacity * sizeof(struct AstPair));
    vec->capacity = new_capacity;
  }
  struct AstPair pair = { key, value };
  vec->data[vec->length++] = pair;
}

static struct AstPairVec ast_pair_vec_clone(const struct AstPairVec* vec) {
  struct AstPairVec ret;
  ret.length = vec->length;
  ret.capacity = vec->capacity;
  ret.data = NULL;
  REALLOC(ret.data, ret.capacity * sizeof(struct AstPair*));
  for (size_t i = 0; i < ret.length; i++) {
    ret.data[i].key = ast_clone(vec->data[i].key);
    ret.data[i].value = ast_clone(vec->data[i].value);
  }
  return ret;
}

#define ALLOC_AST(TYPE, VAR, OFFSET)                \
  struct Ast##TYPE* VAR;                            \
  if (!(VAR = malloc(sizeof(struct Ast##TYPE)))) {  \
    DIE_ERR("malloc()");                            \
  }                                                 \
  VAR->ast.type = AST_##TYPE;                       \
  VAR->ast.offset = OFFSET;                         \

struct Ast* ast_program_create(size_t offset, struct AstVec statements) {
  ALLOC_AST(Program, program, offset);
  program->statements = statements;
  return (struct Ast*) program;
}

static struct Ast* ast_program_clone(const struct AstProgram* ast) {
  ALLOC_AST(Program, program, ast->ast.offset);
  program->statements = ast_vec_clone(&ast->statements);
  return (struct Ast*) program;
}

static int ast_program_print(const struct AstProgram *ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(program"));
  TRY_ACCUM(ret, ast_vec_print(&ast->statements, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_program_free(struct AstProgram* ast) {
  ast_vec_fini(&ast->statements);
  free(ast);
}

struct Ast* ast_block_create(size_t offset, struct AstVec statements, bool last_had_semicolon) {
  ALLOC_AST(Block, block, offset);
  block->last_had_semicolon = last_had_semicolon;
  block->statements = statements;
  return (struct Ast*) block;
}

static struct Ast* ast_block_clone(const struct AstBlock* ast) {
  ALLOC_AST(Block, block, ast->ast.offset);
  block->last_had_semicolon = ast->last_had_semicolon;
  block->statements = ast_vec_clone(&ast->statements);
  return (struct Ast*) block;
}

static int ast_block_print(const struct AstBlock *ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(block <%s>", ast->last_had_semicolon ? "noret" : "ret"));
  TRY_ACCUM(ret, ast_vec_print(&ast->statements, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_block_free(struct AstBlock* ast) {
  ast_vec_fini(&ast->statements);
  free(ast);
}

struct Ast* ast_struct_create(size_t offset, const struct Str* opt_parent, struct Ast* body) {
  ALLOC_AST(Struct, struct_node, offset);
  if (!opt_parent) {
    struct_node->has_parent = false;
    struct_node->opt_parent = (struct Str) { NULL, 0 };
  } else {
    struct_node->has_parent = true;
    struct_node->opt_parent = *opt_parent;
  }
  struct_node->body = body;
  return (struct Ast*) struct_node;
}

static struct Ast* ast_struct_clone(const struct AstStruct* ast) {
  ALLOC_AST(Struct, struct_node, ast->ast.offset);
  struct_node->has_parent = ast->has_parent;
  struct_node->opt_parent = ast->opt_parent;
  struct_node->body = ast_clone(ast->body);
  return (struct Ast*) struct_node;
}

static int ast_struct_print(const struct AstStruct* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(struct "));
  if (ast->has_parent) {
    TRY_ACCUM(ret, writer->writef(writer, "(parent "));
    TRY_ACCUM(ret, str_print(&ast->opt_parent, writer));
    TRY_ACCUM(ret, writer->writef(writer, ") "));
  }
  TRY_ACCUM(ret, ast_print(ast->body, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_struct_free(struct AstStruct* ast) {
  ast_free(ast->body);
  free(ast);
}

struct Ast* ast_function_create(size_t offset, struct AstVec parameters, struct Ast* body) {
  ALLOC_AST(Function, function, offset);
  function->parameters = parameters;
  function->body = body;
  return (struct Ast*) function;
}

static struct Ast* ast_function_clone(const struct AstFunction* ast) {
  ALLOC_AST(Function, function, ast->ast.offset);
  function->parameters = ast_vec_clone(&ast->parameters);
  function->body = ast_clone(ast->body);
  return (struct Ast*) function;
}

static int ast_function_print(const struct AstFunction* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(fn (params"));
  TRY_ACCUM(ret, ast_vec_print(&ast->parameters, writer));
  TRY_ACCUM(ret, writer->writef(writer, ") "));
  TRY_ACCUM(ret, ast_print(ast->body, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_function_free(struct AstFunction* ast) {
  ast_vec_fini(&ast->parameters);
  ast_free(ast->body);
  free(ast);
}

struct Ast* ast_if_create(size_t offset, struct Ast* condition, struct Ast* body, struct Ast* else_part) {
  ALLOC_AST(If, if_statement, offset);
  if_statement->condition = condition;
  if_statement->body = body;
  if_statement->else_part = else_part;
  return (struct Ast*) if_statement;
}

static struct Ast* ast_if_clone(const struct AstIf* ast) {
  ALLOC_AST(If, if_statement, ast->ast.offset);
  if_statement->condition = ast_clone(ast->condition);
  if_statement->body = ast_clone(ast->body);
  if_statement->else_part = ast_clone(ast->else_part);
  return (struct Ast*) if_statement;
}

static int ast_if_print(const struct AstIf* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(if "));
  TRY_ACCUM(ret, ast_print(ast->condition, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, ast_print(ast->body, writer));
  if (ast->else_part) {
    TRY_ACCUM(ret, writer->writef(writer, " (else "));
    TRY_ACCUM(ret, ast_print(ast->else_part, writer));
    TRY_ACCUM(ret, writer->writef(writer, "))"));
    return ret;
  }
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_if_free(struct AstIf* ast) {
  ast_free(ast->condition);
  ast_free(ast->body);
  ast_free(ast->else_part);
  free(ast);
}

struct Ast* ast_while_create(size_t offset, struct Ast* condition, struct Ast* body) {
  ALLOC_AST(While, while_loop, offset);
  while_loop->condition = condition;
  while_loop->body = body;
  return (struct Ast*) while_loop;
}

static struct Ast* ast_while_clone(const struct AstWhile* ast) {
  ALLOC_AST(While, while_loop, ast->ast.offset);
  while_loop->condition = ast_clone(ast->condition);
  while_loop->body = ast_clone(ast->body);
  return (struct Ast*) while_loop;
}

static int ast_while_print(const struct AstWhile* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(while "));
  TRY_ACCUM(ret, ast_print(ast->condition, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, ast_print(ast->body, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_while_free(struct AstWhile* ast) {
  ast_free(ast->condition);
  ast_free(ast->body);
  free(ast);
}

struct Ast* ast_for_create(size_t offset, struct Str identifier, struct Ast* generator, struct Ast* body) {
  ALLOC_AST(For, for_loop, offset);
  for_loop->identifier = identifier;
  for_loop->generator = generator;
  for_loop->body = body;
  return (struct Ast*) for_loop;
}

static struct Ast* ast_for_clone(const struct AstFor* ast) {
  ALLOC_AST(For, for_loop, ast->ast.offset);
  for_loop->identifier = ast->identifier;
  for_loop->generator = ast_clone(ast->generator);
  for_loop->body = ast_clone(ast->body);
  return (struct Ast*) for_loop;
}

static int ast_for_print(const struct AstFor* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(for "));
  TRY_ACCUM(ret, str_print(&ast->identifier, writer));
  TRY_ACCUM(ret, writer->writef(writer, " in "));
  TRY_ACCUM(ret, ast_print(ast->generator, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, ast_print(ast->body, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_for_free(struct AstFor* ast) {
  ast_free(ast->generator);
  ast_free(ast->body);
  free(ast);
}

struct Ast* ast_let_create(size_t offset, bool public, struct Str variable, struct Ast* rhs) {
  ALLOC_AST(Let, let, offset);
  let->public = public;
  let->variable = variable;
  let->rhs = rhs;
  return (struct Ast*) let;
}

static struct Ast* ast_let_clone(const struct AstLet* ast) {
  ALLOC_AST(Let, let, ast->ast.offset);
  let->public = ast->public;
  let->variable = ast->variable;
  let->rhs = ast_clone(ast->rhs);
  return (struct Ast*) let;
}

static int ast_let_print(const struct AstLet* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(let "));
  TRY_ACCUM(ret, str_print(&ast->variable, writer));
  TRY_ACCUM(ret, writer->writef(writer, " <%s> ", ast->public ? "public" : "private"));
  TRY_ACCUM(ret, ast_print(ast->rhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_let_free(struct AstLet* ast) {
  ast_free(ast->rhs);
  free(ast);
}

struct Ast* ast_require_create(size_t offset, struct Str module) {
  ALLOC_AST(Require, require, offset);
  require->module = module;
  return (struct Ast*) require;
}

static struct Ast* ast_require_clone(const struct AstRequire* ast) {
  ALLOC_AST(Require, require, ast->ast.offset);
  require->module = ast->module;
  return (struct Ast*) require;
}

static int ast_require_print(const struct AstRequire* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(require \""));
  TRY_ACCUM(ret, str_print(&ast->module, writer));
  TRY_ACCUM(ret, writer->writef(writer, "\")"));
  return ret;
}

static void ast_require_free(struct AstRequire* ast) {
  free(ast);
}

struct Ast* ast_yield_create(size_t offset, struct Ast* value) {
  ALLOC_AST(Yield, yield, offset);
  yield->value = value;
  return (struct Ast*) yield;
}

static struct Ast* ast_yield_clone(const struct AstYield* ast) {
  ALLOC_AST(Yield, yield, ast->ast.offset);
  yield->value = ast_clone(ast->value);
  return (struct Ast*) yield;
}

static int ast_yield_print(const struct AstYield* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(yield "));
  TRY_ACCUM(ret, ast_print(ast->value, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_yield_free(struct AstYield* ast) {
  ast_free(ast->value);
  free(ast);
}

struct Ast* ast_break_create(size_t offset) {
  ALLOC_AST(Break, break_statement, offset);
  return (struct Ast*) break_statement;
}

static struct Ast* ast_break_clone(const struct AstBreak* ast) {
  ALLOC_AST(Break, break_statement, ast->ast.offset);
  return (struct Ast*) break_statement;
}

static int ast_break_print(const struct AstBreak* ast, struct Writer* writer) {
  UNUSED(ast);
  return writer->writef(writer, "(break)");
}

static void ast_break_free(struct AstBreak* ast) {
  free(ast);
}

struct Ast* ast_continue_create(size_t offset) {
  ALLOC_AST(Continue, continue_statement, offset);
  return (struct Ast*) continue_statement;
}

static struct Ast* ast_continue_clone(const struct AstContinue* ast) {
  ALLOC_AST(Continue, continue_statement, ast->ast.offset);
  return (struct Ast*) continue_statement;
}

static int ast_continue_print(const struct AstContinue* ast, struct Writer* writer) {
  UNUSED(ast);
  return writer->writef(writer, "(continue)");
}

static void ast_continue_free(struct AstContinue* ast) {
  free(ast);
}

struct Ast* ast_return_create(size_t offset, struct Ast* value) {
  ALLOC_AST(Return, return_statement, offset);
  return_statement->value = value;
  return (struct Ast*) return_statement;
}

static struct Ast* ast_return_clone(const struct AstReturn* ast) {
  ALLOC_AST(Return, return_statement, ast->ast.offset);
  return_statement->value = ast_clone(ast->value);
  return (struct Ast*) return_statement;
}

static int ast_return_print(const struct AstReturn* ast, struct Writer* writer) {
  if (ast->value) {
    int ret = 0;
    TRY_ACCUM(ret, writer->writef(writer, "(return "));
    TRY_ACCUM(ret, ast_print(ast->value, writer));
    TRY_ACCUM(ret, writer->writef(writer, ")"));
    return ret;
  } else {
    return writer->writef(writer, "(return)");
  }
}

static void ast_return_free(struct AstReturn* ast) {
  ast_free(ast->value);
  free(ast);
}

struct Ast* ast_member_create(size_t offset, struct Ast* lhs, struct Str member) {
  ALLOC_AST(Member, member_ref, offset);
  member_ref->lhs = lhs;
  member_ref->member = member;
  return (struct Ast*) member_ref;
}

static struct Ast* ast_member_clone(const struct AstMember* ast) {
  ALLOC_AST(Member, member_ref, ast->ast.offset);
  member_ref->lhs = ast_clone(ast->lhs);
  member_ref->member = ast->member;
  return (struct Ast*) member_ref;
}

static int ast_member_print(const struct AstMember* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(. "));
  TRY_ACCUM(ret, ast_print(ast->lhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, str_print(&ast->member, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_member_free(struct AstMember* ast) {
  ast_free(ast->lhs);
  free(ast);
}

struct Ast* ast_index_create(size_t offset, struct Ast* lhs, struct Ast* index) {
  ALLOC_AST(Index, index_op, offset);
  index_op->lhs = lhs;
  index_op->index = index;
  return (struct Ast*) index_op;
}

static struct Ast* ast_index_clone(const struct AstIndex* ast) {
  ALLOC_AST(Index, index_op, ast->ast.offset);
  index_op->lhs = ast_clone(ast->lhs);
  index_op->index = ast_clone(ast->index);
  return (struct Ast*) index_op;
}

static int ast_index_print(const struct AstIndex* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "([] "));
  TRY_ACCUM(ret, ast_print(ast->lhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, ast_print(ast->index, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_index_free(struct AstIndex* ast) {
  ast_free(ast->lhs);
  ast_free(ast->index);
  free(ast);
}

struct Ast* ast_binary_create(size_t offset, enum BinaryOp operation, struct Ast* lhs, struct Ast* rhs) {
  ALLOC_AST(Binary, binary, offset);
  binary->operation = operation;
  binary->lhs = lhs;
  binary->rhs = rhs;
  return (struct Ast*) binary;
}

static struct Ast* ast_binary_clone(const struct AstBinary* ast) {
  ALLOC_AST(Binary, binary, ast->ast.offset);
  binary->operation = ast->operation;
  binary->lhs = ast_clone(ast->lhs);
  binary->rhs = ast_clone(ast->rhs);
  return (struct Ast*) binary;
}

static int ast_binary_print(const struct AstBinary* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(%s ", binary_op_to_str(ast->operation)));
  TRY_ACCUM(ret, ast_print(ast->lhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, ast_print(ast->rhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_binary_free(struct AstBinary* ast) {
  ast_free(ast->lhs);
  ast_free(ast->rhs);
  free(ast);
}

struct Ast* ast_assignment_create(size_t offset, struct Ast* lhs, struct Ast* rhs) {
  ALLOC_AST(Assignment, assignment, offset);
  assignment->lhs = lhs;
  assignment->rhs = rhs;
  return (struct Ast*) assignment;
}

static struct Ast* ast_assignment_clone(const struct AstAssignment* ast) {
  ALLOC_AST(Assignment, assignment, ast->ast.offset);
  assignment->lhs = ast_clone(ast->lhs);
  assignment->rhs = ast_clone(ast->rhs);
  return (struct Ast*) assignment;
}

static int ast_assignment_print(const struct AstAssignment* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(= "));
  TRY_ACCUM(ret, ast_print(ast->lhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, " "));
  TRY_ACCUM(ret, ast_print(ast->rhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_assignment_free(struct AstAssignment* ast) {
  ast_free(ast->lhs);
  ast_free(ast->rhs);
  free(ast);
}

struct Ast* ast_unary_create(size_t offset, enum UnaryOp operation, struct Ast* rhs) {
  ALLOC_AST(Unary, unary, offset);
  unary->operation = operation;
  unary->rhs = rhs;
  return (struct Ast*) unary;
}

static struct Ast* ast_unary_clone(const struct AstUnary* ast) {
  ALLOC_AST(Unary, unary, ast->ast.offset);
  unary->operation = ast->operation;
  unary->rhs = ast_clone(ast->rhs);
  return (struct Ast*) unary;
}

static int ast_unary_print(const struct AstUnary* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(%s ", unary_op_to_str(ast->operation)));
  TRY_ACCUM(ret, ast_print(ast->rhs, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_unary_free(struct AstUnary* ast) {
  ast_free(ast->rhs);
  free(ast);
}

struct Ast* ast_call_create(size_t offset, struct Ast* function, struct AstVec arguments) {
  ALLOC_AST(Call, call, offset);
  call->function = function;
  call->arguments = arguments;
  return (struct Ast*) call;
}

static struct Ast* ast_call_clone(const struct AstCall* ast) {
  ALLOC_AST(Call, call, ast->ast.offset);
  call->function = ast_clone(ast->function);
  call->arguments = ast_vec_clone(&ast->arguments);
  return (struct Ast*) call;
}

static int ast_call_print(const struct AstCall* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(call "));
  TRY_ACCUM(ret, ast_print(ast->function, writer));
  TRY_ACCUM(ret, ast_vec_print(&ast->arguments, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_call_free(struct AstCall* ast) {
  ast_free(ast->function);
  ast_vec_fini(&ast->arguments);
  free(ast);
}

struct Ast* ast_self_create(size_t offset) {
  ALLOC_AST(Self, self, offset);
  return (struct Ast*) self;
}

struct Ast* ast_self_clone(const struct AstSelf* ast) {
  ALLOC_AST(Self, self, ast->ast.offset);
  return (struct Ast*) self;
}

static int ast_self_print(const struct AstSelf* ast, struct Writer* writer) {
  UNUSED(ast);
  return writer->writef(writer, "self");
}

static void ast_self_free(struct AstSelf* ast) {
  free(ast);
}

struct Ast* ast_varargs_create(size_t offset) {
  ALLOC_AST(Varargs, varargs, offset);
  return (struct Ast*) varargs;
}

static struct Ast* ast_varargs_clone(const struct AstVarargs* ast) {
  ALLOC_AST(Varargs, varargs, ast->ast.offset);
  return (struct Ast*) varargs;
}

static int ast_varargs_print(const struct AstVarargs* ast, struct Writer* writer) {
  UNUSED(ast);
  return writer->writef(writer, "varargs");
}

static void ast_varargs_free(struct AstVarargs* ast) {
  free(ast);
}

struct Ast* ast_array_create(size_t offset, struct AstVec elements) {
  ALLOC_AST(Array, array, offset);
  array->elements = elements;
  return (struct Ast*) array;
}

static struct Ast* ast_array_clone(const struct AstArray* ast) {
  ALLOC_AST(Array, array, ast->ast.offset);
  array->elements = ast_vec_clone(&ast->elements);
  return (struct Ast*) array;
}

static int ast_array_print(const struct AstArray* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(arr"));
  TRY_ACCUM(ret, ast_vec_print(&ast->elements, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_array_free(struct AstArray* ast) {
  ast_vec_fini(&ast->elements);
  free(ast);
}

struct Ast* ast_set_create(size_t offset, struct AstVec elements) {
  ALLOC_AST(Set, set, offset);
  set->elements = elements;
  return (struct Ast*) set;
}

static struct Ast* ast_set_clone(const struct AstSet* ast) {
  ALLOC_AST(Set, set, ast->ast.offset);
  set->elements = ast_vec_clone(&ast->elements);
  return (struct Ast*) set;
}

static int ast_set_print(const struct AstSet* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(set"));
  TRY_ACCUM(ret, ast_vec_print(&ast->elements, writer));
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_set_free(struct AstSet* ast) {
  ast_vec_fini(&ast->elements);
  free(ast);
}

struct Ast* ast_dictionary_create(size_t offset, struct AstPairVec kvpairs) {
  ALLOC_AST(Dictionary, dict, offset);
  dict->pairs = kvpairs;
  return (struct Ast*) dict;
}

static struct Ast* ast_dictionary_clone(const struct AstDictionary* ast) {
  ALLOC_AST(Dictionary, dict, ast->ast.offset);
  dict->pairs = ast_pair_vec_clone(&ast->pairs);
  return (struct Ast*) dict;
}

static int ast_dictionary_print(const struct AstDictionary* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "(dict"));
  for (size_t i = 0; i < ast->pairs.length; i++) {
    TRY_ACCUM(ret, writer->writef(writer, " (kvpair "));
    TRY_ACCUM(ret, ast_print(ast->pairs.data[i].key, writer));
    TRY_ACCUM(ret, writer->writef(writer, " "));
    TRY_ACCUM(ret, ast_print(ast->pairs.data[i].value, writer));
    TRY_ACCUM(ret, writer->writef(writer, ")"));
  }
  TRY_ACCUM(ret, writer->writef(writer, ")"));
  return ret;
}

static void ast_dictionary_free(struct AstDictionary* ast) {
  ast_pair_vec_fini(&ast->pairs);
  free(ast);
}

struct Ast* ast_string_create(size_t offset, struct Str str) {
  ALLOC_AST(String, string, offset);
  string->string = str;
  return (struct Ast*) string;
}

static struct Ast* ast_string_clone(const struct AstString* ast) {
  ALLOC_AST(String, string, ast->ast.offset);
  string->string = ast->string;
  return (struct Ast*) string;
}

static int ast_string_print(const struct AstString* ast, struct Writer* writer) {
  int ret = 0;
  TRY_ACCUM(ret, writer->writef(writer, "\""));
  TRY_ACCUM(ret, str_print(&ast->string, writer));
  TRY_ACCUM(ret, writer->writef(writer, "\""));
  return ret;
}

static void ast_string_free(struct AstString* ast) {
  free(ast);
}

struct Ast* ast_identifier_create(size_t offset, struct Str str) {
  ALLOC_AST(Identifier, identifier, offset);
  identifier->identifier = str;
  return (struct Ast*) identifier;
}

static struct Ast* ast_identifier_clone(const struct AstIdentifier* ast) {
  ALLOC_AST(Identifier, identifier, ast->ast.offset);
  identifier->identifier = ast->identifier;
  return (struct Ast*) identifier;
}

static int ast_identifier_print(const struct AstIdentifier* ast, struct Writer* writer) {
  return str_print(&ast->identifier, writer);
}

static void ast_identifier_free(struct AstIdentifier* ast) {
  free(ast);
}

struct Ast* ast_float_create(size_t offset, double f) {
  ALLOC_AST(Float, float_node, offset);
  float_node->f = f;
  return (struct Ast*) float_node;
}

static struct Ast* ast_float_clone(const struct AstFloat* ast) {
  ALLOC_AST(Float, float_node, ast->ast.offset);
  float_node->f = ast->f;
  return (struct Ast*) float_node;
}

static int ast_float_print(const struct AstFloat* ast, struct Writer* writer) {
  return writer->writef(writer, "%lf", ast->f);
}

static void ast_float_free(struct AstFloat* ast) {
  free(ast);
}

struct Ast* ast_integer_create(size_t offset, int64_t i) {
  ALLOC_AST(Integer, integer_node, offset);
  integer_node->i = i;
  return (struct Ast*) integer_node;
}

static struct Ast* ast_integer_clone(const struct AstInteger* ast) {
  ALLOC_AST(Integer, integer_node, ast->ast.offset);
  integer_node->i = ast->i;
  return (struct Ast*) integer_node;
}

static int ast_integer_print(const struct AstInteger* ast, struct Writer* writer) {
  return writer->writef(writer, "%ld", ast->i);
}

static void ast_integer_free(struct AstInteger* ast) {
  free(ast);
}

struct Ast* ast_boolean_create(size_t offset, bool b) {
  ALLOC_AST(Boolean, bool_node, offset);
  bool_node->b = b;
  return (struct Ast*) bool_node;
}

static struct Ast* ast_boolean_clone(const struct AstBoolean* ast) {
  ALLOC_AST(Boolean, bool_node, ast->ast.offset);
  bool_node->b = ast->b;
  return (struct Ast*) bool_node;
}

static int ast_boolean_print(const struct AstBoolean* ast, struct Writer* writer) {
  return writer->writef(writer, ast->b ? "true" : "false");
}

static void ast_boolean_free(struct AstBoolean* ast) {
  free(ast);
}

struct Ast* ast_ellipsis_create(size_t offset) {
  ALLOC_AST(Ellipsis, ellipsis, offset);
  return (struct Ast*) ellipsis;
}

static struct Ast* ast_ellipsis_clone(const struct AstEllipsis* ast) {
  ALLOC_AST(Ellipsis, ellipsis_node, ast->ast.offset);
  return (struct Ast*) ellipsis_node;
}

static int ast_ellipsis_print(const struct AstEllipsis* ast, struct Writer* writer) {
  UNUSED(ast);
  return writer->writef(writer, "...");
}

static void ast_ellipsis_free(struct AstEllipsis* ast) {
  free(ast);
}

struct Ast* ast_nil_create(size_t offset) {
  ALLOC_AST(Nil, nil, offset);
  return (struct Ast*) nil;
}

static struct Ast* ast_nil_clone(const struct AstNil* ast) {
  ALLOC_AST(Nil, nil_node, ast->ast.offset);
  return (struct Ast*) nil_node;
}

static int ast_nil_print(const struct AstNil* ast, struct Writer* writer) {
  UNUSED(ast);
  return writer->writef(writer, "nil");
}

static void ast_nil_free(struct AstNil* ast) {
  free(ast);
}

#undef ALLOC_AST

int ast_print(const struct Ast* ast, struct Writer* writer) {
#define REDIRECT_PRINT(TYPE, NAME) \
  case AST_##TYPE: return ast_##NAME##_print((struct Ast##TYPE *) ast, writer);

  if (!ast) {
    return 0;
  }

  switch (ast->type) {
  REDIRECT_PRINT(Program, program)
  REDIRECT_PRINT(Block, block)
  REDIRECT_PRINT(Struct, struct)
  REDIRECT_PRINT(Function, function)
  REDIRECT_PRINT(If, if)
  REDIRECT_PRINT(While, while)
  REDIRECT_PRINT(For, for)
  REDIRECT_PRINT(Let, let)
  REDIRECT_PRINT(Require, require)
  REDIRECT_PRINT(Yield, yield)
  REDIRECT_PRINT(Break, break)
  REDIRECT_PRINT(Continue, continue)
  REDIRECT_PRINT(Return, return)
  REDIRECT_PRINT(Member, member)
  REDIRECT_PRINT(Index, index)
  REDIRECT_PRINT(Assignment, assignment)
  REDIRECT_PRINT(Binary, binary)
  REDIRECT_PRINT(Unary, unary)
  REDIRECT_PRINT(Call, call)
  REDIRECT_PRINT(Self, self)
  REDIRECT_PRINT(Varargs, varargs)
  REDIRECT_PRINT(Array, array)
  REDIRECT_PRINT(Set, set)
  REDIRECT_PRINT(Dictionary, dictionary)
  REDIRECT_PRINT(String, string)
  REDIRECT_PRINT(Identifier, identifier)
  REDIRECT_PRINT(Float, float)
  REDIRECT_PRINT(Integer, integer)
  REDIRECT_PRINT(Boolean, boolean)
  REDIRECT_PRINT(Ellipsis, ellipsis)
  REDIRECT_PRINT(Nil, nil)
  default: UNREACHABLE();
  };

#undef REDIRECT_PRINT
}

struct Ast* ast_clone(const struct Ast* ast) {
#define REDIRECT_CLONE(TYPE, NAME) \
  case AST_##TYPE: return ast_##NAME##_clone((struct Ast##TYPE *) ast);

  if (!ast) {
    return NULL;
  }

  switch (ast->type) {
  REDIRECT_CLONE(Program, program)
  REDIRECT_CLONE(Block, block)
  REDIRECT_CLONE(Struct, struct)
  REDIRECT_CLONE(Function, function)
  REDIRECT_CLONE(If, if)
  REDIRECT_CLONE(While, while)
  REDIRECT_CLONE(For, for)
  REDIRECT_CLONE(Let, let)
  REDIRECT_CLONE(Require, require)
  REDIRECT_CLONE(Yield, yield)
  REDIRECT_CLONE(Break, break)
  REDIRECT_CLONE(Continue, continue)
  REDIRECT_CLONE(Return, return)
  REDIRECT_CLONE(Member, member)
  REDIRECT_CLONE(Index, index)
  REDIRECT_CLONE(Assignment, assignment)
  REDIRECT_CLONE(Binary, binary)
  REDIRECT_CLONE(Unary, unary)
  REDIRECT_CLONE(Call, call)
  REDIRECT_CLONE(Self, self)
  REDIRECT_CLONE(Varargs, varargs)
  REDIRECT_CLONE(Array, array)
  REDIRECT_CLONE(Set, set)
  REDIRECT_CLONE(Dictionary, dictionary)
  REDIRECT_CLONE(String, string)
  REDIRECT_CLONE(Identifier, identifier)
  REDIRECT_CLONE(Float, float)
  REDIRECT_CLONE(Integer, integer)
  REDIRECT_CLONE(Boolean, boolean)
  REDIRECT_CLONE(Ellipsis, ellipsis)
  REDIRECT_CLONE(Nil, nil)
  default: UNREACHABLE();
  };

#undef REDIRECT_CLONE
}

void ast_free(struct Ast* ast) {
#define REDIRECT_FREE(TYPE, NAME) \
  case AST_##TYPE: ast_##NAME##_free((struct Ast##TYPE *) ast); break;

  if (!ast) {
    return;
  }

  switch (ast->type) {
  REDIRECT_FREE(Program, program)
  REDIRECT_FREE(Block, block)
  REDIRECT_FREE(Struct, struct)
  REDIRECT_FREE(Function, function)
  REDIRECT_FREE(If, if)
  REDIRECT_FREE(While, while)
  REDIRECT_FREE(For, for)
  REDIRECT_FREE(Let, let)
  REDIRECT_FREE(Require, require)
  REDIRECT_FREE(Yield, yield)
  REDIRECT_FREE(Break, break)
  REDIRECT_FREE(Continue, continue)
  REDIRECT_FREE(Return, return)
  REDIRECT_FREE(Member, member)
  REDIRECT_FREE(Index, index)
  REDIRECT_FREE(Assignment, assignment)
  REDIRECT_FREE(Binary, binary)
  REDIRECT_FREE(Unary, unary)
  REDIRECT_FREE(Call, call)
  REDIRECT_FREE(Self, self)
  REDIRECT_FREE(Varargs, varargs)
  REDIRECT_FREE(Array, array)
  REDIRECT_FREE(Set, set)
  REDIRECT_FREE(Dictionary, dictionary)
  REDIRECT_FREE(String, string)
  REDIRECT_FREE(Identifier, identifier)
  REDIRECT_FREE(Float, float)
  REDIRECT_FREE(Integer, integer)
  REDIRECT_FREE(Boolean, boolean)
  REDIRECT_FREE(Ellipsis, ellipsis)
  REDIRECT_FREE(Nil, nil)
  default: UNREACHABLE();
  };

#undef REDIRECT_FREE
}

#undef REALLOC
