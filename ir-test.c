#include "ir.h"

#include "parser.h"
#include "test.h"

#define E2E_TEST(INPUT, TARGET) do {                                    \
    struct String ir_buf;                                               \
    struct Str target_str;                                              \
    struct IrChunk root_chunk;                                          \
    string_init(&ir_buf, "");                                           \
    ir_chunk_init(&root_chunk);                                         \
    struct Writer* err_writer = (struct Writer*) file_writer_create(stderr); \
    struct Writer* ir_writer = (struct Writer*) string_writer_create(&ir_buf); \
    bool incomplete_input = false;                                      \
                                                                        \
    struct Ast* ast = parse(INPUT, err_writer, &incomplete_input);      \
    ASSERT(ast != NULL);                                                \
    ASSERT(!incomplete_input);                                          \
    ASSERT(ir_generate(INPUT, ast, &root_chunk, err_writer));           \
                                                                        \
    ir_chunk_print(&root_chunk, "__main__", ir_writer);                 \
    str_init(&target_str, TARGET, SIZE_MAX);                            \
    ASSERT_STR_EQ(((struct Str) { ir_buf.data, ir_buf.length }), target_str); \
                                                                        \
    ir_chunk_fini(&root_chunk);                                         \
    ast_free(ast);                                                      \
    file_writer_free((struct FileWriter*) err_writer);                  \
    string_writer_free((struct StringWriter*) ir_writer);               \
    string_fini(&ir_buf);                                               \
  } while (0)

TEST(Ir, BasicExpressions) {
  E2E_TEST("1 + 2 * 3 / 4 - 5 % 6",
           "__main__:\n"
           "  t0 := 1\n"
           "  t1 := 2\n"
           "  t2 := 3\n"
           "  t3 := t1 * t2\n"
           "  t4 := 4\n"
           "  t5 := t3 / t4\n"
           "  t6 := t0 + t5\n"
           "  t7 := 5\n"
           "  t8 := 6\n"
           "  t9 := t7 % t8\n"
           "  t10 := t6 - t9\n"
           );
  E2E_TEST("1 + 2 * 3 / (4 - 5) % 6",
           "__main__:\n"
           "  t0 := 1\n"
           "  t1 := 2\n"
           "  t2 := 3\n"
           "  t3 := t1 * t2\n"
           "  t4 := 4\n"
           "  t5 := 5\n"
           "  t6 := t4 - t5\n"
           "  t7 := t3 / t6\n"
           "  t8 := 6\n"
           "  t9 := t7 % t8\n"
           "  t10 := t0 + t9\n"
           );
  E2E_TEST("not a == b and not a ^ b != c",
           "__main__:\n"
           "  t0 := a\n"
           "  t1 := b\n"
           "  t2 := t0 == t1\n"
           "  t3 := not t2\n"
           "  t4 := a\n"
           "  t5 := b\n"
           "  t6 := t4 ^ t5\n"
           "  t7 := c\n"
           "  t8 := t6 != t7\n"
           "  t9 := not t8\n"
           "  t10 := t3 and t9\n"
           );
}

TEST(Ir, FunctionCall) {
  E2E_TEST("a = fun(5, c)",
           "__main__:\n"
           "  t0 := 5\n"
           "  push t0\n"
           "  t1 := c\n"
           "  push t1\n"
           "  t2 := fun\n"
           "  t3 := t2(2)\n"
           "  t4 := &a\n"
           "  *t4 := t3\n"
           );
}

TEST(Ir, MemberIndexAccess) {
  E2E_TEST("a = b.c[d]; e[f].g = h; i[j] = k;",
           "__main__:\n"
           "  t0 := b\n"
           "  t1 := t0.c\n"
           "  t2 := d\n"
           "  t3 := t1[t2]\n"
           "  t4 := &a\n"
           "  *t4 := t3\n"
           "  t5 := h\n"
           "  t6 := e\n"
           "  t7 := f\n"
           "  t8 := t6[t7]\n"
           "  t9 := &t8.g\n"
           "  *t9 := t5\n"
           "  t10 := k\n"
           "  t11 := i\n"
           "  t12 := j\n"
           "  t13 := &t11[t12]\n"
           "  *t13 := t10\n"
           );
}

TEST(Ir, ForLoop) {
  E2E_TEST("for i in range(0, 10) { print(i); }",
           "__main__:\n"
           "  t0 := 0\n"
           "  push t0\n"
           "  t1 := 10\n"
           "  push t1\n"
           "  t2 := range\n"
           "  t3 := t2(2)\n"
           "L0:\n"
           "  t4 := nil\n"
           "  t5 := t3(0)\n"
           "  t6 := t4 != t5\n"
           "  goto L1 if not t6\n"
           "  t7 := &i\n"
           "  *t7 := t5\n"
           "  t8 := i\n"
           "  push t8\n"
           "  t9 := print\n"
           "  t10 := t9(1)\n"
           "  goto L0\n"
           "L1:\n");
}

TEST(Ir, WhileLoop) {
  E2E_TEST("while i < 10 { print(i); i += 1; }",
           "__main__:\n"
           "L0:\n"
           "  t0 := i\n"
           "  t1 := 10\n"
           "  t2 := t0 < t1\n"
           "  goto L1 if not t2\n"
           "  t3 := i\n"
           "  push t3\n"
           "  t4 := print\n"
           "  t5 := t4(1)\n"
           "  t6 := i\n"
           "  t7 := 1\n"
           "  t8 := t6 + t7\n"
           "  t9 := &i\n"
           "  *t9 := t8\n"
           "  goto L0\n"
           "L1:\n");
}

TEST(Ir, IfStatement) {
  E2E_TEST("if a < 1 { 2 } else { 3 }",
           "__main__:\n"
           "  t0 := a\n"
           "  t1 := 1\n"
           "  t2 := t0 < t1\n"
           "  goto L0 if not t2\n"
           "  t3 := 2\n"
           "  t4 := t3\n"
           "  goto L1\n"
           "L0:\n"
           "  t5 := 3\n"
           "  t4 := t5\n"
           "L1:\n");
  E2E_TEST("if a < 1 { 2 }",
           "__main__:\n"
           "  t0 := a\n"
           "  t1 := 1\n"
           "  t2 := t0 < t1\n"
           "  goto L0 if not t2\n"
           "  t3 := 2\n"
           "L0:\n");
  E2E_TEST("ret = if a < 1 { 2 } else { 3 }",
           "__main__:\n"
           "  t0 := a\n"
           "  t1 := 1\n"
           "  t2 := t0 < t1\n"
           "  goto L0 if not t2\n"
           "  t3 := 2\n"
           "  t4 := t3\n"
           "  goto L1\n"
           "L0:\n"
           "  t5 := 3\n"
           "  t4 := t5\n"
           "L1:\n"
           "  t6 := &ret\n"
           "  *t6 := t4\n");
}

TEST_FAIL(Ir, IfWithoutElseRvalue) {
  E2E_TEST("ret = if a < 1 { 2 }",
           "__main__:\n"
           "  t0 := a\n"
           "  t1 := 1\n"
           "  t2 := t0 < t1\n"
           "  goto L0 if not t2\n"
           "  t3 := 2\n"
           "L0:\n"
           "  t4 := &ret\n"
           "  *t4 := t3\n");
}
