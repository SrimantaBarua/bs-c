#include "bs.h"

#include "ast.h"
#include "bytecode.h"
#include "code-gen.h"
#include "memory.h"
#include "parser.h"
#include "writer.h"

void bs_init(struct Bs* bs, struct Writer* writer) {
  mem_init(&bs->mem);
  bs->writer = writer;
}

void bs_fini(struct Bs* bs) {
  UNUSED(bs);
}

enum BsStatus bs_interpret(struct Bs* bs, const char *source) {
  bool incomplete_input = false;

  struct Ast* ast = parse(source, bs->writer, &incomplete_input);
  if (ast) {
    ast_print(ast, bs->writer);
    bs->writer->writef(bs->writer, "\n");
  }
  bool ok = ast != NULL;

  if (ok) {
    struct Chunk chunk;
    chunk_init(&chunk, &bs->mem);
    if (generate_bytecode(ast, &chunk, bs->writer)) {
      chunk_disassemble(&chunk, "__main__", bs->writer);
    }
    chunk_fini(&chunk);
  }

  ast_free(ast);

  if (incomplete_input) {
    return BS_Incomplete;
  }
  return ok ? BS_Ok : BS_Error;
}
