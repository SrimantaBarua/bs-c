#include "string.h"

#include <string.h>

#include "test.h"

TEST(String, CreateValidUTF8) {
  struct String string1;
  ASSERT(string_init(&string1, "abcd"));
  ASSERT(string1.data != NULL);
  ASSERT(string1.capacity == 4);
  ASSERT(string1.length == 4);

  struct String string2;
  ASSERT(string_init(&string2, ""));
  ASSERT(string2.data == NULL);
  ASSERT(string2.capacity == 0);
  ASSERT(string2.length == 0);
}

TEST_FAIL(String, CreateInvalidUTF8) {
  struct String string;
  ASSERT(string_init(&string, "\x80"));
}

TEST(String, Writer) {
  struct String string;
  ASSERT(string_init(&string, ""));
  struct Writer* writer = (struct Writer*) string_writer_create(&string);
  writer->writef(writer, "Hello, %d %s\n", 123, "world");
  string_writer_free((struct StringWriter*) writer);
  ASSERT_INT_EQ(string.length, 17);
  ASSERT(!strncmp((const char*) string.data, "Hello, 123 world\n", 17));
}
