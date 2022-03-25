#include "util.h"

#include "log.h"

bool get_source_line(const char* source, size_t start, size_t length, struct SourceLine* line) {
  CHECK(length > 0);
  bool started = false, ended = false;
  size_t line_number = 0, line_offset = 0;
  for (size_t i = 0; source[i]; i++) {
    if (i == start) {
      line->line_number = line_number;
      line->start = source + i - line_offset;
      line->range_start = line_offset;
      started = true;
    } else if (i == start + length) {
      line->range_end = line_offset;
      ended = true;
    }
    if (source[i] == '\n') {
      if (ended) {
        line->length = line_offset;
        return true;
      }
      if (started) {
        line->range_end = line->length = line_offset;
        return true;
      }
      line_number += 1;
      line_offset = 0;
      continue;
    }
    line_offset++;
  }
  return false;
}
