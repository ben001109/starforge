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
    UINT64 palign = seg->p_align ? seg->p_align : 4096;

    if (palign < 4096) palign = 4096;
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

    SetMem((void*)(UINTN)aligned_start, (UINTN)pages * 4096, 0);
    CopyMem((void*)(UINTN)base, (UINT8*)kbuf + seg->p_offset, (UINTN)filesz);
    if (memsz > filesz) {
        SetMem((void*)((UINTN)base + (UINTN)filesz), (UINTN)(memsz - filesz), 0);
    }

    Print(L"[BL] load seg %d base=%lx align=%lx pages=%lx memsz=%lx filesz=%lx\n",
          index, base, palign, (UINT64)pages, (UINT64)memsz, (UINTN)filesz);
    return EFI_SUCCESS;
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

    Print(L"[BL] Framebuffer: %dx%d pitch=%d bpp=%d format=%d\n",
          bi->fb_width, bi->fb_height, bi->fb_pitch, bi->fb_bpp, bi->fb_format);
    Print(L"[BL] AARCH64 KERNEL OK\n");

    KernelEntry entry = (KernelEntry)(eh->e_entry);
    entry(bi);

    while (1) { __asm__ __volatile__("wfi"); }
    return EFI_SUCCESS;
}
