#include "string.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Checks if the sequence of bytes provided as argument is valid UTF-8. This
// assumes that the sequence of bytes is NULL-terminated.
static bool validate_null_terminated_utf8(const uint8_t* bytes, size_t length) {
  size_t i = 0;
  while (i < length) {
    if (bytes[i] <= 0x7f) {
      // This is regular ASCII
      i++;
      continue;
    }
    codepoint_t codepoint;
    size_t num_bytes;
    if ((bytes[i] & 0xe0) == 0xc0) {
      // 0b110xxxxx 0b10xxxxxx
      codepoint = bytes[i] & 0x1f;
      num_bytes = 1;
    } else if ((bytes[i] & 0xf0) == 0xe0) {
      // 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
      codepoint = bytes[i] & 0x0f;
      num_bytes = 2;
    } else if ((bytes[i] & 0xf8) == 0xf0) {
      // 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
      codepoint = bytes[i] & 0x07;
      num_bytes = 3;
    } else {
      // Outside the bounds of UTF-8
      return false;
    }
    i++;
    while (num_bytes--) {
      // Check if 0b10xxxxxx
      if ((bytes[i] & 0xc0) != 0x80) {
        return false;
      }
      codepoint = (codepoint << 6) | (bytes[i] & 0xdf);
      i++;
    }
    // Check if it's within the UTF-16 surrogates range
    if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
      return false;
    }
  }
  return true;
}

bool string_init(struct String* string, const char* c_string) {
  // It's okay to run `strlen` on UTF-8
  size_t length = strlen(c_string);
  if (!validate_null_terminated_utf8((const uint8_t*) c_string, length)) {
    return false;
  }
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

bool str_init(struct Str* str, const char* c_string, size_t length) {
  if (length == SIZE_MAX) {
    length = strlen(c_string);
  }
  if (!validate_null_terminated_utf8((const uint8_t*) c_string, length)) {
    return false;
  }
  str->data = (const uint8_t*) c_string;
  str->length = length;
  return true;
}

bool str_equal(const struct Str* a, const struct Str* b) {
  if (a->length != b->length) {
    return false;
  }
  return !memcmp(a->data, b->data, a->length);
}

int str_fprint(const struct Str* a, FILE* file) {
  fprintf(file, "%*.s", (int) a->length, (const char*) a->data);
  return a->length;
}
