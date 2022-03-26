#ifndef __BS_PARSER_H__
#define __BS_PARSER_H__

#include "ast.h"
#include "lexer.h"
#include "string.h"
#include "writer.h"

// Parse source source code and return an AST. Returns a NULL AST if the source
// code provided was empty (just whitespace?), or there was an error. Writes
// error messages out to `err_writer`. If the input was incomplete and there was
// no other error, setes `*incomplete_input` to `true`.
struct Ast* parse(const char* source, struct Writer *err_writer, bool* incomplete_input);

#endif  // __BS_PARSER_H__
