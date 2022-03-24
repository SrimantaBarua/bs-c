#ifndef __BS_TEST_MACROS_H__
#define __BS_TEST_MACROS_H__

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

#define LOG_FMT(FMT, ...) do {                               \
    fprintf(stderr, "ERROR: %s:%d: " FMT "\n", __FILENAME__, \
            __LINE__, __VA_ARGS__);                          \
  } while (0)

#define ASSERT(X) do {                     \
    if (!(X)) {                            \
      LOG_FMT("Assertion failed: %s", #X); \
      exit(1);                             \
    }                                      \
  } while (0)

#define ASSERT_FMT(X, FMT, ...) do {                            \
    if (!(X)) {                                                 \
      LOG_FMT("Assertion failed: %s\n  " FMT, #X, __VA_ARGS__); \
      exit(1);                                                  \
    }                                                           \
} while (0)

#endif  // __BS_TEST_MACROS_H__
