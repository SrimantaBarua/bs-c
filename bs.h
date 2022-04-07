#ifndef __BS_BS_H__
#define __BS_BS_H__

#include "memory.h"
#include "writer.h"

enum BsStatus {
  BS_Ok,
  BS_Error,
  BS_Incomplete,
};

// State for the interpreter instance
struct Bs {
  struct Memory mem;
  struct Writer* writer;
};

// Initialize BS state
void bs_init(struct Bs* bs, struct Writer* writer);

// Interpret source code in this BS instance
enum BsStatus bs_interpret(struct Bs* bs, const char *source);

// Free memory for BS state
void bs_fini(struct Bs* bs);

#endif  // __BS_BS_H__
