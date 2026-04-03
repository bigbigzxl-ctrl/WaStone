# STM32F030C8T6 DAPLink UART Family Skill

Use this when you want one consolidated non-DMA UART pack for the STM32F030C8T6 on DAPLink bridge wiring.

Wiring:
- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- standard SWD wiring present

Run:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_family.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

Pack:
- `packs/stm32f030c8t6_daplink_uart_family.json`

Coverage:
- observed TX
- polling RX / echo
- interrupt RX / echo

Reference validation:
- `7 / 7 PASS`
- pack run: `pack_runs/2026-04-03_15-23-46_stm32f030c8t6_daplink_uart_family_stm32f030c8t6_daplink_uart`

Do not use this pack for:
- `UART DMA`
- the canonical local-loopback Golden Suite wiring `PA9 <-> PA10`
