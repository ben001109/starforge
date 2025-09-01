#pragma once
#include <stdint.h>

typedef struct {
  uint64_t fb_base;      // framebuffer physical address
  uint32_t fb_width;
  uint32_t fb_height;
  uint32_t fb_pitch;     // bytes per scanline
  uint32_t fb_bpp;       // bits per pixel
  uint32_t fb_format;    // 0=RGB,1=BGR (simplified)

  uint64_t mmap;         // UEFI memory map physical address
  uint64_t mmap_size;
  uint64_t mmap_desc_size;
  uint32_t mmap_desc_ver;
} BootInfo;
