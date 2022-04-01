#include "bs.h"

#include "ast.h"
#include "ir.h"
#include "parser.h"
#include "writer.h"

enum BsStatus bs_interpret(const char *source) {
  struct Writer* writer = (struct Writer*) file_writer_create(stderr);
  bool incomplete_input = false;

  struct Ast* ast = parse(source, writer, &incomplete_input);
  if (ast) {
    ast_print(ast, writer);
    writer->writef(writer, "\n");
  }
  bool ok = ast != NULL;

  if (ast) {
    struct IrChunk root_chunk;
    ir_chunk_init(&root_chunk);
    ok &= ir_generate(source, ast, &root_chunk, writer);
    if (ok) {
      ir_chunk_print(&root_chunk, "__main__", writer);
    }
    ir_chunk_fini(&root_chunk);
  }

  ast_free(ast);
  file_writer_free((struct FileWriter*) writer);

  if (incomplete_input) {
    return BS_Incomplete;
  }
  return ok ? BS_Ok : BS_Error;
}
