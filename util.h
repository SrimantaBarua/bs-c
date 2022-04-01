#ifndef __BS_UTIL_H__
#define __BS_UTIL_H__

#include <stdbool.h>
#include <stddef.h>

#define __FILENAME__ (&__FILE__[BS_SOURCE_PATH_SIZE])

#define UNUSED(X) ((void) (X))

struct SourceLine {
  size_t line_number;
  const char* start;  // Start of the source code for the line
  size_t length;      // Length of the line in bytes
  size_t range_start; // Start offset of the provided range
  size_t range_end;   // End offset of the provided range
};

// Given a blob of source code, a start offset, and a length, return a pointer
// to the start of the text for the LINE that the offset falls in, the length of
// the line in bytes, the line number itself, the start offset into the line for
// the provided start offset, and same for the provided end offset. If the end
// offset is not in the same line, ends it at the end of the current line. If
// the provided offsets are not within the range of the source code, returns
// `false`, and the fields of the struct are undefined.
bool get_source_line(const char* source, size_t start, size_t length, struct SourceLine*);

#endif  // __BS_UTIL_H__
