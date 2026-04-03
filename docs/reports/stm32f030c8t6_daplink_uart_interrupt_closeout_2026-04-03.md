# STM32F030C8T6 DAPLink UART Interrupt Closeout

Date: 2026-04-03

Pack:
- `packs/stm32f030c8t6_daplink_uart_interrupt.json`

Board:
- `stm32f030c8t6_daplink_uart`

Result:
- `2 / 2 PASS`
- pack run: `pack_runs/2026-04-03_15-23-13_stm32f030c8t6_daplink_uart_interrupt_stm32f030c8t6_daplink_uart`

Validated tests:
- `tests/plans/stm32f030c8t6_uart_rx_irq_probe.json`
- `tests/plans/stm32f030c8t6_uart_echo_irq.json`

Validated runs:
- `runs/2026-04-03_15-23-13_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_rx_irq_probe`
- `runs/2026-04-03_15-23-27_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_echo_irq`

Wiring contract:
- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- `SWDIO/SWCLK/GND` connected as normal DAPLink SWD

Code added:
- `firmware/targets/stm32f030c8t6_uart_rx_irq_probe/main.c`
- `firmware/targets/stm32f030c8t6_uart_echo_irq/main.c`
- `tests/plans/stm32f030c8t6_uart_rx_irq_probe.json`
- `tests/plans/stm32f030c8t6_uart_echo_irq.json`
- `packs/stm32f030c8t6_daplink_uart_interrupt.json`

Shared runtime dependency updated:
- `firmware/targets/stm32f030c8t6/startup.c`

What was required to make it pass:
- The shared STM32F030 startup vector table had one extra reserved slot before `SVC_Handler`.
- That shifted all external IRQ vectors by one entry.
- `USART1_IRQn` therefore landed in `Default_Handler` whenever host-driven RX activity started.
- Fixing the vector table alignment restored correct dispatch to `USART1_IRQHandler`.
- The interrupt handlers also clear `PE/FE/NE/ORE` error conditions so host-side serial open noise does not wedge the test.
- The interrupt tests now prove host-driven RX and echo without relying on startup banner detection.

Coverage statement:
- This pack proves `USART1` host-to-DUT RX handled by interrupt.
- It also proves interrupt-driven echo / request-response on the DAPLink UART bridge.
- It does not cover UART DMA.
