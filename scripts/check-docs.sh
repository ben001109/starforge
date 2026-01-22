#!/bin/bash
# Test: Check README contains AArch64 documentation in Traditional Chinese

set -e

README_PATH="README.md"

# Check if README contains AArch64 section with Traditional Chinese text
if ! grep -q "AArch64.*QEMU virt" "$README_PATH"; then
    echo "FAIL: README missing AArch64 section header"
    exit 1
fi

# Check for Traditional Chinese text: "安裝交叉編譯器"
if ! grep -q "安裝交叉編譯器" "$README_PATH"; then
    echo "FAIL: README AArch64 section missing Traditional Chinese text '安裝交叉編譯器'"
    echo "Expected: - 安裝交叉編譯器: \`aarch64-linux-gnu-gcc\` (優先) 或 \`aarch64-elf-gcc\`"
    exit 1
fi

# Check for the specific cross-compiler names in AArch64 context
if ! grep -q "aarch64-linux-gnu-gcc" "$README_PATH" || ! grep -q "aarch64-elf-gcc" "$README_PATH"; then
    echo "FAIL: README AArch64 section missing cross-compiler references"
    exit 1
fi

# Check for QEMU and Firmware mentions in AArch64 context
if ! grep -A 5 "AArch64.*QEMU virt" "$README_PATH" | grep -q "qemu-system-aarch64"; then
    echo "FAIL: README AArch64 section missing QEMU reference"
    exit 1
fi

if ! grep -A 5 "AArch64.*QEMU virt" "$README_PATH" | grep -q "AAVMF"; then
    echo "FAIL: README AArch64 section missing AAVMF reference"
    exit 1
fi

echo "PASS: README contains complete AArch64 documentation in Traditional Chinese"
exit 0
