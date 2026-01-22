#include <stdint.h>
#include "../bootinfo.h"
#include "dtb.h"

extern void uart_puts(const char *s);
extern void __attribute__((noreturn)) kmain(BootInfo* bi);

__attribute__((noreturn)) void halt_forever(void) {
    while (1) {
        __asm__ volatile("wfi");
    }
}

__attribute__((noreturn)) void dtb_boot_entry(void) {
    uart_puts("[DTB] Starting bare-metal boot\n");
    
    DTBInfo dtb_info;
    
    const void* dtb = (const void*)0x40000000;
    
    if (dtb_parse(dtb, &dtb_info) == 0) {
        if (dtb_info.uart_base != 0) {
            uart_puts("[DTB] UART found at 0x");
            char buf[20];
            int i = 0;
            uint64_t addr = dtb_info.uart_base;
            for (int shift = 60; shift >= 0; shift -= 4) {
                int nibble = (addr >> shift) & 0xF;
                if (nibble < 10) buf[i++] = '0' + nibble;
                else buf[i++] = 'A' + (nibble - 10);
            }
            buf[i] = '\0';
            uart_puts(buf);
            uart_puts("\n");
        } else {
            uart_puts("[DTB] No UART found\n");
        }
        
        if (dtb_info.fb_base != 0) {
            uart_puts("[DTB] Framebuffer found\n");
        } else {
            uart_puts("[DTB] No framebuffer found\n");
        }
        
        uart_puts("AARCH64 BARE OK\n");
    } else {
        uart_puts("[DTB] DTB parse failed\n");
    }
    
    BootInfo bi = {
        .fb_base = dtb_info.fb_base,
        .fb_width = dtb_info.fb_width,
        .fb_height = dtb_info.fb_height,
        .fb_pitch = dtb_info.fb_pitch,
        .fb_bpp = dtb_info.fb_bpp,
        .fb_format = 1,
        .mmap = 0,
        .mmap_size = 0,
        .mmap_desc_size = 0,
        .mmap_desc_ver = 0
    };
    
    kmain(&bi);
}
