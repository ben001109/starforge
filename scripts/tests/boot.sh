#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/common.sh"

if [ "${TEST_BOOT:-1}" = "0" ]; then
  log "boot: skipped (TEST_BOOT=0)"
  exit 0
fi

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "missing command: $1"
}

require_env() {
  [ -n "${!1-}" ] || fail "missing env: $1"
}

require_cmd qemu-system-x86_64
require_env OVMF_CODE

log "boot: qemu"
mkdir -p "${PROJECT_ROOT}/build/test-logs"
log_file="${PROJECT_ROOT}/build/test-logs/boot.log"

run_timeout() {
  local timeout="$1"; shift
  if command -v timeout >/dev/null 2>&1; then
    timeout "$timeout" "$@"
  else
    python3 - <<'PY' "$timeout" "$@"
import os, signal, subprocess, sys
secs=float(sys.argv[1])
cmd=sys.argv[2:]
proc=subprocess.Popen(cmd)
try:
  proc.wait(timeout=secs)
  sys.exit(proc.returncode)
except subprocess.TimeoutExpired:
  proc.kill()
  sys.exit(124)
PY
  fi
}

run_timeout "${TEST_TIMEOUT:-20}" \
  qemu-system-x86_64 -machine q35 -cpu qemu64 -m 512M \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -cdrom "${PROJECT_ROOT}/starforge.iso" -serial stdio -display none -no-reboot -no-shutdown \
  >"$log_file" 2>&1 || true

if ! grep -q "\[KERNEL\] hello" "$log_file"; then
  fail "boot: missing kernel hello"
fi
if ! grep -q "\[KERNEL\] framebuffer painted" "$log_file"; then
  fail "boot: missing framebuffer message"
fi

log "boot: ok"
