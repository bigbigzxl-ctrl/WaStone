# STM32F030C8T6 DAPLink UART Interrupt Skill

Use this when validating interrupt-driven `USART1` RX on the STM32F030C8T6 with DAPLink-hosted UART wiring.

Wiring:
- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- standard SWD wiring present

Run:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_interrupt.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

Pack contents:
- `tests/plans/stm32f030c8t6_uart_rx_irq_probe.json`
- `tests/plans/stm32f030c8t6_uart_echo_irq.json`

Firmware:
- `firmware/targets/stm32f030c8t6_uart_rx_irq_probe/main.c`
- `firmware/targets/stm32f030c8t6_uart_echo_irq/main.c`

Important implementation rule:
- Keep the shared `startup.c` external interrupt vector table aligned exactly to ST's `startup_stm32f030x8`.
- A single extra reserved slot before `SVC_Handler` shifts every external IRQ and makes `USART1_IRQn` fall into `Default_Handler`.

Expected result:
- `2 / 2 PASS`
- pack run reference: `pack_runs/2026-04-03_15-23-13_stm32f030c8t6_daplink_uart_interrupt_stm32f030c8t6_daplink_uart`
