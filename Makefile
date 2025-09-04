# ==== Starforge OS (codename: Aegis-Alpha) ====
ARCH      := x86_64
# Detect gnu-efi artifacts (works on Ubuntu/Debian variants)
GNUEFI_BASE_CAND := /usr/lib/gnu-efi /usr/lib/x86_64-linux-gnu/gnu-efi /usr/lib/$(ARCH)-linux-gnu/gnu-efi /usr/lib/gnuefi /usr/lib
LDS_NAMES  := $(ARCH)/elf_$(ARCH)_efi.lds elf_$(ARCH)_efi.lds
CRT0_NAMES := $(ARCH)/crt0-efi-$(ARCH).o crt0-efi-$(ARCH).o

LDS_EFI := $(firstword $(foreach b,$(GNUEFI_BASE_CAND),$(foreach n,$(LDS_NAMES),$(wildcard $(b)/$(n)))))
CRT0_EFI := $(firstword $(foreach b,$(GNUEFI_BASE_CAND),$(foreach n,$(CRT0_NAMES),$(wildcard $(b)/$(n)))))

ifeq (,$(LDS_EFI))
  $(error Unable to locate elf_$(ARCH)_efi.lds (install gnu-efi))
endif
ifeq (,$(CRT0_EFI))
  $(error Unable to locate crt0-efi-$(ARCH).o (install gnu-efi))
endif
EFIINC    := /usr/include/efi
EFIINCS   := -I$(EFIINC) -I$(EFIINC)/$(ARCH)
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd

BUILD     := build
ESP_IMG   := $(BUILD)/efiboot.img
ISO_DIR   := $(BUILD)/iso
ISO       := starforge.iso

# flags
CFLAGS_EFI := -DEFI_FUNCTION_WRAPPER -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -ffreestanding -Wall -Wextra $(EFIINCS)
# Compose library search paths generously to cover distro layouts
GNUEFI_LIBDIRS := $(sort $(dir $(LDS_EFI)) $(dir $(CRT0_EFI)) /usr/lib /usr/lib/$(ARCH)-linux-gnu /usr/lib/x86_64-linux-gnu /usr/lib/gnuefi)
LIBS_EFI    := $(addprefix -L,$(GNUEFI_LIBDIRS)) -lgnuefi -lefi

CFLAGS_KERN := -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -O2 -Wall -Wextra
LDFLAGS_KERN:= -T kernel/linker.ld -nostdlib

.PHONY: all clean run gdb iso dist dist-src dist-bin print-dist

all: $(ISO)

# Bootloader
$(BUILD)/BOOTX64.EFI: boot/uefi/bootloader.c boot/uefi/elf.h | $(BUILD)
	gcc $(CFLAGS_EFI) -c $< -o $(BUILD)/bootloader.o
	ld  -nostdlib -znocombreloc -T $(LDS_EFI) -shared -Bsymbolic \
	    $(CRT0_EFI) $(BUILD)/bootloader.o -o $(BUILD)/bootloader.so $(LIBS_EFI)
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	        -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
	        --target=efi-app-$(ARCH) $(BUILD)/bootloader.so $(BUILD)/BOOTX64.EFI

# Kernel
$(BUILD)/kernel.elf: kernel/main.c kernel/util.c kernel/bootinfo.h kernel/linker.ld | $(BUILD)
	gcc -c $(CFLAGS_KERN) kernel/util.c -o $(BUILD)/util.o
	gcc -c $(CFLAGS_KERN) kernel/main.c -o $(BUILD)/main.o
	ld  $(LDFLAGS_KERN) $(BUILD)/util.o $(BUILD)/main.o -o $(BUILD)/kernel.elf

# ESP (FAT)
$(ESP_IMG): $(BUILD)/BOOTX64.EFI $(BUILD)/kernel.elf | $(BUILD)
	dd if=/dev/zero of=$(ESP_IMG) bs=1M count=16 status=none
	mkfs.vfat -F 32 $(ESP_IMG)
	mmd   -i $(ESP_IMG) ::/EFI ::/EFI/BOOT
	mcopy -i $(ESP_IMG) $(BUILD)/BOOTX64.EFI ::/EFI/BOOT/
	mcopy -i $(ESP_IMG) $(BUILD)/kernel.elf   ::/

# ISO
$(ISO): $(ESP_IMG)
	mkdir -p $(ISO_DIR)/EFI
	cp $(ESP_IMG) $(ISO_DIR)/EFI/efiboot.img
	xorriso -as mkisofs -R -J -V "STARFORGE" \
	    -e EFI/efiboot.img -no-emul-boot \
	    -o $(ISO) $(ISO_DIR)

run: $(ISO)
		@if [ -f "$(OVMF_CODE)" ]; then \
		./tools/qemu-run.sh $(ISO); \
	else \
		echo "OVMF_CODE not found: $(OVMF_CODE)"; \
		echo "Set OVMF_CODE to the path of OVMF_CODE.fd"; \
	fi

gdb:
	@if [ -f "$(OVMF_CODE)" ]; then \
		./tools/qemu-gdb.sh $(ISO); \
	else \
		echo "OVMF_CODE not found: $(OVMF_CODE)"; \
		echo "Set OVMF_CODE to the path of OVMF_CODE.fd"; \
	fi

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(ISO)

docker-build:
	docker buildx build --platform linux/amd64 -t starforge-build .
	
docker-make:
	docker run --rm -ti --platform=linux/amd64 -v "$(PWD)":/work -w /work starforge-build bash -lc 'make clean && make -j$$(nproc)'

# ===== Packaging =====
DIST_DIR   := dist
PKG_NAME   := starforge
VERSION    := $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)
STAMP      := $(shell date -u +%Y%m%dT%H%M%SZ)

dist-iso: $(ISO)
	@mkdir -p $(DIST_DIR)
	@cp -f $(ISO) $(DIST_DIR)/$(PKG_NAME)-$(VERSION)-$(STAMP).iso

dist-src:
	@mkdir -p $(DIST_DIR)
	@git archive --format=tar --prefix=$(PKG_NAME)-$(VERSION)/ HEAD \
	  | gzip -9 > $(DIST_DIR)/$(PKG_NAME)-$(VERSION)-src.tar.gz

dist-bin: dist-iso
	@mkdir -p $(DIST_DIR)/bundle
	@cp -f README.md tools/qemu-run.sh $(DIST_DIR)/bundle/ || true
	@cd $(DIST_DIR) && tar -czf $(PKG_NAME)-$(VERSION)-bin.tar.gz \
	  $(PKG_NAME)-$(VERSION)-$(STAMP).iso bundle
	@rm -rf $(DIST_DIR)/bundle

dist: clean all dist-src dist-bin
	@cd $(DIST_DIR) && sha256sum $(PKG_NAME)-$(VERSION)-*.tar.gz \
	  $(PKG_NAME)-$(VERSION)-*.iso > SHA256SUMS
	@echo "==> 檔案完成於 $(DIST_DIR)/:"
	@ls -lh $(DIST_DIR)

print-dist:
	@echo "Artifacts in $(DIST_DIR):"
	@ls -lah $(DIST_DIR) || true
