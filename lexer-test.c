#include "lexer.h"

#include "string.h"
#include "test.h"

#define ASSERT_NEXT_IS_TOKEN(TYP, TEXT, LINE) do {   \
    ASSERT(lexer_tok(&lexer, &token));               \
    ASSERT(token.type == TYP);                       \
    ASSERT_INT_EQ(token.line_num, LINE);             \
    ASSERT(str_init(&token_str, TEXT, SIZE_MAX));    \
    ASSERT_STR_EQ(token.text, token_str);            \
} while (0)

#define ASSERT_LEXER_AT_END() ASSERT(!lexer_tok(&lexer, &token))

TEST(Lexer, Operators) {
  struct Lexer lexer;
  struct Str token_str;
  struct Token token;
  lexer_init(&lexer, ";/= *= <<<=");
  ASSERT_NEXT_IS_TOKEN(TOK_SemiColon, ";", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_DivAssign, "/=", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_MulAssign, "*=", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_ShiftLeft, "<<", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_LessEqual, "<=", 0);
  ASSERT_LEXER_AT_END();
}

TEST(Lexer, Primitives) {
  struct Lexer lexer;
  struct Str token_str;
  struct Token token;
  lexer_init(&lexer, "\"hello\"1.2 3 world");
  ASSERT_NEXT_IS_TOKEN(TOK_String, "hello", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_Float, "1.2", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "3", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "world", 0);
  ASSERT_LEXER_AT_END();
}

TEST(Lexer, FibFunction) {
  struct Lexer lexer;
  struct Str token_str;
  struct Token token;
  lexer_init(&lexer, "fn fib(n) {\n  if n <= 1 {\n    return 1;\n  } else {\n    "
             "return fib(n - 1) + fib(n - 2);\n  }\n}\n");
  ASSERT_NEXT_IS_TOKEN(TOK_Fn, "fn", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "fib", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftParen, "(", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_RightParen, ")", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftCurBr, "{", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_If, "if", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_LessEqual, "<=", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "1", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftCurBr, "{", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_Return, "return", 2);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "1", 2);
  ASSERT_NEXT_IS_TOKEN(TOK_SemiColon, ";", 2);
  ASSERT_NEXT_IS_TOKEN(TOK_RightCurBr, "}", 3);
  ASSERT_NEXT_IS_TOKEN(TOK_Else, "else", 3);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftCurBr, "{", 3);
  ASSERT_NEXT_IS_TOKEN(TOK_Return, "return", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "fib", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftParen, "(", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Minus, "-", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "1", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_RightParen, ")", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Plus, "+", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "fib", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftParen, "(", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Minus, "-", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "2", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_RightParen, ")", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_SemiColon, ";", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_RightCurBr, "}", 5);
  ASSERT_NEXT_IS_TOKEN(TOK_RightCurBr, "}", 6);
  ASSERT_LEXER_AT_END();
}
