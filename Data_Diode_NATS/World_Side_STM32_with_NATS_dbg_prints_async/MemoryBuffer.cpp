#include "MemoryBuffer.h"

void membuf_init(struct MemBuffer *buf) {
  buf->length = 0;
  buf->pos = 0;
  membuf_ensure(buf, 0);
}

void membuf_ensure(struct MemBuffer *buf, size_t length) {
  if (buf->length < length) {
    buf->data = (uint8_t *)realloc(buf->data, buf->length + MEMBUF_GROW_BYTES);
    buf->length += MEMBUF_GROW_BYTES;
  }
}

void membuf_drop(struct MemBuffer *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->length = 0;
}

// Prepare the buffer for new data, returning the pointer to copy said data to
uint8_t *membuf_add(struct MemBuffer *buf, size_t nlen) {
  membuf_ensure(buf, buf->pos + nlen + 1);

  uint8_t *p = &buf->data[buf->pos];
  buf->pos += nlen;

  p[nlen] = '\0'; // Make buffer printf safe just in case

  return p;
}

// Consume the first len bytes and return a copy of it.
// Consumer must free the memory
uint8_t *membuf_consume(struct MemBuffer *buf, size_t len) {
  uint8_t *p = (uint8_t *)malloc(len * sizeof(uint8_t));
  memcpy(p, buf->data, len);

  memmove(buf->data, &buf->data[len], buf->pos - len);

  buf->pos -= len;
  buf->data[buf->pos] = '\0';

  return p;
}

// Get a line from the buffer. Return a NULL terminated *copy*
// Consumer must free the memory
char *membuf_getline(struct MemBuffer *buf) {
  // Look for end of line
  uint8_t *end = (uint8_t *)memchr(buf->data, '\n', buf->pos);

  // If no line, return NULL
  if (end == NULL) {
    return NULL;
  }

  // Length of line
  size_t lineLen = end - buf->data + 1; // Include the delimiter
  //
  // Replace the delimiter with a null character
  char *line = (char *)membuf_consume(buf, lineLen);
  line[lineLen - 1] = '\0';

  return line;
}
