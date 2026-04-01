# STM32F103C6T6 Golden Suite Closeout

**Date:** 2026-04-01
**Board:** `stm32f103c6t6_bluepill_like`
**Pack:** `packs/stm32f103c6t6_golden.json`
**Instrument:** `esp32jtag_stm32_golden` @ `192.168.2.98:4242`
**Status:** final canonical full-pack rerun completed `18/18 PASS`

## Summary

`STM32F103C6T6` is now formalized as a canonical staged golden suite on the
ESP32JTAG STM32 bench. The final live proof is the repo-native pack run:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c6t6_golden.json --board stm32f103c6t6_bluepill_like --stop-on-fail
```

The suite now covers Stage 0 board-life checks, Stage 1 internal/self tests,
pre-Stage2 connectivity proof, and Stage 2 functional loopback/timing/UART
tests on real hardware.

## Canonical Bench Contract

- SWD: `P3`
- reset: `NC`
- verify view: `P0.0`
- `PC13 -> P0.0`
- `PC13 -> LED`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`
- `PA9 <-> PA10`
- common ground to probe ground

## Evidence

Final all-pass pack run:

- `pack_runs/2026-04-01_11-11-47_stm32f103c6t6_golden_stm32f103c6t6_bluepill_like`

Representative run artifacts from that rerun:

- `runs/2026-04-01_11-11-47_stm32f103c6t6_bluepill_like_stm32f103c6_pc13_blinky_visual`
- `runs/2026-04-01_11-12-03_stm32f103c6t6_bluepill_like_stm32f103c6_timer_mailbox`
- `runs/2026-04-01_11-15-07_stm32f103c6t6_bluepill_like_stm32f103c6_exti_trigger`
- `runs/2026-04-01_11-17-21_stm32f103c6t6_bluepill_like_stm32f103c6_capture_mailbox`
- `runs/2026-04-01_11-17-33_stm32f103c6t6_bluepill_like_stm32f103c6_pwm_capture`
- `runs/2026-04-01_11-18-10_stm32f103c6t6_bluepill_like_stm32f103c6_uart_dma`

Earlier targeted live runs used to stabilize the suite before the final rerun:

- `runs/2026-04-01_10-57-23_stm32f103c6t6_bluepill_like_stm32f103c6_capture_mailbox`
- `runs/2026-04-01_10-57-37_stm32f103c6t6_bluepill_like_stm32f103c6_pwm_capture`
- `runs/2026-04-01_10-57-53_stm32f103c6t6_bluepill_like_stm32f103c6_uart_loopback_mailbox`
- `runs/2026-04-01_10-58-11_stm32f103c6t6_bluepill_like_stm32f103c6_uart_multibyte`
- `runs/2026-04-01_10-58-32_stm32f103c6t6_bluepill_like_stm32f103c6_uart_dma`

## What Was Validated

Validated on real hardware in the final canonical rerun:

- `stm32f103c6_pc13_blinky_visual`
- `stm32f103c6_minimal_runtime_mailbox`
- `stm32f103c6_timer_mailbox`
- `stm32f103c6_systick_mailbox`
- `stm32f103c6_internal_temp_mailbox`
- `stm32f103c6_iwdg`
- `stm32f103c6_pb0_pb1_probe`
- `stm32f103c6_pb8_pb9_probe`
- `stm32f103c6_pa0_pa1_adc_probe`
- `stm32f103c6_pb15_pb14_probe`
- `stm32f103c6_gpio_loopback`
- `stm32f103c6_exti_trigger`
- `stm32f103c6_adc_loopback`
- `stm32f103c6_capture_mailbox`
- `stm32f103c6_pwm_capture`
- `stm32f103c6_uart_loopback_mailbox`
- `stm32f103c6_uart_multibyte`
- `stm32f103c6_uart_dma`

## Key Repair Findings

1. Exact density matters.

The first SPI loopback attempt was modeled like a larger F103 part and used the
`PB13/PB14/PB15` SPI2 pin family. That is invalid for `STM32F103C6T6`, which
exposes only one SPI. The canonical suite therefore defers SPI until the bench
adds a real `SPI1` loopback path on `PA5/PA6/PA7`.

Reference used during the bounded repair step:

- ST official product page: `STM32F103C6` shows `1 x SPI`

2. Copied startup code must include the real interrupt vectors.

The initial copied `C6` startup only exposed a tiny vector table. Interrupt-
driven tests such as timer and SysTick did not become reliable until the shared
`stm32f103c6` startup was fixed to include the needed vectors.

3. Pre-Stage2 connectivity is worth keeping.

The dedicated probe layer for `PB0/PB1`, `PB8/PB9`, `PA0/PA1`, and `PB15/PB14`
made the Stage 2 loopback and timing failures much easier to classify as
wiring-vs-firmware instead of guessing from richer tests.

## Deferred Coverage

Not in the canonical suite yet:

- SPI loopback on `SPI1` because the current bench contract does not expose the
  required `PA5/PA6/PA7` jumper path
- any host-external UART proof beyond the board-local `PA9 <-> PA10` loopback

This is an intentional scope boundary, not a silent failure.

## Process Note

This work would have been incomplete if it stopped at the code and first pass
of live validation. The reusable outcome here depends on both:

- a closeout report tying the suite to real run ids
- a reusable skill capturing the exact density and bench-contract rules

That is why the suite is only considered complete after this closeout and the
matching skill capture landed.

## Closeout Decision

This round is a valid golden-suite closeout for `STM32F103C6T6`.

The correct conclusion is:

- canonical pack: complete
- live validation: complete
- final canonical full-pack rerun: `18/18 PASS`
- DUT manifest verified state: promoted
- reusable skill capture: completed
