# STM32F103C8T6 Golden Suite Live Validation Closeout

**Date:** 2026-03-30
**Board:** `stm32f103_gpio`
**Pack:** `packs/stm32f103c8t6_golden.json`
**Instrument:** initial run used stale binding `esp32jtag_stm32_golden` @ `192.168.2.109:4242`
**Status:** stale `.109` binding identified and corrected; rerun on `.99` reached real DUT execution

## Summary

The staged `STM32F103C8T6` golden suite was exercised through the real
repo-native entrypoint:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio
```

Selection and build were correct at the pack level, but the board profile was
still bound to the wrong control instrument. The pack resolved the expected
board and wiring, but selected `.109` instead of the actual F103 golden setup
at `.99`.

The first live validation attempt did not reach DUT execution because the
configured control instrument in the board profile was stale. After rebinding
the board to the actual F103 golden instrument at `.99`, rerun reached real DUT
execution and then validated Stage 0, Stage 1, and the full Stage 2 mailbox
self-check set on real hardware.

## Setup Confirmed

- board: `stm32f103_gpio`
- MCU: `stm32f103c8t6`
- stale instrument selected during first run: `esp32jtag_stm32_golden`
- stale GDB remote selected during first run: `192.168.2.109:4242`
- actual F103 golden instrument confirmed afterwards: `192.168.2.99:4242`
- actual F103 golden web API confirmed afterwards: `https://192.168.2.99:443`
- canonical wiring:
  - `PA8 ↔ PB8`
  - `PA9 ↔ PA10`
  - `PA7 ↔ PA6`
  - `PA1 ↔ PA0`
  - `PA4 → P0.0`
  - `PA5 → P0.1`
  - `PC13 → LED`

## Evidence

Representative pack runs:

- sandbox-limited run: `pack_runs/2026-03-30_07-55-51_stm32f103c8t6_golden_stm32f103_gpio`
- escalated live-bench run: `pack_runs/2026-03-30_07-57-14_stm32f103c8t6_golden_stm32f103_gpio`
- corrected-binding rerun: `pack_runs` session starting `2026-03-30 08:12`

Representative test runs:

- `runs/2026-03-30_08-00-33_stm32f103_gpio_stm32f103_wiring_verify`
- `runs/2026-03-30_08-01-44_stm32f103_gpio_stm32f103_exti_mailbox`

Key observations:

- `wiring_verify` failed in `preflight`, not build:
  - `Preflight: ping 192.168.2.109 -> FAIL`
  - `TCP 192.168.2.109:4242 -> FAIL`
  - `probe_transport_unhealthy`
- mailbox and visual tests built successfully, then failed in `load`:
  - repeated `could not connect: No route to host`
- the stale instrument web surface was also unreachable:
  - `curl -k -sS https://192.168.2.109/`
  - result: `Failed to connect ... No route to host`
- the actual F103 golden bench was reachable:
  - `curl -k -sS https://192.168.2.99/`
  - result: `Authentication Required`
  - TCP `192.168.2.99:4242` -> `open`

This establishes that:

1. pack selection is correct
2. build paths are correct
3. the blocker in that run was stale board-to-instrument binding to `.109`

Corrected-binding rerun established that:

1. `.99` is the correct reachable control instrument for this suite
2. Stage 0 real flash and verify succeed on `.99`
3. Stage 1 real flash and mailbox verify succeed on `.99`
4. `wiring_verify` preflight passes on `.99` with healthy GDB and LA surfaces

## What Was Validated

Validated at control-plane level:

- new `stm32f103c8t6_golden` pack resolves and starts correctly
- stage ordering is accepted by `ael pack`
- new tests integrate correctly into the pack
- representative new targets build successfully in live execution:
  - `stm32f103_systick_mailbox`
  - `stm32f103_gpio_loopback_mailbox`
  - `stm32f103_capture_mailbox`
  - `stm32f103_uart_multibyte`
  - `stm32f103_uart_dma`

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

Not yet fully closed out on real hardware:

- Stage 3 LA capture pass/fail matrix
- full-pack completion under corrected `.99` binding

## Root Cause Classification

Primary classification: `stale board/instrument binding`

Why this is not a suite bug:

- failure happens before DUT execution
- the same host cannot reach either stale `.109` surface
- `wiring_verify` preflight independently reports transport unhealthy before any
  firmware load attempt
- the actual `.99` F103 instrument was independently confirmed reachable

## Recommended Next Step

1. Use the corrected `.99` instrument binding
2. Continue / re-run:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio
```

3. If needed, start with:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio --stage 0,1
```

4. Run Stage 3 LA/banner validation and then the full pack end-to-end
5. Update the DUT as validated if the suite passes cleanly

## Closeout Decision

This round is a valid live-validation closeout for the suite integration work,
but it is not a golden-pass closeout.

The correct conclusion is:

- code and pack formalization: complete
- live repo-native execution: performed
- blocker in first run: stale `.109` binding
- next action: finish Stage 3 and full-pack closeout on corrected `.99` binding
