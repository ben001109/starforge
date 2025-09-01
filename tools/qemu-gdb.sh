#!/usr/bin/env bash
set -euo pipefail
ISO="${1:-starforge.iso}"
OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}
qemu-system-x86_64 -machine q35 -cpu qemu64 -m 512M   -bios "$OVMF_CODE" -serial stdio -cdrom "$ISO" -S -s
