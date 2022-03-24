#include "string.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Checks if the sequence of bytes provided as argument is valid UTF-8. This
// assumes that the sequence of bytes is NULL-terminated.
static bool validate_null_terminated_utf8(const uint8_t* bytes) {
  while (*bytes) {
    if (*bytes <= 0x7f) {
      // This is regular ASCII
      bytes++;
      continue;
    }
    codepoint_t codepoint;
    size_t num_bytes;
    if ((*bytes & 0xe0) == 0xc0) {
      // 0b110xxxxx 0b10xxxxxx
      codepoint = *bytes & 0x1f;
      num_bytes = 1;
    } else if ((*bytes & 0xf0) == 0xe0) {
      // 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      codepoint = *bytes & 0x0f;
      num_bytes = 2;
    } else if ((*bytes & 0xf8) == 0xf0) {
      // 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      codepoint = *bytes & 0x07;
      num_bytes = 3;
    } else {
      // Outside the bounds of UTF-8
      return false;
    }
    bytes++;
    while (num_bytes--) {
      // Check if 0b10xxxxxx
      if ((*bytes & 0xc0) != 0x80) {
        return false;
      }
      codepoint = (codepoint << 6) | (*bytes & 0xdf);
      bytes++;
    }
    // Check if it's within the UTF-16 surrogates range
    if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
      return false;
    }
  }
  return true;
}

bool string_init(struct String* string, const char* c_string) {
  if (!validate_null_terminated_utf8((const uint8_t*) c_string)) {
    return false;
  }
  // It's okay to run `strlen` on UTF-8
  size_t length = strlen(c_string);
  string->data = malloc(length);
  string->capacity = string->length = length;
  memcpy(string->data, c_string, length);
  return true;
}

void string_fini(struct String* string) {
  if (string->capacity != 0) {
    free(string->data);
  }
  string->data = NULL;
  string->capacity = string->length = 0;
}
