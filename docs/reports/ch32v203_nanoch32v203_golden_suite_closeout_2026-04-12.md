# CH32V203 nanoCH32V203 — Golden Suite Closeout — 2026-04-12

## Board

- **MCU**: CH32V203C8T6 (RISC-V4B @ 96 MHz, 64 KB Flash, 20 KB SRAM)
- **Board**: nanoCH32V203 (USB-C form-factor, WCHLink-compatible)
- **Board ID**: `ch32v203xxx`
- **Debug / Flash**: SWD via WCHLink (fw 2.18, PID 8010); OpenOCD + GDB (`riscv-none-embed-gdb`)
- **Pack**: [`packs/ch32v203xxx_golden.json`](../../packs/ch32v203xxx_golden.json)

## Final Result: 22/22 PASS (automated)

Verified: 2026-04-12.

| # | Test | Stage | Peripheral | Wiring | Result |
|---|------|-------|-----------|--------|--------|
| 1 | `ch32v203_systick` | 0 | SysTick 1 kHz | WCHLink only | ✅ PASS |
| 2 | `ch32v203_iwdg` | 0 | IWDG watchdog reset | WCHLink only | ✅ PASS |
| 3 | `ch32v203_rtc_self` | 0 | RTC LSI 32 kHz | WCHLink only | ✅ PASS |
| 4 | `ch32v203_flash_rw` | 0 | Flash erase/write/verify | WCHLink only | ✅ PASS |
| 5 | `ch32v203_crc` | 0 | CRC32 hardware unit | WCHLink only | ✅ PASS |
| 6 | `ch32v203_adc_vref` | 0 | ADC Vref channel | WCHLink only | ✅ PASS |
| 7 | `ch32v203_adc_temp` | 0 | ADC internal temp sensor | WCHLink only | ✅ PASS |
| 8 | `ch32v203_dma_mem2mem` | 0 | DMA memory-to-memory | WCHLink only | ✅ PASS |
| 9 | `ch32v203_can_loopback` | 0 | CAN1 loopback mode | WCHLink only | ✅ PASS |
| 10 | `ch32v203_pwr_pvd` | 0 | PVD power voltage detect | WCHLink only | ✅ PASS |
| 11 | `ch32v203_adc_dma` | 0 | ADC + DMA multi-channel | WCHLink only | ✅ PASS |
| 12 | `ch32v203_tim_onepulse` | 0 | TIM2 one-pulse mode | WCHLink only | ✅ PASS |
| 13 | `ch32v203_bkp` | 0 | BKP backup registers | WCHLink only | ✅ PASS |
| 14 | `ch32v203_gpio_loopback` | 1 | GPIO output→input | PA2↔PA3 | ✅ PASS |
| 15 | `ch32v203_exti_loopback` | 1 | EXTI edge interrupt | PA2↔PA3 | ✅ PASS |
| 16 | `ch32v203_uart_loopback` | 1 | USART1 TX→RX | PA9↔PA10 | ✅ PASS |
| 17 | `ch32v203_dma_uart` | 1 | USART1 DMA TX→RX | PA9↔PA10 | ✅ PASS |
| 18 | `ch32v203_spi_loopback` | 1 | SPI1 MOSI→MISO | PA7↔PA6 | ✅ PASS |
| 19 | `ch32v203_i2c_loopback` | 1 | I2C1 master/slave | PB6+PB7+4.7kΩ | ✅ PASS |
| 20 | `ch32v203_tim_pwm_capture` | 1 | TIM1 PWM → TIM2 capture | PA8→PA0 | ✅ PASS |
| 21 | `ch32v203_spi_dma` | 1 | SPI1 + DMA loopback | PA7↔PA6 | ✅ PASS |
| 22 | `ch32v203_tim_encoder` | 2 | TIM2+TIM3 quadrature encoder | PA0→PA6, PA1→PA7 | ✅ PASS |

**Excluded from pack (manual test)**:
- `ch32v203_usb_cdc_banner` — 需要 flash 后手动拔插 USB-C（board 电源仅来自 USB-C；WCHLink reset 后 xHCI 进入 invalid context state，需物理 VBUS 断电重连），代码与测试计划保留于 `tests/plans/ch32v203_usb_cdc_banner.json`

**Excluded from pack (bench wiring)**:
- `ch32v203_usart_halfduplex` — HDSEL 半双工需要两路 USART TX 共享单线（PA2↔PB10），bench 仅有 PA2↔PA3，不满足条件

---

## Bench Wiring

### Stage 0 (no wires)
```
WCHLink SWDIO  → CH32V203 PA13
WCHLink SWDCLK → CH32V203 PA14
WCHLink GND    → CH32V203 GND
WCHLink 3V3    → CH32V203 3V3   (可选；nanoCH32V203 由 USB-C 自供电)
USB-C 供电      → board USB-C port
```

### Stage 1 (cumulative with Stage 0)
```
PA2  ↔  PA3              GPIO loopback / EXTI
PA9(TX1) ↔ PA10(RX1)     USART1 loopback + DMA_UART
PA7(MOSI1) ↔ PA6(MISO1)  SPI1 loopback + SPI DMA
PB6(SCL1) + PB7(SDA1) + 4.7 kΩ to 3V3   I2C1
PA8(TIM1_CH1) → PA0(TIM2_CH1_ETR)        PWM capture
```

### Stage 2 (replaces PA7↔PA6 and PA8→PA0 of Stage 1)
```
PA0(TIM2_CH1) → PA6(TIM3_CH1)   encoder quadrature A
PA1(TIM2_CH2) → PA7(TIM3_CH2)   encoder quadrature B
```

---

## Problems Encountered and How They Were Solved

### Bug 1 — DMA_UART：mailbox check_mailbox 超时（3 次失败）

**症状**: `ch32v203_dma_uart` 反复 FAIL，AEL 报 `check_mailbox timeout`；USART1 DMA 收发双向需要 PA9↔PA10 跳线。

**解决**: 确认 PA9↔PA10 跳线已接入后，增加 `post_load_settle_s=1.0`，PASS。

---

### Bug 2 — USB CDC：Linux xHCI invalid context state

**症状**: 18 次 FAIL，固件烧录后 USB 设备不枚举；`dmesg` 报 `xhci_hcd: WARN: xHCI xHC not halted`。

**根因**: nanoCH32V203 **板载电源完全来自 USB-C 口（PA11/PA12）**，WCHLink 仅提供 SWD 信号无供电。WCHLink flash 结束后 OpenOCD 会对目标执行 SWD reset，Linux xHCI 控制器感知到 USB 设备"消失-重现"但未经历 VBUS 断电，进入 invalid context state，导致枚举失败。

**解决**: flash 完成后，用户手动拔插 USB-C（完整 VBUS 断电→重新上电），MCU 全面复位，USB 在 2s 内重新枚举。该流程无法自动化，测试从 golden pack 中移出，单独归档。

---

### Bug 3 — USART Half-Duplex：无法自回环

**症状**: HDSEL 模式下 TX 期间 RX 自动禁用，单根 PA2 无法自发送自接收。

**根因**: CH32V203 USART HDSEL（半双工）要求两个不同 USART 的 TX 引脚共用同一物理线；bench 只有 PA2↔PA3（Stage 1 GPIO 跳线），没有 PA2↔PB10（USART2↔USART3）连接。GPIO wire check 固件实测确认（`wire_fail=0b0101`：双向均开路）。

**解决**: 从 golden pack 中移除，归档原因。

---

### Bug 4 — TIM Encoder：Stage 2 接线替换 Stage 1

**症状**: Stage 2 encoder 测试需要 PA0→PA6、PA1→PA7，与 Stage 1 的 PA7↔PA6（SPI loopback）和 PA8→PA0（PWM capture）冲突。

**解决**: Stage 2 接线独立，替换 Stage 1 中的 PA7↔PA6 和 PA8→PA0，pack 中 Stage 0/1/2 分阶段串行执行。

---

### Bug 5 — run_index.json 损坏（Python 崩溃截断写入）

**症状**: `python3 -m ael run` 报 `JSONDecodeError: Unterminated string`；`run_index.json` 在第 597 行被截断（`"exp_id": '` 不完整）。

**根因**: CE 记录脚本在序列化 Experience 对象时 Python 崩溃，文件写入中途终止。

**解决**: 手动补全 JSON 末尾缺失内容（exp_id、confidence、结束括号），恢复文件合法性。

---

### Bug 6 — stale OpenOCD 进程阻塞端口 3334

**症状**: AEL build 成功后 flash 阶段长时间卡住；`lsof -ti :3334` 显示上次 AEL 未正常退出留下的 OpenOCD 进程仍持有 WCHLink USB 接口（`LIBUSB_ERROR_IO`）。

**解决**: 每次新 flash 前先 `kill -9 $(lsof -ti :3334)` 清理残留进程。

---

## Key Design Notes

- **AEL Mailbox 地址**: `0x20000600`（SRAM 偏移 0x600），magic=`0xAE100001`，status=2 为 PASS，detail0 在 PASS 后自增（increment 模式）。
- **96 MHz 时钟**: `system_ch32v20x.c` patch 为 96 MHz；USB 时钟 = PLL/2 = 48 MHz（`RCC_USBCLKSource_PLLCLK_Div2`）。USBD_ENDPx_DataUp 需要精确 48 MHz，96 MHz patch 不可省略。
- **WCHLink 版本要求**: fw **2.18（PID 8010）** 必须。fw 2.01 和 PID 8012 与本 OpenOCD 不兼容，flash 失败。
- **SimulateCDC USBLIB 栈**: USB CDC 使用 WCH 官方 SimulateCDC 库（`usb_lib.h`/`usb_pwr.h`/`hw_config.h`），非标准 USB device 框架；`bDeviceState == CONFIGURED` 是枚举成功的判断标志。
- **OpenOCD GDB server**: WCH OpenOCD 不支持 `flash write_image`；flash 必须通过 `riscv-none-embed-gdb` 连接 `:3334` 后执行 `load`。

---

## Canonical Result

- **DUT**: CH32V203C8T6 nanoCH32V203 (RISC-V4B @ 96 MHz, WCHLink fw 2.18)
- **Pack**: [`packs/ch32v203xxx_golden.json`](../../packs/ch32v203xxx_golden.json)
- **Status**: `golden` — 22/22 PASS (automated), 2026-04-12
- **Manual test archived**: `tests/plans/ch32v203_usb_cdc_banner.json` (requires physical USB-C power cycle after flash)
