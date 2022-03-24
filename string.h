#ifndef __BS_STRING_H__
#define __BS_STRING_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

// Non-owning slice of a UTF-8 string
struct Str {
  const uint8_t* data; // Pointer to the start of the string slice
  size_t length;       // Length of the string in bytes
};

// Initialize string slice given pointer to start of UTF-8 data, and length of
// the slice. This validates the data (checks if it's valid UTF-8). If not, it
// return false. If the supplied length is SIZE_MAX, the c_string is assumed to
// be null-terminated, and the full length is used.
bool str_init(struct Str* str, const char* c_string, size_t length);

// Check if two string slices are equal. This is O(n) - it checks every byte.
bool str_equal(const struct Str* a, const struct Str* b);

// Print str out to given FILE*
int str_fprint(const struct Str* a, FILE* file);

#endif  // __BS_STRING_H__
