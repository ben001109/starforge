#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
  __asm__ __volatile__ ("outb %0,%1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
  uint8_t ret; __asm__ __volatile__ ("inb %1,%0" : "=a"(ret) : "Nd"(port)); return ret;
}

void *memset(void *s, int c, unsigned long n) {
  unsigned char *p = s; while (n--) *p++ = (unsigned char)c; return s;
}

static void serial_init(void) {
  outb(0x3F8 + 1, 0x00);
  outb(0x3F8 + 3, 0x80);
  outb(0x3F8 + 0, 0x03);
  outb(0x3F8 + 1, 0x00);
  outb(0x3F8 + 3, 0x03);
  outb(0x3F8 + 2, 0xC7);
  outb(0x3F8 + 4, 0x0B);
}

static int serial_tx_ready(void) { return inb(0x3F8 + 5) & 0x20; }
static void serial_putc(char c) { while(!serial_tx_ready()); outb(0x3F8, (uint8_t)c); }
void serial_write(const char* s){ static int initd=0; if(!initd){serial_init(); initd=1;} for (; *s; ++s) serial_putc(*s); }

void halt_forever(void){ for(;;) __asm__ __volatile__("hlt"); }
