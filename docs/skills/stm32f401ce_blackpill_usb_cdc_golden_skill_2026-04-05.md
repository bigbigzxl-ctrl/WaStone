# STM32F401 Black Pill USB CDC Golden Pack Skill

## Use When

Use this skill when validating the STM32F401 Black Pill on the local DAPLink
bench where:

- DAPLink provides SWD flashing/debug
- the target board USB connector is plugged directly into the host PC
- the test goal is USB CDC ACM enumeration and host/device roundtrip proof

## Entry Point

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f401ce_blackpill_usb_cdc_golden.json --board stm32f401ce_blackpill_daplink --stop-on-fail
```

## Fixture Contract

- `SWDIO -> DAPLink SWDIO`
- `SWCLK -> DAPLink SWCLK`
- `GND -> DAPLink GND`
- target board USB connector -> host PC USB

Target USB FS pins:

- `PA11 = USB_DM`
- `PA12 = USB_DP`

## Pack Coverage

- `stm32f401ce_blackpill_usb_cdc_banner`
- `stm32f401ce_blackpill_usb_cdc_ping_pong`
- `stm32f401ce_blackpill_usb_cdc_echo`

## Core Rules

1. Treat Stage 1 as enumeration plus host port open, not as a passive boot-banner
   assertion.
2. Put the real correctness proof on active command/response:
   `PING/PONG` and `ECHO`.
3. Match the target USB CDC device by descriptor filters, not by assuming a
   fixed `/dev/ttyACM*` number.
4. Prefer `/dev/serial/by-id/...` when the host exposes it.
5. Assert `DTR` for this target USB CDC path.
6. Keep `vbus_sensing_enable = 0` in the firmware so USB FS on `PA11/PA12` does
   not steal `PA9/PA10` from the existing USART1 DAPLink UART bench wiring.
7. Treat `port 3333 taken; using 3334 instead` as normal local DAPLink runtime
   behavior, not as a validation failure by itself.

## Working Expectations

Expected host-visible identity:

- VID `0x1209`
- PID `0x4010`
- manufacturer `OpenAI AEL`
- product `STM32F401 USB CDC Test`

Expected protocol behavior:

- open succeeds on the discovered CDC ACM port
- `PING` -> `PONG`
- `ECHO hello-from-usb` -> `ECHO hello-from-usb`

## Troubleshooting Order

1. Confirm the target still flashes cleanly over DAPLink SWD.
2. Confirm the host sees the STM32 USB device with `1209:4010`.
3. Confirm `/dev/serial/by-id/usb-OpenAI_AEL_STM32F401_USB_CDC_Test_...` exists.
4. If passive output is missing, do not assume the USB path is dead; try an
   active `PING` probe first.
5. If command/response works but a boot banner does not, treat that as a test
   contract issue, not a transport failure.

## Lesson To Keep

For STM32 USB CDC on this bench, “device enumerates and answers commands” is a
trustworthy automation contract. “Device emits a passive startup banner that the
host always catches” is not.
