# STM32F030C8T6 Pre-Stage2 Connectivity Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink`
**Pack:** `packs/stm32f030c8t6_pre_stage2_connectivity.json`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** partial live validation complete, `4/5 PASS`

## Summary

`STM32F030C8T6` now has a formal pre-Stage2 connectivity pack on the local
DAPLink bench. The current canonical bench contract for this pack is:

- SWD via DAPLink
- `NRST -> DAPLink NRST`
- `GND -> probe GND`
- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PA9 <-> PA10`
- `PC13 -> onboard LED`

The live result from this round is not full green yet. Four loopback paths are
confirmed by mailbox-backed runs, while `PA7 <-> PA6` remains unconfirmed and
currently fails at the first sampled high level.

## Evidence

Passing representative runs:

- `runs/2026-04-03_08-46-02_stm32f030c8t6_daplink_stm32f030c8t6_pb0_pb1_probe`
- `runs/2026-04-03_08-46-09_stm32f030c8t6_daplink_stm32f030c8t6_pb8_pb9_probe`
- `runs/2026-04-03_08-46-15_stm32f030c8t6_daplink_stm32f030c8t6_pa0_pa1_adc_probe`
- `runs/2026-04-03_08-46-33_stm32f030c8t6_daplink_stm32f030c8t6_uart_loopback_probe`

Failing representative runs for the remaining path:

- `runs/2026-04-03_08-46-22_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`
- `runs/2026-04-03_08-50-59_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`
- `runs/2026-04-03_08-51-34_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`

## What Was Validated

Confirmed on real hardware:

- `stm32f030c8t6_pb0_pb1_probe`
- `stm32f030c8t6_pb8_pb9_probe`
- `stm32f030c8t6_pa0_pa1_adc_probe`
- `stm32f030c8t6_uart_loopback_probe`

Not yet confirmed:

- `stm32f030c8t6_pa7_pa6_spi_probe`

## Key Findings

1. Current pre-Stage2 UART should stay board-local.

For this pack, `PA9 <-> PA10` is the correct loopback contract. DAPLink UART
attachment caused avoidable routing confusion and was intentionally deferred to
a separate future pack.

2. DAPLink-backed mailbox runs must honor the instrument GDB binary.

The local setup uses `gdb-multiarch`, not `arm-none-eabi-gdb`. The pack only
became runnable after the mailbox verify path was updated to use the configured
`gdb_cmd` from the instrument instance.

3. `PA7 <-> PA6` should be treated as a connectivity gate first.

The first attempt used SPI peripheral loopback directly. After the path still
failed, the pre-Stage2 check was reduced to a plain GPIO connectivity probe so
that pre-Stage2 remains a wiring gate instead of a mixed wiring-plus-peripheral
debug exercise.

4. The remaining `PA7 <-> PA6` failure currently points at wiring, not host tooling.

The latest mailbox failure is immediate `ERR_HIGH_MISS` on the first sampled
high level (`0xE601`, `detail0=0`). That means software successfully flashed,
ran, and toggled the designated output path, but the paired input did not see
the asserted state.

## Deferred Scope

Not part of this pack:

- host-observed DAPLink UART tests on `PA9/PA10`
- full SPI1 peripheral loopback proof on `PA5/PA6/PA7`

Those belong to later packs or Stage 2 tests.

## Process Note

This work would be incomplete if it stopped after code generation and the first
few live passes. The pack now has:

- code and config
- live evidence
- explicit remaining blocker
- reusable skill capture

That is the minimum acceptable closeout state for a new pack, even when one
path is still blocked on bench wiring.

## Closeout Decision

This round is a valid pre-Stage2 pack closeout for `STM32F030C8T6`, but not a
full-pack promotion.

The correct current conclusion is:

- canonical pack shape: complete
- live validation: partial
- confirmed nets: `PB0/PB1`, `PB8/PB9`, `PA0/PA1`, `PA9/PA10`
- blocked net: `PA7/PA6`
- next action: re-check or re-seat the `PA7 <-> PA6` jumper, then rerun the pack
