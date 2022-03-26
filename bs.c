#include "bs.h"

#include "ast.h"
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
  ast_free(ast);
  file_writer_free((struct FileWriter*) writer);
  if (incomplete_input) {
    return BS_Incomplete;
  }
  return ok ? BS_Ok : BS_Error;
}
