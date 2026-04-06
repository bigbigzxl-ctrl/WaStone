# STM32F401 Black Pill USB CDC Golden Pack Closeout

**Date:** 2026-04-05
**Board:** `stm32f401ce_blackpill_daplink`
**Pack:** `packs/stm32f401ce_blackpill_usb_cdc_golden.json`
**Instrument:** `daplink_stm32f401_blackpill_local` @ `127.0.0.1:3333`
**Status:** validated, `3/3 PASS`

## Summary

This work added a USB-device side-pack for the STM32F401 Black Pill bench where:

- DAPLink remains the SWD flash/debug path
- the target board USB connector is plugged directly into the host PC
- AEL validates the STM32 USB FS CDC ACM path by host-side discovery and
  command/response checks

Entry point:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f401ce_blackpill_usb_cdc_golden.json --board stm32f401ce_blackpill_daplink --stop-on-fail
```

Fixture contract:

- `SWDIO -> DAPLink SWDIO`
- `SWCLK -> DAPLink SWCLK`
- `GND -> DAPLink GND`
- target board USB connector -> host PC USB

The USB FS device uses:

- `PA11 = USB_DM`
- `PA12 = USB_DP`

## Validated Coverage

Validated by the final serialized pack rerun:

- `stm32f401ce_blackpill_usb_cdc_banner`
- `stm32f401ce_blackpill_usb_cdc_ping_pong`
- `stm32f401ce_blackpill_usb_cdc_echo`

Authoritative pack artifact:

- `pack_runs/2026-04-05_20-16-16_stm32f401ce_blackpill_usb_cdc_golden_stm32f401ce_blackpill_daplink/pack_result.json`

Representative single-run artifacts:

- `runs/2026-04-05_20-16-16_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_usb_cdc_banner/result.json`
- `runs/2026-04-05_20-16-28_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_usb_cdc_ping_pong/result.json`
- `runs/2026-04-05_20-16-38_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_usb_cdc_echo/result.json`

## Important Implementation Details

The target firmware is a minimal STM32CubeF4 USB CDC ACM image at:

- `firmware/targets/stm32f401ce_usb_cdc`

Notable implementation choices:

- USB FS is on the target board USB port, not the DAPLink USB serial port
- `vbus_sensing_enable = 0` so the USB device stack does not consume `PA9/PA10`
  and therefore does not conflict with the existing USART1 DAPLink UART wiring
- descriptors identify the target as:
  - VID `0x1209`
  - PID `0x4010`
  - manufacturer `OpenAI AEL`
  - product `STM32F401 USB CDC Test`

On the host side, `check.uart_roundtrip` was extended so USB CDC tests can:

- discover the correct host port by USB descriptor filters
- use `/dev/serial/by-id/...` when available
- optionally assert `DTR`

## Real Failure That Changed The Test Contract

The first USB test design assumed a passive startup banner was a stable proof of
target USB health.

Observed failures:

- the target enumerated correctly as `1209:4010`
- the host could open the CDC ACM port
- direct command probes worked:
  - `PING` -> `PONG`
  - `ECHO hello-from-usb` -> `ECHO hello-from-usb`
- but the passive startup banner was not reliable enough to anchor the pack

Root cause:

- the one-shot boot banner can be emitted before the host consumer is actually
  attached to the CDC stream
- that means a passive banner is not a trustworthy, repeatable machine check on
  this bench

Fix:

- redefined Stage 1 to prove what the bench can honestly measure:
  - target USB CDC enumeration
  - successful host open of the discovered CDC port
- kept runtime proof on active command/response:
  - `PING/PONG`
  - `ECHO`

This was the key correction from a plausible-looking USB test to a genuinely
validated USB test.

## Additional Bench Note

The pack repeatedly reported:

- `port 3333 taken by another instrument; using 3334 instead`

That behavior is correct. It demonstrates that the managed local DAPLink path
can start or reuse a conflict-free OpenOCD server instead of assuming fixed
ownership of `127.0.0.1:3333`.

## Closeout Decision

This pack is now a validated reusable USB CDC side-pack for the STM32F401
Black Pill local DAPLink bench.

Current state:

- board/bench contract: formalized
- target USB CDC firmware: implemented
- host USB CDC discovery: implemented
- pack: formalized and rerun
- live validation: complete for all 3 included tests
- closeout: completed
- reusable skill capture: completed

Not claimed here:

- USB MSC, HID, DFU, or composite coverage
- bus-level USB protocol timing analysis
- passive banner as a stable correctness signal
