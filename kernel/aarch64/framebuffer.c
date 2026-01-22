#include "framebuffer.h"

void fb_clear(Framebuffer* fb, uint32_t color) {
    uint32_t* pixels = (uint32_t*)(uintptr_t)fb->base;
    uint32_t pitch_px = fb->pitch / 4;
    
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            pixels[y * pitch_px + x] = color;
        }
    }
}

void fb_draw_rect(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t* pixels = (uint32_t*)(uintptr_t)fb->base;
    uint32_t pitch_px = fb->pitch / 4;
    
    for (uint32_t dy = 0; dy < h && (y + dy) < fb->height; dy++) {
        for (uint32_t dx = 0; dx < w && (x + dx) < fb->width; dx++) {
            pixels[(y + dy) * pitch_px + (x + dx)] = color;
        }
    }
}

void fb_draw_test_pattern(Framebuffer* fb) {
    uint32_t bar_width = fb->width / 4;
    
    uint32_t colors[] = {
        pack_rgb(0xFF, 0x00, 0x00),
        pack_rgb(0x00, 0xFF, 0x00),
        pack_rgb(0x00, 0x00, 0xFF),
        pack_rgb(0xFF, 0xFF, 0x00)
    };
    
    for (int i = 0; i < 4; i++) {
        fb_draw_rect(fb, i * bar_width, 0, bar_width, fb->height / 2, colors[i]);
    }
    
    fb_draw_rect(fb, 0, fb->height / 2, fb->width, 1, pack_rgb(0xFF, 0xFF, 0xFF));
}
