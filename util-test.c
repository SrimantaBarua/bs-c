#include "util.h"

#include "test.h"

TEST(Util, SourceLine) {
  struct SourceLine line;
  const char *source = "fn fib(n) {\n"
    "  if (n <= 1) {\n"
    "    return 1;\n"
    "  } else {\n"
    "    return fib(n - 1) + fib(n - 2);\n"
    "  }\n"
    "}\n";
  ASSERT(get_source_line(source, 3, 3, &line));
  ASSERT_INT_EQ(line.line_number, 0);
  ASSERT_INT_EQ(line.length, 11);
  ASSERT_INT_EQ(line.range_start, 3);
  ASSERT_INT_EQ(line.range_end, 6);
  ASSERT(line.start == source);

  ASSERT(get_source_line(source, 18, 1, &line));
  ASSERT_INT_EQ(line.line_number, 1);
  ASSERT_INT_EQ(line.length, 15);
  ASSERT_INT_EQ(line.range_start, 6);
  ASSERT_INT_EQ(line.range_end, 7);
  ASSERT(line.start == source + 12);

  ASSERT(!get_source_line(source, 100, 1, &line));
}
