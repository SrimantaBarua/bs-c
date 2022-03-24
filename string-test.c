#include "string.h"

#include "test.h"

TEST(String, CreateValidUTF8) {
  struct String string;
  ASSERT(string_init(&string, "abcd"));
  ASSERT(string.data != NULL);
  ASSERT(string.capacity == 4);
  ASSERT(string.length == 4);
}

TEST_FAIL(String, CreateInvalidUTF8) {
  struct String string;
  ASSERT(string_init(&string, "\x80"));
}
