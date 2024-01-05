#include "MemoryBuffer.h"

void membuf_init(struct MemBuffer *buf) {
  buf->length = 500;
  buf->pos = 0;
  buf->data = (uint8_t *)calloc(500, sizeof(uint8_t));
}

void membuf_ensure(struct MemBuffer *buf, size_t length) {
  if (buf->length < length) {
    buf->data = (uint8_t *)realloc(buf->data, length + MEMBUF_GROW_BYTES);
    buf->length = length + MEMBUF_GROW_BYTES;
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

  p[nlen] = '\0';  // Make buffer printf safe just in case

  return p;
}

// Push bytes to the end of a MemBuffer
void membuf_push(struct MemBuffer *buf, const void *data, size_t nlen) {
  membuf_ensure(buf, buf->pos + nlen + 1);

  memcpy(&buf->data[buf->pos], data, nlen);
  buf->pos += nlen;

  buf->data[buf->pos] = '\0';  // Make buffer printf safe just in case
}

// Prepare the buffer to add a new string, and then add the string
void membuf_str(struct MemBuffer *buf, const char *str) {
  size_t len = strlen(str);

  memcpy(membuf_add(buf, len), str, len);
}

// Drop the first len bytes from the buffer
size_t membuf_shift(struct MemBuffer *buf, size_t len) {
  memmove(buf->data, &buf->data[len], buf->pos - len);

  buf->pos -= len;
  buf->data[buf->pos] = '\0';

  return membuf_size(buf);
}

// Delete everything from buffer
void membuf_clear(struct MemBuffer *buf) {
  buf->pos = 0;
  buf->data[0] = '\0';
}

// Number of bytes currently stored in buffer
size_t membuf_size(struct MemBuffer *buf) {
  return buf->pos;
}

// Take the first len bytes and return a copy of it.
// Consumer must free the memory
void membuf_take(void *dest, struct MemBuffer *buf, size_t len) {
  // Copy data to destination
  memcpy(dest, buf->data, len);

  // Shift the buffer down
  membuf_shift(buf, len);
}

// Read first byte in buffer
// Note: This is not very efficient
uint8_t membuf_first(struct MemBuffer *buf) {
  uint8_t my_byte = buf->data[0];

  membuf_shift(buf, 1);

  return my_byte;
}

// Pointer to frst byte of underlying storage
uint8_t *membuf_head(struct MemBuffer *buf) {
  return buf->data;
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
  size_t lineLen = end - buf->data + 1;  // Include the delimiter

  char *line = (char *)malloc(lineLen * sizeof(char));
  membuf_take(line, buf, lineLen);

  if (line[lineLen - 2] == '\r') {
    // Replace the delimiter with a null character
    line[lineLen - 2] = '\0';
  } else {
    line[lineLen - 1] = '\0';
  }

  return line;
}
