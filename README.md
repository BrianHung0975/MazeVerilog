# 32x32 捲動視窗迷宮遊戲 FPGA 硬體專案（Verilator + SDL2 模擬）

本專案實作 32x32 捲動視窗迷宮遊戲之 Verilog HDL 設計（目標晶片 Altera Cyclone `EP1C6Q240C8N` 與北瀚科技 SMIMS VeriLite 開發板），並提供可在 macOS 上直接執行的 Verilator + SDL2 即時互動模擬器與自動化測試平台。

## 專案結構

- `rtl/maze_32x32_top.v`：Verilog-2001 頂層模組，完整支援 Quartus II 13.0sp1 合成與 Verilator 模擬。
- `quartus/maze_32x32_top.qsf`：Quartus II 腳位限制與未用腳位三態保護設定。
- `sim/sim_sdl.cpp`：Verilator + SDL2 互動式模擬器（模擬 VeriInstrument 16x16 點陣 LED、兩顆七段顯示器、D0/D1 指示燈、虛擬方向鍵、BT0 重置、SW0 極性開關與可切換的 32x32 除錯小地圖）。
- `sim/tb_maze.cpp`：自動化 C++ 測試程式，驗證系統功能並產生 VCD 波形檔 `build/maze_test.vcd`。
- `Makefile`：編譯、執行模擬器、跑測試與語法檢查腳本。

## 設計調整說明

1. **迷宮通路修正**：原設計文件的 32x32 迷宮從起點 `(1, 1)` 出發在第 5～6 列即被牆壁封死（僅可到達 16 格），無法抵達終點 `(30, 30)`。本專案在不破壞整體迷宮結構的前提下，於 11 處牆壁打通單格通道（座標 `(4,5)`, `(8,5)`, `(10,5)`, `(15,10)`, `(15,12)`, `(20,16)`, `(22,18)`, `(22,20)`, `(24,24)`, `(27,26)`, `(27,28)`），使起點至終點最短路徑為 **90 步**，恰好符合兩位數 BCD 七段顯示器（`00`～`99`）的上限。
2. **模擬時間縮放參數 `SIM_SHIFT`**：
   - 預設值 `SIM_SHIFT = 0`，所有計數器與除頻邏輯維持原始 12 MHz 硬體規格，直接用於 Quartus II 合成不需任何修改。
   - SDL2 互動模擬器使用 `SIM_SHIFT = 8`（等效 46.875 kHz 時脈），於 60 FPS 下維持與實機一致的閃爍體感與 20ms 除彈跳手感。
   - 自動化測試使用 `SIM_SHIFT = 12` 以加速驗證。

## macOS 模擬環境建置與執行

### 先決條件

需先透過 Homebrew 安裝 `verilator` 與 `sdl2`：

```bash
brew install verilator sdl2 pkg-config
```

### 常用指令

```bash
# 1. RTL 語法與 Lint 檢查
make lint

# 2. 執行自動化驗證測試（涵蓋 5 項檢核並輸出 build/maze_test.vcd）
make test

# 3. 編譯並啟動 SDL2 互動式模擬器
make run
```

若要在互動模擬時同時錄製 VCD 波形，可直接執行：

```bash
./build/maze_sim --vcd build/play.vcd
```

### 模擬器操作說明

- **方向鍵 / `WASD`**：控制玩家移動（亦可直接用滑鼠點擊畫面上的 `UP` / `DN` / `LT` / `RT`）。
- **`R` 鍵 / 長按 `BT0 RST`**：系統重置（Active-Low `rst_n`）。
- **`I` 鍵 / 點擊 `SW0 INV`**：切換點陣 Row 極性反轉（可重現實機 Row 極性相反時的反白現象）。
- **`M` 鍵 / 點擊 `MAP DBG`**：開啟或關閉右側 32x32 全圖與 16x16 攝影機視窗除錯檢視。
- **`ESC`**：結束模擬器。

## Quartus II 13.0sp1 與 VeriInstrument 燒錄步驟

1. 建立 Quartus II 專案並選擇晶片 `EP1C6Q240C8`，將 `rtl/maze_32x32_top.v` 設為 Top-Level Entity（維持預設 `SIM_SHIFT = 0`）。
2. 將 `quartus/maze_32x32_top.qsf` 內容加入專案 `.qsf` 檔末尾後執行 Full Compilation。
3. 透過 `File -> Convert Programming Files` 產生 Passive Serial 無壓縮 `.rbf` 檔。
4. 使用 VeriInstrument 載入 `.rbf` 下載至 SMIMS VeriLite 開發板，設定系統時脈為 `12000000` Hz 並點擊 Go 啟動。
