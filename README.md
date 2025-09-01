# Starforge OS — codename **Aegis-Alpha**

自製 UEFI Bootloader + x86_64 Kernel 的教學/研究型作業系統骨架。

## 快速開始

安裝相依（Debian/Ubuntu）：
```bash
sudo apt update
sudo apt install -y build-essential gnu-efi mtools xorriso qemu-system-x86 ovmf
```

建置與執行：
```bash
make run
```

除錯（QEMU GDB stub）：
```bash
./tools/qemu-gdb.sh
# 另開一個終端
gdb build/kernel.elf -ex "target remote :1234" -ex "b kmain" -ex "c"
```

## 目錄結構
- `boot/uefi/`：自製 UEFI Bootloader（讀取 `kernel.elf`、傳遞 BootInfo、ExitBootServices）
- `kernel/`：最小核心（序列輸出、幀緩衝上色）
- `tools/`：啟動與 GDB 連線腳本
- `scripts/`：打包腳本
- `.github/workflows/ci.yml`：GitHub Actions 自動建置與打包
