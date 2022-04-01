#include "parser.h"

#include "test.h"
#include "util.h"
#include "writer.h"

#define SETUP()                                                         \
  struct String ast_buf;                                                \
  struct Str target_str;                                                \
  string_init(&ast_buf, "");                                            \
  struct Writer* err_writer = (struct Writer*) file_writer_create(stderr); \
  struct Writer* ast_writer = (struct Writer*) string_writer_create(&ast_buf);

#define TEARDOWN() \
  ast_free(ast);                                                        \
  file_writer_free((struct FileWriter*) err_writer);                    \
  string_writer_free((struct StringWriter*) ast_writer);                \
  string_fini(&ast_buf);

#define COMPARE_TO(TARGET)                                              \
  ASSERT(ast != NULL);                                                  \
  ast_print(ast, err_writer);                                           \
  ast_print(ast, ast_writer);                                           \
  str_init(&target_str, TARGET, SIZE_MAX);                              \
  ASSERT_STR_EQ(target_str, ((struct Str) { ast_buf.data, ast_buf.length }));

#define E2E_TEST(INPUT, TARGET) do {                               \
    SETUP()                                                        \
    bool incomplete_input = false;                                 \
    struct Ast* ast = parse(INPUT, err_writer, &incomplete_input); \
    ASSERT(ast != NULL);                                           \
    ASSERT(!incomplete_input);                                     \
    COMPARE_TO(TARGET)                                             \
    TEARDOWN();                                                    \
  } while (0)

#define E2E_INCOMPLETE(INPUT) do {                                 \
    SETUP()                                                        \
    UNUSED(target_str);                                            \
    bool incomplete_input = false;                                 \
    struct Ast* ast = parse(INPUT, err_writer, &incomplete_input); \
    ASSERT(ast == NULL);                                           \
    ASSERT(incomplete_input);                                      \
    TEARDOWN();                                                    \
  } while (0)

TEST(Parser, SimpleIfElse) {
  E2E_TEST("if n < 1 { 1 } else { 2 }",
           "(program (if (< n 1) (block <ret> 1) (else (block <ret> 2))))");
}

TEST(Parser, SimpleForLoop) {
  E2E_TEST("for i in range(10) { print(i); }",
           "(program (for i in (call range 10) (block <noret> (call print i))))");
}

TEST(Parser, FibonacciFunction) {
  E2E_TEST("fn fib(n) { if n <= 1 { 1 } else { fib(n - 1) + fib(n - 2) } }",
           "(program (let fib <private> (fn (params n) (block <ret> (if (<= n 1) (block <ret> 1) "
           "(else (block <ret> (+ (call fib (- n 1)) (call fib (- n 2))))))))))");
}

TEST(Parser, FactorialWithNewlines) {
  E2E_TEST("fn fact(n) {\n"
           "  if n <= 1 {\n"
           "    1\n"
           "  } else {\n"
           "    n * fact(n - 1)\n"
           "  }\n"
           "}\n",
           "(program (let fact <private> (fn (params n) (block <ret> (if (<= n 1) (block <ret> 1) "
           "(else (block <ret> (* n (call fact (- n 1))))))))))");
}

TEST(Parser, PointStruct) {
  E2E_TEST("struct Point : Object { fn init(self, x, y) { self.x = x; self.y = y; } }",
           "(program (let Point <private> (struct (parent Object) (block <noret> (let init <private>"
           " (fn (params self x y) (block <noret> (= (. self x) x) (= (. self y) y))))))))");
}

TEST(Parser, LetStatements) {
  E2E_TEST("let x = 1",
           "(program (let x <private> 1))");
  E2E_TEST("let git = require(\"git\")",
           "(program (let git <private> (require \"git\")))");
}

TEST(Parser, BasicExpressions) {
  E2E_TEST("1 + 2 * 3 / 4 - 5 % 6",
           "(program (- (+ 1 (/ (* 2 3) 4)) (% 5 6)))");
  E2E_TEST("1 + 2 * 3 / (4 - 5) % 6",
           "(program (+ 1 (% (/ (* 2 3) (- 4 5)) 6)))");
  E2E_TEST("not a == b and not a ^ b != c",
           "(program (and (not (== a b)) (not (!= (^ a b) c))))");
  E2E_TEST("a ^= b; c /= d; e <<= f; g >>= h;",
           "(program (= a (^ a b)) (= c (/ c d)) (= e (<< e f)) (= g (>> g h)))");
}

TEST(Parser, SetDictArr) {
  E2E_TEST("a ^= b; c /= d; e <<= f; g >>= h;",
           "(program (= a (^ a b)) (= c (/ c d)) (= e (<< e f)) (= g (>> g h)))");
}

TEST(Parser, Return) {
  E2E_TEST("fn f1() { return; } fn f2() { return 1; } fn f3() { return } fn f4() { return 1 }",
           "(program (let f1 <private> (fn (params) (block <noret> (return)))) "
           "(let f2 <private> (fn (params) (block <noret> (return 1)))) "
           "(let f3 <private> (fn (params) (block <ret> (return)))) "
           "(let f4 <private> (fn (params) (block <ret> (return 1)))))");
}

TEST_FAIL(Parser, MissedSemicolon) {
  E2E_TEST("fn f1() { let a = 2\nreturn a; }",
           "(program (let f1 <private> (fn (params) (block <noret> (let a <private> 2) (return a)))))");
}

TEST(Parser, Incomplete) {
  E2E_INCOMPLETE("fn f1() {");
}
