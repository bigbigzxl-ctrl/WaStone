# STM32F401CCU6 DAPLink Local Stage 0 Closeout 2026-04-10

Board: `stm32f401ccu6_daplink`
Instrument: `daplink_stm32f401ccu6_local`
Date: `2026-04-10`

## What Was Validated

- local USB DAPLink enumerates and is usable through OpenOCD/CMSIS-DAP
- SWD attach succeeds against the target MCU
- F401xC LED blink image programs and runs on `PC13`
- Stage 0 mailbox runtime image programs and reaches `PASS`

## Hardware Evidence

- DAPLink enumerated as `CMSIS-DAP_CZ`
- serial path used:
  - `/dev/serial/by-id/usb-jixin.pro_CMSIS-DAP_CZ_CZ_2023_6688-if00`
- SWD identity reads succeeded repeatedly
- chip silk reported by user: `STM32F401CCU6`

## AEL Paths Added

- instrument instance:
  - `configs/instrument_instances/daplink_stm32f401ccu6_local.yaml`
- board profile:
  - `configs/boards/stm32f401ccu6_daplink.yaml`
- bench profile:
  - `configs/bench_profiles/stm32f401ccu6_daplink__default.yaml`
- stage 0 visual test:
  - `tests/plans/stm32f401ccu6_daplink_led_blink.json`
- stage 0 mailbox test:
  - `tests/plans/stm32f401ccu6_daplink_minimal_runtime_mailbox.json`

## Live Run Results

- LED blink:
  - run id: `2026-04-10_13-08-47_stm32f401ccu6_daplink_stm32f401ccu6_daplink_led_blink`
  - result: `PASS`
- Minimal runtime mailbox:
  - run id: `2026-04-10_13-08-47_stm32f401ccu6_daplink_stm32f401ccu6_daplink_minimal_runtime_mailbox`
  - result: `PASS`

## Important Findings

- The bench is usable as an `STM32F401xC` path even though earlier raw debug ID reads looked more like an F411-class signature.
- The decisive board identification for this bring-up was the user-confirmed package silk `STM32F401CCU6`.
- For this board, `PC13` LED blink and the `0x2000FC00` mailbox stage both worked with only SWD and GND.

## Reusable Conclusion

For a local DAPLink bench with an STM32F401xC target:

- prefer an explicit instrument instance file with the stable `/dev/serial/by-id/...` path
- use `firmware/targets/stm32f401ce` for the visual stage 0 blink image
- use `firmware/targets/stm32f401rct6_minimal_runtime_mailbox` for the zero-extra-wire stage 0 runtime gate
- pass `--controller configs/instrument_instances/<instance>.yaml` explicitly when invoking `ael run`
