#include "MemoryBuffer.h"
#include <stdio.h>

struct MemBuffer buffer;

int main() {
  membuf_init(&buffer);

  uint8_t *p = membuf_add(&buffer, 20);

  memcpy(p, "Test 1\nTest 2\nTest 3", 20);

  printf("Starting buffer (%d of %d):\n%s\n----\n", buffer.pos, buffer.length,
         buffer.data);

  // Test 1: get line
  char *line = membuf_getline(&buffer);
  if (strcmp(line, "Test 1") == 0) {
    printf("[Ok]: Line is correct\n");
  } else {
    printf("[Err]: '%s == Test 1'?\n", line);
  }
  free(line);

  // Test 2: get line
  line = membuf_getline(&buffer);
  if (strcmp(line, "Test 2") == 0) {
    printf("[Ok]: Line is correct\n");
  } else {
    printf("[Err]: '%s == Test 2'?\n", line);
  }
  free(line);

  // Test 3: no lone to get
  line = membuf_getline(&buffer);

  if (line == NULL && strcmp((char *)buffer.data, "Test 3") == 0) {
    printf("[OK]: No new line. Left over correct.\n");
  } else {
    printf("[Err]: New line or wrong left over? '%s == Test 3'?\n",
           buffer.data);
  }
  free(line);

  // Test 4: Add data
  p = membuf_add(&buffer, 11);
  memcpy(p, "\nTest 4\nabc", 11);

  if (strcmp((char *)buffer.data, "Test 3\nTest 4\nabc") == 0) {
    printf("[OK]: New data added.\n");
  } else {
    printf("[Err]: New data added wrong? '%s == Test 3\nTest4\nabc'?\n",
           buffer.data);
  }

  // Test 5: get line
  line = membuf_getline(&buffer);
  if (strcmp(line, "Test 3") == 0) {
    printf("[Ok]: Line is correct\n");
  } else {
    printf("[Err]: '%s == Test 3'?\n", line);
  }
  free(line);

  // Test 6: get line
  line = membuf_getline(&buffer);
  if (strcmp(line, "Test 4") == 0) {
    printf("[Ok]: Line is correct\n");
  } else {
    printf("[Err]: '%s == Test 4'?\n", line);
  }
  free(line);

  // Test 3: no lone to get
  line = membuf_getline(&buffer);

  if (line == NULL && strcmp((char *)buffer.data, "abc") == 0) {
    printf("[OK]: No new line. Left over correct.\n");
  } else {
    printf("[Err]: New line or wrong left over? '%s == abc'?\n", buffer.data);
  }
  free(line);

  return 0;
}
