#!/usr/bin/env bash
set -euo pipefail
ISO="${1:-starforge.iso}"

# Respect environment override; otherwise try to autodetect
OVMF_CODE="${OVMF_CODE:-}"
OVMF_VARS="${OVMF_VARS:-}"
uname_s=$(uname -s 2>/dev/null || echo Unknown)
if [[ -z "${OVMF_CODE}" && "$uname_s" == "Darwin" ]]; then
  echo "[qemu-gdb] macOS detected. If QEMU is installed via Homebrew, OVMF is usually at:" >&2
  echo "  /opt/homebrew/share/qemu/edk2-x86_64-code.fd (Apple Silicon/Intel Homebrew)" >&2
  echo "  /usr/local/share/qemu/edk2-x86_64-code.fd    (Intel Homebrew)" >&2
  echo "Set it with: export OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd" >&2
fi

if [[ -z "${OVMF_CODE}" || ! -f "${OVMF_CODE}" ]]; then
  candidates=(
    "${OVMF_CODE:-}"
    "/opt/homebrew/share/qemu/edk2-x86_64-code.fd"
    "/usr/local/share/qemu/edk2-x86_64-code.fd"
    "/opt/homebrew/share/edk2/ovmf/OVMF_CODE.fd"
    "/usr/local/share/edk2/ovmf/OVMF_CODE.fd"
    "/usr/share/OVMF/OVMF_CODE.fd"
    "/usr/share/edk2-ovmf/OVMF_CODE.fd"
  )
  for c in "${candidates[@]}"; do
    if [[ -n "$c" && -f "$c" ]]; then OVMF_CODE="$c"; break; fi
  done
fi

if [[ -z "${OVMF_VARS}" || ! -f "${OVMF_VARS}" ]]; then
  vars_candidates=(
    "${OVMF_VARS:-}"
    "/opt/homebrew/share/qemu/edk2-x86_64-vars.fd"
    "/usr/local/share/qemu/edk2-x86_64-vars.fd"
    "/usr/share/OVMF/OVMF_VARS.fd"
    "/usr/share/edk2-ovmf/OVMF_VARS.fd"
  )
  for v in "${vars_candidates[@]}"; do
    if [[ -n "$v" && -f "$v" ]]; then OVMF_VARS="$v"; break; fi
  done
fi

if [[ ! -f "${OVMF_CODE:-}" ]]; then
  echo "[qemu-gdb] ERROR: OVMF_CODE not found." >&2
  echo " - Current OVMF_CODE=\"${OVMF_CODE:-unset}\"" >&2
  if [[ "$uname_s" == "Darwin" ]]; then
    echo " - Install QEMU via Homebrew: brew install qemu" >&2
    echo " - Then try: export OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd" >&2
  else
    echo " - Common paths: /usr/share/OVMF/OVMF_CODE.fd or /usr/share/edk2-ovmf/OVMF_CODE.fd" >&2
  fi
  exit 1
fi

VARS_ARG=()
if [[ -n "${OVMF_VARS:-}" && -f "${OVMF_VARS}" ]]; then
  mkdir -p build
  cp -f "${OVMF_VARS}" build/OVMF_VARS.fd
  VARS_ARG=(-drive if=pflash,format=raw,file="$(pwd)/build/OVMF_VARS.fd")
fi

exec qemu-system-x86_64 -machine q35 -cpu qemu64 -m 512M \
  -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
  "${VARS_ARG[@]}" \
  -serial stdio -cdrom "${ISO}" -S -s
