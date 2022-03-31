#ifndef __BS_LEXER_H__
#define __BS_LEXER_H__

#include <stddef.h>

#include "string.h"

// The type of token being returned
enum TokenType {
  // Terminals
  TOK_Integer,
  TOK_Float,
  TOK_Identifier,
  TOK_String,
  // Keywords
  TOK_True,     // true
  TOK_False,    // false
  TOK_Nil,      // nil
  TOK_Fn,       // fn
  TOK_And,      // and
  TOK_Or,       // or
  TOK_Not,      // not
  TOK_Pub,      // pub
  TOK_Let,      // let
  TOK_For,      // for
  TOK_In,       // in
  TOK_If,       // if
  TOK_Else,     // else
  TOK_While,    // while
  TOK_Struct,   // struct
  TOK_Break,    // break
  TOK_Continue, // continue
  TOK_Self,     // self
  TOK_Require,  // require
  TOK_Return,   // return
  TOK_Yield,    // yield
  TOK_Varargs,  // varargs
  // Operators
  TOK_SemiColon,        // ;
  TOK_LeftCurBr,        // {
  TOK_RightCurBr,       // }
  TOK_LeftSqBr,         // [
  TOK_RightSqBr,        // ]
  TOK_LeftParen,        // (
  TOK_RightParen,       // )
  TOK_Colon,            // :
  TOK_Assign,           // =
  TOK_Dot,              // .
  TOK_Ellipsis,         // ...
  TOK_Comma,            // ,
  TOK_Equal,            // ==
  TOK_NotEqual,         // !=
  TOK_LessEqual,        // <=
  TOK_LessThan,         // <
  TOK_GreaterEqual,     // >=
  TOK_GreaterThan,      // >
  TOK_BitOr,            // |
  TOK_BitOrAssign,      // |=
  TOK_BitXor,           // ^
  TOK_BitXorAssign,     // ^=
  TOK_BitAnd,           // &
  TOK_BitAndAssign,     // &=
  TOK_BitNot,           // !
  TOK_ShiftLeft,        // <<
  TOK_ShiftLeftAssign,  // <<=
  TOK_ShiftRight,       // >>
  TOK_ShiftRightAssign, // >>=
  TOK_Plus,             // +
  TOK_AddAssign,        // +=
  TOK_Minus,            // -
  TOK_SubAssign,        // -=
  TOK_Star,             // *
  TOK_MulAssign,        // *=
  TOK_Slash,            // /
  TOK_DivAssign,        // /=
  TOK_Percent,          // %
  TOK_ModAssign,        // %=
  // Misc.
  TOK_Error,
  TOK_EOF,
  TOK_Undefined,
};

// Get string representation for token
const char* token_type_to_string(enum TokenType token_type);

// Tokens returned by the lexer
struct Token {
  enum TokenType type; // Type of token
  struct Str text;     // Slice of input string for the token
  size_t offset;       // Offset into the input string where the token starts
};

// Initialize something as an "undefined" token
void token_init_undefined(struct Token* token);

// Pull-based lexer. Call `lexer_tok` to advance
struct Lexer {
  const char* source;  // Input source
  // Offsets to be used by the next call to `lexer_tok`
  size_t start_offset;
  size_t current_offset;
};

// Initialize the lexer with input source code.
void lexer_init(struct Lexer* lexer, const char* source);

// Get the next token (or error) from the lexer. Returns `false` if we're at
// EOF, otherwise returns `true` regardless of token or error.
bool lexer_tok(struct Lexer* lexer, struct Token* token);

#endif  // __BS_LEXER_H__
