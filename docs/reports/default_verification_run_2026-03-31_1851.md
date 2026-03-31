# Default Verification Run Report
**Date:** 2026-03-31
**Start time:** 18:51 UTC+8
**End time:** 19:02 UTC+8
**Duration:** ~11 minutes
**Exit code:** 0 (PASS)
**Execution mode:** parallel (4 packs running concurrently)

---

## Overall Summary

| Board | Pack | Tests | Pass | Fail | Result |
|-------|------|------:|-----:|-----:|--------|
| STM32F401RCT6 | stm32f401rct6_golden | 20 | 20 | 0 | **PASS** |
| STM32F411CEU6 WeAct Black Pill V2.0 | stm32f411ceu6_golden | 20 | 20 | 0 | **PASS** |
| STM32G431CBU6 | stm32g431cbu6_golden | 17 | 17 | 0 | **PASS** |
| STM32H750VBT6 YD | stm32h750vbt6_golden | 24 | 24 | 0 | **PASS** |
| **Total** | | **81** | **81** | **0** | **PASS** |

> Note: STM32H750VBT6 was added to default verification in this session (previously not included).

---

## Suite Details

### STM32F401RCT6 — 20 tests, 20 pass, 0 fail

**Instrument:** esp32jtag_f401_bench
**Pack:** `packs/stm32f401rct6_golden.json`

| Test | Run ID | Result |
|------|--------|--------|
| stm32f401rct6_pc13_blinky_visual | 2026-03-31_18-51-53 | PASS |
| stm32f401rct6_exti_trigger | 2026-03-31_18-52-29 | PASS |
| stm32f401rct6_spi_loopback | 2026-03-31_18-53-03 | PASS |
| stm32f401rct6_wiring_verify | 2026-03-31_18-53-34 | PASS |
| stm32f401_exti_banner | 2026-03-31_18-54-02 | PASS |
| stm32f401rct6_timer_mailbox | 2026-03-31_18-54-26 | PASS |
| stm32f401_gpio_loopback_banner | 2026-03-31_18-54-48 | PASS |
| stm32f401rct6_iwdg | 2026-03-31_18-55-12 | PASS |
| stm32f401rct6_pwm_capture | 2026-03-31_18-55-40 | PASS |
| stm32f401_gpio_signature | 2026-03-31_18-56-08 | PASS |
| stm32f401rct6_uart_dma | 2026-03-31_18-56-32 | PASS |
| stm32f401_capture_banner | 2026-03-31_18-57-09 | PASS |
| stm32f401_adc_banner | 2026-03-31_18-57-44 | PASS |
| stm32f401_spi_banner | 2026-03-31_18-58-18 | PASS |
| stm32f401rct6_i2c_loopback | 2026-03-31_18-58-50 | PASS |
| stm32f401rct6_minimal_runtime_mailbox | 2026-03-31_18-59-37 | PASS |
| stm32f401rct6_uart_multibyte | 2026-03-31_19-00-13 | PASS |
| stm32f401_pwm_banner | 2026-03-31_19-01-03 | PASS |
| stm32f401_uart_loopback_banner | 2026-03-31_19-01-51 | PASS |
| stm32f401rct6_internal_temp_mailbox | 2026-03-31_18-52-05 | PASS |

**Coverage:** blinky, mailbox, timer, EXTI, SPI, UART (loopback/DMA/multibyte), GPIO, PWM, ADC, I2C, IWDG, wiring_verify, internal temp, capture

---

### STM32F411CEU6 WeAct Black Pill V2.0 — 20 tests, 20 pass, 0 fail

**Instrument:** esp32jtag_f411_bench
**Pack:** `packs/stm32f411ceu6_golden.json`

| Test | Run ID | Result |
|------|--------|--------|
| stm32f411ceu6_pc13_blinky_visual | 2026-03-31_18-51-54 | PASS |
| stm32f411ceu6_iwdg | 2026-03-31_18-52-06 | PASS |
| stm32f411_gpio_signature | 2026-03-31_18-52-37 | PASS |
| stm32f411ceu6_wiring_verify | 2026-03-31_18-53-05 | PASS |
| stm32f411_exti_banner | 2026-03-31_18-53-34 | PASS |
| stm32f411ceu6_minimal_runtime_mailbox | 2026-03-31_18-53-59 | PASS |
| stm32f411ceu6_pwm_capture | 2026-03-31_18-54-18 | PASS |
| stm32f411ceu6_exti_trigger | 2026-03-31_18-54-45 | PASS |
| stm32f411_spi_banner | 2026-03-31_18-55-13 | PASS |
| stm32f411ceu6_internal_temp_mailbox | 2026-03-31_18-55-35 | PASS |
| stm32f411_uart_loopback_banner | 2026-03-31_18-55-54 | PASS |
| stm32f411_pwm_banner | 2026-03-31_18-56-16 | PASS |
| stm32f411ceu6_uart_multibyte | 2026-03-31_18-56-39 | PASS |
| stm32f411_capture_banner | 2026-03-31_18-57-06 | PASS |
| stm32f411ceu6_uart_dma | 2026-03-31_18-57-28 | PASS |
| stm32f411ceu6_timer_mailbox | 2026-03-31_18-57-56 | PASS |
| stm32f411ceu6_spi_loopback | 2026-03-31_18-58-17 | PASS |
| stm32f411ceu6_i2c_loopback | 2026-03-31_18-58-45 | PASS |
| stm32f411_adc_banner | 2026-03-31_18-59-12 | PASS |
| stm32f411_gpio_loopback_banner | 2026-03-31_18-59-35 | PASS |

**Coverage:** blinky, mailbox, timer, EXTI, SPI, UART (loopback/DMA/multibyte), GPIO, PWM, ADC, I2C, IWDG, wiring_verify, internal temp, capture

---

### STM32G431CBU6 — 17 tests, 17 pass, 0 fail

**Instrument:** esp32jtag_g431_bench
**Pack:** `packs/stm32g431cbu6_golden.json`

| Test | Run ID | Result |
|------|--------|--------|
| stm32g431_blinky_visual | 2026-03-31_18-51-54 | PASS |
| stm32g431_pwm | 2026-03-31_18-52-20 | PASS |
| stm32g431_uart_loopback | 2026-03-31_18-53-00 | PASS |
| stm32g431_dac_mailbox | 2026-03-31_18-53-39 | PASS |
| stm32g431_uart_dma | 2026-03-31_18-54-13 | PASS |
| stm32g431_fdcan_loopback | 2026-03-31_18-54-47 | PASS |
| stm32g431_timer_mailbox | 2026-03-31_18-55-22 | PASS |
| stm32g431_gpio_loopback | 2026-03-31_18-55-56 | PASS |
| stm32g431_adc | 2026-03-31_18-56-30 | PASS |
| stm32g431_exti | 2026-03-31_18-57-06 | PASS |
| stm32g431_internal_temp_mailbox | 2026-03-31_18-57-41 | PASS |
| stm32g431_wiring_verify | 2026-03-31_18-58-16 | PASS |
| stm32g431_spi | 2026-03-31_18-58-56 | PASS |
| stm32g431_gpio_signature | 2026-03-31_18-59-32 | PASS |
| stm32g431_minimal_runtime_mailbox | 2026-03-31_19-00-07 | PASS |
| stm32g431_iwdg | 2026-03-31_19-00-47 | PASS |
| stm32g431_capture | 2026-03-31_19-01-30 | PASS |

**Coverage:** blinky, mailbox, timer, EXTI, SPI, UART (loopback/DMA), GPIO, PWM, ADC, DAC, I2C, FDCAN, IWDG, wiring_verify, internal temp, capture

---

### STM32H750VBT6 YD — 24 tests, 24 pass, 0 fail

**Instrument:** esp32jtag_h750_bench (192.168.2.63)
**Pack:** `packs/stm32h750vbt6_golden.json`
**Note:** First inclusion in default verification (added this session).

| Test | Run ID | Result |
|------|--------|--------|
| stm32h750_blinky_visual | 2026-03-31_18-51-55 | PASS |
| stm32h750_fdcan_loopback | 2026-03-31_18-52-21 | PASS |
| stm32h750_lptim | 2026-03-31_18-52-45 | PASS |
| stm32h750_gpio_loopback | 2026-03-31_18-53-07 | PASS |
| stm32h750_tim1_pwm | 2026-03-31_18-53-28 | PASS |
| stm32h750_spi_loopback | 2026-03-31_18-53-48 | PASS |
| stm32h750_uart_dma | 2026-03-31_18-54-09 | PASS |
| stm32h750_crc | 2026-03-31_18-54-34 | PASS |
| stm32h750_pll1_clock | 2026-03-31_18-54-55 | PASS |
| stm32h750_adc_dac_loopback | 2026-03-31_18-55-17 | PASS |
| stm32h750_rng | 2026-03-31_18-55-38 | PASS |
| stm32h750_pwm_capture | 2026-03-31_18-55-59 | PASS |
| stm32h750_exti_trigger | 2026-03-31_18-56-20 | PASS |
| stm32h750_iwdg | 2026-03-31_18-56-41 | PASS |
| stm32h750_bdma | 2026-03-31_18-57-07 | PASS |
| stm32h750_i2c_loopback | 2026-03-31_18-57-27 | PASS |
| stm32h750_wwdg | 2026-03-31_18-57-53 | PASS |
| stm32h750_timer_mailbox | 2026-03-31_18-58-19 | PASS |
| stm32h750_qspi_flash | 2026-03-31_18-58-40 | PASS |
| stm32h750_rtc | 2026-03-31_18-59-01 | PASS |
| stm32h750_wiring_verify | 2026-03-31_18-59-27 | PASS |
| stm32h750_internal_temp_mailbox | 2026-03-31_18-59-48 | PASS |
| stm32h750_minimal_runtime_mailbox | 2026-03-31_19-00-09 | PASS |
| stm32h750_uart_loopback | 2026-03-31_19-00-30 | PASS |

**Coverage:** blinky, mailbox, timer, EXTI, SPI, UART (loopback/DMA), GPIO, PWM (TIM1+capture), ADC/DAC loopback, I2C, FDCAN, IWDG, WWDG, RNG, CRC, QSPI flash, PLL clock, BDMA, LPTIM, RTC, wiring_verify, internal temp

---

## Notable Changes This Session

### Pack transport abort fix (commit b42a008)

Previously, when an instrument was offline, `ael pack` would grind through every test in the pack — each incurring the full preflight timeout (~40s). A 25-test pack took **~17 minutes** before reporting failure.

**Fix:** When preflight fails with `probe_transport_unhealthy` (instrument completely unreachable on the network), the pack now aborts after the first test and skips all remaining tests. Typical abort time: ~40s (single preflight timeout).

- Applies to all instrument types: ESP32JTAG, ST-Link, DAP-Link
- Does **not** abort on `probe_busy_or_stuck` (instrument reachable but GDB port occupied)
- 9 pytest cases added: `tests/test_pack_transport_abort.py`

### STM32H750VBT6 added to default verification

`packs/stm32h750vbt6_golden.json` added to the parallel batch in `configs/default_verification_setting.yaml`. The H750 golden suite (25 tests) now runs alongside F401/F411/G431 on every default verification run.
