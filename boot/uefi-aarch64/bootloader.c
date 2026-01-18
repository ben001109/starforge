#include <efi.h>
#include <efilib.h>

static EFI_STATUS uart_init(EFI_SYSTEM_TABLE* ST) {
    EFI_STATUS st;
    EFI_SERIAL_IO_PROTOCOL* serial;

    st = uefi_call_wrapper(ST->BootServices->LocateProtocol, 3, 
                           &gEfiSerialIoProtocolGuid, NULL, (void**)&serial);
    if (EFI_ERROR(st)) {
        return st;
    }

    EFI_SERIAL_IO_MODE mode = *serial->Mode;
    mode.BaudRate = 115200;
    mode.DataBits = 8;
    mode.Parity = NoParity;
    mode.StopBits = OneStopBit;

    st = uefi_call_wrapper(serial->SetAttributes, 6,
                           serial, mode.BaudRate, mode.ReceiveFifoDepth,
                           mode.Timeout, mode.Parity, mode.DataBits, mode.StopBits);
    return st;
}

static void uart_putc(EFI_SYSTEM_TABLE* ST, char c) {
    CHAR16 s[2];
    s[0] = (CHAR16)c;
    s[1] = 0;
    ST->ConOut->OutputString(ST->ConOut, s);
}

static void uart_puts(EFI_SYSTEM_TABLE* ST, const char* s) {
    while (*s) {
        uart_putc(ST, *s++);
    }
}

void halt() {
    while (1) {
        __asm__ volatile("wfi");
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    InitializeLib(ImageHandle, ST);

    uart_puts(ST, "AARCH64 KERNEL OK\n");

    halt();

    return EFI_SUCCESS;
}
