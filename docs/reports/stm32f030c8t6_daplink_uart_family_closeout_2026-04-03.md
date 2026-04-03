# STM32F030C8T6 DAPLink UART Family Closeout

Date: 2026-04-03

Pack:
- `packs/stm32f030c8t6_daplink_uart_family.json`

Board:
- `stm32f030c8t6_daplink_uart`

Result:
- `7 / 7 PASS`
- pack run: `pack_runs/2026-04-03_15-23-46_stm32f030c8t6_daplink_uart_family_stm32f030c8t6_daplink_uart`

Validated tests:
- `tests/plans/stm32f030c8t6_uart_tx_probe.json`
- `tests/plans/stm32f030c8t6_uart_multibyte_observed.json`
- `tests/plans/stm32f030c8t6_uart_banner.json`
- `tests/plans/stm32f030c8t6_uart_rx_probe.json`
- `tests/plans/stm32f030c8t6_uart_echo.json`
- `tests/plans/stm32f030c8t6_uart_rx_irq_probe.json`
- `tests/plans/stm32f030c8t6_uart_echo_irq.json`

Wiring contract:
- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- standard SWD wiring present

What this family pack covers:
- host-observed TX probe
- host-observed multibyte TX
- host-observed banner TX
- host-driven polling RX proof
- host-driven polling echo
- host-driven interrupt RX proof
- host-driven interrupt echo

What it does not cover:
- `UART DMA`
- canonical local-loopback UART wiring (`PA9 <-> PA10`)

Related packs:
- `packs/stm32f030c8t6_daplink_uart_observed.json`
- `packs/stm32f030c8t6_daplink_uart_bidirectional.json`
- `packs/stm32f030c8t6_daplink_uart_interrupt.json`

Primary code and plan locations:
- `firmware/targets/stm32f030c8t6_uart_tx_probe/main.c`
- `firmware/targets/stm32f030c8t6_uart_multibyte_observed/main.c`
- `firmware/targets/stm32f030c8t6_uart_banner/main.c`
- `firmware/targets/stm32f030c8t6_uart_rx_probe/main.c`
- `firmware/targets/stm32f030c8t6_uart_echo/main.c`
- `firmware/targets/stm32f030c8t6_uart_rx_irq_probe/main.c`
- `firmware/targets/stm32f030c8t6_uart_echo_irq/main.c`

Operational conclusion:
- This is the consolidated non-DMA UART validation entry point for the STM32F030C8T6 on DAPLink bridge wiring.
- Use the canonical Golden Suite separately when the board is wired for `PA9 <-> PA10` local loopback.
