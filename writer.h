#ifndef __BS_WRITER_H__
#define __BS_WRITER_H__

#include <stdarg.h>
#include <stdio.h>

// An abstract interface that allows writing to. Specific implementers can
// either be a file, a string, stderr, whatever.
struct Writer {
  // Printf-style formatted output
  int (*writef)(struct Writer*, const char *fmt, ...);

  // vprintf-style formatted output
  int (*vwritef)(struct Writer*, const char *fmt, va_list ap);

  // Flush any buffers
  int (*flush)(struct Writer*);
};

// Default writef just redirects to vwritef
extern int DEFAULT_WRITEF(struct Writer*, const char*, ...);

// Some specific implementations follow -

// Write out to a FILE*
struct FileWriter {
  struct Writer writer;
  FILE* file;
};

// Allocate a file writer
struct FileWriter* file_writer_create(FILE* file);

// Free a file writer. Note: This DOES NOT close the FILE*
void file_writer_free(struct FileWriter* file_writer);

#endif  // __BS_WRITER_H__
