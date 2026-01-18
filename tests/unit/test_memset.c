#include <assert.h>
#include <stdint.h>

extern void *memset(void *s, int c, unsigned long n);

int main(void) {
  uint8_t buf[8] = {0};
  memset(buf, 0xAA, sizeof(buf));
  assert(buf[0] == 0xAA);
  return 0;
}