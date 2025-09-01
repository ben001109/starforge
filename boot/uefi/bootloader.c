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

static EFI_STATUS get_mm_and_copy(EFI_SYSTEM_TABLE* ST, VOID** map_out, UINTN* sz_out, UINTN* key_out, UINTN* desc_sz_out, UINT32* desc_ver_out) {
    EFI_STATUS st;
    UINTN mm_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;

    st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mm_size, NULL, &map_key, &desc_size, &desc_ver);
    if (st != EFI_BUFFER_TOO_SMALL) return st;

    mm_size += 4096;
    VOID* buf;
    st = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, mm_size, &buf);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mm_size, buf, &map_key, &desc_size, &desc_ver);
    if (EFI_ERROR(st)) return st;

    UINTN pages = (mm_size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS dest = 0;
    st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &dest);
    if (EFI_ERROR(st)) return st;

    CopyMem((void*)(UINTN)dest, buf, mm_size);
    uefi_call_wrapper(ST->BootServices->FreePool, 1, buf);

    *map_out = (void*)(UINTN)dest;
    *sz_out = mm_size;
    *key_out = map_key;
    *desc_sz_out = desc_size;
    *desc_ver_out = desc_ver;
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
        if (ph[i].p_type != PT_LOAD) continue;
        UINT64 paddr = ph[i].p_paddr ? ph[i].p_paddr : ph[i].p_vaddr;
        UINTN  memsz = (UINTN)ph[i].p_memsz;
        UINTN  filesz= (UINTN)ph[i].p_filesz;
        UINTN  pages = (memsz + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS alloc = paddr;

        st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &alloc);
        if (EFI_ERROR(st)) { Print(L"[BL] AllocatePages @%lx failed: %r\n", paddr, st); return st; }

        SetMem((void*)(UINTN)paddr, memsz, 0);
        CopyMem((void*)(UINTN)paddr, (UINT8*)kbuf + ph[i].p_offset, filesz);
        Print(L"[BL] load seg %d paddr=%lx memsz=%lx filesz=%lx\n", i, paddr, (UINT64)memsz, (UINT64)filesz);
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    st = get_gop(ST, &gop);
    if (EFI_ERROR(st)) { Print(L"[BL] GOP not found: %r\n", st); return st; }

    VOID* mm; UINTN mmsz, key, dsz; UINT32 dver;
    st = get_mm_and_copy(ST, &mm, &mmsz, &key, &dsz, &dver);
    if (EFI_ERROR(st)) { Print(L"[BL] get memory map failed: %r\n", st); return st; }

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

    bi->mmap         = (uint64_t)(UINTN)mm;
    bi->mmap_size    = mmsz;
    bi->mmap_desc_size = dsz;
    bi->mmap_desc_ver  = dver;

    st = uefi_call_wrapper(ST->BootServices->ExitBootServices, 2, ImageHandle, key);
    if (EFI_ERROR(st)) {
        return st;
    }

    KernelEntry entry = (KernelEntry)(eh->e_entry);
    entry(bi);

    while (1) { __asm__ __volatile__("hlt"); }
    return EFI_SUCCESS;
}
