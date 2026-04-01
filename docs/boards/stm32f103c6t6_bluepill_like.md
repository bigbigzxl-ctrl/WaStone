# STM32F103C6T6 Bluepill-like

**MCU:** STM32F103C6T6 — Cortex-M3, 72 MHz max (8 MHz HSI default), 32 KB flash, 10 KB RAM, 48-pin LQFP
**Family:** STM32F1
**Status:** canonical golden suite verified
**Verification date:** 2026-04-01

---

## Canonical Suite

Suite name: `stm32f103c6t6_golden`
Pack: `packs/stm32f103c6t6_golden.json`
Result: **18 / 18 PASS** on the canonical ESP32JTAG bench

| # | Experiment | Test Plan | Verification |
|---|-----------|-----------|--------------|
| 1 | PC13 blinky visual | stm32f103c6_pc13_blinky_visual | operator-visible LED blink |
| 2 | Runtime mailbox | stm32f103c6_minimal_runtime_mailbox | mailbox PASS |
| 3 | Timer | stm32f103c6_timer_mailbox | mailbox PASS |
| 4 | SysTick | stm32f103c6_systick_mailbox | mailbox PASS |
| 5 | Internal temp | stm32f103c6_internal_temp_mailbox | mailbox PASS |
| 6 | IWDG | stm32f103c6_iwdg | mailbox PASS |
| 7 | PB0/PB1 connectivity | stm32f103c6_pb0_pb1_probe | mailbox PASS |
| 8 | PB8/PB9 connectivity | stm32f103c6_pb8_pb9_probe | mailbox PASS |
| 9 | PA0/PA1 ADC connectivity | stm32f103c6_pa0_pa1_adc_probe | mailbox PASS |
| 10 | PB15/PB14 connectivity | stm32f103c6_pb15_pb14_probe | mailbox PASS |
| 11 | GPIO loopback | stm32f103c6_gpio_loopback | mailbox PASS |
| 12 | EXTI | stm32f103c6_exti_trigger | mailbox PASS |
| 13 | ADC loopback | stm32f103c6_adc_loopback | mailbox PASS |
| 14 | Capture | stm32f103c6_capture_mailbox | mailbox PASS |
| 15 | PWM capture | stm32f103c6_pwm_capture | mailbox PASS |
| 16 | UART loopback | stm32f103c6_uart_loopback_mailbox | mailbox PASS |
| 17 | UART multibyte | stm32f103c6_uart_multibyte | mailbox PASS |
| 18 | UART DMA | stm32f103c6_uart_dma | mailbox PASS |

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
| GND | probe GND | Common ground |
| RESET | NC | Not connected |

Instrument: `esp32jtag_stm32_golden` @ `192.168.2.98:4242`

---

## Notes

- This suite is validated on the exact `STM32F103C6T6` density. Do not copy
  `STM32F103C8T6` assumptions blindly.
- `STM32F103C6T6` exposes only one SPI. A previous `SPI2` attempt on
  `PB13/PB14/PB15` was invalid and is intentionally deferred from the canonical
  suite.
- The exact live closeout is recorded in
  `docs/reports/stm32f103c6t6_golden_suite_closeout_2026-04-01.md`.
