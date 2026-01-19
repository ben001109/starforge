#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
source "${SCRIPT_DIR}/common.sh"

LOG_DIR="${PROJECT_ROOT}/test-logs"
LOG_FILE="${LOG_DIR}/build.log"
ISO_FILE="${PROJECT_ROOT}/starforge.iso"

mkdir -p "${LOG_DIR}"

log "Starting build test"
log "Logging to ${LOG_FILE}"

{
    log "Running make docker-build"
    cd "${PROJECT_ROOT}"
    make docker-build
    log "make docker-build completed successfully"

    log "Running make docker-make"
    make docker-make
    log "make docker-make completed successfully"

} 2>&1 | tee "${LOG_FILE}"

log "Checking for ISO file: ${ISO_FILE}"
if [ -f "${ISO_FILE}" ]; then
    log "Build test passed: ${ISO_FILE} exists"
else
    fail "Build test failed: ${ISO_FILE} not found"
fi
