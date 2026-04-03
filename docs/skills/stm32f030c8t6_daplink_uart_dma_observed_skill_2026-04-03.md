# STM32F030C8T6 DAPLink UART DMA Observed Skill

Use this when you want the validated split UART DMA side-pack for the
`STM32F030C8T6` on DAPLink bridge wiring.

Wiring:

- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- standard SWD wiring present

Run:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_dma_observed.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

Pack:

- `packs/stm32f030c8t6_daplink_uart_dma_observed.json`

Coverage:

- bare-metal `USART1 TX DMA` observed on `PA9`
- bare-metal `USART1 RX DMA` host-driven on `PA10`

Reference validation:

- `2 / 2 PASS`
- pack run: `pack_runs/2026-04-03_17-13-00_stm32f030c8t6_daplink_uart_dma_observed_stm32f030c8t6_daplink_uart`

Reference tests:

- `tests/plans/stm32f030c8t6_uart_dma_observed.json`
- `tests/plans/stm32f030c8t6_uart_dma_rx_observed.json`

Important lesson:

- if a small Cortex-M0 target uses a target-local linker layout, check whether
  `_etext` is aligned before reusing a word-copy reset stub
- for these DMA observed proofs, byte-copy `.data` startup was required to avoid
  pre-`main()` hard faults

Do not use this pack for:

- canonical local-loopback Golden Suite wiring `PA9 <-> PA10`
- full-duplex combined TX/RX DMA in one target
- claiming DMA coverage on boards that have not replicated this exact DAPLink
  UART fixture
