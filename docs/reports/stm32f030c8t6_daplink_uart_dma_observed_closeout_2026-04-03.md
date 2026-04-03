# STM32F030C8T6 DAPLink UART DMA Observed Pack Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink_uart`
**Pack:** `packs/stm32f030c8t6_daplink_uart_dma_observed.json`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** validated side-pack rerun completed, `2/2 PASS`

## Summary

This pack formalizes the split bare-metal UART DMA proofs for the
`STM32F030C8T6` on DAPLink bridge wiring.

Entry point:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_dma_observed.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

Fixture contract:

- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- `GND -> probe GND`

Shared bench jumpers remain:

- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PC13 -> onboard LED`

## Validated Coverage

Validated by the serialized rerun:

- `stm32f030c8t6_uart_dma_observed`
- `stm32f030c8t6_uart_dma_rx_observed`

Final all-pass pack run:

- `pack_runs/2026-04-03_17-13-00_stm32f030c8t6_daplink_uart_dma_observed_stm32f030c8t6_daplink_uart`

Representative rerun artifacts:

- `runs/2026-04-03_17-13-00_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`
- `runs/2026-04-03_17-13-15_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_rx_observed`

## What This Pack Proves

TX-side proof:

- bare-metal `USART1 TX DMA`
- `DMA1 Channel2 -> USART1_TDR`
- host observation of the actual DMA-transmitted payload on `PA9`

RX-side proof:

- bare-metal `USART1 RX DMA`
- `USART1_RDR -> DMA1 Channel3`
- host-driven payload from `DAPLink TX -> PA10`
- exact payload match before PASS

## Important Implementation Detail

The main blocker on both deferred targets was not the DMA path itself. It was
startup code.

Failure mode:

- target-local `_etext` could land on a non-4-byte-aligned address
- inherited reset code copied `.data` with `uint32_t *`
- Cortex-M0 raised an unaligned access fault before `main()`

Fix:

- both targets now use local byte-copy startup files
- both still call ST `SystemInit()` from `system_stm32f0xx.c`
- both targets were reduced to one direct DMA path instead of multiple remap and
  sequencing variants

This changed the observed outcome from "UART readiness missing" to stable DMA
PASS on both sides.

## Relation To Other DMA Work

ST HAL reference proofs remain separately available:

- `tests/plans/stm32f030c8t6_uart_dma_hal_observed.json`
- `tests/plans/stm32f030c8t6_uart_dma_rx_hal_observed.json`

Detailed investigation log remains:

- [stm32f030c8t6_uart_dma_investigation_2026-04-03.md](/home/ali/work/ai-embedded-lab/docs/reports/stm32f030c8t6_uart_dma_investigation_2026-04-03.md)

## Closeout Decision

This pack is now a reusable validated UART DMA observed side-pack for the
`STM32F030C8T6` on DAPLink bridge wiring.

The correct state is:

- pack: formalized and rerun
- live validation: complete for all included tests
- reusable skill capture: completed
- canonical golden suite: unchanged
- DMA coverage: now available as a dedicated side-pack rather than folded into
  the canonical local-loopback suite
