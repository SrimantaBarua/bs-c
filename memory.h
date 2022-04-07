#ifndef __BS_MEMORY_H__
#define __BS_MEMORY_H__

#include <stddef.h>

#include "util.h"

// Handle to the "managed heap". This tracks allocations and frees to figure out
// how much memory is in use. This will also track all allocated objects and act
// as an entrypoint for the garbage collector.
struct Memory {
  size_t mem_used;  // Current amount of memory used for this BS instance
};

// Initialize memory tracker
void mem_init(struct Memory* mem);

// Allocate managed memory.
void* mem_alloc(struct Memory* mem, size_t size, const char *file, int line);

// Free managed memory.
void mem_free(struct Memory* mem, void* ptr, size_t size, const char *file, int line);

// Reallocate managed memory
void* mem_realloc(struct Memory* mem, void* ptr, size_t old_size, size_t new_size, const char *file,
                  int line);

// Macros to use for memory allocation - adds debug information
#define MEM_ALLOC(MEM, SIZE) \
  mem_alloc(MEM, SIZE, __FILENAME__, __LINE__)
#define MEM_FREE(MEM, PTR, SIZE) \
  mem_free(MEM, PTR, SIZE, __FILENAME__, __LINE__)
#define MEM_REALLOC(MEM, PTR, OLD_SIZE, NEW_SIZE) \
  mem_realloc(MEM, PTR, OLD_SIZE, NEW_SIZE, __FILENAME__, __LINE__)

#endif  // __BS_MEMORY_H__
