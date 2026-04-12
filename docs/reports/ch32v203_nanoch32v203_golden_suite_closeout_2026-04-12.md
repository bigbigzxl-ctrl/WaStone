# CH32V203 nanoCH32V203 — Golden Suite Closeout — 2026-04-12

## Board

- **MCU**: CH32V203C8T6 (RISC-V4B @ 96 MHz, 64 KB Flash, 20 KB SRAM)
- **Board**: nanoCH32V203 (USB-C form-factor, WCHLink-compatible)
- **Board ID**: `ch32v203xxx`
- **Debug / Flash**: SWD via WCHLink (fw 2.18, PID 8010); OpenOCD + GDB (`riscv-none-embed-gdb`)
- **Pack**: [`packs/ch32v203xxx_golden.json`](../../packs/ch32v203xxx_golden.json)

## Final Result: 23/23 PASS (automated)

Verified: 2026-04-12.

| # | Test | Stage | Peripheral | Wiring | Result |
|---|------|-------|-----------|--------|--------|
| 1 | `ch32v203_blinky_visual` | 0 | GPIO PA15 LED (visual, no mailbox) | WCHLink only | ✅ PASS |
| 2 | `ch32v203_blinky_mailbox` | 0 | GPIO PA15 LED + AEL mailbox | WCHLink only | ✅ PASS |
| 3 | `ch32v203_systick` | 1 | SysTick 1 kHz | WCHLink only | ✅ PASS |
| 4 | `ch32v203_iwdg` | 1 | IWDG watchdog reset | WCHLink only | ✅ PASS |
| 5 | `ch32v203_rtc_self` | 1 | RTC LSI 32 kHz | WCHLink only | ✅ PASS |
| 6 | `ch32v203_flash_rw` | 1 | Flash erase/write/verify | WCHLink only | ✅ PASS |
| 7 | `ch32v203_crc` | 1 | CRC32 hardware unit | WCHLink only | ✅ PASS |
| 8 | `ch32v203_adc_vref` | 1 | ADC Vref channel | WCHLink only | ✅ PASS |
| 9 | `ch32v203_adc_temp` | 1 | ADC internal temp sensor | WCHLink only | ✅ PASS |
| 10 | `ch32v203_dma_mem2mem` | 1 | DMA memory-to-memory | WCHLink only | ✅ PASS |
| 11 | `ch32v203_can_loopback` | 1 | CAN1 loopback mode | WCHLink only | ✅ PASS |
| 12 | `ch32v203_pwr_pvd` | 1 | PVD power voltage detect | WCHLink only | ✅ PASS |
| 13 | `ch32v203_adc_dma` | 1 | ADC + DMA multi-channel | WCHLink only | ✅ PASS |
| 14 | `ch32v203_tim_onepulse` | 1 | TIM2 one-pulse mode | WCHLink only | ✅ PASS |
| 15 | `ch32v203_bkp` | 1 | BKP backup registers | WCHLink only | ✅ PASS |
| 16 | `ch32v203_wire_check` | 2 | GPIO continuity (pre-stage gate) | PA2↔PA3, PA9↔PA10, PA7↔PA6, PA8→PA0 | ✅ PASS |
| 17 | `ch32v203_gpio_loopback` | 2 | GPIO output→input | PA2↔PA3 | ✅ PASS |
| 18 | `ch32v203_exti_loopback` | 2 | EXTI edge interrupt | PA2↔PA3 | ✅ PASS |
| 19 | `ch32v203_uart_loopback` | 2 | USART1 TX→RX | PA9↔PA10 | ✅ PASS |
| 20 | `ch32v203_dma_uart` | 2 | USART1 DMA TX→RX | PA9↔PA10 | ✅ PASS |
| 21 | `ch32v203_spi_loopback` | 2 | SPI1 MOSI→MISO | PA7↔PA6 | ✅ PASS |
| 22 | `ch32v203_tim_pwm_capture` | 2 | TIM1 PWM → TIM2 capture | PA8→PA0 | ✅ PASS |
| 23 | `ch32v203_spi_dma` | 2 | SPI1 + DMA loopback | PA7↔PA6 | ✅ PASS |

**Excluded from pack**:
- `ch32v203_i2c_loopback` — bench に 4.7kΩ プルアップ抵抗（PB6/PB7）なし → 排除
- `ch32v203_tim_encoder` — 需要另设接线（PA0→PA6, PA1→PA7，与 Stage-2 SPI/PWM 跳线冲突）
- `ch32v203_usart_halfduplex` — 需要 PA2↔PB10（USART2↔USART3），bench 不满足
- `ch32v203_usb_cdc_banner` — 需要 flash 后手动拔插 USB-C，不可自动化

---

## Bench Wiring

### Stage 0 (no wires — LED baseline)
```
WCHLink SWDIO  → CH32V203 PA13
WCHLink SWDCLK → CH32V203 PA14
WCHLink GND    → CH32V203 GND
USB-C 供电      → board USB-C port
```

### Stage 1 (no wires — internal peripherals)
```
同 Stage 0，无额外接线
```

### Stage 2 (GPIO jumpers)
```
PA2  ↔  PA3              GPIO loopback / EXTI / wire_check
PA9(TX1) ↔ PA10(RX1)     USART1 loopback + DMA_UART / wire_check
PA7(MOSI1) ↔ PA6(MISO1)  SPI1 loopback + SPI DMA / wire_check
PA8(TIM1_CH1) → PA0(TIM2_CH1_ETR)  PWM capture / wire_check
```

---

## Problems Encountered and How They Were Solved

### Bug 1 — blinky_visual / blinky_mailbox：build fail (elf not found)

**症状**: AEL `arm_debug` build 调用 `make` 时传入 `OUT_ELF=.../artifacts/...`，但 Makefile 用硬编码的 `$(TARGET).elf` 忽略该变量，ELF 落在 project_dir 而非 artifacts，AEL 报 `elf not found`。

**解决**: 将两个 Makefile 的输出目标改为 `OUT_ELF ?= $(TARGET).elf` / `OUT_BIN ?= $(TARGET).bin`，与其他 ch32v203 目标对齐。

---

### Bug 2 — wire_check：detail0_increment 检查必然失败

**症状**: 测试计划 `check_mode=detail0_increment`，但固件在 PASS 路径写死 `0x5A5A`，AEL 两次读到相同值，报 `toggle_count did not increment`。

**解决**: 固件改为 `AEL_MAILBOX->detail0 = ++tick`，每个循环自增，满足 increment 检查。同时清理测试计划中从 USB-CDC 测试复制的多余 `skip_attach` / `detach_resume_target` 字段。

---

### Bug 3 — DMA_UART：mailbox check_mailbox 超时（3 次失败）

**症状**: `ch32v203_dma_uart` 反复 FAIL，AEL 报 `check_mailbox timeout`；USART1 DMA 收发双向需要 PA9↔PA10 跳线。

**解决**: 确认 PA9↔PA10 跳线已接入后，增加 `post_load_settle_s=1.0`，PASS。

---

### Bug 4 — I2C Loopback：bench 无 4.7kΩ 上拉电阻

**症状**: I2C1 (PB6/PB7) 开漏总线无外部上拉，mailbox 报 FAIL (status=FAIL)。

**解决**: 从 golden pack 中移除，排除原因记录为"bench 无 PB6/PB7 上拉"。

---

### Bug 5 — USB CDC：Linux xHCI invalid context state

**症状**: 18 次 FAIL，固件烧录后 USB 设备不枚举。

**根因**: nanoCH32V203 板载电源完全来自 USB-C 口，WCHLink SWD reset 后 Linux xHCI 进入 invalid context state。

**解决**: flash 后手动拔插 USB-C（完整 VBUS 断电→重新上电）。该流程无法自动化，移出 golden pack 归档。

---

### Bug 6 — run_index.json 损坏（Python 崩溃截断写入）

**症状**: `JSONDecodeError: Unterminated string`，文件在第 597 行被截断。

**解决**: 手动补全 JSON 末尾缺失内容，恢复文件合法性。

---

### Bug 7 — stale OpenOCD 进程阻塞端口 3334

**症状**: AEL build 成功后 flash 阶段长时间卡住（`LIBUSB_ERROR_IO`）。

**解决**: 每次新 flash 前先 `kill -9 $(lsof -ti :3334)` 清理残留进程。

---

## Key Design Notes

- **Stage 0 = board health baseline**: blinky_visual（无 mailbox，肉眼确认 LED）→ blinky_mailbox（有 mailbox，确认 AEL 管道通）。
- **Stage 2 gate**: wire_check 作为 Stage 2 第一项，验证所有跳线连通后再运行外设测试。
- **LED D1**: PA15 LOW=ON，板载 10kΩ 限流电阻。
- **AEL Mailbox**: `0x20000600`，magic=`0xAE100001`，status=2=PASS，detail0 自增。
- **96 MHz 时钟**: `system_ch32v20x.c` patch，USB 时钟=PLL/2=48 MHz。
- **WCHLink**: fw **2.18（PID 8010）** 必须；fw 2.01 和 PID 8012 不兼容。

---

## Canonical Result

- **DUT**: CH32V203C8T6 nanoCH32V203 (RISC-V4B @ 96 MHz, WCHLink fw 2.18)
- **Pack**: [`packs/ch32v203xxx_golden.json`](../../packs/ch32v203xxx_golden.json)
- **Status**: `golden` — 23/23 PASS (automated), 2026-04-12
- **Excluded (no bench HW)**: `ch32v203_i2c_loopback` (no pull-ups), `ch32v203_tim_encoder` (wiring conflict)
- **Excluded (manual)**: `ch32v203_usb_cdc_banner` (power cycle required), `ch32v203_usart_halfduplex` (missing wire)
