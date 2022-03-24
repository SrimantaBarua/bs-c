#ifndef __BS_LOG_H__
#define __BS_LOG_H__

#include <stdio.h>

#include "util.h"

#define DIE(FMT, ...) do { \
    fprintf(stderr, "ERROR: %s:%d: " FMT, __FILENAME__, __LINE__, __VA_ARGS__); \
} while (0)

#define CHECK(X) do {                           \
    if (!(X)) {                                 \
      DIE("Assertion failed: %s", #X);          \
    }                                           \
}while (0)

#endif  // __BS_LOG_H__
