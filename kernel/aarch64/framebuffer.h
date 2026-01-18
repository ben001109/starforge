#pragma once
#include <stdint.h>

typedef struct {
    uint64_t base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} Framebuffer;

static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | b;
}

void fb_clear(Framebuffer* fb, uint32_t color);
void fb_draw_rect(Framebuffer* fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_test_pattern(Framebuffer* fb);
