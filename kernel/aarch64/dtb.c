#include "dtb.h"
#include <stddef.h>

#define NULL ((void*)0)

#define FDT_MAGIC 0xd00dfeed

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

#define FDT_BEGIN_NODE 0x1
#define FDT_END_NODE 0x2
#define FDT_PROP 0x3
#define FDT_NOP 0x4
#define FDT_END 0x9

static const uint32_t* dtb_strings;
static const char* dtb_data;
static uint32_t dtb_size;

static uint32_t be32toh(uint32_t val) {
    return ((val >> 24) & 0xFF) |
           ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) |
           ((val << 24) & 0xFF000000);
}

static const char* get_string(uint32_t offset) {
    return (const char*)dtb_strings + offset;
}

static const uint32_t* parse_prop(const uint32_t* p, const char* name,
                                   uint32_t* len, const void** value) {
    uint32_t prop_len = be32toh(*p++);
    uint32_t nameoff = be32toh(*p++);
    
    if (len) *len = prop_len;
    if (value) *value = p;
    
    p = (const uint32_t*)((const char*)p + ((prop_len + 3) & ~3));
    
    const char* prop_name = get_string(nameoff);
    if (prop_name && (prop_name[0] == '\0' || 
        (name && (prop_name[0] == name[0]) && 
         __builtin_strcmp(prop_name, name) == 0))) {
        return p;
    }
    
    return p;
}

static void scan_for_uart(const uint32_t* p, DTBInfo* info) {
    while (*p != FDT_END) {
        uint32_t token = be32toh(*p++);
        
        if (token == FDT_BEGIN_NODE) {
            const char* node_name = (const char*)p;
            p = (const uint32_t*)((const char*)p + __builtin_strlen(node_name) + 1);
            
            if (__builtin_strstr(node_name, "uart@") != NULL) {
                uint64_t reg_base = 0;
                
                while (*p != FDT_END_NODE) {
                    uint32_t prop_token = be32toh(*p++);
                    
                    if (prop_token == FDT_PROP) {
                        uint32_t len;
                        const void* value;
                        p = parse_prop(p, NULL, &len, &value);
                        
                        if (len >= 16) {
                            const uint32_t* v = (const uint32_t*)value;
                            reg_base = ((uint64_t)be32toh(v[0]) << 32) | be32toh(v[1]);
                        }
                    } else if (prop_token == FDT_END_NODE) {
                        break;
                    }
                }
                
                if (reg_base != 0) {
                    info->uart_base = reg_base;
                }
            } else {
                scan_for_uart(p, info);
            }
        } else if (token == FDT_PROP) {
            uint32_t len;
            const void* value;
            p = parse_prop(p, NULL, &len, &value);
        } else if (token == FDT_END_NODE) {
            return;
        }
    }
}

static void scan_for_framebuffer(const uint32_t* p, DTBInfo* info) {
    while (*p != FDT_END) {
        uint32_t token = be32toh(*p++);
        
        if (token == FDT_BEGIN_NODE) {
            const char* node_name = (const char*)p;
            p = (const uint32_t*)((const char*)p + __builtin_strlen(node_name) + 1);
            
            if (__builtin_strstr(node_name, "simple-framebuffer") != NULL) {
                uint64_t reg_base = 0;
                uint32_t width = 0, height = 0, stride = 0;
                
                while (*p != FDT_END_NODE) {
                    uint32_t prop_token = be32toh(*p++);
                    
                    if (prop_token == FDT_PROP) {
                        uint32_t len;
                        const void* value;
                        const char* prop_name = (const char*)dtb_strings + be32toh(p[1]);
                        p = parse_prop(p, NULL, &len, &value);
                        
                        if (len >= 16 && __builtin_strcmp(prop_name, "reg") == 0) {
                            const uint32_t* v = (const uint32_t*)value;
                            reg_base = ((uint64_t)be32toh(v[0]) << 32) | be32toh(v[1]);
                        } else if (len >= 4 && __builtin_strcmp(prop_name, "width") == 0) {
                            const uint32_t* v = (const uint32_t*)value;
                            width = be32toh(v[0]);
                        } else if (len >= 4 && __builtin_strcmp(prop_name, "height") == 0) {
                            const uint32_t* v = (const uint32_t*)value;
                            height = be32toh(v[0]);
                        } else if (len >= 4 && __builtin_strcmp(prop_name, "stride") == 0) {
                            const uint32_t* v = (const uint32_t*)value;
                            stride = be32toh(v[0]);
                        }
                    } else if (prop_token == FDT_END_NODE) {
                        break;
                    }
                }
                
                if (reg_base != 0 && width != 0 && height != 0) {
                    info->fb_base = reg_base;
                    info->fb_width = width;
                    info->fb_height = height;
                    info->fb_pitch = stride ? stride : width * 4;
                    info->fb_bpp = 32;
                }
            } else {
                scan_for_framebuffer(p, info);
            }
        } else if (token == FDT_PROP) {
            uint32_t len;
            const void* value;
            p = parse_prop(p, NULL, &len, &value);
        } else if (token == FDT_END_NODE) {
            return;
        }
    }
}

int dtb_parse(const void* dtb, DTBInfo* info) {
    if (!dtb || !info) return -1;
    
    const struct fdt_header* hdr = (const struct fdt_header*)dtb;
    
    if (be32toh(hdr->magic) != FDT_MAGIC) return -1;
    
    dtb_size = be32toh(hdr->totalsize);
    dtb_strings = (const uint32_t*)((const char*)dtb + be32toh(hdr->off_dt_strings));
    dtb_data = (const char*)dtb + be32toh(hdr->off_dt_struct);
    
    __builtin_memset(info, 0, sizeof(DTBInfo));
    
    const uint32_t* p = (const uint32_t*)dtb_data;
    
    scan_for_uart(p, info);
    scan_for_framebuffer(p, info);
    
    return 0;
}
