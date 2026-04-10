# Skill: Bring Up STM32F401CCU6 On Local DAPLink To Stage 0

Date: `2026-04-10`
Scope: `stm32f401ccu6`, local USB DAPLink, SWD-only Stage 0 bring-up

## When To Use

Use this workflow when:

- the target MCU silk is `STM32F401CCU6`
- the debug probe is a local USB DAPLink / CMSIS-DAP device
- the goal is to reach a repeatable Stage 0 baseline quickly

## Required Wiring

- SWDIO
- SWCLK
- GND

Optional:

- `PC13` onboard LED for visual confirmation
- DAPLink UART bridge on `PA9/PA10` if UART tests are needed later

## Recommended AEL Shape

- instrument instance: explicit local `daplink` instance using `/dev/serial/by-id/...`
- board id: `stm32f401ccu6_daplink`
- visual stage 0 test: `stm32f401ccu6_daplink_led_blink`
- runtime stage 0 test: `stm32f401ccu6_daplink_minimal_runtime_mailbox`

## Why This Worked

- `STM32F401CCU6` is an `STM32F401xC` device class, so the `stm32f401ce` target assets are compatible for basic bring-up:
  - `startup_stm32f401xc.s`
  - `stm32f401xc.h`
  - `256 KB` flash-class linker layout
- the stage 0 mailbox runtime target at `0x2000FC00` requires only SWD access and no external observation hardware

## Fast Validation Sequence

1. Confirm DAPLink enumeration and stable `/dev/serial/by-id/...` path.
2. Run LED blink first to prove flash + reset + board LED path.
3. Run minimal runtime mailbox second to prove boot-to-PASS behavior without extra wiring.

## Expected PASS Signals

- LED blink:
  - `PC13` blinks after flash
- mailbox stage:
  - `magic = 0xAE100001`
  - `status = 0x00000002`
  - `detail0` increments over time

## CLI Pattern

```bash
python3 -m ael run \
  --board stm32f401ccu6_daplink \
  --test tests/plans/stm32f401ccu6_daplink_led_blink.json \
  --controller configs/instrument_instances/daplink_stm32f401ccu6_local.yaml

python3 -m ael run \
  --board stm32f401ccu6_daplink \
  --test tests/plans/stm32f401ccu6_daplink_minimal_runtime_mailbox.json \
  --controller configs/instrument_instances/daplink_stm32f401ccu6_local.yaml
```

## Lesson

When raw debug ID readings and user-reported silk appear inconsistent, do not anchor only on the early debug inference. If the package silk is explicit and the `STM32F401xC` build path passes both LED blink and Stage 0 mailbox on real hardware, formalize the local board around the confirmed `F401xC` identity and move forward.
