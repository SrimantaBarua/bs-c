#include "lexer.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "string.h"

const char* token_type_to_string(enum TokenType token_type) {
  switch (token_type) {
  case TOK_Integer:          return "integer";
  case TOK_Float:            return "float";
  case TOK_Identifier:       return "identifier";
  case TOK_String:           return "string";
  case TOK_True:             return "true";
  case TOK_False:            return "false";
  case TOK_Nil:              return "nil";
  case TOK_Fn:               return "fn";
  case TOK_And:              return "and";
  case TOK_Or:               return "or";
  case TOK_Not:              return "not";
  case TOK_Pub:              return "pub";
  case TOK_Let:              return "let";
  case TOK_For:              return "for";
  case TOK_In:               return "in";
  case TOK_If:               return "if";
  case TOK_Else:             return "else";
  case TOK_While:            return "while";
  case TOK_Struct:           return "struct";
  case TOK_Break:            return "break";
  case TOK_Continue:         return "continue";
  case TOK_Self:             return "self";
  case TOK_Require:          return "require";
  case TOK_Return:           return "return";
  case TOK_Yield:            return "yield";
  case TOK_Varargs:          return "varargs";
  case TOK_SemiColon:        return ";";
  case TOK_LeftCurBr:        return "{";
  case TOK_RightCurBr:       return "}";
  case TOK_LeftSqBr:         return "[";
  case TOK_RightSqBr:        return "]";
  case TOK_LeftParen:        return "(";
  case TOK_RightParen:       return ")";
  case TOK_Colon:            return ":";
  case TOK_Assign:           return "=";
  case TOK_Dot:              return ".";
  case TOK_Ellipsis:         return "...";
  case TOK_Comma:            return ",";
  case TOK_Equal:            return "==";
  case TOK_NotEqual:         return "!=";
  case TOK_LessEqual:        return "<=";
  case TOK_LessThan:         return "<";
  case TOK_GreaterEqual:     return ">=";
  case TOK_GreaterThan:      return ">";
  case TOK_BitOr:            return "|";
  case TOK_BitOrAssign:      return "|=";
  case TOK_BitXor:           return "^";
  case TOK_BitXorAssign:     return "^=";
  case TOK_BitAnd:           return "&";
  case TOK_BitAndAssign:     return "&=";
  case TOK_BitNot:           return "!";
  case TOK_ShiftLeft:        return "<<";
  case TOK_ShiftLeftAssign:  return "<<=";
  case TOK_ShiftRight:       return ">>";
  case TOK_ShiftRightAssign: return ">>=";
  case TOK_Plus:             return "+";
  case TOK_AddAssign:        return "+=";
  case TOK_Minus:            return "-";
  case TOK_SubAssign:        return "-=";
  case TOK_Star:             return "*";
  case TOK_MulAssign:        return "*=";
  case TOK_Slash:            return "/";
  case TOK_DivAssign:        return "/=";
  case TOK_Percent:          return "%";
  case TOK_ModAssign:        return "%=";
  case TOK_EOF:              return "EOF";
  }
}

void lexer_init(struct Lexer* lexer, const char *source) {
  lexer->source = source;
  lexer->start_offset = lexer->current_offset = 0;
}

static bool is_at_end(const struct Lexer* lexer) {
  return lexer->source[lexer->current_offset] == '\0';
}

static char peek(const struct Lexer* lexer) {
  return lexer->source[lexer->current_offset];
}

static char peek2(const struct Lexer* lexer) {
  if (is_at_end(lexer)) {
    return '\0';
  }
  return lexer->source[lexer->current_offset + 1];
}

static char advance(struct Lexer* lexer) {
  CHECK(!is_at_end(lexer));
  return lexer->source[lexer->current_offset++];
}

static bool match(struct Lexer* lexer, char c) {
  if (peek(lexer) == c) {
    advance(lexer);
    return true;
  }
  return false;
}

static void skip_whitespace_and_comments(struct Lexer* lexer) {
  while (!is_at_end(lexer)) {
    char c = peek(lexer);
    if (isspace(c)) {
      advance(lexer);
    } else if (c == '/' && peek2(lexer) == '/') {
      advance(lexer);
      advance(lexer);
      while (!is_at_end(lexer) && peek(lexer) != '\n') {
        advance(lexer);
      }
    } else {
      break;
    }
  }
}

static inline const char* tok_start(const struct Lexer* lexer) {
  return lexer->source + lexer->start_offset;
}

static inline size_t tok_len(const struct Lexer* lexer) {
  return lexer->current_offset - lexer->start_offset;
}

static bool make_invalid_utf8_error(const struct Lexer* lexer, struct Token* token) {
  token->type = TOK_Error;
  token->offset = lexer->start_offset;
  str_init(&token->text, "invalid UTF-8", SIZE_MAX);
  return true;
}

static bool make_unexpected_char_error(const struct Lexer* lexer, struct Token* token) {
  token->type = TOK_Error;
  token->offset = lexer->start_offset;
  str_init(&token->text, "unexpected character", SIZE_MAX);
  return true;
}

static bool make_unterminated_string_error(const struct Lexer* lexer, struct Token* token) {
  token->type = TOK_Error;
  token->offset = lexer->start_offset;
  str_init(&token->text, "unterminated string", SIZE_MAX);
  return true;
}

static bool make_tok(const struct Lexer* lexer, struct Token* token, enum TokenType type) {
  token->type = type;
  token->offset = lexer->start_offset;
  if (!str_init(&token->text, tok_start(lexer), tok_len(lexer))) {
    return make_invalid_utf8_error(lexer, token);
  }
  return true;
}

static bool string(struct Lexer* lexer, struct Token* token) {
  while (!is_at_end(lexer) && peek(lexer) != '"') {
    char c = advance(lexer);
    if (c == '\\') {
      if (is_at_end(lexer)) {
        break;
      }
      advance(lexer);
    }
  }
  if (is_at_end(lexer)) {
    return make_unterminated_string_error(lexer, token);
  }
  advance(lexer);
  // Need to trim off opening and closing quotes, so we can't use `make_tok`
  token->type = TOK_String;
  token->offset = lexer->start_offset + 1;
  if (!str_init(&token->text, tok_start(lexer) + 1, tok_len(lexer) - 2)) {
    return make_invalid_utf8_error(lexer, token);
  }
  return true;
}

static bool number(struct Lexer* lexer, struct Token* token) {
  bool found_point = false;
  while (!is_at_end(lexer)) {
    char c = peek(lexer);
    if (c == '.') {
      if (found_point) {
        break;
      }
      if (isdigit(peek2(lexer))) {
        advance(lexer);
        advance(lexer);
        found_point = true;
      }
    } else if (isdigit(c)) {
      advance(lexer);
    } else {
      break;
    }
  }
  if (found_point) {
    return make_tok(lexer, token, TOK_Float);
  }
  return make_tok(lexer, token, TOK_Integer);
}

static enum TokenType check_keyword(const struct Lexer* lexer, size_t offset, size_t len,
                                    const char *rem, enum TokenType type) {
  if (tok_len(lexer) == offset + len && !memcmp(tok_start(lexer) + offset, rem, len)) {
    return type;
  }
  return TOK_Identifier;
}

static enum TokenType keyword_or_identifier(const struct Lexer* lexer) {
#define BYTE(I) (lexer->source[lexer->start_offset + (I)])

  switch (BYTE(0)) {
  case 'a': return check_keyword(lexer, 1, 2, "nd", TOK_And);
  case 'b': return check_keyword(lexer, 1, 4, "reak", TOK_Break);
  case 'c': return check_keyword(lexer, 1, 7, "ontinue", TOK_Continue);
  case 'e': return check_keyword(lexer, 1, 3, "lse", TOK_Else);
  case 'f':
    if (tok_len(lexer) >= 2) {
      switch (BYTE(1)) {
      case 'a': return check_keyword(lexer, 2, 3, "lse", TOK_False);
      case 'n':
        if (tok_len(lexer) == 2) {
          return TOK_Fn;
        }
        return TOK_Identifier;
      case 'o':
        if (tok_len(lexer) == 3 && BYTE(2) == 'r') {
          return TOK_For;
        }
        return TOK_Identifier;
      }
    }
    return TOK_Identifier;
  case 'i':
    if (tok_len(lexer) == 2) {
      switch (BYTE(1)) {
      case 'n': return TOK_In;
      case 'f': return TOK_If;
      }
    }
    return TOK_Identifier;
  case 'l': return check_keyword(lexer, 1, 2, "et", TOK_Let);
  case 'n':
    if (tok_len(lexer) == 3) {
      switch (BYTE(1)) {
      case 'o': return BYTE(2) == 't' ? TOK_Not : TOK_Identifier;
      case 'i': return BYTE(2) == 'i' ? TOK_Nil : TOK_Identifier;
      }
    }
    return TOK_Identifier;
  case 'o':
    if (tok_len(lexer) == 2 && BYTE(1) == 'r') {
      return TOK_Or;
    }
    return TOK_Identifier;
  case 'p': return check_keyword(lexer, 1, 2, "ub", TOK_Pub);
  case 'r':
    if (tok_len(lexer) > 2 && BYTE(1) == 'e') {
      switch (BYTE(2)) {
      case 't': return check_keyword(lexer, 3, 3, "urn", TOK_Return);
      case 'q': return check_keyword(lexer, 3, 4, "uire", TOK_Require);
      }
    }
    return TOK_Identifier;
  case 's':
    if (tok_len(lexer) >= 4) {
      switch (BYTE(1)) {
      case 'e': return check_keyword(lexer, 2, 2, "lf", TOK_Self);
      case 't': return check_keyword(lexer, 2, 4, "ruct", TOK_Struct);
      }
    }
    return TOK_Identifier;
  case 't': return check_keyword(lexer, 1, 3, "rue", TOK_True);
  case 'v': return check_keyword(lexer, 1, 6, "arargs", TOK_Varargs);
  case 'w': return check_keyword(lexer, 1, 4, "hile", TOK_While);
  case 'y': return check_keyword(lexer, 1, 4, "ield", TOK_Yield);
  default:
    return TOK_Identifier;
  }

#undef BYTE
}

static bool identifier(struct Lexer* lexer, struct Token* token) {
  while (!is_at_end(lexer)) {
    char c = peek(lexer);
    if (c == '_' || isalnum(c)) {
      advance(lexer);
    } else {
      break;
    }
  }
  return make_tok(lexer, token, keyword_or_identifier(lexer));
}

bool lexer_tok(struct Lexer* lexer, struct Token* token) {
#define MAKE_TOK(TYP) make_tok(lexer, token, TYP)
#define MATCH(C) match(lexer, C)

  skip_whitespace_and_comments(lexer);
  if (is_at_end(lexer)) {
    make_tok(lexer, token, TOK_EOF);
    return false;
  }
  lexer->start_offset = lexer->current_offset;
  char c = advance(lexer);
  switch (c) {
  case ';': return MAKE_TOK(TOK_SemiColon);
  case '{': return MAKE_TOK(TOK_LeftCurBr);
  case '}': return MAKE_TOK(TOK_RightCurBr);
  case '[': return MAKE_TOK(TOK_LeftSqBr);
  case ']': return MAKE_TOK(TOK_RightSqBr);
  case '(': return MAKE_TOK(TOK_LeftParen);
  case ')': return MAKE_TOK(TOK_RightParen);
  case ':': return MAKE_TOK(TOK_Colon);
  case ',': return MAKE_TOK(TOK_Comma);
  case '.':
    if (peek(lexer) == '.' && peek2(lexer) == '.') {
      advance(lexer);
      advance(lexer);
      return MAKE_TOK(TOK_Ellipsis);
    }
    return MAKE_TOK(TOK_Dot);
  case '=':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_Equal);
    }
    return MAKE_TOK(TOK_Assign);
  case '!':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_NotEqual);
    }
    return MAKE_TOK(TOK_BitNot);
  case '<':
    if (MATCH('<')) {
      if (MATCH('=')) {
        return MAKE_TOK(TOK_ShiftLeftAssign);
      }
      return MAKE_TOK(TOK_ShiftLeft);
    }
    if (MATCH('=')) {
      return MAKE_TOK(TOK_LessEqual);
    }
    return MAKE_TOK(TOK_LessThan);
  case '>':
    if (MATCH('>')) {
      if (MATCH('=')) {
        return MAKE_TOK(TOK_ShiftRightAssign);
      }
      return MAKE_TOK(TOK_ShiftRight);
    }
    if (MATCH('=')) {
      return MAKE_TOK(TOK_GreaterEqual);
    }
    return MAKE_TOK(TOK_GreaterThan);
  case '|':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_BitOrAssign);
    }
    return MAKE_TOK(TOK_BitOr);
  case '^':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_BitXorAssign);
    }
    return MAKE_TOK(TOK_BitXor);
  case '&':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_BitAndAssign);
    }
    return MAKE_TOK(TOK_BitAnd);
  case '+':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_AddAssign);
    }
    return MAKE_TOK(TOK_Plus);
  case '-':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_SubAssign);
    }
    return MAKE_TOK(TOK_Minus);
  case '*':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_MulAssign);
    }
    return MAKE_TOK(TOK_Star);
  case '/':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_DivAssign);
    }
    return MAKE_TOK(TOK_Slash);
  case '%':
    if (MATCH('=')) {
      return MAKE_TOK(TOK_ModAssign);
    }
    return MAKE_TOK(TOK_Percent);
  case '"': return string(lexer, token);
  default:
    if (isdigit(c)) {
      return number(lexer, token);
    }
    if (c == '_' || isalpha(c)) {
      return identifier(lexer, token);
    }
    return make_unexpected_char_error(lexer, token);
  }

#undef MATCH
#undef MAKE_TOK
}
