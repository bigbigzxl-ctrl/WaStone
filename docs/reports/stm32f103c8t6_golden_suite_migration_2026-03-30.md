# STM32F103C8T6 Golden Suite Migration

**Date:** 2026-03-30
**Board:** `stm32f103_gpio`
**New pack:** `packs/stm32f103c8t6_golden.json`

## Goal

Retire the old `192.168.2.99` UART-bridge path as the implied STM32F103 golden
entry, and replace it with one canonical staged suite aligned with the current
STM32 golden-suite model used by `STM32G431` and `STM32F401`.

## Canonical wiring

This migration standardizes STM32F103C8T6 onto one fixed stage2/3 bench wiring:

- `PA8 ↔ PB8` for GPIO / EXTI / PWM-capture style checks
- `PA9 ↔ PA10` for UART loopback
- `PA7 ↔ PA6` for SPI loopback
- `PA1 ↔ PA0` for ADC loopback
- `PA4 → P0.0` as the primary proof capture
- `PA5 → P0.1` as an auxiliary observed pin
- `PC13 → LED`
- `GND → probe GND`

Bench profile: `configs/bench_profiles/stm32f103_gpio__stage3.yaml`

## Stage layout

### Stage 0

- `stm32f103_pc13_blinky_visual`
- `stm32f103_minimal_runtime_mailbox`

### Stage 1

- `stm32f103_timer_mailbox`
- `stm32f103_internal_temp_mailbox`

### Stage 2

- `stm32f103_wiring_verify`
- `stm32f103_exti_mailbox`
- `stm32f103_pwm_capture`
- `stm32f103_uart_loopback_mailbox`
- `stm32f103_spi_mailbox`
- `stm32f103_adc_mailbox`
- `stm32f103_iwdg`

### Stage 3

- `stm32f103_gpio_signature`
- `stm32f103_gpio_loopback_banner`
- `stm32f103_exti_banner`
- `stm32f103_capture_banner`
- `stm32f103_pwm_banner`
- `stm32f103_uart_loopback_banner`
- `stm32f103_spi_banner`
- `stm32f103_adc_banner`

## Legacy cleanup decision

The older `.99` path remains in the tree for historical evidence:

- board profile: `configs/boards/stm32f103_uart.yaml`
- instrument: `configs/instrument_instances/esp32jtag_stm32_uart.yaml`
- test: `tests/plans/stm32f103_uart_banner.json`

But it is no longer the default or canonical golden entry path for STM32F103.
The DUT default pack now points at `packs/stm32f103c8t6_golden.json`.

## Current scope

This migration intentionally maximizes reuse of existing F103 assets:

- preserved all validated stage3 banner/signature tests
- preserved existing mailbox tests for `exti`, `uart`, `spi`, and `adc`
- added new stage0/stage1 tests and new mailbox targets for:
  - `minimal_runtime_mailbox`
  - `timer_mailbox`
  - `internal_temp_mailbox`
  - `wiring_verify`
  - `pwm_capture`
  - `iwdg`

## Intentional deviation from the F401/F411 20-test shape

This first F103 canonical suite is **19 tests**, not 20.

The missing F401/F411-style pieces are:

- dedicated `uart_dma`
- dedicated `i2c_loopback`

Those are left as follow-on work rather than being faked through aliases or by
keeping the old UART-bridge test in the golden path.
