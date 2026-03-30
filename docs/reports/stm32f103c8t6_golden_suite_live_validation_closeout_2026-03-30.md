# STM32F103C8T6 Golden Suite Live Validation Closeout

**Date:** 2026-03-30
**Board:** `stm32f103_gpio`
**Pack:** `packs/stm32f103c8t6_golden.json`
**Instrument:** canonical live bench `esp32jtag_stm32f103_golden` @ `192.168.2.99:4242`
**Status:** stale `.109` binding corrected to `.99`; final canonical full-pack rerun completed `24/24 PASS`

## Summary

The staged `STM32F103C8T6` golden suite was exercised through the real
repo-native entrypoint:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio
```

The first live validation attempt exposed that the board profile was still
bound to a stale `.109` instrument. After rebinding the active
`stm32f103_gpio` path to `.99`, the suite reached real DUT execution and
validated the board on the intended F103 bench.

One corrected-binding full-pack run on `.99` completed 22 of 24 tests PASS and
isolated the only remaining failures to:

- `stm32f103_capture_mailbox`
- `stm32f103_pwm_capture`

Both of those mailbox tests were then corrected and revalidated directly on the
live `.99` bench. A final post-fix full-pack rerun was then completed and
finished `24/24 PASS`.

## Setup Confirmed

- board: `stm32f103_gpio`
- MCU: `stm32f103c8t6`
- stale instrument that originally blocked the suite: `192.168.2.109:4242`
- actual F103 golden control instrument: `192.168.2.99:4242`
- actual F103 golden web API: `https://192.168.2.99:443`
- canonical wiring:
  - `PA8 ↔ PB8`
  - `PA9 ↔ PA10`
  - `PA7 ↔ PA6`
  - `PA1 ↔ PA0`
  - `PA4 → P0.0`
  - `PA5 → P0.1`
  - `PC13 → LED`
  - `GND → probe GND`

## Evidence

Representative pack runs:

- stale-binding run family: `pack_runs/2026-03-30_07-55-51_stm32f103c8t6_golden_stm32f103_gpio`
- first corrected-binding full-pack run: `pack_runs/2026-03-30_09-10-33_stm32f103c8t6_golden_stm32f103_gpio`
- follow-up full-pack rerun started before the final mailbox stabilization patch:
  `pack_runs/2026-03-30_09-23-33_stm32f103c8t6_golden_stm32f103_gpio`
- final all-pass canonical rerun:
  `pack_runs/2026-03-30_09-35-39_stm32f103c8t6_golden_stm32f103_gpio`

Representative targeted runs:

- `runs/2026-03-30_09-09-37_stm32f103_gpio_stm32f103_gpio_loopback_banner`
- `runs/2026-03-30_09-10-02_stm32f103_gpio_stm32f103_uart_loopback_banner`
- `runs/2026-03-30_09-19-48_stm32f103_gpio_stm32f103_capture_mailbox`
- `runs/2026-03-30_09-23-02_stm32f103_gpio_stm32f103_pwm_capture`
- `runs/2026-03-30_09-31-57_stm32f103_gpio_stm32f103_capture_mailbox`
- `runs/2026-03-30_09-32-20_stm32f103_gpio_stm32f103_pwm_capture`

Key observations:

- `.109` failure mode was bench transport, not firmware:
  - `ping 192.168.2.109 -> FAIL`
  - `TCP 192.168.2.109:4242 -> FAIL`
  - direct HTTPS probe also failed with `No route to host`
- `.99` is reachable on both BMDA and web surfaces:
  - `TCP 192.168.2.99:4242 -> OK`
  - HTTPS to `.99` returns `Authentication Required`
- Stage 3 banner failures on the first `.99` attempts were plan-metadata
  blockers, not runtime failures:
  - F103 banner plans still marked external loopbacks as
    `manual_loopback_required`
  - changing those external inputs to `provisioned` allowed Stage 3 to run on
    the confirmed bench wiring
- the first corrected-binding full-pack run proved the suite at 22/24 PASS and
  isolated the remaining instability to the two timer/capture mailbox tests
- targeted post-fix reruns closed both remaining failures on the live bench

## What Was Validated

Validated at pack / integration level:

- new `stm32f103c8t6_golden` pack resolves and starts correctly
- the active `STM32F103C8T6` path now binds to `.99`, not `.109`
- Stage 3 F103 banner plans now describe the confirmed loopback wiring as
  provisioned bench state

Validated on real hardware after correcting the binding:

- `stm32f103_pc13_blinky_visual` PASS
- `stm32f103_minimal_runtime_mailbox` PASS
- `stm32f103_timer_mailbox` PASS
- `stm32f103_systick_mailbox` PASS
- `stm32f103_internal_temp_mailbox` PASS
- `stm32f103_wiring_verify` PASS
- `stm32f103_gpio_loopback_mailbox` PASS
- `stm32f103_exti_mailbox` PASS
- `stm32f103_capture_mailbox` PASS
- `stm32f103_pwm_capture` PASS
- `stm32f103_uart_loopback_mailbox` PASS
- `stm32f103_uart_multibyte` PASS
- `stm32f103_uart_dma` PASS
- `stm32f103_spi_mailbox` PASS
- `stm32f103_adc_mailbox` PASS
- `stm32f103_iwdg` PASS
- `stm32f103_gpio_signature` PASS
- `stm32f103_gpio_loopback_banner` PASS
- `stm32f103_exti_banner` PASS
- `stm32f103_capture_banner` PASS
- `stm32f103_pwm_banner` PASS
- `stm32f103_uart_loopback_banner` PASS
- `stm32f103_spi_banner` PASS
- `stm32f103_adc_banner` PASS

Representative corrected-binding full-pack status:

- `pack_runs/2026-03-30_09-10-33_stm32f103c8t6_golden_stm32f103_gpio`
  completed 22/24 PASS
- after that run, the remaining two failures were fixed and each revalidated on
  the live bench:
  - `stm32f103_capture_mailbox`
  - `stm32f103_pwm_capture`
- final confirmation:
  - `pack_runs/2026-03-30_09-35-39_stm32f103c8t6_golden_stm32f103_gpio`
  - result: `24/24 PASS`

## Root Cause Classification

Primary classifications:

- `stale board/instrument binding`
- `stale Stage 3 external-input metadata`
- `fragile single-interval mailbox capture check`

Why this is not a broad suite regression:

- the first blocker happened before DUT execution and was purely a stale `.109`
  transport binding
- after rebinding to `.99`, the suite ran on real hardware and passed across
  Stage 0, Stage 1, Stage 2, and Stage 3
- the only remaining runtime issues reduced to two isolated mailbox targets on
  the same timer/capture path
- those two targets were closed with bounded mailbox-firmware changes and then
  passed live reruns

## Recommended Next Step

1. Keep the corrected `.99` instrument binding
2. Keep the confirmed canonical wiring:

```text
PA8↔PB8  PA9↔PA10  PA7↔PA6  PA1↔PA0
PA4→P0.0 PA5→P0.1 PC13→LED GND→probe GND
```

3. Run one final post-fix full-pack confirmation:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio
```

4. Mark the `STM32F103C8T6` staged suite as fully validated on the canonical
   `.99` bench

## Closeout Decision

This round is a valid live-validation closeout for the suite migration and
bench-correction work. It is also the final golden-pass closeout for the
canonical STM32F103C8T6 staged suite on `.99`.

The correct conclusion is:

- code and pack formalization: complete
- stale `.109` C8T6 binding: removed from the active path
- canonical `.99` wiring contract: confirmed on live hardware
- Stage 0 to Stage 3 targeted validation: complete
- final canonical full-pack rerun: `24/24 PASS`
