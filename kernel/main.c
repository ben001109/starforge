#include <stdint.h>
#include "bootinfo.h"

void serial_write(const char* s);
__attribute__((noreturn)) void halt_forever(void);
void *memset(void *s, int c, unsigned long n);

#ifdef __aarch64__
void uart_putc(char c);
void uart_puts(const char *s);

void serial_write(const char* s) {
    uart_puts(s);
}
#else
void serial_write(const char* s);
#endif

static inline uint32_t pack_rgb(uint8_t r,uint8_t g,uint8_t b){ return (r<<16)|(g<<8)|b; }

__attribute__((noreturn))
void kmain(BootInfo* bi) {
    serial_write("[KERNEL] hello from Starforge kernel!\n");

#ifndef __aarch64__
    uint32_t* fb = (uint32_t*)(uintptr_t)bi->fb_base;
    uint32_t pitch_px = bi->fb_pitch / 4;
    uint32_t color = bi->fb_format ? pack_rgb(0x20,0x20,0xC0) : pack_rgb(0xC0,0x20,0x20);
    for (uint32_t y=0; y<bi->fb_height; ++y) {
        for (uint32_t x=0; x<bi->fb_width; ++x) {
            fb[y*pitch_px + x] = color;
        }
    }
    for (uint32_t x=10; x< (bi->fb_width-10); ++x)
        fb[(bi->fb_height/2)*pitch_px + x] = pack_rgb(0xFF,0xFF,0xFF);

    serial_write("[KERNEL] framebuffer painted. halting.\n");
#endif

    halt_forever();
}
