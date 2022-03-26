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
};

// Initialize the parser
static void parser_init(struct Parser* parser, const char* source, struct Writer* writer) {
  lexer_init(&parser->lexer, source);
  parser->writer = writer;
  parser->had_error = false;
  parser->panic_mode = false;
}

// Prints error messages to the parsers "writer", and indicates that we've
// encountered an error.
static void error_at(struct Parser* parser, const struct Token* token, const char *msg, va_list ap) {
  if (parser->panic_mode) {
    return;
  }
  parser->writer->writef(parser->writer, "\x1b[1;31mERROR\x1b[0m: ");
  parser->writer->vwritef(parser->writer, msg, ap);
  str_print(&token->text, parser->writer);
  struct SourceLine line;
  const char *source = parser->lexer.source;
  size_t source_len = token->type == TOK_Error ? 1 : token->text.length;
  if (get_source_line(source, token->offset, source_len, &line)) {
    parser->writer->writef(parser->writer, "\n[%lu]: %.*s\x1b[1;4m%.*s\x1b[0m%.*s\n",
                           line.line_number, line.range_start, line.start,
                           line.range_end - line.range_start, line.start + line.range_start,
                           line.length - line.range_end, line.start + line.range_end);
  }
  parser->had_error = true;
  parser->panic_mode = true;
}

static void error_at_previous(struct Parser* parser, const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  return error_at(parser, &parser->previous, msg, ap);
  va_end(ap);
}

static void error_at_current(struct Parser* parser, const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  return error_at(parser, &parser->current, msg, ap);
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
static void consume(struct Parser* parser, enum TokenType type) {
  if (parser->current.type == type) {
    advance(parser);
  } else {
    error_at_current(parser, "expected %s, found ", token_type_to_string(type));
  }
}

// Forward declaration
static struct Ast* expression(struct Parser* parser);

static void expressions(struct Parser* parser, struct AstVec* vec, enum TokenType terminator) {
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
  return ast_integer_create(parser->previous.offset, value);
}

static struct Ast* float_node(struct Parser* parser) {
  char *end_ptr;
  double value = strtod((const char*) parser->previous.text.data, &end_ptr);
  size_t length = end_ptr - (const char*) parser->previous.text.data;
  if (length != parser->previous.text.length) {
    error_at_previous(parser, "float is not in a format we can parse (yet)");
    return NULL;
  }
  return ast_float_create(parser->previous.offset, value);
}

static struct Ast* atom(struct Parser* parser) {
  struct Ast* ast;
  size_t offset = parser->current.offset;
  advance(parser);
  switch (parser->previous.type) {
  case TOK_Nil:        return ast_nil_create(offset);
  case TOK_True:       return ast_true_create(offset);
  case TOK_False:      return ast_false_create(offset);
  case TOK_Integer:    return integer(parser);
  case TOK_Float:      return float_node(parser);
  case TOK_Identifier: return ast_identifier_create(offset, parser->previous.text);
  case TOK_String:     return ast_string_create(offset, parser->previous.text);
  case TOK_Self:       return ast_self_create(offset);
  case TOK_Varargs:    return ast_varargs_create(offset);
  // TODO case TOK_If:         return if_statement_suffix(parser);
  // TODO case TOK_Require:    return require(parser);
  // TODO case TOK_Yield:      return yield(parser);
  case TOK_LeftParen:
    advance(parser);
    ast = expression(parser);
    consume(parser, TOK_RightParen);
    return ast;
  // TODO case TOK_Fn:         return lambda(parser);
  // TODO case TOK_LeftSqBr:   return array(parser);
  // TODO case TOK_LeftCurBr:  return dictionary_or_set(parser);
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:
    error_at_previous(parser, "expected an atom, found: ");
    return NULL;
  }
}

static struct Ast* primary(struct Parser* parser) {
  size_t offset;
  struct Ast *ret, *index;
  struct AstVec arguments;
  ret = atom(parser);
  while (true) {
    offset = parser->current.offset;
    switch (parser->current.type) {
    case TOK_Dot:
      advance(parser);
      consume(parser, TOK_Identifier);
      ret = ast_member_create(offset, ret, parser->previous.text);
      break;
    case TOK_LeftParen:
      advance(parser);
      ast_vec_init(&arguments);
      expressions(parser, &arguments, TOK_RightParen);
      ret = ast_call_create(offset, ret, arguments);
      break;
    case TOK_LeftSqBr:
      advance(parser);
      index = expression(parser);
      consume(parser, TOK_RightSqBr);
      ret = ast_index_create(offset, ret, index);
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
  size_t offset = parser->current.offset;
  switch (parser->current.type) {
  case TOK_Plus:
    advance(parser);
    return unary(parser, NULL);
  case TOK_Minus:
    return ast_unary_create(offset, UO_Minus, unary(parser, NULL));
  case TOK_BitNot:
    return ast_unary_create(offset, UO_BitNot, unary(parser, NULL));
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:      return primary(parser);
  }
}

static struct Ast* term(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = unary(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    switch (parser->current.type) {
    case TOK_Star:    operation = BO_Multiply; break;
    case TOK_Slash:   operation = BO_Divide; break;
    case TOK_Percent: operation = BO_Modulo; break;
    default:          return ret;
    }
    advance(parser);
    struct Ast* rhs = unary(parser, NULL);
    ret = ast_binary_create(offset, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* sum(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = term(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    switch (parser->current.type) {
    case TOK_Plus:  operation = BO_Add; break;
    case TOK_Minus: operation = BO_Subtract; break;
    default:        return ret;
    }
    advance(parser);
    struct Ast* rhs = term(parser, NULL);
    ret = ast_binary_create(offset, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* shift_expression(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = sum(parser, opt_primary);
  while (true) {
    enum BinaryOp operation;
    switch (parser->current.type) {
    case TOK_ShiftLeft:  operation = BO_ShiftLeft; break;
    case TOK_ShiftRight: operation = BO_ShiftRight; break;
    default:             return ret;
    }
    advance(parser);
    struct Ast* rhs = sum(parser, NULL);
    ret = ast_binary_create(offset, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* bitwise_and(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = shift_expression(parser, opt_primary);
  while (true) {
    if (parser->current.type != TOK_BitAnd) {
      return ret;
    }
    advance(parser);
    struct Ast* rhs = shift_expression(parser, NULL);
    ret = ast_binary_create(offset, BO_BitAnd, ret, rhs);
  }
  return ret;
}

static struct Ast* bitwise_xor(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = bitwise_and(parser, opt_primary);
  while (true) {
    if (parser->current.type != TOK_BitXor) {
      return ret;
    }
    advance(parser);
    struct Ast* rhs = bitwise_and(parser, NULL);
    ret = ast_binary_create(offset, BO_BitXor, ret, rhs);
  }
  return ret;
}

static struct Ast* bitwise_or(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = bitwise_xor(parser, opt_primary);
  while (true) {
    if (parser->current.type != TOK_BitOr) {
      return ret;
    }
    advance(parser);
    struct Ast* rhs = bitwise_xor(parser, NULL);
    ret = ast_binary_create(offset, BO_BitOr, ret, rhs);
  }
  return ret;
}

static struct Ast* comparison(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
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
    advance(parser);
    struct Ast* rhs = bitwise_or(parser, NULL);
    ret = ast_binary_create(offset, operation, ret, rhs);
  }
  return ret;
}

static struct Ast* inversion(struct Parser* parser, struct Ast* opt_primary) {
  if (opt_primary) {
    return comparison(parser, opt_primary);
  }
  size_t offset = parser->current.offset;
  switch (parser->current.type) {
  case TOK_Not:
    advance(parser);
    return ast_unary_create(offset, UO_LogicalNot, inversion(parser, NULL));
  case TOK_EOF:
    parser->incomplete_input = true;
    return NULL;
  default:
    return comparison(parser, NULL);
  }
}

static struct Ast* conjunction(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = inversion(parser, opt_primary);
  while (true) {
    if (parser->current.type != TOK_And) {
      break;
    }
    advance(parser);
    struct Ast* rhs = inversion(parser, NULL);
    ret = ast_binary_create(offset, BO_LogicalAnd, ret, rhs);
  }
  return ret;
}

static struct Ast* disjunction(struct Parser* parser, struct Ast* opt_primary) {
  size_t offset = opt_primary ? opt_primary->offset : parser->current.offset;
  struct Ast* ret = conjunction(parser, opt_primary);
  while (true) {
    if (parser->current.type != TOK_Or) {
      break;
    }
    advance(parser);
    struct Ast* rhs = conjunction(parser, NULL);
    ret = ast_binary_create(offset, BO_LogicalOr, ret, rhs);
  }
  return ret;
}

static struct Ast* expression(struct Parser* parser) {
  return disjunction(parser, NULL);
}

static struct Ast* assignment_or_expression(struct Parser* parser) {
  // TODO
  return expression(parser);
}

static struct Ast* let_declaration(struct Parser* parser, bool public) {
  DIE("%s", "Unimplemented");
}

static struct Ast* declaration(struct Parser* parser, bool public) {
  DIE("%s", "Unimplemented");
}

static struct Ast* for_statement(struct Parser* parser) {
  DIE("%s", "Unimplemented");
}

static struct Ast* while_statement(struct Parser* parser) {
  DIE("%s", "Unimplemented");
}

static struct Ast* if_statement(struct Parser* parser) {
  DIE("%s", "Unimplemented");
}

static struct Ast* statement_list(struct Parser* parser, bool inside_block) {
  struct AstVec statements;
  bool is_semicolon_statement = false;
  size_t start_offset = parser->current.offset, offset = parser->current.offset;
  if (inside_block) {
    consume(parser, TOK_LeftCurBr);
  }
  ast_vec_init(&statements);
  while (true) {
    if (inside_block && parser->current.type == TOK_RightCurBr) {
      break;
    }
    is_semicolon_statement = true;
    offset = parser->current.offset;
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
      is_semicolon_statement = false;
      break;
    case TOK_Break:
      advance(parser);
      ast_vec_push(&statements, ast_break_create(offset));
      is_semicolon_statement = true;
      break;
    case TOK_Continue:
      advance(parser);
      ast_vec_push(&statements, ast_continue_create(offset));
      is_semicolon_statement = true;
      break;
    case TOK_Return:
      advance(parser);
      if (parser->current.type == TOK_SemiColon) {
        ast_vec_push(&statements, ast_return_create(offset, expression(parser)));
      } else {
        ast_vec_push(&statements, ast_return_create(offset, NULL));
      }
      is_semicolon_statement = true;
      break;
    default:
      ast_vec_push(&statements, assignment_or_expression(parser));
      is_semicolon_statement = true;
    }
    if (is_semicolon_statement) {
      if (parser->current.type == TOK_SemiColon) {
        consume(parser, TOK_SemiColon);
      } else {
        is_semicolon_statement = false;
        break;
      }
    }
  }
  if (inside_block) {
    consume(parser, TOK_RightCurBr);
    return ast_block_create(start_offset, statements, is_semicolon_statement);
  } else {
    return ast_program_create(start_offset, statements);
  }
}

struct Ast* parse(const char* source, struct Writer *err_writer, bool* incomplete_input) {
  struct Parser parser;
  parser_init(&parser, source, err_writer);
  advance(&parser); // Jump-start parsing
  struct Ast* ast = statement_list(&parser, false);
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
