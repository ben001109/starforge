# AArch64 Support Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add AArch64 support with UEFI (AAVMF) and bare-metal boot paths on QEMU virt, with UART+framebuffer validation when available.

**Architecture:** Introduce AArch64-specific bootloader, kernel entry, and DTB parsing while keeping existing x86_64 code intact. Build system selects toolchain and emits firmware/kernel artifacts per architecture, with Makefile-driven tests launching QEMU and validating UART/framebuffer markers.

**Tech Stack:** C, assembly, GNU binutils/ld, QEMU (aarch64), UEFI (AAVMF), Makefile, shell scripts.

### Task 1: Define AArch64 build targets and toolchain detection

**Files:**
- Modify: `Makefile`
- Modify: `README.md`
- Test: `Makefile` (new check target referenced below)

**Step 1: Write the failing test**

Create a new Makefile check target stub that fails for missing AArch64 toolchain.

```makefile
.PHONY: check-aarch64-toolchain
check-aarch64-toolchain:
	@echo "ERROR: aarch64 toolchain not found"; exit 1
```

**Step 2: Run test to verify it fails**

Run: `make check-aarch64-toolchain`
Expected: FAIL with "ERROR: aarch64 toolchain not found"

**Step 3: Write minimal implementation**

Replace the stub with toolchain detection (prefer `aarch64-linux-gnu-gcc`, fallback `aarch64-elf-gcc`) and store `AARCH64_CC` for later tasks.

```makefile
AARCH64_CC ?= $(firstword $(foreach c,aarch64-linux-gnu-gcc aarch64-elf-gcc,$(shell command -v $(c) 2>/dev/null)))

.PHONY: check-aarch64-toolchain
check-aarch64-toolchain:
	@if [ -z "$(AARCH64_CC)" ]; then \
	  echo "ERROR: aarch64 toolchain not found."; \
	  echo " - Install aarch64-linux-gnu-gcc or aarch64-elf-gcc"; \
	  exit 1; \
	fi
```

**Step 4: Run test to verify it passes**

Run: `make check-aarch64-toolchain`
Expected: PASS (no output)

**Step 5: Commit**

```bash
git add Makefile
# README update will be done in Task 2
git commit -m "feat: add aarch64 toolchain detection"
```

### Task 2: Document AArch64 prerequisites

**Files:**
- Modify: `README.md`

**Step 1: Write the failing test**

Add a placeholder check in `scripts/check-docs.sh` that fails if README lacks AArch64 toolchain mention.

```bash
#!/usr/bin/env bash
set -euo pipefail
if ! grep -q "aarch64-linux-gnu-gcc" README.md; then
  echo "Missing AArch64 toolchain docs"; exit 1;
fi
```

**Step 2: Run test to verify it fails**

Run: `bash scripts/check-docs.sh`
Expected: FAIL with "Missing AArch64 toolchain docs"

**Step 3: Write minimal implementation**

Add a short AArch64 section in README:

```markdown
## AArch64 (QEMU virt)
- Install cross-compiler: `aarch64-linux-gnu-gcc` (preferred) or `aarch64-elf-gcc`
- QEMU: `qemu-system-aarch64`
- Firmware: AAVMF (edk2)
```

**Step 4: Run test to verify it passes**

Run: `bash scripts/check-docs.sh`
Expected: PASS

**Step 5: Commit**

```bash
git add README.md scripts/check-docs.sh
git commit -m "docs: add aarch64 toolchain prerequisites"
```

### Task 3: Add AArch64 UEFI bootloader skeleton

**Files:**
- Create: `boot/uefi-aarch64/bootloader.c`
- Create: `boot/uefi-aarch64/elf.h`
- Modify: `Makefile`
- Test: `scripts/test-aarch64-uefi.sh`

**Step 1: Write the failing test**

Create a test script that expects a UART marker in QEMU UEFI output (will fail until bootloader and kernel exist).

```bash
#!/usr/bin/env bash
set -euo pipefail
log=build/aarch64-uefi.log
rm -f "$log"
qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 512M \
  -bios /usr/share/AAVMF/AAVMF_CODE.fd \
  -drive if=none,file=starforge-aarch64.iso,format=raw,id=cd0 \
  -device virtio-scsi-device \
  -device scsi-cd,drive=cd0 \
  -nographic -serial file:"$log" -no-reboot

grep -q "AARCH64 UEFI OK" "$log"
```

**Step 2: Run test to verify it fails**

Run: `bash scripts/test-aarch64-uefi.sh`
Expected: FAIL (missing ISO or marker)

**Step 3: Write minimal implementation**

Add build rule to produce `BOOTAA64.EFI` and `starforge-aarch64.iso` with a minimal bootloader that prints UART marker then halts.

```c
// boot/uefi-aarch64/bootloader.c
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
  Print(L"AARCH64 UEFI OK\n");
  for(;;) __asm__ __volatile__("wfi");
}
```

Update Makefile to add `ARCH=aarch64` targets and ISO name.

**Step 4: Run test to verify it passes**

Run: `bash scripts/test-aarch64-uefi.sh`
Expected: PASS (marker found)

**Step 5: Commit**

```bash
git add boot/uefi-aarch64/bootloader.c boot/uefi-aarch64/elf.h Makefile scripts/test-aarch64-uefi.sh
git commit -m "feat: add aarch64 uefi bootloader skeleton"
```

### Task 4: Add AArch64 kernel entry + UART output

**Files:**
- Create: `kernel/aarch64/entry.S`
- Create: `kernel/aarch64/uart.c`
- Create: `kernel/aarch64/uart.h`
- Modify: `kernel/main.c` (split per-arch or dispatch)
- Modify: `Makefile`
- Test: `scripts/test-aarch64-uefi.sh`

**Step 1: Write the failing test**

Extend the UEFI test to require UART marker from kernel after bootloader transfers control.

```bash
grep -q "AARCH64 KERNEL OK" "$log"
```

**Step 2: Run test to verify it fails**

Run: `bash scripts/test-aarch64-uefi.sh`
Expected: FAIL (kernel marker missing)

**Step 3: Write minimal implementation**

Add AArch64 entry, wire bootloader to load `kernel-aarch64.elf`, and write UART to PL011. Print `AARCH64 KERNEL OK` on boot.

**Step 4: Run test to verify it passes**

Run: `bash scripts/test-aarch64-uefi.sh`
Expected: PASS

**Step 5: Commit**

```bash
git add kernel/aarch64/entry.S kernel/aarch64/uart.c kernel/aarch64/uart.h kernel/main.c Makefile scripts/test-aarch64-uefi.sh
git commit -m "feat: add aarch64 kernel entry and uart output"
```

### Task 5: Add framebuffer output when available

**Files:**
- Create: `kernel/aarch64/framebuffer.c`
- Create: `kernel/aarch64/framebuffer.h`
- Modify: `boot/uefi-aarch64/bootloader.c`
- Modify: `kernel/main.c`
- Test: `scripts/test-aarch64-uefi.sh`

**Step 1: Write the failing test**

Require framebuffer marker in UART log when GOP provides framebuffer.

```bash
grep -q "AARCH64 FB OK" "$log"
```

**Step 2: Run test to verify it fails**

Run: `bash scripts/test-aarch64-uefi.sh`
Expected: FAIL (framebuffer marker missing)

**Step 3: Write minimal implementation**

Bootloader passes framebuffer info in BootInfo; kernel draws a test pattern and logs `AARCH64 FB OK`.

**Step 4: Run test to verify it passes**

Run: `bash scripts/test-aarch64-uefi.sh`
Expected: PASS

**Step 5: Commit**

```bash
git add kernel/aarch64/framebuffer.c kernel/aarch64/framebuffer.h boot/uefi-aarch64/bootloader.c kernel/main.c scripts/test-aarch64-uefi.sh
git commit -m "feat: add aarch64 framebuffer output"
```

### Task 6: Add bare-metal boot path

**Files:**
- Create: `kernel/aarch64/dtb.c`
- Create: `kernel/aarch64/dtb.h`
- Modify: `Makefile`
- Create: `scripts/test-aarch64-bare.sh`

**Step 1: Write the failing test**

Add bare-metal test that expects `AARCH64 BARE OK` marker.

**Step 2: Run test to verify it fails**

Run: `bash scripts/test-aarch64-bare.sh`
Expected: FAIL

**Step 3: Write minimal implementation**

Add DTB parser for UART base + optional framebuffer; create bare-metal entry and link target; log UART marker.

**Step 4: Run test to verify it passes**

Run: `bash scripts/test-aarch64-bare.sh`
Expected: PASS

**Step 5: Commit**

```bash
git add kernel/aarch64/dtb.c kernel/aarch64/dtb.h scripts/test-aarch64-bare.sh Makefile
git commit -m "feat: add aarch64 bare-metal boot path"
```

### Task 7: Integrate into `make check`

**Files:**
- Modify: `Makefile`

**Step 1: Write the failing test**

Add a `check` invocation that calls missing scripts (will fail).

**Step 2: Run test to verify it fails**

Run: `make check`
Expected: FAIL due to missing scripts or markers

**Step 3: Write minimal implementation**

Wire `make check` to run `check-aarch64-toolchain`, `scripts/test-aarch64-uefi.sh`, and `scripts/test-aarch64-bare.sh`.

**Step 4: Run test to verify it passes**

Run: `make check`
Expected: PASS

**Step 5: Commit**

```bash
git add Makefile
git commit -m "test: add aarch64 checks"
```

### Task 8: Add QEMU machine variants (virt + raspi3)

**Files:**
- Modify: `scripts/test-aarch64-uefi.sh`
- Modify: `scripts/test-aarch64-bare.sh`
- Modify: `Makefile`

**Step 1: Write the failing test**

Add a script flag `--machine` and require it in Makefile (tests should fail without support).

**Step 2: Run test to verify it fails**

Run: `bash scripts/test-aarch64-uefi.sh --machine raspi3`
Expected: FAIL

**Step 3: Write minimal implementation**

Support `virt` and `raspi3`, with `virt` as default priority.

**Step 4: Run test to verify it passes**

Run: `bash scripts/test-aarch64-uefi.sh --machine raspi3`
Expected: PASS

**Step 5: Commit**

```bash
git add scripts/test-aarch64-uefi.sh scripts/test-aarch64-bare.sh Makefile
git commit -m "test: support aarch64 qemu machine variants"
```
