#ifndef __BS_LOG_H__
#define __BS_LOG_H__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define DIE(FMT, ...) do {                                   \
    fprintf(stderr, "ERROR: %s:%d: " FMT "\n", __FILENAME__, \
            __LINE__, __VA_ARGS__);                          \
    exit(1);                                                 \
} while (0)

#define DIE_ERR(MSG) do {                                        \
    fprintf(stderr, "ERROR: %s:%d: " MSG ": %s\n", __FILENAME__, \
            __LINE__, strerror(errno));                          \
    exit(1);                                                     \
} while (0)

#define UNREACHABLE() do {                                        \
    fprintf(stderr, "ERROR: %s:%d: unreachable!\n", __FILENAME__, \
            __LINE__);                                            \
    exit(1);                                                      \
} while (0)

#define UNIMPLEMENTED() do {                                        \
    fprintf(stderr, "ERROR: %s:%d: unimplemented!\n", __FILENAME__, \
            __LINE__);                                              \
    exit(1);                                                        \
} while (0)

#define CHECK(X) do {                  \
    if (!(X)) {                        \
      DIE("Assertion failed: %s", #X); \
    }                                  \
  } while (0)

#endif  // __BS_LOG_H__
