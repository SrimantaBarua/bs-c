#ifndef __BS_STRING_H__
#define __BS_STRING_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define codepoint_t uint32_t

// Growable UTF-8 string
struct String {
  uint8_t* data;   // Pointer to a buffer that holds the actual string data
  size_t length;   // Length of the string in bytes
  size_t capacity; // Capacity of the buffer
};

// Initialize a new string wrapping the given C-style string. This verifies if
// the C string is valid UTF-8. If valid, it returns `true`, else it returns
// `false`. If it returns `false`, the state of the string is undefined.
bool string_init(struct String* string, const char* c_string);

// Free memory for a string
void string_fini(struct String* string);

#endif  // __BS_STRING_H__
