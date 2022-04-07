#include "parser.h"

#include <stdarg.h>

#include "ast.h"
#include "lexer.h"
#include "log.h"
#include "string.h"
#include "util.h"
#include "writer.h"

// Parser state. This maintains 2 tokens of look-ahead
struct Parser {
  struct Lexer lexer;    // Handle to the lexer
  struct Token previous; // Token that just passed us by
  struct Token current;  // Current token
  struct Writer* writer; // Writer for error messages
  bool had_error;        // Whether we had an error while parsing
  bool panic_mode;       // Whether we're in panic mode and need to recover
  bool incomplete_input; // Whether the error was because we had incomplete input
  bool inside_block;     // Whether we're inside a block statement
};

// Prints error messages to the parsers "writer", and indicates that we've
// encountered an error.
static void error_at(struct Parser* parser, const struct Token* token, const char *msg, va_list ap) {
  if (parser->panic_mode) {
    return;
  }
  parser->writer->writef(parser->writer, "\x1b[1;31mERROR\x1b[0m: [%lu]: ", token->line_num);
  parser->writer->vwritef(parser->writer, msg, ap);
  str_print(&token->text, parser->writer);
  parser->writer->writef(parser->writer, "'\n");
  parser->had_error = true;
  parser->panic_mode = true;
}

static void error_at_previous(struct Parser* parser, const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  error_at(parser, &parser->previous, msg, ap);
  va_end(ap);
}

static void error_at_current(struct Parser* parser, const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  error_at(parser, &parser->current, msg, ap);
  va_end(ap);
}

// Advances the parser - moves `next` to `current`, and fills the next token
// from the lexer into `next`.
static void advance(struct Parser* parser) {
  parser->previous = parser->current;
  while (true) {
    lexer_tok(&parser->lexer, &parser->current);
    if (parser->current.type != TOK_Error) {
      break;
    }
    error_at_current(parser, "lexer error: ");
  }
}

// Checks if the next token's type is `type`. If yes, advances the parser.
// Otherwise just returns false.
static bool match(struct Parser* parser, enum TokenType type) {
  if (parser->current.type == type) {
    advance(parser);
    return true;
  }
  return false;
}

// Checks if the next token's type is `type`. If yes, advances the parser.
// Otherwise trigers an error.
static void consume(struct Parser* parser, enum TokenType type) {
  if (parser->current.type == TOK_EOF) {
    parser->incomplete_input = true;
    return;
  }
  if (!match(parser, type)) {
    error_at_current(parser, "expected '%s', found '", token_type_to_string(type));
  }
}

// Forward declarations
static struct Ast* expression(struct Parser* parser);
static struct Ast* statement_list(struct Parser* parser);
static struct Ast* block_statement(struct Parser* parser);

static void expressions(struct Parser* parser, struct AstVec* vec, enum TokenType terminator) {
  if (match(parser, terminator)) {
    return;
  }
  while (true) {
    switch (parser->current.type) {
    case TOK_EOF:
      parser->incomplete_input = true;
      return;
    default:
      ast_vec_push(vec, expression(parser));
      if (match(parser, terminator)) {
        return;
      }
      consume(parser, TOK_Comma);
    }
  }
}

static struct Ast* integer(struct Parser* parser) {
  int64_t value = 0;
  const struct Str* text = &parser->previous.text;
  for (size_t i = 0; i < text->length; i++) {
    int64_t digit = text->data[i] - '0';
    int64_t new_value = (value * 10) + digit;
    if (new_value < value) {
      error_at_previous(parser, "integer is too large, we only support 64-bit signed integers");
      return NULL;
    }
    value = new_value;
  }
  return ast_integer_create(parser->previous.line_num, value);
}

static struct Ast* float_node(struct Parser* parser) {
  char *end_ptr;
  double value = strtod((const char*) parser->previous.text.data, &end_ptr);
  size_t length = end_ptr - (const char*) parser->previous.text.data;
  if (length != parser->previous.text.length) {
    error_at_previous(parser, "float is not in a format we can parse (yet)");
    return NULL;
  }
  return ast_float_create(parser->previous.line_num, value);
}

static void parameters(struct Parser* parser, struct AstVec* vec, enum TokenType terminator,
                       bool can_have_self) {
  if (match(parser, terminator)) {
    return;
  }
  if (can_have_self) {
    if (match(parser, TOK_Self)) {
      ast_vec_push(vec, ast_self_create(parser->previous.line_num));
      if (match(parser, terminator)) {
        return;
      } else {
        consume(parser, TOK_Comma);
      }
    }
  }
  while (true) {
    switch (parser->current.type) {
    case TOK_EOF:
      parser->incomplete_input = true;
      return;
    case TOK_Identifier:
      ast_vec_push(vec, ast_identifier_create(parser->current.line_num, parser->current.text));
      advance(parser);
      if (match(parser, terminator)) {
        return;
      } else {
        consume(parser, TOK_Comma);
      }
      break;
    case TOK_Ellipsis:
      ast_vec_push(vec, ast_ellipsis_create(parser->current.line_num));
      advance(parser);
      if (!match(parser, terminator)) {
        error_at_current(parser, "expected '%s' after '...', found: '",
                         token_type_to_string(terminator));
      } else {
        return;
      }
    default:
      error_at_current(parser, "expected identifier or '...', found: '");
      return;
    }
  }
}

static struct Ast* lambda(struct Parser* parser) {
  struct AstVec params;
  ast_vec_init(&params);
  size_t line_num = parser->current.line_num;
  consume(parser, TOK_LeftParen);
  parameters(parser, &params, TOK_RightParen, false);
  struct Ast* body = block_statement(parser);
  return ast_function_create(line_num, params, body);
}

static struct Ast* array(struct Parser* parser) {
  struct AstVec elements;
  size_t line_num = parser->previous.line_num;
  ast_vec_init(&elements);
  expressions(parser, &elements, TOK_RightSqBr);
  return ast_array_create(line_num, elements);
}

static struct Ast* dictionary_or_set(struct Parser* parser) {
  struct AstPairVec kvpairs;
  struct AstVec elements;
  ast_pair_vec_init(&kvpairs);
  ast_vec_init(&elements);
  size_t line_num = parser->previous.line_num;
  if (match(parser, TOK_RightCurBr)) {
    return ast_dictionary_create(line_num, kvpairs);
  }
  struct Ast* key = expression(parser);
  if (match(parser, TOK_Colon)) {
    struct Ast* value = expression(parser);
    ast_pair_vec_push(&kvpairs, key, value);
    while (!match(parser, TOK_RightCurBr)) {
      if (parser->current.type == TOK_EOF) {
        parser->incomplete_input = true;
        break;
      }
      consume(parser, TOK_Comma);
      key = expression(parser);
      consume(parser, TOK_Colon);
      value = expression(parser);
      ast_pair_vec_push(&kvpairs, key, value);
    }
    return ast_dictionary_create(line_num, kvpairs);
  } else {
    ast_vec_push(&elements, key);
    while (!match(parser, TOK_RightCurBr)) {
      if (parser->current.type == TOK_EOF) {
        parser->incomplete_input = true;
        break;
      }
      consume(parser, TOK_Comma);
      ast_vec_push(&elements, expression(parser));
    }
    return ast_set_create(line_num, elements);
  }
}

static struct Ast* require(struct Parser* parser) {
  size_t line_num = parser->previous.line_num;
  consume(parser, TOK_LeftParen);
  consume(parser, TOK_String);
  struct Str module = parser->previous.text;
  consume(parser, TOK_RightParen);
  return ast_require_create(line_num, module);
}

static struct Ast* yield(struct Parser* parser) {
  size_t line_num = parser->previous.line_num;
  consume(parser, TOK_LeftParen);
  struct Ast* value = expression(parser);
  consume(parser, TOK_RightParen);
  return ast_yield_create(line_num, value);
}

static struct Ast* if_statement_suffix(struct Parser* parser) {
  size_t line_num = parser->previous.line_num;
  struct Ast* condition = expression(parser);
  struct Ast* body = block_statement(parser);
  struct Ast* else_part = NULL;
  if (match(parser, TOK_Else)) {
    else_part = block_statement(parser);
  }
  return ast_if_create(line_num, condition, body, else_part);
}

static struct Ast* if_statement(struct Parser* parser) {
  consume(parser, TOK_If);
  return if_statement_suffix(parser);
}

static struct Ast* atom(struct Parser* parser) {
  struct Ast* ast;
  size_t line_num = parser->current.line_num;
  advance(parser);
  switch (parser->previous.type) {
  case TOK_Nil:        return ast_nil_create(line_num);
  case TOK_True:       return ast_boolean_create(line_num, true);
  case TOK_False:      return ast_boolean_create(line_num, false);
  case TOK_Integer:    return integer(parser);
  case TOK_Float:      return float_node(parser);
  case TOK_Identifier: return ast_identifier_create(line_num, parser->previous.text);
  case TOK_String:     return ast_string_create(line_num, parser->previous.text);
  case TOK_Self:       return ast_self_create(line_num);
  case TOK_Varargs:    return ast_varargs_create(line_num);
  case TOK_If:         return if_statement_suffix(parser);
  case TOK_Require:    return require(parser);
  case TOK_Yield:      return yield(parser);
  case TOK_LeftParen:
    ast = expression(parser);
    consume(parser, TOK_RightParen);
    return ast;
  case TOK_Fn:         return lambda(parser);
  case TOK_LeftSqBr:   return array(parser);
  case TOK_LeftCurBr:  return dictionary_or_set(parser);
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:
    error_at_previous(parser, "expected an atom, found: '");
    return NULL;
  }
}

static struct Ast* primary(struct Parser* parser) {
  size_t line_num;
  struct Ast *ret, *index;
  struct AstVec arguments;
  ret = atom(parser);
  ast_vec_init(&arguments);
  while (true) {
    line_num = parser->current.line_num;
    switch (parser->current.type) {
    case TOK_Dot:
      advance(parser);
      consume(parser, TOK_Identifier);
      ret = ast_member_create(line_num, ret, parser->previous.text);
      break;
    case TOK_LeftParen:
      advance(parser);
      expressions(parser, &arguments, TOK_RightParen);
      ret = ast_call_create(line_num, ret, arguments);
      break;
    case TOK_LeftSqBr:
      advance(parser);
      index = expression(parser);
      consume(parser, TOK_RightSqBr);
      ret = ast_index_create(line_num, ret, index);
      break;
    default:
      return ret;
    }
  }
}

static struct Ast* unary(struct Parser* parser, struct Ast* opt_primary) {
  if (opt_primary) {
    return opt_primary;
  }
  size_t line_num = parser->current.line_num;
  switch (parser->current.type) {
  case TOK_Plus:
    advance(parser);
    return unary(parser, NULL);
  case TOK_Minus:
    advance(parser);
    return ast_unary_create(line_num, UO_Minus, unary(parser, NULL));
  case TOK_BitNot:
    advance(parser);
    return ast_unary_create(line_num, UO_BitNot, unary(parser, NULL));
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:      return primary(parser);
  }
}

static struct Ast* term(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = unary(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    size_t line_num = parser->current.line_num;
    switch (parser->current.type) {
    case TOK_Star:    operation = BO_Multiply; break;
    case TOK_Slash:   operation = BO_Divide; break;
    case TOK_Percent: operation = BO_Modulo; break;
    default:          return ret;
    }
    advance(parser);
    struct Ast* rhs = unary(parser, NULL);
    ret = ast_binary_create(line_num, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* sum(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = term(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    size_t line_num = parser->current.line_num;
    switch (parser->current.type) {
    case TOK_Plus:  operation = BO_Add; break;
    case TOK_Minus: operation = BO_Subtract; break;
    default:        return ret;
    }
    advance(parser);
    struct Ast* rhs = term(parser, NULL);
    ret = ast_binary_create(line_num, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* shift_expression(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = sum(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    size_t line_num = parser->current.line_num;
    switch (parser->current.type) {
    case TOK_ShiftLeft:  operation = BO_ShiftLeft; break;
    case TOK_ShiftRight: operation = BO_ShiftRight; break;
    default:             return ret;
    }
    advance(parser);
    struct Ast* rhs = sum(parser, NULL);
    ret = ast_binary_create(line_num, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* bitwise_and(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = shift_expression(parser, opt_primary);
  while (match(parser, TOK_BitAnd)) {
    size_t line_num = parser->previous.line_num;
    struct Ast* rhs = shift_expression(parser, NULL);
    ret = ast_binary_create(line_num, BO_BitAnd, ret, rhs);
  }
  return ret;
}

static struct Ast* bitwise_xor(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = bitwise_and(parser, opt_primary);
  while (match(parser, TOK_BitXor)) {
    size_t line_num = parser->previous.line_num;
    struct Ast* rhs = bitwise_and(parser, NULL);
    ret = ast_binary_create(line_num, BO_BitXor, ret, rhs);
  }
  return ret;
}

static struct Ast* bitwise_or(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = bitwise_xor(parser, opt_primary);
  while (match(parser, TOK_BitOr)) {
    size_t line_num = parser->previous.line_num;
    struct Ast* rhs = bitwise_xor(parser, NULL);
    ret = ast_binary_create(line_num, BO_BitOr, ret, rhs);
  }
  return ret;
}

static struct Ast* comparison(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = bitwise_or(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    switch (parser->current.type) {
    case TOK_Equal:        operation = BO_Equal; break;
    case TOK_NotEqual:     operation = BO_NotEqual; break;
    case TOK_LessEqual:    operation = BO_LessEqual; break;
    case TOK_LessThan:     operation = BO_LessThan; break;
    case TOK_GreaterEqual: operation = BO_GreaterEqual; break;
    case TOK_GreaterThan:  operation = BO_GreaterThan; break;
    default:               return ret;
    }
    size_t line_num = parser->current.line_num;
    advance(parser);
    struct Ast* rhs = bitwise_or(parser, NULL);
    ret = ast_binary_create(line_num, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* inversion(struct Parser* parser, struct Ast* opt_primary) {
  if (opt_primary) {
    return comparison(parser, opt_primary);
  }
  size_t line_num = parser->current.line_num;
  switch (parser->current.type) {
  case TOK_Not:
    advance(parser);
    return ast_unary_create(line_num, UO_LogicalNot, inversion(parser, NULL));
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:
    return comparison(parser, NULL);
  }
}

static struct Ast* conjunction(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = inversion(parser, opt_primary);
  while (match(parser, TOK_And)) {
    size_t line_num = parser->previous.line_num;
    struct Ast* rhs = inversion(parser, NULL);
    ret = ast_binary_create(line_num, BO_LogicalAnd, ret, rhs);
  }
  return ret;
}

static struct Ast* disjunction(struct Parser* parser, struct Ast* opt_primary) {
  struct Ast* ret = conjunction(parser, opt_primary);
  while (match(parser, TOK_Or)) {
    size_t line_num = parser->previous.line_num;
    struct Ast* rhs = conjunction(parser, NULL);
    ret = ast_binary_create(line_num, BO_LogicalOr, ret, rhs);
  }
  return ret;
}

static struct Ast* expression_prime(struct Parser* parser, struct Ast* primary) {
  return disjunction(parser, primary);
}

static struct Ast* expression(struct Parser* parser) {
  return disjunction(parser, NULL);
}

static bool is_assignment_op(enum TokenType type) {
  switch (type) {
  case TOK_Assign:
  case TOK_AddAssign:
  case TOK_SubAssign:
  case TOK_MulAssign:
  case TOK_DivAssign:
  case TOK_ModAssign:
  case TOK_ShiftLeftAssign:
  case TOK_ShiftRightAssign:
  case TOK_BitOrAssign:
  case TOK_BitXorAssign:
  case TOK_BitAndAssign:
    return true;
  default:
    return false;
  }
}

static struct Ast* assignment_or_expression(struct Parser* parser) {
  switch (parser->current.type) {
  case TOK_Identifier:
  case TOK_Self: {
    struct Ast* lhs = primary(parser);
    if (!is_assignment_op(parser->current.type)) {
      return expression_prime(parser, lhs);
    }
    size_t line_num = parser->current.line_num;
    enum TokenType token_type = parser->current.type;
    advance(parser);
    struct Ast* rhs = expression(parser);
    switch (token_type) {
    case TOK_AddAssign:
      rhs = ast_binary_create(line_num, BO_Add, ast_clone(lhs), rhs);
      break;
    case TOK_SubAssign:
      rhs = ast_binary_create(line_num, BO_Subtract, ast_clone(lhs), rhs);
      break;
    case TOK_MulAssign:
      rhs = ast_binary_create(line_num, BO_Multiply, ast_clone(lhs), rhs);
      break;
    case TOK_DivAssign:
      rhs = ast_binary_create(line_num, BO_Divide, ast_clone(lhs), rhs);
      break;
    case TOK_ModAssign:
      rhs = ast_binary_create(line_num, BO_Modulo, ast_clone(lhs), rhs);
      break;
    case TOK_ShiftLeftAssign:
      rhs = ast_binary_create(line_num, BO_ShiftLeft, ast_clone(lhs), rhs);
      break;
    case TOK_ShiftRightAssign:
      rhs = ast_binary_create(line_num, BO_ShiftRight, ast_clone(lhs), rhs);
      break;
    case TOK_BitOrAssign:
      rhs = ast_binary_create(line_num, BO_BitOr, ast_clone(lhs), rhs);
      break;
    case TOK_BitXorAssign:
      rhs = ast_binary_create(line_num, BO_BitXor, ast_clone(lhs), rhs);
      break;
    case TOK_BitAndAssign:
      rhs = ast_binary_create(line_num, BO_BitAnd, ast_clone(lhs), rhs);
      break;
    case TOK_Assign:
      break;
    default:
      UNREACHABLE();
    }
    return ast_assignment_create(line_num, lhs, rhs);
  }
  default:
    return expression(parser);
  }
}

static struct Ast* let_declaration(struct Parser* parser, bool public) {
  if (public && parser->inside_block) {
    error_at_previous(parser, "public let declaration inside a block: ");
  }
  size_t line_num = parser->current.line_num;
  consume(parser, TOK_Let);
  consume(parser, TOK_Identifier);
  struct Str variable = parser->previous.text;
  struct Ast* rhs = NULL;
  if (match(parser, TOK_Assign)) {
    rhs = expression(parser);
  } else {
    rhs = ast_nil_create(line_num);
  };
  return ast_let_create(line_num, public, variable, rhs);
}

static struct Ast* function_declaration(struct Parser* parser, bool public, bool can_have_self) {
  if (public && parser->inside_block) {
    error_at_previous(parser, "public function declaration inside a block: ");
  }
  struct AstVec params;
  ast_vec_init(&params);
  size_t line_num = parser->current.line_num;
  consume(parser, TOK_Fn);
  consume(parser, TOK_Identifier);
  struct Str name = parser->previous.text;
  consume(parser, TOK_LeftParen);
  parameters(parser, &params, TOK_RightParen, can_have_self);
  struct Ast* body = block_statement(parser);
  struct Ast* ast_function = ast_function_create(line_num, params, body);
  return ast_let_create(line_num, public, name, ast_function);
}

static struct Ast* struct_declaration(struct Parser* parser, bool public) {
  if (parser->inside_block) {
    error_at_current(parser, "struct declaration inside a block: ");
  }
  struct AstVec members;
  ast_vec_init(&members);
  size_t line_num = parser->current.line_num;
  consume(parser, TOK_Struct);
  consume(parser, TOK_Identifier);
  struct Str name = parser->previous.text;
  struct Str parent;
  bool has_parent = false;
  if (match(parser, TOK_Colon)) {
    consume(parser, TOK_Identifier);
    parent = parser->previous.text;
    has_parent = true;
  }
  consume(parser, TOK_LeftCurBr);
  size_t body_line_num = parser->current.line_num;
  while (!match(parser, TOK_RightCurBr)) {
    if (parser->current.type == TOK_EOF) {
      parser->incomplete_input = true;
      break;
    }
    bool public = match(parser, TOK_Pub);
    ast_vec_push(&members, function_declaration(parser, public, true));
  }
  struct Ast* ast_struct = ast_struct_create(line_num,
                                             has_parent ? &parent : NULL,
                                             ast_block_create(body_line_num, members, true));
  return ast_let_create(line_num, public, name, ast_struct);
}

static struct Ast* declaration(struct Parser* parser, bool public) {
  switch (parser->current.type) {
  case TOK_Fn:     return function_declaration(parser, public, false);
  case TOK_Struct: return struct_declaration(parser, public);
  case TOK_Let:    return let_declaration(parser, public);
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:
    error_at_current(parser, "expected fn, struct, or let, found '",
                     token_type_to_string(parser->current.type));
    return NULL;
  }
}

static struct Ast* block_statement(struct Parser* parser) {
  bool was_inside_block = parser->inside_block;
  parser->inside_block = true;
  struct Ast* ret = statement_list(parser);
  parser->inside_block = was_inside_block;
  return ret;
}

// "for" loops are just syntactic sugar for a while loop calling an iterator
// returned by a generator. For example -
// Original:
//   for i in range(0, 10) {
//     print(i);
//   }
// De-sugared:
//   {
//     let __iter = generator();
//     let i = next(__iter);
//     while (i != nil) {
//       print(i);
//       i = next(__iter);
//     }
//   }
static struct Ast* for_statement(struct Parser* parser) {
  size_t start_line_num = parser->previous.line_num;
  consume(parser, TOK_For);
  consume(parser, TOK_Identifier);
  size_t identifier_line_num = parser->current.line_num;
  struct Str identifier = parser->previous.text;
  consume(parser, TOK_In);
  size_t generator_line_num = parser->current.line_num;
  struct Ast* generator = expression(parser);
  struct AstBlock* body = (struct AstBlock*) block_statement(parser);
  CHECK(body->ast.type == AST_Block);

  struct AstVec statements, next_args;
  ast_vec_init(&statements);
  ast_vec_init(&next_args);

  struct Str iterator = (struct Str) { (const uint8_t*)"__iter", 6 };
  struct Str next = (struct Str) { (const uint8_t*)"next", 4 };
  struct Ast* ast_iterator = ast_identifier_create(generator_line_num, iterator);
  struct Ast* ast_identifier = ast_identifier_create(identifier_line_num, identifier);
  struct Ast* ast_next = ast_identifier_create(generator_line_num, next);

  ast_vec_push(&next_args, ast_clone(ast_iterator));
  struct Ast* call_next = ast_call_create(generator_line_num, ast_clone(ast_next), next_args);

  // let __iter = generator()
  ast_vec_push(&statements, ast_let_create(generator_line_num, false, iterator, generator));
  // let identifier = next(__iter)
  ast_vec_push(&statements, ast_let_create(identifier_line_num, false, identifier, ast_clone(call_next)));
  // identifer != nil
  struct Ast* condition = ast_binary_create(generator_line_num, BO_NotEqual, ast_clone(ast_identifier),
                                            ast_nil_create(identifier_line_num));
  // identifer = next(__iter)
  struct Ast* update = ast_assignment_create(generator_line_num, ast_identifier, call_next);
  // Append "update" to the loop body
  ast_vec_push(&body->statements, update);
  // Create while loop
  ast_vec_push(&statements, ast_while_create(start_line_num, condition, (struct Ast*) body));
  // Return enclosing block
  return ast_block_create(start_line_num, statements, true);
}

static struct Ast* while_statement(struct Parser* parser) {
  size_t line_num = parser->previous.line_num;
  consume(parser, TOK_While);
  struct Ast* condition = expression(parser);
  struct Ast* body = block_statement(parser);
  return ast_while_create(line_num, condition, body);
}

static void synchronize(struct Parser* parser) {
  parser->panic_mode = false;
  while (parser->current.type != TOK_EOF) {
    // Check if the previous token was a good synchronization point
    switch (parser->previous.type) {
    case TOK_SemiColon:
    case TOK_RightCurBr:
      return;
    default:
      break;
    }
    // Check the current token for a good synchronization point
    switch (parser->current.type) {
    case TOK_Fn:
    case TOK_Pub:
    case TOK_Let:
    case TOK_For:
    case TOK_If:
    case TOK_While:
    case TOK_Struct:
    case TOK_Break:
    case TOK_Continue:
    case TOK_Return:
      return;
    default:
      break;
    }
    advance(parser);
  }
}

static struct Ast* statement_list(struct Parser* parser) {
  struct AstVec statements;
  bool is_semicolon_statement = false;
  size_t start_line_num = parser->current.line_num, line_num = parser->current.line_num;
  if (parser->inside_block) {
    consume(parser, TOK_LeftCurBr);
  }
  ast_vec_init(&statements);
  while (parser->current.type != TOK_EOF) {
    if (parser->inside_block && parser->current.type == TOK_RightCurBr) {
      break;
    }
    is_semicolon_statement = false;
    line_num = parser->current.line_num;
    switch (parser->current.type) {
    case TOK_EOF:
      break;
    case TOK_Pub:
      advance(parser);
      is_semicolon_statement = parser->current.type == TOK_Let;
      ast_vec_push(&statements, declaration(parser, true));
      break;
    case TOK_Fn:
    case TOK_Struct: ast_vec_push(&statements, declaration(parser, false)); break;
    case TOK_If:     ast_vec_push(&statements, if_statement(parser)); break;
    case TOK_While:  ast_vec_push(&statements, while_statement(parser)); break;
    case TOK_For:    ast_vec_push(&statements, for_statement(parser)); break;
    case TOK_Let:
      ast_vec_push(&statements, let_declaration(parser, false));
      is_semicolon_statement = true;
      break;
    case TOK_Break:
      if (!parser->inside_block) {
        error_at_current(parser, "'break' outside of a block: ");
      }
      advance(parser);
      ast_vec_push(&statements, ast_break_create(line_num));
      is_semicolon_statement = true;
      break;
    case TOK_Continue:
      if (!parser->inside_block) {
        error_at_current(parser, "'continue' outside of a block: ");
      }
      advance(parser);
      ast_vec_push(&statements, ast_continue_create(line_num));
      is_semicolon_statement = true;
      break;
    case TOK_Return:
      if (!parser->inside_block) {
        error_at_current(parser, "'return' outside of a block: ");
      }
      advance(parser);
      switch (parser->current.type) {
      case TOK_SemiColon:
      case TOK_RightCurBr:
        ast_vec_push(&statements, ast_return_create(line_num, NULL));
        break;
      default:
        ast_vec_push(&statements, ast_return_create(line_num, expression(parser)));
        break;
      }
      is_semicolon_statement = true;
      break;
    default:
      ast_vec_push(&statements, assignment_or_expression(parser));
      is_semicolon_statement = true;
    }
    // Error synchronization
    if (parser->panic_mode) {
      synchronize(parser);
    } else if (is_semicolon_statement) {
      // Otherwise if everything went okay, try to match a semicolon
      if (!match(parser, TOK_SemiColon)) {
        is_semicolon_statement = false;
        break;
      }
    }
  }
  if (parser->inside_block) {
    consume(parser, TOK_RightCurBr);
    return ast_block_create(start_line_num, statements, is_semicolon_statement);
  } else {
    return ast_program_create(start_line_num, statements);
  }
}

// Initialize the parser
static void parser_init(struct Parser* parser, const char* source, struct Writer* writer) {
  lexer_init(&parser->lexer, source);
  token_init_undefined(&parser->previous);
  token_init_undefined(&parser->current);
  parser->writer = writer;
  parser->had_error = false;
  parser->panic_mode = false;
  parser->inside_block = false;
  advance(parser); // Jump-start parsing
}

struct Ast* parse(const char* source, struct Writer *err_writer, bool* incomplete_input) {
  struct Parser parser;
  parser_init(&parser, source, err_writer);
  struct Ast* ast = statement_list(&parser);
  *incomplete_input = !parser.had_error && parser.incomplete_input;
  // If an error was encountered, free the AST and return NULL
  if (parser.had_error || parser.incomplete_input) {
    ast_free(ast);
    ast = NULL;
  }
  // If there are more tokens from the lexer, that's also an error
  if (ast && parser.current.type != TOK_EOF) {
    ast_free(ast);
    ast = NULL;
    // Check if what the lexer returned is itself an error
    error_at_current(&parser, "we should have covered all tokens, found: ");
  }
  return ast;
}
