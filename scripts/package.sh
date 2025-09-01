#!/usr/bin/env bash
set -euo pipefail
make dist
echo
echo "打包完成。校驗："
( cd dist && sha256sum -c SHA256SUMS )
