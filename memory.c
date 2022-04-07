#include "memory.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_DIE(FMT, ...) do {                                          \
    fprintf(stderr, "ERROR: %s:%d: " FMT "\n", file, line, __VA_ARGS__); \
    exit(1);                                                            \
  } while (0)

// Allocate managed memory.
void* mem_alloc(struct Memory* mem, size_t size, const char *file, int line) {
  void* ptr = malloc(size);
  if (!ptr) {
    MEM_DIE("malloc(): %s", strerror(errno));
  }
  mem->mem_used += size;
  return ptr;
}

// Free managed memory.
void mem_free(struct Memory* mem, void* ptr, size_t size, const char *file, int line) {
  if (size > mem->mem_used) {
    MEM_DIE("free(): size > mem->mem_used (%lu > %lu)", size, mem->mem_used);
  }
  free(ptr);
  mem->mem_used -= size;
}

// Reallocate managed memory
void* mem_realloc(struct Memory* mem, void* ptr, size_t old_size, size_t new_size, const char *file,
                  int line) {
  if (old_size > mem->mem_used) {
    MEM_DIE("realloc(): old_size > mem->mem_used (%lu > %lu)", old_size, mem->mem_used);
  }
  void* ret = realloc(ptr, new_size);
  if (!ptr) {
    MEM_DIE("realloc(): %s", strerror(errno));
  }
  mem->mem_used -= old_size;
  mem->mem_used += new_size;
  return ret;
}

#undef MEM_DIE
