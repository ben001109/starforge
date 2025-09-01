#!/usr/bin/env bash
set -euo pipefail
ISO="${1:-starforge.iso}"
OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}
if [[ ! -f "$OVMF_CODE" ]]; then
  if [[ "$(uname)" == "Darwin" ]]; then
    echo "OVMF_CODE not found: $OVMF_CODE" >&2
    echo "Try 'brew install qemu' or 'brew install edk2-ovmf' and set OVMF_CODE." >&2
    echo "Common path: /opt/homebrew/share/edk2/ovmf/OVMF_CODE.fd" >&2
  else
    echo "OVMF_CODE not found: $OVMF_CODE" >&2
  fi
  exit 1
