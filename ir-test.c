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
