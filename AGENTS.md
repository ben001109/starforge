# AGENTS

This repository is a small research OS with a UEFI bootloader and x86_64 kernel. Use this file as guidance when running automated changes.

## Quick Commands
- Build (host): `make -j$(nproc)`
- Build (Docker): `make docker-build` then `make docker-make`
- Run ISO in QEMU: `make run`
- Debug (QEMU + GDB stub): `./tools/qemu-gdb.sh`

## Outputs
- Kernel ELF: `build/kernel.elf`
- EFI bootloader: `build/BOOTX64.EFI`
- ISO image: `starforge.iso`

## Layout
- `boot/uefi/`: UEFI bootloader sources
- `kernel/`: kernel sources and linker script
- `tools/`: QEMU helper scripts
- `scripts/`: packaging and helper scripts
- `docs/`: documentation

## Notes
- There is no automated test suite; use builds as verification.
- Prefer Docker builds for reproducible environments.
- Require PRs for all changes, regardless of size.
