#!/usr/bin/env bash
set -euo pipefail

. "$(dirname "$0")/common.sh"

TEST_LOG_DIR=${TEST_LOG_DIR:-build/test-logs}

log "unit: build"
command -v docker >/dev/null 2>&1 || fail "missing command: docker"

mkdir -p "$TEST_LOG_DIR"
log_file="$TEST_LOG_DIR/unit.log"

docker run --rm -v "$PWD:/work" -w /work starforge-build \
  gcc -std=c11 -Wall -Wextra -Werror \
  -I./kernel \
  tests/unit/test_memset.c kernel/util.c -o /work/build/test_memset \
  >"$log_file" 2>&1

log "unit: run"
docker run --rm -v "$PWD:/work" -w /work starforge-build \
  /work/build/test_memset >>"$log_file" 2>&1
log "unit: ok"