# CH32V305RBT6 Golden Test Suite — Validation Report

**Date:** 2026-04-12  
**Board:** CH32V305RBT6 (nanoCH32V305, RISC-V4F @ 96MHz, 128 KB Flash, 32 KB SRAM)  
**Instrument:** WCHLink-E fw 2.18 (PID 8010), SDI interface  
**Pack:** `packs/ch32v305xxx_golden.json`  
**Result:** ✅ **25/25 PASS**

---

## Test Results

| # | Test | Stage | Covers | Wiring | Result |
|---|------|-------|--------|--------|--------|
| 1 | ch32v305_blinky_visual | 0 | gpio, cpu | WCHLink only | PASS |
| 2 | ch32v305_blinky_mailbox | 0 | gpio, mailbox | WCHLink only | PASS |
| 3 | ch32v305_systick | 1 | systick, timer | WCHLink only | PASS |
| 4 | ch32v305_iwdg | 1 | iwdg, lsi | WCHLink only | PASS |
| 5 | ch32v305_rtc_self | 1 | rtc, lsi | WCHLink only | PASS |
| 6 | ch32v305_flash_rw | 1 | flash | WCHLink only | PASS |
| 7 | ch32v305_crc | 1 | crc | WCHLink only | PASS |
| 8 | ch32v305_adc_vref | 1 | adc, vrefint | WCHLink only | PASS |
| 9 | ch32v305_adc_temp | 1 | adc, temp_sensor | WCHLink only | PASS |
| 10 | ch32v305_adc_dma | 1 | adc, dma | WCHLink only | PASS |
| 11 | ch32v305_dma_mem2mem | 1 | dma | WCHLink only | PASS |
| 12 | ch32v305_can_loopback | 1 | can | WCHLink only | PASS |
| 13 | ch32v305_pwr_pvd | 1 | pwr, pvd | WCHLink only | PASS |
| 14 | ch32v305_bkp | 1 | bkp | WCHLink only | PASS |
| 15 | ch32v305_rng | 1 | rng | WCHLink only | PASS |
| 16 | ch32v305_wire_check | 2 | gpio, connectivity | 4 jumpers | PASS |
| 17 | ch32v305_gpio_loopback | 2 | gpio | PA1↔PA2 | PASS |
| 18 | ch32v305_exti_loopback | 2 | exti, gpio | PA1↔PA2 | PASS |
| 19 | ch32v305_uart_loopback | 2 | usart | PA9↔PA10 | PASS |
| 20 | ch32v305_dma_uart | 2 | usart, dma | PA9↔PA10 | PASS |
| 21 | ch32v305_spi_loopback | 2 | spi | PA7↔PA6 | PASS |
| 22 | ch32v305_spi_dma | 2 | spi, dma | PA7↔PA6 | PASS |
| 23 | ch32v305_tim_pwm_capture | 2 | timer, pwm | PA8→PA0 | PASS |
| 24 | ch32v305_tim_onepulse | 2 | timer, opm | WCHLink only | PASS |
| 25 | ch32v305_usart_halfduplex | 2 | usart, half_duplex | PA9↔PA10 | PASS |

---

## Peripheral Coverage

| Peripheral | Test(s) | Method |
|-----------|---------|--------|
| GPIO | blinky_visual, gpio_loopback, wire_check | Output toggle + PA1↔PA2 loopback |
| SysTick | systick | 100ms tick count via mailbox |
| IWDG + LSI | iwdg | Feed loop with timeout detection |
| RTC + LSI | rtc_self | 32kHz oscillator, 2-second increment verify |
| Flash (internal) | flash_rw | Fast-program page erase + write + readback |
| CRC unit | crc | Hardware CRC32, SW cross-validation |
| ADC1 (Vrefint) | adc_vref | Channel 17, range 800..2100 |
| ADC1 (temp sensor) | adc_temp | Channel 16, range 1500..2600 |
| ADC1 + DMA1_Ch1 | adc_dma | 16-sample scan, Vrefint ch17 |
| DMA1 (M2M) | dma_mem2mem | SRAM→SRAM, 16 words, compare |
| CAN1 | can_loopback | Internal loopback (LBKM=1), no transceiver |
| PWR + PVD | pwr_pvd | PVD threshold set/detect, PVDO flag |
| BKP registers | bkp | BKP_DR1..DR4 write/read with 4 patterns |
| RNG (hardware) | rng | 8 samples, diversity + no all-0/all-F |
| EXTI2 | exti_loopback | PA1 drive → PA2 EXTI rising edge, 5 counts |
| USART1 | uart_loopback, dma_uart | PA9↔PA10 polling + DMA1_Ch4/5 loopback |
| SPI1 | spi_loopback, spi_dma | PA7↔PA6 MOSI↔MISO + DMA1_Ch2/3 |
| TIM1 + TIM2 | tim_pwm_capture | TIM1 1kHz PWM (PA8) → TIM2 capture (PA0) |
| TIM2 (OPM) | tim_onepulse | One-pulse mode, UIF flag verify, no wiring |
| USART1 (HDSEL) | usart_halfduplex | PA9 AF_OD + PA10 GPIO PP bit-bang echo |

---

## Bench Wiring

### Stage 0 & 1 — WCHLink Only

| Signal | Connection |
|--------|-----------|
| SWDIO | WCHLink → PA13 |
| SWDCLK | WCHLink → PA14 |
| GND | WCHLink → GND |

### Stage 2 — WCHLink + 4 jumpers

| Jumper | Pins | Tests |
|--------|------|-------|
| J1 | PA1 ↔ PA2 | wire_check, gpio_loopback, exti_loopback |
| J2 | PA9 ↔ PA10 | wire_check, uart_loopback, dma_uart, usart_halfduplex |
| J3 | PA7 ↔ PA6 | wire_check, spi_loopback, spi_dma |
| J4 | PA8 → PA0 | wire_check, tim_pwm_capture |

*No extra wiring for tim_onepulse (self-test via UIF flag).*

---

## Key Engineering Notes

### 1. RISC-V アーキテクチャフラグ
CH32V305 は RV32IMAFCXW（浮動小数点付き）:
```
-march=rv32imafcxw -mabi=ilp32f
```
CH32V203 の `rv32imac` と異なる。F 拡張がないと F レジスタ命令がエラーになる。

### 2. HAL コンパイルマクロ
```c
-DCH32V30x_D8C   // CH32V305/307 向け peripheral library に必須
```

### 3. RNG ヘッダの手動インクルード
`ch32v30x_conf.h` は `ch32v30x_rng.h` を include していない（他の周辺機器と異なる）。RNG 使用時は main.c で直接 include が必要:
```c
#include "ch32v30x_rng.h"   // NOT in ch32v30x_conf.h — must be explicit
```

### 4. ADC クロック上限
CH32V305 ADC の最大入力クロックは 14 MHz。96 MHz APB2 を使う場合:
```c
RCC_ADCCLKConfig(RCC_PCLK2_Div8);   // 96MHz / 8 = 12MHz (within 14MHz max)
```

### 5. TIM2 は 32-bit カウンタ
CH32V203 の TIM2 は 16-bit だが、CH32V305 の TIM2 は 32-bit。PWM キャプチャ差分演算で `& 0xFFFF` マスクは不要（むしろ誤った結果になる）。

### 6. TIM PSC ダブルバッファリング
PSC を設定した後は必ず EGR.UG=1 で即時反映させる（HIGH_PRIORITY pattern `27de4499`）:
```c
TIM_GenerateEvent(TIMx, TIM_EventSource_Update);
```

### 7. USART HDSEL モードの RX ガード時間
CH32V30x の HDSEL モードでは **送信中は RX が無効化される**（CH32V20x と異なる）。TC 後 RX が再有効化されるまでに約 1〜2 bit 周期の守護時間がある。echo テスト設計:
- 送信側: USART1 HDSEL on PA9 (AF_OD)
- echo 側: PA10 GPIO PP bit-bang（PA9↔PA10 ジャンパ経由）
- **TC 後 3 bit 周期 (≈625µs @ 4800 baud) 待ってから echo start bit を送出**
- タイミング: TIM2 (PSC=0, ARR=19999, APB1 timer 96MHz → 正確 4800 baud)

### 8. DMA チャンネルマッピング (CH32V305)
| DMA | Ch | Peripheral |
|-----|----|-----------|
| DMA1 | Ch1 | ADC1 |
| DMA1 | Ch2 | SPI1_RX |
| DMA1 | Ch3 | SPI1_TX |
| DMA1 | Ch4 | USART1_TX |
| DMA1 | Ch5 | USART1_RX |

### 9. delay_us スケーリング
96 MHz ベースで `us * 96` サイクル（72 MHz の CH32V203 の `us * 72` と区別）。

### 10. CAN ループバック
`BTR.LBKM=1` で内部ループバック有効化、外部トランシーバ不要。最低 1 つの RX フィルタ設定が必須（なければ FIFO は空のまま）。

---

## 発見した問題と修正

| 問題 | 症状 | 根本原因 | 修正 |
|------|------|---------|------|
| RNG ビルドエラー | `RNG_FLAG_DRDY` 未定義 | `ch32v30x_conf.h` が `ch32v30x_rng.h` を include しない | main.c で明示的に `#include "ch32v30x_rng.h"` |
| USART HDSEL データ不一致 | 受信 0xE9 (expected 0xA5) | TC 後 RX ガード時間不足 (17µs vs 必要 400µs+) | TC 後 `wait_bit()×3` (625µs) 待ち |
| BKP Makefile ビルドエラー | リンクエラー | HAL ソースなし（テンプレートコピー） | Makefile に ch32v30x_pwr.c, ch32v30x_bkp.c 追加 |

---

## Deferred Tests

| Test | 理由 |
|------|------|
| ch32v305_i2c_loopback | PB6/PB7↔PB10/PB11 の追加ジャンパ配線が必要 |
| ch32v305_dac_adc | DAC + ADC 両方の外部ループバック配線が必要 |
| ch32v305_usb_cdc_banner | USB CDC は USB コネクタ経由の observe_uart が必要 |

---

## Civilization Engine Reference

| 種別 | ID | 内容 |
|------|-----|------|
| Golden suite closure | `7434ef74` | CH32V305RBT6 25/25 PASS 完了記録 |
| HDSEL guard time fix | `f903f0ac` | CH32V30x HDSEL TC 後 guard time + bit-bang echo 設計 |
| TIM PSC double-buffer | `27de4499` | [HIGH_PRIORITY] EGR.UG=1 after PSC (cross-board) |
| HardFault handler | `3f13ca66` | [HIGH_PRIORITY] ARM Cortex-M bare-metal HardFault 必須 |
