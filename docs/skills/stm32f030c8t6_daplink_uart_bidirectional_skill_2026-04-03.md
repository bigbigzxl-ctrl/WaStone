# STM32F030C8T6 DAPLink UART Bidirectional Pack Skill

## Use When

Use this skill when validating `STM32F030C8T6` on the local DAPLink UART bench
with true bidirectional host interaction, not only host-observed TX.

## Entry Point

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_bidirectional.json --board stm32f030c8t6_daplink_uart --stop-on-fail
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
- `stm32f030c8t6_uart_rx_probe`
- `stm32f030c8t6_uart_echo`

## Core Rules

1. Keep this pack separate from the canonical local-loopback Golden Suite.
2. Run the pack serially on this single board and probe.
3. Use `uart_tx_probe` first if TX-only observed tests start failing.
4. Use `uart_rx_probe` before trusting higher-level echo failures.
5. For host-driven UART tests, firmware must emit periodic ready text instead of
   a single boot-only banner.
6. Do not quietly include `uart_dma_observed` here until it has independent
   live proof on this exact MCU and bench.

## Working Expectations

Representative expected strings:

- `AEL_UART_TX_PROBE`
- `AEL_UART_MB 55 AA 12 34`
- `AEL_READY STM32F030C8T6 UART`
- `AEL_UART_RX_READY` then `AEL_UART_RX_OK`
- `AEL_ECHO_READY` then `AEL_ECHO:PING_ECHO_42`

## Deferred Scope

Still deferred:

- `stm32f030c8t6_uart_dma_observed`

Related deferred writeup:

- [stm32f030c8t6_uart_dma_investigation_2026-04-03.md](/home/ali/work/ai-embedded-lab/docs/reports/stm32f030c8t6_uart_dma_investigation_2026-04-03.md)
