#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bs.h"

struct LineBuffer {
  char *data;
  size_t size;
  size_t capacity;
};

static void line_buffer_init(struct LineBuffer* buffer) {
  buffer->data = NULL;
  buffer->size = 0;
  buffer->capacity = 0;
}

static void line_buffer_clear(struct LineBuffer* buffer) {
  buffer->size = 0;
}

static void line_buffer_fini(struct LineBuffer* buffer) {
  free(buffer->data);
}

static void line_buffer_resize_if_required(struct LineBuffer* buffer) {
  if (buffer->capacity <= buffer->size + 8) {
    buffer->capacity = buffer->capacity == 0 ? 128 : buffer->capacity * 2;
    if (!(buffer->data = realloc(buffer->data, buffer->capacity))) {
      perror("realloc()");
      exit(1);
    }
  }
}

static void line_buffer_read(struct LineBuffer* buffer, FILE* stream) {
  while (true) {
    line_buffer_resize_if_required(buffer);
    if (!fgets(buffer->data + buffer->size, buffer->capacity - buffer->size, stream)) {
      if (ferror(stream)) {
        perror("fgets()");
        exit(1);
      }
      return;
    }
    while (buffer->data[buffer->size]) {
      buffer->size++;
    }
    if (buffer->data[buffer->size - 1] == '\n') {
      return;
    }
  }
}

static int repl() {
  struct LineBuffer buffer;
  line_buffer_init(&buffer);
  const char *prompt = ">>>";
  while (true) {
    printf("%s ", prompt);
    fflush(stdout);
    line_buffer_read(&buffer, stdin);
    enum BsStatus status = bs_interpret(buffer.data);
    switch (status) {
    case BS_Ok:
      prompt = ">>>";
      line_buffer_clear(&buffer);
      break;
    case BS_Error:
      prompt = ">>>";
      line_buffer_clear(&buffer);
      break;
    case BS_Incomplete:
      prompt = "...";
      break;
    }
  }
  line_buffer_fini(&buffer);
  return 0;
}

int main(int argc, char *const *argv) {
  return repl();
}
