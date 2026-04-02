# STM32F103C6T6 UART Roundtrip Pack Closeout

**Date:** 2026-04-01
**Board:** `stm32f103c6t6_bluepill_like`
**Pack:** `packs/stm32f103c6t6_golden_with_uart_roundtrip.json`
**Instrument:** `esp32jtag_stm32_golden` @ `192.168.2.98:4242`
**Status:** corrected opt-in pack path validated live

## Scope

This closeout covers the opt-in `STM32F103C6T6` pack that replaces the
board-local UART trio with the real cross-instrument UART roundtrip test and
promotes that UART test into pre-Stage2 connectivity:

- removed from this pack:
  - `stm32f103c6_uart_loopback_mailbox`
  - `stm32f103c6_uart_multibyte`
  - `stm32f103c6_uart_dma`
- promoted into `pre_stage2_connectivity` in this pack:
  - `stm32f103c6_uart_roundtrip_with_esp32jtag`

The canonical golden suite remains unchanged:

- `packs/stm32f103c6t6_golden.json`

## Why The Repair Was Needed

The first opt-in pack shape was wrong. It added the ESP32JTAG roundtrip test
without removing the older local `PA9 <-> PA10` UART tests.

That cannot work on the live bench because the opt-in UART mode rewires the
same STM32 pins:

- `STM32 PA9/USART1_TX -> ESP32JTAG UART RX`
- `ESP32JTAG UART TX -> STM32 PA10/USART1_RX`

With that wiring active, the old local UART assumptions are invalid.

## Repair Summary

Three fixes were required to make the opt-in path stable:

1. Fix the pack shape.

   Remove the three incompatible board-local UART tests from
   `packs/stm32f103c6t6_golden_with_uart_roundtrip.json`.

2. Move the cross-instrument UART proof earlier.

   The working `stm32f103c6_uart_roundtrip_with_esp32jtag` test now runs in
   `pre_stage2_connectivity` so the wrong UART mode fails before the richer
   Stage 2 loopback and timing tests run.

3. Harden the ESP32JTAG web-UART observe adapter.

   The web terminal path can emit websocket frames that are not safe to treat
   as always-valid UTF-8 text. The adapter now captures raw bytes safely
   instead of failing on decode.

4. Relax the roundtrip plan's readiness assertion.

   The one-shot `AEL_READY ...` boot banner is not a stable requirement under
   the generic runner timing because the post-flash settle can miss it. The
   repeating idle line is the correct durable readiness signal.

## Evidence

Initial wrong-pack failure:

- `pack_runs/2026-04-01_20-07-34_stm32f103c6t6_golden_with_uart_roundtrip_stm32f103c6t6_bluepill_like`
- failed at
  `runs/2026-04-01_20-17-39_stm32f103c6t6_bluepill_like_stm32f103c6_uart_loopback_mailbox`

Transient unrelated bench-control failure encountered during rerun:

- `runs/2026-04-01_20-29-57_stm32f103c6t6_bluepill_like_stm32f103c6_timer_mailbox`

Adapter hardening proof:

- `runs/2026-04-01_21-13-11_stm32f103c6t6_bluepill_like_stm32f103c6_uart_roundtrip_with_esp32jtag`

Corrected opt-in pack stage rerun with the UART roundtrip check promoted into
pre-Stage2:

- `pack_runs/2026-04-01_21-14-21_stm32f103c6t6_golden_with_uart_roundtrip_stm32f103c6t6_bluepill_like`

Representative early UART gate pass with the promoted ordering:

- `runs/2026-04-01_21-35-21_stm32f103c6t6_bluepill_like_stm32f103c6_uart_roundtrip_with_esp32jtag`

Earlier cross-instrument UART pass used during stabilization:

- `runs/2026-04-01_21-22-28_stm32f103c6t6_bluepill_like_stm32f103c6_uart_roundtrip_with_esp32jtag`

## Validated Result

The corrected opt-in pack path is valid for the roundtrip wiring mode.

Validated Stage 2 slice from the corrected pack:

- `stm32f103c6_gpio_loopback`
- `stm32f103c6_exti_trigger`
- `stm32f103c6_adc_loopback`
- `stm32f103c6_capture_mailbox`
- `stm32f103c6_pwm_capture`
- `stm32f103c6_tim3_pwm_pb0_pb1_mailbox`
- `stm32f103c6_spi1_loopback_mailbox`
- `stm32f103c6_uart_roundtrip_with_esp32jtag`

Pack result:

- `12/12 PASS` for the `--stage 2` validation slice recorded in
  `pack_runs/2026-04-01_21-14-21_stm32f103c6t6_golden_with_uart_roundtrip_stm32f103c6t6_bluepill_like/pack_result.json`

## Decision

The right model is:

- keep `packs/stm32f103c6t6_golden.json` as the stable canonical suite
- keep `packs/stm32f103c6t6_golden_with_uart_roundtrip.json` as an opt-in
  wiring-mode variant
- fail the UART wiring mode in `pre_stage2_connectivity`, not late in Stage 2
- never mix the local UART trio and the ESP32JTAG roundtrip test in the same
  active wiring mode

This closes the pack-shape repair with live evidence, not just code changes.
