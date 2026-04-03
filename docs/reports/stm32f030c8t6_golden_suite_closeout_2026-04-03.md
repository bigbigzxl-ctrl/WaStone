# STM32F030C8T6 Golden Suite Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink`
**Pack:** `packs/stm32f030c8t6_golden.json`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** final canonical full-pack rerun completed, `24/24 PASS`

## Summary

`STM32F030C8T6` now has a formal staged Golden Suite shape on the local
DAPLink bench. The canonical pack is:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_golden.json --board stm32f030c8t6_daplink --stop-on-fail
```

The pack covers Stage 0 board-life checks, Stage 1 internal/self tests,
pre-Stage2 connectivity proof, and Stage 2 functional loopback and timing
tests on real hardware. `uart_dma` is intentionally deferred from the canonical
pack until it is independently stabilized on this exact MCU and bench.

## Canonical Bench Contract

- SWD via DAPLink
- `NRST -> DAPLink NRST`
- `GND -> probe GND`
- `PC13 -> onboard LED`
- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PA9 <-> PA10`

## Evidence

Final all-pass pack run:

- `pack_runs/2026-04-03_09-49-06_stm32f030c8t6_golden_stm32f030c8t6_daplink`

Representative run artifacts from that rerun:

- `runs/2026-04-03_09-49-06_stm32f030c8t6_daplink_stm32f030c8t6_pc13_blinky_visual`
- `runs/2026-04-03_09-49-38_stm32f030c8t6_daplink_stm32f030c8t6_system_identity_mailbox`
- `runs/2026-04-03_09-49-49_stm32f030c8t6_daplink_stm32f030c8t6_sleep_wfi_mailbox`
- `runs/2026-04-03_09-49-54_stm32f030c8t6_daplink_stm32f030c8t6_adc_vref_mailbox`
- `runs/2026-04-03_09-51-10_stm32f030c8t6_daplink_stm32f030c8t6_exti_trigger`
- `runs/2026-04-03_09-53-20_stm32f030c8t6_daplink_stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`
- `runs/2026-04-03_09-53-53_stm32f030c8t6_daplink_stm32f030c8t6_spi1_loopback_mailbox`
- `runs/2026-04-03_09-54-58_stm32f030c8t6_daplink_stm32f030c8t6_uart_multibyte`

Earlier targeted live runs used to stabilize the suite before the final rerun:

Stage 0 / Stage 1 live runs:

- `runs/2026-04-03_09-44-40_stm32f030c8t6_daplink_stm32f030c8t6_minimal_runtime_mailbox`
- `runs/2026-04-03_09-44-56_stm32f030c8t6_daplink_stm32f030c8t6_timer_mailbox`
- `runs/2026-04-03_09-45-06_stm32f030c8t6_daplink_stm32f030c8t6_systick_mailbox`
- `runs/2026-04-03_09-45-16_stm32f030c8t6_daplink_stm32f030c8t6_system_identity_mailbox`
- `runs/2026-04-03_09-45-25_stm32f030c8t6_daplink_stm32f030c8t6_reset_cause_mailbox`
- `runs/2026-04-03_09-45-34_stm32f030c8t6_daplink_stm32f030c8t6_sleep_wfi_mailbox`
- `runs/2026-04-03_09-45-47_stm32f030c8t6_daplink_stm32f030c8t6_adc_vref_mailbox`
- `runs/2026-04-03_09-45-56_stm32f030c8t6_daplink_stm32f030c8t6_internal_temp_mailbox`
- `runs/2026-04-03_09-46-12_stm32f030c8t6_daplink_stm32f030c8t6_iwdg`

Pre-Stage2 live rerun:

- `runs/2026-04-03_08-55-54_stm32f030c8t6_daplink_stm32f030c8t6_pb0_pb1_probe`
- `runs/2026-04-03_08-56-01_stm32f030c8t6_daplink_stm32f030c8t6_pb8_pb9_probe`
- `runs/2026-04-03_08-56-07_stm32f030c8t6_daplink_stm32f030c8t6_pa0_pa1_adc_probe`
- `runs/2026-04-03_08-56-13_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`
- `runs/2026-04-03_08-56-19_stm32f030c8t6_daplink_stm32f030c8t6_uart_loopback_probe`

Stage 2 live runs:

- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_gpio_loopback`
- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_exti_trigger`
- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_adc_loopback`
- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_spi1_loopback_mailbox`
- `runs/2026-04-03_09-09-41_stm32f030c8t6_daplink_stm32f030c8t6_uart_loopback_mailbox`
- `runs/2026-04-03_09-10-30_stm32f030c8t6_daplink_stm32f030c8t6_uart_multibyte`
- `runs/2026-04-03_09-42-49_stm32f030c8t6_daplink_stm32f030c8t6_capture_mailbox`
- `runs/2026-04-03_09-43-24_stm32f030c8t6_daplink_stm32f030c8t6_pwm_capture`
- `runs/2026-04-03_09-43-59_stm32f030c8t6_daplink_stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`

## What Was Validated

Validated on real hardware in the final canonical rerun:

- `stm32f030c8t6_pc13_blinky_visual`
- `stm32f030c8t6_minimal_runtime_mailbox`
- `stm32f030c8t6_timer_mailbox`
- `stm32f030c8t6_systick_mailbox`
- `stm32f030c8t6_internal_temp_mailbox`
- `stm32f030c8t6_system_identity_mailbox`
- `stm32f030c8t6_reset_cause_mailbox`
- `stm32f030c8t6_sleep_wfi_mailbox`
- `stm32f030c8t6_adc_vref_mailbox`
- `stm32f030c8t6_iwdg`
- `stm32f030c8t6_pb0_pb1_probe`
- `stm32f030c8t6_pb8_pb9_probe`
- `stm32f030c8t6_pa0_pa1_adc_probe`
- `stm32f030c8t6_pa7_pa6_spi_probe`
- `stm32f030c8t6_uart_loopback_probe`
- `stm32f030c8t6_gpio_loopback`
- `stm32f030c8t6_exti_trigger`
- `stm32f030c8t6_adc_loopback`
- `stm32f030c8t6_capture_mailbox`
- `stm32f030c8t6_pwm_capture`
- `stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`
- `stm32f030c8t6_spi1_loopback_mailbox`
- `stm32f030c8t6_uart_loopback_mailbox`
- `stm32f030c8t6_uart_multibyte`

## Key Repair Findings

1. The startup vector table must expose SysTick explicitly.

`sleep_wfi_mailbox` depends on a real `SysTick_Handler` vector. The shared
`stm32f030c8t6` startup was updated so SysTick wakeup tests land on the right
symbol instead of a dead default entry.

2. Single-board real-hardware runs must stay serialized.

An earlier attempt to overlap timer-group runs on the same board produced
untrusted results. The canonical evidence in this closeout comes only from the
final serialized pack rerun.

3. The canonical UART path stays local.

For this Golden Suite, `PA9 <-> PA10` is the stable contract. DAPLink-hosted
UART observation is intentionally a separate future pack, not a hidden change
inside the canonical suite.

4. Pre-Stage2 is still worth keeping.

The earlier bad `PA7 <-> PA6` jumper was caught cleanly by the dedicated
connectivity layer before it was misdiagnosed as an SPI firmware problem.

## Known Deferred Scope

Not in the canonical pack yet:

- `stm32f030c8t6_uart_dma`
- host-observed DAPLink UART pack
- I2C and any test that needs an external partner device

## Process Note

This suite would still be incomplete if it stopped at code generation and
first-pass live runs. The reusable outcome depends on all of these existing
together:

- the canonical pack
- a clean full-pack rerun
- written closeout evidence
- reusable skill capture
- an explicit deferred list

## Closeout Decision

This round is a valid Golden Suite closeout for `STM32F030C8T6`.

The correct conclusion is:

- canonical pack: complete
- live validation: complete
- final canonical full-pack rerun: `24/24 PASS`
- reusable skill capture: completed
- `uart_dma`: intentionally deferred from the canonical pack
