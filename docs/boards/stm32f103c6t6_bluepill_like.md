# STM32F103C6T6 Bluepill-like

**MCU:** STM32F103C6T6 — Cortex-M3, 72 MHz max (8 MHz HSI default), 32 KB flash, 10 KB RAM, 48-pin LQFP
**Family:** STM32F1
**Status:** canonical golden suite verified
**Verification date:** 2026-04-01

---

## Canonical Suite

Suite name: `stm32f103c6t6_golden`
Pack: `packs/stm32f103c6t6_golden.json`
Result: **24 / 24 PASS** on the canonical ESP32JTAG bench

Opt-in extended suite:

- Suite name: `stm32f103c6t6_golden_with_uart_roundtrip`
- Pack: `packs/stm32f103c6t6_golden_with_uart_roundtrip.json`
- Use this only when you also wire `ESP32JTAG UART TX -> STM32 PA10` and want
  a real cross-instrument UART request/response proof in addition to the normal
  DUT-local UART tests.

| # | Experiment | Test Plan | Verification |
|---|-----------|-----------|--------------|
| 1 | PC13 blinky visual | stm32f103c6_pc13_blinky_visual | operator-visible LED blink |
| 2 | Runtime mailbox | stm32f103c6_minimal_runtime_mailbox | mailbox PASS |
| 3 | Timer | stm32f103c6_timer_mailbox | mailbox PASS |
| 4 | SysTick | stm32f103c6_systick_mailbox | mailbox PASS |
| 5 | Internal temp | stm32f103c6_internal_temp_mailbox | mailbox PASS |
| 6 | IWDG | stm32f103c6_iwdg | mailbox PASS |
| 7 | System identity | stm32f103c6_system_identity_mailbox | mailbox PASS |
| 8 | Reset flags | stm32f103c6_reset_cause_mailbox | mailbox PASS |
| 9 | Sleep / wake | stm32f103c6_sleep_wfi_mailbox | mailbox PASS |
| 10 | Internal VREFINT | stm32f103c6_adc_vref_mailbox | mailbox PASS |
| 11 | PB0/PB1 connectivity | stm32f103c6_pb0_pb1_probe | mailbox PASS |
| 12 | PB8/PB9 connectivity | stm32f103c6_pb8_pb9_probe | mailbox PASS |
| 13 | PA0/PA1 ADC connectivity | stm32f103c6_pa0_pa1_adc_probe | mailbox PASS |
| 14 | PB15/PB14 connectivity | stm32f103c6_pb15_pb14_probe | mailbox PASS |
| 15 | GPIO loopback | stm32f103c6_gpio_loopback | mailbox PASS |
| 16 | EXTI | stm32f103c6_exti_trigger | mailbox PASS |
| 17 | ADC loopback | stm32f103c6_adc_loopback | mailbox PASS |
| 18 | Capture | stm32f103c6_capture_mailbox | mailbox PASS |
| 19 | PWM capture | stm32f103c6_pwm_capture | mailbox PASS |
| 20 | TIM3 hardware PWM | stm32f103c6_tim3_pwm_pb0_pb1_mailbox | mailbox PASS |
| 21 | SPI1 loopback | stm32f103c6_spi1_loopback_mailbox | mailbox PASS |
| 22 | UART loopback | stm32f103c6_uart_loopback_mailbox | mailbox PASS |
| 23 | UART multibyte | stm32f103c6_uart_multibyte | mailbox PASS |
| 24 | UART DMA | stm32f103c6_uart_dma | mailbox PASS |

---

## Bench Wiring

| DUT pin | Instrument (ESP32JTAG) | Role |
|---------|------------------------|------|
| SWDIO / SWDCLK | P3 | SWD debug / flash |
| PC13 | P0.0 | machine-observed LED net |
| PC13 | LED | onboard active-low LED |
| PB0 | PB1 | GPIO loopback |
| PB8 | PB9 | capture / PWM / connectivity |
| PA0 | PA1 | ADC / EXTI |
| PB15 | PB14 | digital connectivity |
| PA9 | PA10 | UART local loopback |
| PA7 | PA6 | SPI1 MOSI to MISO loopback |
| PA5 | P0.1 | optional SPI1 SCK observation |
| GND | probe GND | Common ground |
| RESET | NC | Not connected |

Instrument: `esp32jtag_stm32_golden` @ `192.168.2.98:4242`

---

## Notes

- This suite is validated on the exact `STM32F103C6T6` density. Do not copy
  `STM32F103C8T6` assumptions blindly.
- Live identity on this board reports `DBGMCU_IDCODE low bits = 0x412`, which
  is the correct low-density class for `STM32F103C6T6`.
- `STM32F103C6T6` exposes only one SPI. `SPI1` is now covered with
  `PA7 -> PA6`; `I2C` remains deferred because the current bench has no valid
  partner wiring for it.
- The canonical suite stays the default. The new extended pack is opt-in
  because it depends on extra bench UART wiring and the ESP32JTAG UART bridge
  firmware state.
- The exact live closeout is recorded in
  `docs/reports/stm32f103c6t6_golden_suite_closeout_2026-04-01.md`.
