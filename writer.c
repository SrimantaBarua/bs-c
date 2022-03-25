#include "writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

// Default writef just redirects to vwritef
int DEFAULT_WRITEF(struct Writer* writer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = writer->vwritef(writer, fmt, args);
  va_end(args);
  return ret;
}

static int file_vwritef(struct Writer* writer, const char *fmt, va_list ap) {
  struct FileWriter* file_writer = (struct FileWriter*) writer;
  int ret = vfprintf(file_writer->file, fmt, ap);
  return ret;
}

static int file_flush(struct Writer* writer) {
  struct FileWriter* file_writer = (struct FileWriter*) writer;
  return fflush(file_writer->file);
}

struct FileWriter* file_writer_create(FILE* file) {
  struct FileWriter* writer = malloc(sizeof(struct FileWriter));
  if (!writer) {
    DIE_ERR("malloc()");
  }
  writer->writer.writef = DEFAULT_WRITEF;
  writer->writer.vwritef = file_vwritef;
  writer->writer.flush = file_flush;
  writer->file = file;
  return writer;
}

void file_writer_free(struct FileWriter* file_writer) {
  free(file_writer);
}
