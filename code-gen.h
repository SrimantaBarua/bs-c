#ifndef __BS_CODE_GEN_H__
#define __BS_CODE_GEN_H__

#include <stdbool.h>

#include "ast.h"
#include "bytecode.h"
#include "writer.h"

// Generate bytecode for the stack-based VM from an AST. Returns `false` on
// failure, `true` on success.
bool generate_bytecode(const struct Ast* ast, struct Chunk* chunk, struct Writer* writer);

#endif  // __BS_CODE_GEN_H__
