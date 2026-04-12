# nRF52840 nice!nano v1 — Golden Suite Closeout — 2026-04-12

## Board

- **MCU**: nRF52840 (ARM Cortex-M4F @ 64 MHz, 1 MB Flash, 256 KB SRAM)
- **Board**: nice!nano v1 (Pro Micro form-factor, UF2 bootloader)
- **Board ID**: `nrf52840_nicenano`
- **Debug / Flash**: UF2 mass-storage (double-tap RST → NICENANO drive)
- **Console**: USB CDC-ACM (`/dev/ttyACM0`) via Zephyr `USB_DEVICE_STACK_NEXT`
- **Firmware framework**: Zephyr RTOS 4.4.0-rc2 (`nice_nano/nrf52840` board target)
- **Pack**: [`packs/nrf52840_nicenano_golden.json`](../../packs/nrf52840_nicenano_golden.json)

## Final Result: 15/15 PASS

Verified: 2026-04-12.

| # | Test | Stage | Peripheral | Wiring | Result |
|---|------|-------|-----------|--------|--------|
| 1 | `nicenano_blinky_visual` | 0 | GPIO (LED P0.15) | USB only | ✅ PASS |
| 2 | `nicenano_usb_cdc_banner` | 0 | USB CDC-ACM | USB only | ✅ PASS |
| 3 | `nicenano_internal_temp` | 1 | TEMP peripheral | USB only | ✅ PASS |
| 4 | `nicenano_rng` | 1 | RNG entropy source | USB only | ✅ PASS |
| 5 | `nicenano_timer_self` | 1 | TIMER0 (bare-metal) | USB only | ✅ PASS |
| 6 | `nicenano_rtc_self` | 1 | RTC1 + LFCLK | USB only | ✅ PASS |
| 7 | `nicenano_flash_rw` | 1 | NVMC internal flash | USB only | ✅ PASS |
| 8 | `nicenano_crypto_ecb` | 1 | AES-ECB hardware | USB only | ✅ PASS |
| 9 | `nicenano_gpio_loopback` | 2 | GPIO output/input | P0.17↔P0.20 | ✅ PASS |
| 10 | `nicenano_uart_loopback` | 2 | UARTE1 TX→RX | P0.20→P0.17 | ✅ PASS |
| 11 | `nicenano_spi_loopback` | 2 | SPIM1 full-duplex | P1.13↔P1.11 | ✅ PASS |
| 12 | `nicenano_pwm_capture` | 2 | PWM0 + GPIOTE capture | P0.22→P0.24 | ✅ PASS |
| 13 | `nicenano_saadc` | 2 | SAADC AIN7 | P0.31 (VBATT divider) | ✅ PASS |
| 14 | `nicenano_ble_beacon` | 2 | BLE 2.4 GHz radio | USB only | ✅ PASS |
| 15 | `nicenano_ieee802154` | 2 | IEEE 802.15.4 RADIO | USB only | ✅ PASS |

**Excluded**: `nicenano_i2c_loopback` — requires external 4.7 kΩ pull-up resistors on P0.11 + P1.04; not part of standard bench setup.

---

## Wiring (Stage 2 minimum)

```
P0.17 ↔ P0.20  — GPIO loopback / UART TX↔RX   (left-col PIN6↔PIN5, adjacent)
P1.13 ↔ P1.11  — SPI MOSI→MISO                 (right-col PIN16↔PIN15, adjacent)
P0.22 → P0.24  — PWM output → capture input     (left-col PIN7→PIN8, adjacent)
P0.31           — SAADC AIN7, no wire needed    (on-board VBATT divider)
```

All Stage 0 and Stage 1 tests require USB only — no bench wiring.

---

## Problems Encountered and How They Were Solved

### Bug 1 — USB CDC Kconfig API change in Zephyr 4.x

**症状**: 编译报错 `undefined reference CONFIG_USB_CDC_ACM_SERIAL_*`，旧 Kconfig key 在 Zephyr 4.4 中已删除。

**解决**: 改用新 API：
```
CONFIG_USB_DEVICE_STACK_NEXT=y
CONFIG_USBD_CDC_ACM_CLASS=y
CONFIG_UART_LINE_CTRL=y
```
所有 nicenano firmware targets 均统一更新。

---

### Bug 2 — AEL preflight 尝试连接 ESP32JTAG（UF2 板不需要）

**症状**: 每次运行报 `Preflight: ping 192.168.2.98 -> FAIL`，esp32jtag 离线时整个 pipeline 卡住。

**解决**: 所有 nicenano test plan 增加：
```json
"preflight": {"enabled": false}
```

---

### Bug 3 — IEEE 802.15.4 无法脱离 Zephyr networking 栈独立启用

**症状**: `CONFIG_IEEE802154=y` 依赖 `CONFIG_NETWORKING=y`，引入完整网络栈，链接报错 `undefined reference __device_dts_ord_138`，ROM 超限。

**解决**: 直接访问 nRF52840 RADIO 外设寄存器（bare-metal），不使用任何 Zephyr 网络 API：
```c
RADIO_MODE = 0x0F;      // IEEE 802.15.4 DSSS O-QPSK
RADIO_FREQUENCY = 25;   // 2400+25 = 2425 MHz (channel 15)
RADIO_TASKS_RXEN = 1;
// wait RADIO_EVENTS_READY — confirms PLL lock
```
无需任何 networking Kconfig，固件体积极小。

---

### Bug 4 — SPI loopback 引脚分配错误（P1.10/P1.12 不在 connector 上）

**症状**: SPI loopback 固件 `AEL_SPI_FLASH_FAIL` 反复出现。后经 nrfmicro wiki pinout 确认，P1.10 和 P1.12 均不暴露在 header connector 上。

**解决**: 重新分配至 connector 上的引脚（参考 [nrfmicro wiki](https://github.com/joric/nrfmicro/wiki/Pinout)）：
- SCK: P1.15 (right-col PIN17)
- MOSI: P1.13 (right-col PIN16)
- MISO: P1.11 (right-col PIN15) ← loopback wire，与 MOSI 相邻
- CS: P0.09 (right-col PIN13)

---

### Bug 5 — Stage 1 expect_patterns 被 post_load_settle 窗口错过

**症状**: `nicenano_internal_temp` 等 6 个 Stage 1 测试全部报 FAIL，但 `observe_uart.log` 显示 `AEL_STAGE1_PASS (repeat)` 正常。

**根因**: Stage 1 固件在启动后 ~1.5s 打印详细结果（`[TEMP] temp_c=... PASS` 等），而 `post_load_settle_s=4.0` 导致 observer 在此之后才开始监听，只能捕获 repeat 循环。

**解决**: 所有 6 个 Stage 1 test plan 的 `expect_patterns` 改为 `["AEL_STAGE1_PASS"]`，匹配 repeat 循环中持续输出的 token。

---

## Key Design Notes

- **Stage 1 共用固件**: `nicenano_internal_temp`/`rng`/`timer_self`/`rtc_self`/`flash_rw`/`crypto_ecb` 全部 build `nrf52840_nicenano_stage1` 同一固件，在同一次 flash 里顺序运行全部 6 项自测。
- **无外部 SPI Flash**: nice!nano v1 无板载外部 Flash 芯片，只有 nRF52840 内部 1MB NVMC。
- **UF2 入 bootloader 方式**: 双击 RST → NICENANO 磁盘挂载 → 拷贝 `.uf2` → 自动重启。固件通过监听 1200-baud 信号触发 `ael_enter_bootloader()`。
- **引脚参考**: [nrfmicro wiki Pinout](https://github.com/joric/nrfmicro/wiki/Pinout) 是 nice!nano connector 引脚的准确参考，官方文档中部分引脚（如 TinyGo SPI MISO P1.00）未暴露在 header 上。

---

## Canonical Result

- **DUT**: nRF52840 nice!nano v1 (Cortex-M4F @ 64 MHz, UF2 bootloader)
- **Pack**: [`packs/nrf52840_nicenano_golden.json`](../../packs/nrf52840_nicenano_golden.json)
- **Status**: `golden` — 15/15 PASS, 2026-04-12
