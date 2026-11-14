# 燒錄 ATtiny85

用 USBasp 燒錄器加 Arduino IDE 2。以下是在 Windows 11 上實際走過的流程，包含遇到的坑。

## 需要的東西

- USBasp 燒錄器（10 針排線版）
- ATtiny85-20PU 裸晶片、麵包板、公對公杜邦線
- Arduino IDE 2.x
- [Zadig](https://zadig.akeo.ie/)（Windows 驅動）

不建議用 Digispark 開發板代替裸晶片：它的 PB3、PB4 上焊了 USB 用的電阻和稽納二極體，會干擾遠燈偵測，開機還有 5 秒的 bootloader 延遲。

## 1. 安裝 USBasp 驅動

插上 USBasp，打開 Zadig，選 **USBasp**，驅動選 **libusbK**，按 Install / Replace Driver。

- Arduino AVR Boards 附的 avrdude 8.0 配 libusb-win32 驅動會出現 `cannot open USB device: Permission denied`，所以要用 libusbK（不行再試 WinUSB）。

## 2. 安裝 ATTinyCore 1.5.2

開發板管理員的網址（偏好設定 → 其他開發板管理員網址）用 GitHub 上的備用網址：

```
https://raw.githubusercontent.com/SpenceKonde/ReleaseScripts/refs/heads/master/package_drazzy.com_index.json
```

原本的 `drazzy.com` 網址憑證過期，IDE 會搜尋不到 ATTinyCore。

**如果安裝失敗（卡在 micronucleus）**：改用手動安裝。

1. 開發板管理員先裝好 `Arduino AVR Boards`（提供 avr-gcc 和 avrdude）。
2. 下載 `https://github.com/SpenceKonde/ATTinyCore/archive/refs/tags/1.5.2.zip`，解壓縮後把裡面的 `avr` 資料夾複製到「草稿碼簿位置」底下的 `hardware\ATTinyCore\avr`。草稿碼簿位置在偏好設定裡看，不一定是「文件\Arduino」。
3. 重開 IDE，開發板會出現在「ATTinyCore (in sketchbook)」底下。

**手動安裝的話，avrdude.conf 要換掉**：ATTinyCore 1.5.2 附的 `avrdude.conf` 是舊格式，avrdude 8.0 會報 `unknown token` / `syntax error`。把 `hardware\ATTinyCore\avr\avrdude.conf` 備份後，用 avrdude 8 自己的設定檔取代：

```
%LOCALAPPDATA%\Arduino15\packages\arduino\tools\avrdude\8.0.0-arduino1\etc\avrdude.conf
```

## 3. 接線

USBasp 的電壓跳帽放 **5V**。新晶片出廠是 1MHz，第一次燒錄要**短接 JP3**（降速）；之後插著也能燒，只是比較慢。

| USBasp 10 針 | 功能 | ATtiny85 腳位 |
|---|---|---|
| 1 | MOSI | 5 |
| 2 | VCC 5V | 8 |
| 5 | RST | 1 |
| 7 | SCK | 7 |
| 9 | MISO | 6 |
| 8 或 10 | GND | 4 |

- 排線的紅線那端是第 1 腳。
- 標準 USBasp 的 4、6、8、10 腳都是 GND，但**有些板子 4、6 腳是空接**，只有 8、10 是 GND。不確定的話用電錶量。
- 晶片缺口旁的小圓點是第 1 腳，逆時針數到第 8 腳。

## 4. 開發板設定

| 項目 | 設定 |
|---|---|
| 開發板 | ATtiny25/45/85 (No bootloader) |
| Chip | ATtiny85 |
| Clock Source | 8 MHz (internal) |
| B.O.D. Level | B.O.D. Enabled (4.3v) |
| Save EEPROM | 預設 |
| LTO | Enabled |
| millis()/micros() | Enabled |
| Timer 1 Clock | CPU |
| 燒錄器 | USBasp (ATTinyCore) |

## 5. 燒錄

1. **工具 → 燒錄 Bootloader**：選 No bootloader 時，這一步只會寫入熔絲（8MHz、BOD）並清空晶片。每顆晶片做一次。
2. **草稿碼 → 使用燒錄器上傳**（Ctrl+Shift+U）。不是工具列的箭頭按鈕。

順序不能反過來：燒錄 Bootloader 會清空晶片，先上傳程式的話會被清掉。

成功時會看到：

- 熔絲：`lfuse 0xE2`、`hfuse 0xD4`、`efuse 0xFF`，各自 `1 byte written, 1 verified`
- 程式：約 1500 bytes `flash verified`
- `cannot set sck period` 是便宜版 USBasp 韌體的老問題，可以忽略
- `Avrdude done. Thank you.` 不管成功失敗都會出現，不能拿來判斷；要看有沒有 `Error:` 開頭的行

## 讀取熔絲確認

```
avrdude -C <avrdude.conf 路徑> -c usbasp -p attiny85 -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h
```

lfuse 是 `0x62` 代表還在出廠的 1MHz，熔絲沒寫進去：所有時間會慢 8 倍（按住遠燈要 16 秒才提示）。

## 桌上測試

燒完先在麵包板上測：LED 長腳接 5V、短腳經 1k 接晶片第 6 腳，第 8 腳接 5V、第 4 腳接地，**第 2 腳經 10k 接地**（沒接的話會浮接亂讀）。通電時 LED 閃一下就代表程式在跑。拿一條線從 5V 碰第 2 腳約 2 秒，LED 會快閃，這時把線拿開，LED 就會恆亮（近燈切成開）。
