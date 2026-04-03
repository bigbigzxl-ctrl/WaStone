# STM32F030C8T6 Pre-Stage2 Connectivity Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink`
**Pack:** `packs/stm32f030c8t6_pre_stage2_connectivity.json`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** final canonical pre-Stage2 rerun complete, `5/5 PASS`

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

The live result from this round is now full green. All five local loopback
paths are confirmed by mailbox-backed runs on the same bench contract.

## Evidence

Final all-pass rerun:

- `runs/2026-04-03_08-55-54_stm32f030c8t6_daplink_stm32f030c8t6_pb0_pb1_probe`
- `runs/2026-04-03_08-56-01_stm32f030c8t6_daplink_stm32f030c8t6_pb8_pb9_probe`
- `runs/2026-04-03_08-56-07_stm32f030c8t6_daplink_stm32f030c8t6_pa0_pa1_adc_probe`
- `runs/2026-04-03_08-56-13_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`
- `runs/2026-04-03_08-56-19_stm32f030c8t6_daplink_stm32f030c8t6_uart_loopback_probe`

Earlier targeted runs that established the initial shape:

- `runs/2026-04-03_08-46-02_stm32f030c8t6_daplink_stm32f030c8t6_pb0_pb1_probe`
- `runs/2026-04-03_08-46-09_stm32f030c8t6_daplink_stm32f030c8t6_pb8_pb9_probe`
- `runs/2026-04-03_08-46-15_stm32f030c8t6_daplink_stm32f030c8t6_pa0_pa1_adc_probe`
- `runs/2026-04-03_08-46-33_stm32f030c8t6_daplink_stm32f030c8t6_uart_loopback_probe`

The initial failing runs that correctly exposed the bad `PA7 <-> PA6` jumper:

- `runs/2026-04-03_08-46-22_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`
- `runs/2026-04-03_08-50-59_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`
- `runs/2026-04-03_08-51-34_stm32f030c8t6_daplink_stm32f030c8t6_pa7_pa6_spi_probe`

## What Was Validated

Confirmed on real hardware:

- `stm32f030c8t6_pb0_pb1_probe`
- `stm32f030c8t6_pb8_pb9_probe`
- `stm32f030c8t6_pa0_pa1_adc_probe`
- `stm32f030c8t6_pa7_pa6_spi_probe`
- `stm32f030c8t6_uart_loopback_probe`

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

4. A single failing pre-Stage2 net can still make the pack useful.

The earlier `PA7 <-> PA6` failure was immediate `ERR_HIGH_MISS` on the first
sampled high level (`0xE601`, `detail0=0`). That correctly pointed at the
physical jumper rather than host tooling. After the jumper was corrected, the
same probe passed without software changes.

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
- a full green rerun
- reusable skill capture

That is the minimum acceptable closeout state for a new pack.

## Closeout Decision

This round is a valid pre-Stage2 pack closeout for `STM32F030C8T6`.

The correct current conclusion is:

- canonical pack shape: complete
- live validation: complete
- confirmed nets: `PB0/PB1`, `PB8/PB9`, `PA0/PA1`, `PA7/PA6`, `PA9/PA10`
- pre-Stage2 gate: green
- next action: proceed to Stage 2 suite generation and live validation
