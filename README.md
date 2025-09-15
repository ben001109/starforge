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
## mac + OrbStack
    - Docker context：`docker context use orbstack`（OrbStack 提供更輕量的 Docker 後端）
    - 建置與編譯（容器）：
      ```bash
      docker buildx build --platform linux/amd64 -t starforge-build .
      docker run --rm -ti --platform=linux/amd64 -v "$(pwd)":/work -w /work starforge-build \
        bash -lc 'make clean && make -j$(nproc)'
      ```
    - 安裝 QEMU（主機，用於啟動 ISO）：`brew install qemu`
    - 設定 OVMF 並啟動：
      ```bash
      export OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd
      make run
      ```
    - 其他常見 OVMF 路徑（Intel Homebrew）：`/usr/local/share/qemu/edk2-x86_64-code.fd`
    - 更完整的 mac 說明見：`docs/mac.md`

> Apple Silicon 跑 x86_64 為全模擬，效能顯著低於原生（建議增大 QEMU 記憶體、減少不必要輸出）。

## Windows
    - WSL2（推薦）
      - 安裝 Ubuntu（WSL2），進入後安裝依賴：
        ```bash
        sudo apt update && sudo apt install -y build-essential gnu-efi mtools dosfstools xorriso qemu-system-x86 ovmf
        ```
      - 直接在 WSL2 內建置：`make -j$(nproc)`；或使用容器：
        ```bash
        docker buildx build --platform linux/amd64 -t starforge-build .
        docker run --rm -ti --platform=linux/amd64 -v "$PWD":/work -w /work starforge-build bash -lc 'make clean && make -j$(nproc)'
        ```
      - 啟動（在 Windows 主機或 WSL2 內）：
        - Windows 主機安裝 QEMU 後，設定 OVMF 路徑（依安裝來源而定）；或在 WSL2 內 `export OVMF_CODE=/usr/share/OVMF/OVMF_CODE.fd && make run`
    - 原生 Windows（可選，不保證）
      - 可研究以 MinGW-w64/Clang 建置，但 gnu-efi 與工具鏈路徑差異較大、不在主要支援範圍；建議採 WSL2。

## Troubleshooting
    - OVMF_CODE not found：
      - macOS（Homebrew）：`/opt/homebrew/share/qemu/edk2-x86_64-code.fd` 或 `/usr/local/share/qemu/edk2-x86_64-code.fd`
      - Linux：`/usr/share/OVMF/OVMF_CODE.fd` 或 `/usr/share/edk2-ovmf/OVMF_CODE.fd`
      - 設定範例：`export OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd`
    - gnu-efi 缺少（原生編譯）：
      - Linux：安裝 `gnu-efi` 套件，或改走容器（`make docker-build && make docker-make`）
    - QEMU 未安裝：
      - macOS：`brew install qemu`；Linux：`sudo apt install qemu-system-x86`
