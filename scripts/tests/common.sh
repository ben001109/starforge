#!/bin/bash

set -euo pipefail

log() {
    echo "[INFO] $*"
}

warn() {
    echo "[WARN] $*" >&2
}

error() {
    echo "[ERROR] $*" >&2
}

fail() {
    echo "[FAIL] $*" >&2
    exit 1
}
