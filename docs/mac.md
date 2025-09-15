# macOS（含 OrbStack）建置與啟動指南

本專案推薦以 Docker（OrbStack 後端）進行建置，再於主機以 QEMU 啟動 ISO。此流程避免在主機安裝 gnu-efi 與 mtools/xorriso。

## 1. 前置環境
- 安裝 OrbStack 或 Docker Desktop（建議 OrbStack，啟動/IO 較快）
- Homebrew 安裝 QEMU：
  ```bash
  brew install qemu
  ```

## 2. 切換 Docker context（OrbStack）
```bash
docker context use orbstack
docker buildx ls   # 確認有 default builder
```

## 3. 以容器建置專案（linux/amd64）
```bash
docker buildx build --platform linux/amd64 -t starforge-build .
docker run --rm -ti --platform=linux/amd64 -v "$(pwd)":/work -w /work starforge-build \
  bash -lc 'make clean && make -j$(nproc)'
```

產出檔：`starforge.iso` 與 `dist/` 內打包檔案。

## 4. 設定 OVMF 並啟動
Homebrew 的 OVMF 通常放在 QEMU 的 share 目錄（非 edk2-ovmf 公式）。

常見路徑：
- Apple Silicon/Intel（Homebrew）：`/opt/homebrew/share/qemu/edk2-x86_64-code.fd`
- Intel（舊 Homebrew 前綴）：`/usr/local/share/qemu/edk2-x86_64-code.fd`

設定環境變數並執行：
```bash
export OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd
make run
```

若使用 `./tools/qemu-run.sh`，腳本會嘗試自動偵測上述路徑，找不到時才提示錯誤與設定方式。

## 5. 效能注意事項（Apple Silicon）
- x86_64 在 Apple Silicon 上屬全模擬，效能顯著不如原生。
- 建議：
  - 提高 QEMU 記憶體（例如 `-m 1024` 或更高，腳本已設 512 MB，可自行調整）。
  - 減少序列輸出或密集 I/O。
  - 僅於容器建置；執行則儘量縮短測試時間。

