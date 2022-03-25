#include "writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

static int file_writef(struct Writer* writer, const char *fmt, ...) {
  struct FileWriter* file_writer = (struct FileWriter*) writer;
  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(file_writer->file, fmt, args);
  va_end(args);
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
  writer->writer.writef = file_writef;
  writer->writer.flush = file_flush;
  writer->file = file;
  return writer;
}

void file_writer_free(struct FileWriter* file_writer) {
  free(file_writer);
}
