# STM32F401 Black Pill DAPLink UART Golden Pack Skill

## Use When

Use this skill when validating the STM32F401 Black Pill on the local DAPLink
bench where the same DAPLink provides both SWD and the host UART bridge.

## Entry Point

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f401ce_blackpill_daplink_uart_golden.json --board stm32f401ce_blackpill_daplink --stop-on-fail
```

## Fixture Contract

- `SWDIO -> DAPLink SWDIO`
- `SWCLK -> DAPLink SWCLK`
- `PA9/USART1_TX -> DAPLink RX`
- `DAPLink TX -> PA10/USART1_RX`
- `PC13 -> onboard LED`
- `GND -> DAPLink GND`

## Pack Coverage

- `stm32f401ce_blackpill_daplink_led_blink`
- `stm32f401ce_blackpill_daplink_uart_banner`
- `stm32f401ce_blackpill_daplink_uart_ping_pong`
- `stm32f401ce_blackpill_daplink_uart_echo`

## Core Rules

1. Keep this as a DAPLink side-pack, not a substitute for a broader STM32F401
   peripheral Golden Suite.
2. Treat Stage 0 as visual/program-only unless the bench gains a real LED
   observer.
3. Put all machine-verified runtime proof on UART, because DAPLink actually
   exposes that surface to AEL.
4. Use a stable `/dev/serial/by-id/...` path for the DAPLink CDC port instead
   of assuming `/dev/ttyACM0` stays fixed.
5. Expect local DAPLink OpenOCD to allocate a conflict-free port if `3333` is
   already occupied; do not hardcode the first port seen in logs as a failure.
6. Keep host-driven UART firmware ready for the generic post-flash settle
   window by emitting periodic `READY` text, not only a one-shot boot banner.

## Working Expectations

Representative expected strings:

- `STM32F401 UART READY`
- `READY`
- `PONG`
- `ECHO hello-from-pc`

## Troubleshooting Order

1. Confirm the board still identifies as an STM32F401-class target over SWD.
2. Run the passive banner test first to prove TX and CDC-path health.
3. Run `uart_ping_pong` before trusting `uart_echo` failures.
4. If Stage 0 fails, check whether the plan incorrectly expects a measurable
   LED surface on a bench that only has SWD and UART.
5. If logs show `port 3333 taken`, treat it as normal unless the allocated port
   also fails to start listening.

## Lesson To Keep

On a DAPLink-only bench, do not pretend a visual LED heartbeat is a
machine-verified signal. Formalize LED as visual/program-only and reserve
automation claims for the surfaces the hardware actually exposes: SWD and CDC
UART.
