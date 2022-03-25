#include "string.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "writer.h"

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

static void string_resize_to_atleast(struct String* string, size_t capacity) {
  CHECK(capacity > string->capacity);
  // Round up to a power of 2
  capacity--;
  capacity |= capacity >> 1;
  capacity |= capacity >> 2;
  capacity |= capacity >> 4;
  capacity |= capacity >> 8;
  capacity |= capacity >> 16;
  if (sizeof(size_t) == 8) {
    capacity |= capacity >> 32;
  }
  capacity++;
  // Reallocate
  if (!(string->data = realloc(string->data, capacity))) {
    DIE_ERR("realloc()");
  }
  string->capacity = capacity;
}

bool string_init(struct String* string, const char* c_string) {
  // It's okay to run `strlen` on UTF-8
  size_t length = strlen(c_string);
  if (length == 0) {
    string->data = NULL;
    string->length = string->capacity = 0;
    return true;
  }
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

int str_print(const struct Str* a, struct Writer* writer) {
  return writer->writef(writer, "%.*s", (int) a->length, (const char*) a->data);
}

static int string_vwritef(struct Writer* writer, const char *fmt, va_list ap) {
  struct StringWriter* string_writer = (struct StringWriter*) writer;
  struct String* string = string_writer->string;
  va_list args;
  va_copy(args, ap);
  char* ptr = (char*) string->data + string->length;
  int remaining = (int) (string->capacity - string->length); // I know, I know...
  int ret;

  if ((ret = vsnprintf(ptr, remaining, fmt, ap)) >= remaining) {
    string_resize_to_atleast(string, string->length + ret + 1);
    ptr = (char*) string->data + string->length;
    remaining = (int) (string->capacity - string->length);
    ret = vsnprintf(ptr, remaining, fmt, args);
  }
  va_end(ap);
  va_end(args);

  // Validate UTF-8
  if (!validate_null_terminated_utf8((const uint8_t*) ptr, ret)) {
    return -1;
  }
  string->length += ret;
  return ret;
}

static int string_flush(struct Writer* writer) {
  // Nothing to do
  (void) writer;
  return 0;
}

struct StringWriter* string_writer_create(struct String* string) {
  struct StringWriter* writer = malloc(sizeof(struct StringWriter));
  if (!writer) {
    DIE_ERR("malloc()");
  }
  writer->writer.writef = DEFAULT_WRITEF;
  writer->writer.vwritef = string_vwritef;
  writer->writer.flush = string_flush;
  writer->string = string;
  return writer;
}

void string_writer_free(struct StringWriter* string_writer) {
  free(string_writer);
}
