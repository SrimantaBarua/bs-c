#include "lexer.h"

#include "string.h"
#include "test.h"

static struct Token make_token(enum TokenType type, const char* text, size_t offset) {
  struct Token token = {
    .offset = offset,
    .type = type
  };
  str_init(&token.text, text, SIZE_MAX);
  return token;
}

#define ASSERT_NEXT_IS_TOKEN(TYP, TEXT, OFFSET) do {                    \
    ASSERT(lexer_tok(&lexer, &result));                                 \
    ASSERT(!result.is_error);                                           \
    ASSERT(result.token.type == TYP);                                   \
    ASSERT_INT_EQ(result.token.offset, OFFSET);                         \
    ASSERT(str_init(&token_str, TEXT, SIZE_MAX));                       \
    ASSERT_STR_EQ(result.token.text, token_str);                        \
} while (0)

#define ASSERT_LEXER_AT_END() ASSERT(!lexer_tok(&lexer, &result))

TEST(Lexer, Operators) {
  struct Lexer lexer;
  struct Str token_str;
  struct TokenOrError result;
  lexer_init(&lexer, ";/= *= <<<=");
  ASSERT_NEXT_IS_TOKEN(TOK_SemiColon, ";", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_DivAssign, "/=", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_MulAssign, "*=", 4);
  ASSERT_NEXT_IS_TOKEN(TOK_ShiftLeft, "<<", 7);
  ASSERT_NEXT_IS_TOKEN(TOK_LessEqual, "<=", 9);
  ASSERT_LEXER_AT_END();
}

TEST(Lexer, Primitives) {
  struct Lexer lexer;
  struct Str token_str;
  struct TokenOrError result;
  lexer_init(&lexer, "\"hello\"1.2 3 world");
  ASSERT_NEXT_IS_TOKEN(TOK_String, "hello", 1);
  ASSERT_NEXT_IS_TOKEN(TOK_Float, "1.2", 7);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "3", 11);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "world", 13);
  ASSERT_LEXER_AT_END();
}

TEST(Lexer, FibFunction) {
  struct Lexer lexer;
  struct Str token_str;
  struct TokenOrError result;
  lexer_init(&lexer, "fn fib(n) {\n  if n <= 1 {\n    return 1;\n  } else {\n    "
             "return fib(n - 1) + fib(n - 2);\n  }\n}\n");
  ASSERT_NEXT_IS_TOKEN(TOK_Fn, "fn", 0);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "fib", 3);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftParen, "(", 6);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 7);
  ASSERT_NEXT_IS_TOKEN(TOK_RightParen, ")", 8);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftCurBr, "{", 10);
  ASSERT_NEXT_IS_TOKEN(TOK_If, "if", 14);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 17);
  ASSERT_NEXT_IS_TOKEN(TOK_LessEqual, "<=", 19);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "1", 22);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftCurBr, "{", 24);
  ASSERT_NEXT_IS_TOKEN(TOK_Return, "return", 30);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "1", 37);
  ASSERT_NEXT_IS_TOKEN(TOK_SemiColon, ";", 38);
  ASSERT_NEXT_IS_TOKEN(TOK_RightCurBr, "}", 42);
  ASSERT_NEXT_IS_TOKEN(TOK_Else, "else", 44);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftCurBr, "{", 49);
  ASSERT_NEXT_IS_TOKEN(TOK_Return, "return", 55);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "fib", 62);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftParen, "(", 65);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 66);
  ASSERT_NEXT_IS_TOKEN(TOK_Minus, "-", 68);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "1", 70);
  ASSERT_NEXT_IS_TOKEN(TOK_RightParen, ")", 71);
  ASSERT_NEXT_IS_TOKEN(TOK_Plus, "+", 73);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "fib", 75);
  ASSERT_NEXT_IS_TOKEN(TOK_LeftParen, "(", 78);
  ASSERT_NEXT_IS_TOKEN(TOK_Identifier, "n", 79);
  ASSERT_NEXT_IS_TOKEN(TOK_Minus, "-", 81);
  ASSERT_NEXT_IS_TOKEN(TOK_Integer, "2", 83);
  ASSERT_NEXT_IS_TOKEN(TOK_RightParen, ")", 84);
  ASSERT_NEXT_IS_TOKEN(TOK_SemiColon, ";", 85);
  ASSERT_NEXT_IS_TOKEN(TOK_RightCurBr, "}", 89);
  ASSERT_NEXT_IS_TOKEN(TOK_RightCurBr, "}", 91);
  ASSERT_LEXER_AT_END();
}
