#include <efi.h>
#include <efilib.h>
#include "elf.h"
#include "../../kernel/bootinfo.h"

static EFI_STATUS load_file(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* ST, CHAR16* path,
                            void** buffer, UINTN* size) {
    EFI_STATUS st;
    EFI_LOADED_IMAGE* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_FILE_PROTOCOL* root;
    EFI_FILE_PROTOCOL* file;

    st = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&loaded_image);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(root->Open, 5, root, &file, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    EFI_FILE_INFO* finfo;
    UINTN infosz = SIZE_OF_EFI_FILE_INFO + 256;
    st = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, infosz, (void**)&finfo);
    if (EFI_ERROR(st)) return st;

    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    st = uefi_call_wrapper(file->GetInfo, 4, file, &fi_guid, &infosz, finfo);
    if (EFI_ERROR(st)) return st;

    *size = finfo->FileSize;
    st = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, *size, buffer);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(file->Read, 3, file, size, *buffer);
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(ST->BootServices->FreePool, 1, finfo);
    return st;
}

static EFI_STATUS get_gop(EFI_SYSTEM_TABLE* ST, EFI_GRAPHICS_OUTPUT_PROTOCOL** out) {
    EFI_STATUS st;
    st = uefi_call_wrapper(ST->BootServices->LocateProtocol, 3, &gEfiGraphicsOutputProtocolGuid, NULL, (void**)out);
    return st;
}

static inline UINT64 align_down(UINT64 x, UINT64 a) {
    if (a == 0) return x;
    return x & ~(a - 1);
}

static inline UINT64 align_up(UINT64 x, UINT64 a) {
    if (a == 0) return x;
    return (x + a - 1) & ~(a - 1);
}

static EFI_STATUS load_elf_segment(EFI_SYSTEM_TABLE* ST, void* kbuf, Elf64_Phdr* seg, UINT16 index) {
    if (seg->p_type != PT_LOAD) return EFI_SUCCESS;

    UINT64 base = seg->p_paddr ? seg->p_paddr : seg->p_vaddr;
    UINT64 memsz = seg->p_memsz;
    UINT64 filesz = seg->p_filesz;
    UINT64 palign = seg->p_align ? seg->p_align : 4096; // default alignment
    // TODO: strict page alignment policy — optionally align only when p_align>0.

    // Ensure at least page alignment for AllocatePages
    if (palign < 4096) palign = 4096;
    // Guard: filesz should not exceed memsz
    if (filesz > memsz) filesz = memsz;

    UINT64 aligned_start = align_down(base, palign);
    UINT64 offset_into_segment = base - aligned_start;
    UINT64 total_bytes = offset_into_segment + memsz;

    UINTN pages = (UINTN)align_up(total_bytes, 4096) / 4096;
    EFI_PHYSICAL_ADDRESS alloc = (EFI_PHYSICAL_ADDRESS)aligned_start;
    EFI_STATUS st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &alloc);
    if (EFI_ERROR(st)) {
        Print(L"[BL] AllocatePages seg %d @%lx size=%lx failed: %r\n", index, aligned_start, (UINT64)pages*4096, st);
        return st;
    }

    // Zero-fill the entire allocated span, then copy file contents to the intended base
    SetMem((void*)(UINTN)aligned_start, (UINTN)pages * 4096, 0);
    CopyMem((void*)(UINTN)base, (UINT8*)kbuf + seg->p_offset, (UINTN)filesz);
    if (memsz > filesz) {
        // 明確零填充尾段 [filesz, memsz)
        SetMem((void*)((UINTN)base + (UINTN)filesz), (UINTN)(memsz - filesz), 0);
    }

    Print(L"[BL] load seg %d base=%lx align=%lx pages=%lx memsz=%lx filesz=%lx\n",
          index, base, palign, (UINT64)pages, (UINT64)memsz, (UINT64)filesz);
    return EFI_SUCCESS;
}

static EFI_STATUS exit_boot_services_with_retry(
    EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* ST,
    EFI_PHYSICAL_ADDRESS* out_map_pa,
    UINTN* out_map_size,
    UINTN* out_desc_size,
    UINT32* out_desc_ver) {
    // TODO: make retry attempts configurable (currently single retry, 2 attempts total)
    EFI_STATUS st;
    UINTN key = 0, desc_size = 0; UINT32 desc_ver = 0;

    EFI_PHYSICAL_ADDRESS map_pa; UINTN pages; UINTN mm_size; EFI_STATUS ex;

    // Attempt 1: allocate with +4096 slack
    {
        UINTN req = 0; key = 0; desc_size = 0; desc_ver = 0;
        st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &req, NULL, &key, &desc_size, &desc_ver);
        if (st != EFI_BUFFER_TOO_SMALL) return st;

        UINTN slack = desc_size + 4096; // ensure >= +4096 extra
        pages = (req + slack + 4095) / 4096;
        map_pa = 0;
        st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &map_pa);
        if (EFI_ERROR(st)) return st;

        while (1) {
            mm_size = pages * 4096;
            st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mm_size, (VOID*)(UINTN)map_pa, &key, &desc_size, &desc_ver);
            if (st == EFI_BUFFER_TOO_SMALL) {
                uefi_call_wrapper(ST->BootServices->FreePages, 2, map_pa, pages);
                pages = (mm_size + desc_size + slack + 4095) / 4096;
                st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &map_pa);
                if (EFI_ERROR(st)) return st;
                continue;
            }
            if (EFI_ERROR(st)) { uefi_call_wrapper(ST->BootServices->FreePages, 2, map_pa, pages); return st; }
            break;
        }

        ex = uefi_call_wrapper(ST->BootServices->ExitBootServices, 2, ImageHandle, key);
        if (ex == EFI_SUCCESS) { *out_map_pa = map_pa; *out_map_size = mm_size; *out_desc_size = desc_size; *out_desc_ver = desc_ver; return EFI_SUCCESS; }
        // Free buffer to avoid leak before retry
        uefi_call_wrapper(ST->BootServices->FreePages, 2, map_pa, pages);
        if (ex != EFI_INVALID_PARAMETER) { /* TODO: error code table for diagnostics */ return ex; }
    }

    // Attempt 2: retry once, allocate with larger slack (>= +4096)
    {
        UINTN req = 0; key = 0; desc_size = 0; desc_ver = 0;
        st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &req, NULL, &key, &desc_size, &desc_ver);
        if (st != EFI_BUFFER_TOO_SMALL) return st;

        UINTN slack = desc_size + 8192; // larger slack for retry
        pages = (req + slack + 4095) / 4096;
        map_pa = 0;
        st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &map_pa);
        if (EFI_ERROR(st)) return st;

        while (1) {
            mm_size = pages * 4096;
            st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mm_size, (VOID*)(UINTN)map_pa, &key, &desc_size, &desc_ver);
            if (st == EFI_BUFFER_TOO_SMALL) {
                uefi_call_wrapper(ST->BootServices->FreePages, 2, map_pa, pages);
                pages = (mm_size + desc_size + slack + 4095) / 4096;
                st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &map_pa);
                if (EFI_ERROR(st)) return st;
                continue;
            }
            if (EFI_ERROR(st)) { uefi_call_wrapper(ST->BootServices->FreePages, 2, map_pa, pages); return st; }
            break;
        }

        ex = uefi_call_wrapper(ST->BootServices->ExitBootServices, 2, ImageHandle, key);
        if (ex == EFI_SUCCESS) { *out_map_pa = map_pa; *out_map_size = mm_size; *out_desc_size = desc_size; *out_desc_ver = desc_ver; return EFI_SUCCESS; }
        // Final failure after one retry
        uefi_call_wrapper(ST->BootServices->FreePages, 2, map_pa, pages);
        return ex;
    }
}

typedef void (*KernelEntry)(BootInfo*);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    InitializeLib(ImageHandle, ST);
    Print(L"[BL] Bootloader start\n");

    void* kbuf = NULL; UINTN ksize = 0;
    EFI_STATUS st = load_file(ImageHandle, ST, L"\\kernel.elf", &kbuf, &ksize);
    if (EFI_ERROR(st)) { Print(L"[BL] load kernel.elf failed: %r\n", st); return st; }

    Elf64_Ehdr* eh = (Elf64_Ehdr*)kbuf;
    Elf64_Phdr* ph = (Elf64_Phdr*)((UINT8*)kbuf + eh->e_phoff);
    Print(L"[BL] ELF phnum=%d\n", eh->e_phnum);

    for (UINT16 i=0;i<eh->e_phnum;i++) {
        st = load_elf_segment(ST, kbuf, &ph[i], i);
        if (EFI_ERROR(st)) return st;
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    st = get_gop(ST, &gop);
    if (EFI_ERROR(st)) { Print(L"[BL] GOP not found: %r\n", st); return st; }

    BootInfo* bi = NULL;
    EFI_PHYSICAL_ADDRESS bi_pa = 0;
    UINTN bi_pages = (sizeof(BootInfo)+4095)/4096;
    st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, bi_pages, &bi_pa);
    if (EFI_ERROR(st)) { Print(L"[BL] Allocate BootInfo failed: %r\n", st); return st; }
    bi = (BootInfo*)(UINTN)bi_pa;

    bi->fb_base   = gop->Mode->FrameBufferBase;
    bi->fb_width  = gop->Mode->Info->HorizontalResolution;
    bi->fb_height = gop->Mode->Info->VerticalResolution;
    bi->fb_pitch  = gop->Mode->Info->PixelsPerScanLine * 4;
    bi->fb_bpp    = 32;
    bi->fb_format = (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) ? 1 : 0;

    // Acquire memory map and ExitBootServices with robust retry. The map buffer is physically
    // backed and passed to the kernel in BootInfo.
    EFI_PHYSICAL_ADDRESS map_pa = 0; UINTN mm_size = 0, desc_size = 0; UINT32 desc_ver = 0;
    st = exit_boot_services_with_retry(ImageHandle, ST, &map_pa, &mm_size, &desc_size, &desc_ver);
    if (EFI_ERROR(st)) { Print(L"[BL] ExitBootServices failed after retry: %r\n", st); return st; }

    bi->mmap            = (uint64_t)(UINTN)map_pa;
    bi->mmap_size       = (uint64_t)mm_size;
    bi->mmap_desc_size  = (uint64_t)desc_size;
    bi->mmap_desc_ver   = (uint32_t)desc_ver;

    KernelEntry entry = (KernelEntry)(eh->e_entry);
    entry(bi);

    while (1) { __asm__ __volatile__("hlt"); }
    return EFI_SUCCESS;
}
