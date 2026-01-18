#pragma once
#include <stdint.h>

typedef struct {
    uint64_t uart_base;
    uint64_t fb_base;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
} DTBInfo;

int dtb_parse(const void* dtb, DTBInfo* info);
