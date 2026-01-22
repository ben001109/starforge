# ==== Starforge OS (codename: Aegis-Alpha) ====
ARCH      ?= x86_64
# Detect gnu-efi artifacts (Linux distros). We no longer hard-fail at parse time;
# the bootloader rule will check and error with guidance when actually building.
GNUEFI_BASE_CAND := /usr/lib/gnu-efi /usr/lib/x86_64-linux-gnu/gnu-efi /usr/lib/$(ARCH)-linux-gnu/gnu-efi /usr/lib/gnuefi /usr/lib
LDS_NAMES  := $(ARCH)/elf_$(ARCH)_efi.lds elf_$(ARCH)_efi.lds
CRT0_NAMES := $(ARCH)/crt0-efi-$(ARCH).o crt0-efi-$(ARCH).o

LDS_EFI := $(firstword $(foreach b,$(GNUEFI_BASE_CAND),$(foreach n,$(LDS_NAMES),$(wildcard $(b)/$(n)))))
CRT0_EFI := $(firstword $(foreach b,$(GNUEFI_BASE_CAND),$(foreach n,$(CRT0_NAMES),$(wildcard $(b)/$(n)))))

EFIINC    := /usr/include/efi
EFIINCS   := -I$(EFIINC) -I$(EFIINC)/$(ARCH)
# OVMF firmware path; allow override via environment. Only checked when running.
# Note: On macOS with Homebrew, OVMF often lives under share/qemu (edk2-x86_64-code.fd).
OVMF_CANDIDATES := $(wildcard \
  /usr/share/OVMF/OVMF_CODE.fd \
  /usr/share/edk2-ovmf/OVMF_CODE.fd \
  /usr/local/share/edk2-ovmf/OVMF_CODE.fd \
  /opt/homebrew/share/edk2/ovmf/OVMF_CODE.fd \
  /opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  /usr/local/share/qemu/edk2-x86_64-code.fd)
OVMF_CODE ?= $(firstword $(OVMF_CANDIDATES))

# AArch64 UEFI firmware path
AAVMF_CANDIDATES := $(wildcard \
  /usr/share/AAVMF/AAVMF_CODE.fd \
  /usr/share/edk2-aarch64/QEMU_EFI.fd \
  /usr/local/share/edk2-aarch64/QEMU_EFI.fd \
  /opt/homebrew/share/qemu/edk2-aarch64-code.fd \
  /usr/local/share/qemu/edk2-aarch64-code.fd)
AAVMF_CODE ?= $(firstword $(AAVMF_CANDIDATES))

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

.PHONY: all clean run gdb iso dist dist-src dist-bin print-dist check-aarch64-toolchain aarch64-iso run-aarch64 build/kernel-bare-aarch64.elf

all: $(ISO)

test: test-build test-boot test-unit

test-build:
	./scripts/tests/build.sh

test-boot:
	./scripts/tests/boot.sh

test-unit:
	./scripts/tests/unit.sh

AARCH64_CC ?= $(firstword $(foreach c,clang aarch64-linux-gnu-gcc aarch64-elf-gcc,$(shell command -v $(c) 2>/dev/null)))

check-aarch64-toolchain:
	@if [ -z "$(AARCH64_CC)" ]; then \
	  echo "ERROR: aarch64 toolchain not found."; \
	  echo " - Install aarch64-linux-gnu-gcc or aarch64-elf-gcc"; \
	  exit 1; \
	fi

# Bootloader
$(BUILD)/BOOTX64.EFI: boot/uefi/bootloader.c boot/uefi/elf.h | $(BUILD)
	@if [ -z "$(LDS_EFI)" ] || [ -z "$(CRT0_EFI)" ]; then \
	  echo "ERROR: gnu-efi not found on host."; \
	  echo " - Install gnu-efi (Linux) OR run: make docker-build && make docker-make"; \
	  echo " - Missing: elf_$(ARCH)_efi.lds=$(LDS_EFI) crt0-efi-$(ARCH).o=$(CRT0_EFI)"; \
	  exit 1; \
	fi
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
	mkfs.vfat -F 16 $(ESP_IMG)
	mmd   -i $(ESP_IMG) ::/EFI ::/EFI/BOOT
	mcopy -i $(ESP_IMG) $(BUILD)/BOOTX64.EFI ::/EFI/BOOT/
	mcopy -i $(ESP_IMG) $(BUILD)/kernel.elf   ::/

# ISO
$(ISO): $(ESP_IMG)
	mkdir -p $(ISO_DIR)/EFI
	cp $(ESP_IMG) $(ISO_DIR)/EFI/efiboot.img
	mkdir -p $(ISO_DIR)/EFI/BOOT
	cp $(BUILD)/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	xorriso -as mkisofs -R -J -V "STARFORGE" \
	    -e EFI/efiboot.img -no-emul-boot \
	    -o $(ISO) $(ISO_DIR)

run: $(ISO)
	./tools/qemu-run.sh $(ISO)

gdb:
	./tools/qemu-gdb.sh $(ISO)

# AArch64 targets
aarch64-iso: starforge-aarch64.iso

starforge-aarch64.iso: $(BUILD)/BOOTAA64.EFI $(BUILD)/kernel-aarch64.elf
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	mkfs.vfat -F 32 $@
	mmd -i $@ ::/EFI ::/EFI/BOOT
	mcopy -i $@ $(BUILD)/BOOTAA64.EFI ::/EFI/BOOT/
	mcopy -i $@ $(BUILD)/kernel-aarch64.elf ::/

$(BUILD)/BOOTAA64.EFI: boot/uefi-aarch64/bootloader.c boot/uefi-aarch64/elf.h | $(BUILD)
	@if [ -z "$(AARCH64_CC)" ]; then \
	  echo "ERROR: aarch64 toolchain not found."; \
	  echo " - Install clang (macOS), aarch64-linux-gnu-gcc (Linux), or aarch64-elf-gcc"; \
	  echo " - Run: make check-aarch64-toolchain"; \
	  exit 1; \
	fi
	@echo "Using $(AARCH64_CC) for AArch64 build"
	$(AARCH64_CC) -target aarch64-unknown-none -ffreestanding -fno-stack-protector -fno-builtin -c $< -o $(BUILD)/bootloader-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -nostdlib -e efi_main $(BUILD)/bootloader-aarch64.o -o $(BUILD)/bootloader-aarch64.elf
	objcopy -O binary $(BUILD)/bootloader-aarch64.elf $(BUILD)/BOOTAA64.EFI

$(BUILD)/kernel-aarch64.elf: kernel/main.c kernel/util.c kernel/aarch64/uart.c kernel/aarch64/framebuffer.c kernel/bootinfo.h kernel/linker.ld | $(BUILD)
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/util.c -o $(BUILD)/util-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/uart.c -o $(BUILD)/uart-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/framebuffer.c -o $(BUILD)/framebuffer-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/main.c -o $(BUILD)/main-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -nostdlib -T kernel/linker.ld $(BUILD)/util-aarch64.o $(BUILD)/uart-aarch64.o $(BUILD)/framebuffer-aarch64.o $(BUILD)/main-aarch64.o -o $(BUILD)/kernel-aarch64.elf

$(BUILD)/kernel-bare-aarch64.elf: kernel/main.c kernel/aarch64/uart.c kernel/aarch64/framebuffer.c kernel/aarch64/dtb.c kernel/aarch64/dtb_boot.c kernel/aarch64/entry.S kernel/bootinfo.h kernel/linker-aarch64-bare.ld | $(BUILD)
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/uart.c -o $(BUILD)/uart-bare-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/framebuffer.c -o $(BUILD)/framebuffer-bare-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/dtb.c -o $(BUILD)/dtb-bare-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/dtb_boot.c -o $(BUILD)/dtb_boot-bare-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/main.c -o $(BUILD)/main-bare-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -c $(CFLAGS_KERN) kernel/aarch64/entry.S -o $(BUILD)/entry-bare-aarch64.o
	$(AARCH64_CC) -target aarch64-unknown-none -nostdlib -T kernel/linker-aarch64-bare.ld $(BUILD)/uart-bare-aarch64.o $(BUILD)/framebuffer-bare-aarch64.o $(BUILD)/dtb-bare-aarch64.o $(BUILD)/dtb_boot-bare-aarch64.o $(BUILD)/main-bare-aarch64.o $(BUILD)/entry-bare-aarch64.o -o $(BUILD)/kernel-bare-aarch64.elf

run-aarch64: starforge-aarch64.iso
	@if [ -z "$(AAVMF_CODE)" ]; then \
	  echo "ERROR: AAVMF firmware not found."; \
	  echo " - Install edk2-aarch64 or QEMU UEFI firmware"; \
	  exit 1; \
	fi
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 512M -nographic -drive if=virtio,file=starforge-aarch64.iso,format=raw -bios "$(AAVMF_CODE)"

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(ISO)

docker-build:
	docker buildx build --platform linux/amd64 -t starforge-build .
	
docker-make:
	docker run --rm --platform=linux/amd64 -v "$(PWD)":/work -w /work starforge-build bash -lc 'make clean && make -j$$(nproc)'

# Code quality hooks (no-op by default). Create stubs if missing, then run.
.PHONY: check
check:
	@mkdir -p scripts
	@([ -f scripts/lint.sh ] || (echo '#!/usr/bin/env bash' > scripts/lint.sh && echo 'exit 0' >> scripts/lint.sh && chmod +x scripts/lint.sh))
	@([ -f scripts/format.sh ] || (echo '#!/usr/bin/env bash' > scripts/format.sh && echo 'exit 0' >> scripts/format.sh && chmod +x scripts/format.sh))
	@./scripts/lint.sh
	@./scripts/format.sh
	@if [ "$(ARCH)" = "aarch64" ]; then \
	  echo "Running AArch64-specific checks..."; \
	  $(MAKE) check-aarch64-toolchain; \
	  ./scripts/test-aarch64-uefi.sh; \
	  ./scripts/test-aarch64-bare.sh; \
	fi

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
