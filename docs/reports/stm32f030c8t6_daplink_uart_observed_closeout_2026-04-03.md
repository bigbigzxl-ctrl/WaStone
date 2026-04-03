# STM32F030C8T6 DAPLink UART Observed Pack Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink_uart`
**Pack:** `packs/stm32f030c8t6_daplink_uart_observed.json`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** validated side-pack rerun completed, `3/3 PASS`

## Summary

This pack formalizes the non-canonical DAPLink-hosted UART path for
`STM32F030C8T6`. It is intentionally separate from the canonical local-loopback
Golden Suite.

Entry point:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_observed.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

This pack proves host-observed UART TX on `USART1` with the wiring contract:

- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- `GND -> probe GND`

The other jumpers remain the same as the canonical bench:

- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PC13 -> onboard LED`

## Validated Coverage

Validated by the final serialized rerun:

- `stm32f030c8t6_uart_tx_probe`
- `stm32f030c8t6_uart_multibyte_observed`
- `stm32f030c8t6_uart_banner`

Final all-pass pack run:

- `pack_runs/2026-04-03_14-22-37_stm32f030c8t6_daplink_uart_observed_stm32f030c8t6_daplink_uart`

Representative run artifacts from that rerun:

- `runs/2026-04-03_14-22-37_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_tx_probe`
- `runs/2026-04-03_14-22-50_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_multibyte_observed`
- `runs/2026-04-03_14-23-04_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_banner`

Earlier targeted live runs used to stabilize the pack before the rerun:

- `runs/2026-04-03_14-12-46_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_tx_probe`
- `runs/2026-04-03_14-18-16_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_multibyte_observed`
- `runs/2026-04-03_14-18-33_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_banner`

## Key Findings

1. The DAPLink UART path is now a proven alternate fixture for this MCU.

The canonical F030 Golden Suite keeps `PA9 <-> PA10` local. This pack proves
the alternate bench shape where UART is host-observed through DAPLink instead.

2. `uart_tx_probe` is still worth keeping.

It gave a low-cost proof that `PA9 -> DAPLink RX` was correct before relying on
higher-level multibyte and banner checks.

3. This pack is validated, but not canonical golden.

The canonical suite remains `packs/stm32f030c8t6_golden.json` on local
`PA9 <-> PA10` loopback. This pack is an additional validated fixture path, not
a silent contract change inside the canonical suite.

## Deferred Scope

Not included in this validated pack:

- `stm32f030c8t6_uart_dma_observed`

Current deferred evidence:

- `runs/2026-04-03_14-19-36_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`

Observed failure shape on that run:

- `failure_class = uart_expected_patterns_missing`
- `bytes = 0`
- expected string not seen: `AEL_UART_DMA A1 B2 C3 D4 55 66 77 88`

So the honest current conclusion is that host-observed UART TX is stable, while
DMA-based UART TX is still unresolved on this exact MCU and bench.

## Closeout Decision

This pack is now a reusable validated side-pack for `STM32F030C8T6` on the
local DAPLink UART fixture.

The correct state is:

- board profile: formalized
- pack: formalized and rerun
- live validation: complete for the 3 included tests
- reusable skill capture: completed
- `uart_dma_observed`: explicitly deferred
