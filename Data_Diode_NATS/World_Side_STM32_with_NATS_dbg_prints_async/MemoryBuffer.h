// License: MIT
// Author: Andrew Balmos <abalmos@purdue.edu>
#ifndef MEMORY_BUFFER_H
#define MEMORY_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef MEMBUF_GROW_BYTES
// The number of bytes to a grow a too short buffer by
#define MEMBUF_GROW_BYTES 50
#endif

struct MemBuffer {
  uint8_t *data; // Buffer space
  size_t length; // End of allocated buffer space
  size_t pos;    // End of current data
};

//////
// Buffer management
//////
void membuf_init(struct MemBuffer *buf);
void membuf_drop(struct MemBuffer *buf);

// Expand the internal buffer to have at least length bytes
void membuf_ensure(struct MemBuffer *buf, size_t length);
// Prepare the buffer for new data, returning the pointer to copy said data to
uint8_t *membuf_add(struct MemBuffer *buf, size_t nlen);
// Prepare the buffer for data and then add the string
void membuf_str(struct MemBuffer *buf, const char *str);
// Prepare the buffer and copy said data
void membuf_cpy(struct MemBuffer *buf, const void *data, size_t nlen);
// Drop the first len bytes from the buffer
size_t membuf_shift(struct MemBuffer *buf, size_t len);
// Delete everything from buffer
void membuf_clear(struct MemBuffer *buf);
// Number of bytes currently stored in buffer
size_t membuf_size(struct MemBuffer *buf);

/////
// Buffer consumption
/////

// Consume the first len bytes and return a copy of it.
// Consumer must free the memory
void membuf_take(void *dest, struct MemBuffer *buf, size_t len);

// Read first byte in buffer
// Note: This is not very efficient
uint8_t membuf_first(struct MemBuffer *buf);

// Pointer to frst byte of underlying storage
uint8_t *membuf_head(struct MemBuffer *buf);

// Get a line from the buffer. Return a NULL terminated *copy*
// Consumer must free the memory
char *membuf_getline(struct MemBuffer *buf);

#endif
