#ifndef __BS_TEST_MACROS_H__
#define __BS_TEST_MACROS_H__

#include <stdio.h>
#include <stdlib.h>

#include "string.h"
#include "util.h"

#define ERR_FMT(FMT, ...) do {                          \
    fprintf(stderr, "ERROR: %s:%d: " FMT, __FILENAME__, \
            __LINE__, __VA_ARGS__);                     \
  } while (0)

#define ASSERT(X) do {                       \
    if (!(X)) {                              \
      ERR_FMT("Assertion failed: %s\n", #X); \
      exit(1);                               \
    }                                        \
  } while (0)

#define ASSERT_INT_EQ(X, Y) do {                                        \
    int64_t x = (int64_t) X;                                            \
    int64_t y = (int64_t) Y;                                            \
    if (x != y) {                                                       \
      ERR_FMT("Assertion failed: %s == %s\n  LHS = %lld; RHS = %lld\n", #X, #Y, x, y); \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

#define ASSERT_STR_EQ(X, Y) do {                                        \
    struct Str x = X;                                                   \
    struct Str y = Y;                                                   \
    if (!str_equal(&x, &y)) {                                           \
      ERR_FMT("Assertion failed: %s == %s\n  LHS = ", #X, #Y);          \
      struct Writer* writer = (struct Writer*) file_writer_create(stderr); \
      str_print(&x, writer);                                            \
      writer->writef(writer, "\n  RHS = ");                             \
      str_print(&y, writer);                                            \
      writer->writef(writer, "\n");                                     \
      file_writer_free((struct FileWriter*) writer);                    \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

#endif  // __BS_TEST_MACROS_H__
