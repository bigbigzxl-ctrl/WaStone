# STM32F030C8T6 DAPLink UART Observed Pack Skill

## Use When

Use this skill when validating `STM32F030C8T6` on the alternate local DAPLink
fixture where UART is host-observed instead of locally looped back.

## Entry Point

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_observed.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

## Fixture Contract

- SWD via DAPLink
- `NRST -> DAPLink NRST`
- `GND -> probe GND`
- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PC13 -> onboard LED`

## Pack Coverage

- `stm32f030c8t6_uart_tx_probe`
- `stm32f030c8t6_uart_multibyte_observed`
- `stm32f030c8t6_uart_banner`

## Core Rules

1. Keep this pack separate from the canonical local-loopback Golden Suite.
2. Run the pack serially on this single board.
3. Use `uart_tx_probe` first if the higher-level UART proofs fail unexpectedly.
4. Treat `PA9 -> DAPLink RX` as the directly proven path.
5. Treat `DAPLink TX -> PA10` as part of the fixture contract, but do not
   claim RX proof unless a dedicated RX-consuming test actually measures it.
6. If `uart_dma_observed` is revisited, compare against ST official
   `STM32F030x8` UART DMA examples before spending more bench time.

## Working Expectations

Representative expected strings:

- `AEL_UART_TX_PROBE`
- `AEL_UART_MB 55 AA 12 34`
- `AEL_READY STM32F030C8T6 UART`

## Deferred Scope

Still deferred on this fixture:

- `stm32f030c8t6_uart_dma_observed`

Known deferred run:

- `runs/2026-04-03_14-19-36_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`

The current failure shape is no UART bytes observed at `/dev/ttyACM0`, so this
should not be quietly folded into the validated pack.
