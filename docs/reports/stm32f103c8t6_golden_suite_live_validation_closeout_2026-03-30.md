# STM32F103C8T6 Golden Suite Live Validation Closeout

**Date:** 2026-03-30
**Board:** `stm32f103_gpio`
**Pack:** `packs/stm32f103c8t6_golden.json`
**Instrument:** `esp32jtag_stm32_golden` @ `192.168.2.109:4242`
**Status:** blocked by bench reachability, not by suite logic

## Summary

The staged `STM32F103C8T6` golden suite was exercised through the real
repo-native entrypoint:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio
```

Selection and build were correct. The pack resolved the expected board,
instrument, and canonical wiring, and the newly added tests built normally.

Live validation did not reach DUT execution because the configured control
instrument was unreachable from the current host. The failure is a bench access
problem, not evidence against the new Stage 1/2 tests or the pack structure.

## Setup Confirmed

- board: `stm32f103_gpio`
- MCU: `stm32f103c8t6`
- instrument: `esp32jtag_stm32_golden`
- GDB remote: `192.168.2.109:4242`
- web API: `https://192.168.2.109:443`
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
- the instrument web surface was also unreachable:
  - `curl -k -sS https://192.168.2.109/`
  - result: `Failed to connect ... No route to host`

This establishes that:

1. pack selection is correct
2. build paths are correct
3. the blocker is bench transport reachability to `192.168.2.109`

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

Not yet validated on real hardware:

- flash success on `esp32jtag_stm32_golden`
- DUT runtime behavior
- mailbox PASS/FAIL semantics on bench
- Stage 3 LA capture behavior

## Root Cause Classification

Primary classification: `bench/resource reachability`

Why this is not a suite bug:

- failure happens before DUT execution
- the same host cannot reach either the GDB surface or HTTPS surface
- `wiring_verify` preflight independently reports transport unhealthy before any
  firmware load attempt

## Recommended Next Step

1. Restore network reachability to `192.168.2.109`
2. Re-run:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio
```

3. If needed, start with:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c8t6_golden.json --board stm32f103_gpio --stage 0,1
```

4. After the bench is reachable, record the full pass/fail matrix and update the
   DUT as validated if the suite passes cleanly

## Closeout Decision

This round is a valid live-validation closeout for the suite integration work,
but it is not a golden-pass closeout.

The correct conclusion is:

- code and pack formalization: complete
- live repo-native execution: performed
- blocker: external bench reachability
- next action: restore instrument access, then rerun
