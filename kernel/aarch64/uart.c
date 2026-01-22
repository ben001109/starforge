#include <stdint.h>

#define PL011_BASE 0x09000000

#define PL011_DR   (*(volatile uint32_t*)(PL011_BASE + 0x00))
#define PL011_FR   (*(volatile uint32_t*)(PL011_BASE + 0x18))
#define PL011_FR_TXFF (1 << 5)

void uart_putc(char c) {
    while (PL011_FR & PL011_FR_TXFF);
    PL011_DR = (uint32_t)c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}
