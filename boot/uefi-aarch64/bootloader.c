void uart_putc(char c) {
    volatile char *uart = (volatile char *)0x09000000;
    *uart = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

void halt() {
    while (1) {
        __asm__ volatile("wfi");
    }
}

void _start() {
    uart_puts("AARCH64 UEFI OK\n");
    halt();
}
