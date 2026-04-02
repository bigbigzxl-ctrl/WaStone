# STM32F103C6T6 UART Roundtrip Pack Skill

## Use When

Use this skill when maintaining the opt-in
`STM32F103C6T6 + ESP32JTAG` UART roundtrip pack:

- `packs/stm32f103c6t6_golden_with_uart_roundtrip.json`

This is the pack for the bench wiring mode where ESP32JTAG is the real UART
peer, not just the SWD/programming instrument.

## Core Rule

Do not keep the local STM32 UART tests in the same pack when the bench is wired
for ESP32JTAG roundtrip on `PA9/PA10`.

These are incompatible in the same active wiring mode:

- local-only tests:
  - `stm32f103c6_uart_loopback_mailbox`
  - `stm32f103c6_uart_multibyte`
  - `stm32f103c6_uart_dma`
- cross-instrument test:
  - `stm32f103c6_uart_roundtrip_with_esp32jtag`

Reason:

- local tests assume `PA9 <-> PA10`
- roundtrip mode uses:
  - `PA9 -> ESP32JTAG UART RX`
  - `ESP32JTAG UART TX -> PA10`

## Adapter Rule

Treat ESP32JTAG web-UART websocket traffic as bytes first, text second.

If the observe adapter assumes every websocket payload is valid UTF-8 text, the
test can fail before real UART verification even starts. Use raw-byte capture
and tolerate non-UTF8 frames.

## Plan Rule

Do not require a one-shot boot banner as the stable readiness proof when the
generic runner has a post-flash settle window.

For this target, the durable readiness indicator is the repeating idle line:

- `AEL_IDLE count=`

The boot-only `AEL_READY ...` banner is useful during direct bench work, but it
is not a reliable machine-required assertion for the generic pack runner.

## Validation Order

1. Keep the canonical suite separate:
   - `packs/stm32f103c6t6_golden.json`
2. Use the opt-in pack only when the extra UART wiring is present:
   - `packs/stm32f103c6t6_golden_with_uart_roundtrip.json`
3. If the opt-in pack fails in UART, check the observe adapter before blaming
   the DUT firmware.
4. If it fails in the old local UART tests, the pack shape is wrong rather than
   the firmware being broken.

## Known-Good Evidence

- corrected pack slice:
  `pack_runs/2026-04-01_21-14-21_stm32f103c6t6_golden_with_uart_roundtrip_stm32f103c6t6_bluepill_like`
- final roundtrip run:
  `runs/2026-04-01_21-22-28_stm32f103c6t6_bluepill_like_stm32f103c6_uart_roundtrip_with_esp32jtag`

## Keep

- Opt-in pack variants should encode wiring mode, not silently fight it.
- Cross-instrument UART validation is stronger than local loopback, but more
  bench-coupled, so keep it opt-in.
- New or repaired packs are not done at code plus commit; they close at code,
  live validation, closeout, and reusable skill capture.
