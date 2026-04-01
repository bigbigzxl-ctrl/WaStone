# STM32F103C6T6 ESP32JTAG UART Roundtrip Closeout

**Date:** 2026-04-01
**Board:** `stm32f103c6t6_bluepill_like`
**Firmware target:** `firmware/targets/stm32f103c6_uart_roundtrip`
**Plan:** `tests/plans/stm32f103c6_uart_roundtrip_with_esp32jtag.json`
**Helper:** `experiments/esp32jtag/uart_roundtrip_check.py`
**Instrument:** `esp32jtag_stm32_golden` at `192.168.2.98`
**ESP32JTAG firmware:** `9a1610f`
**Status:** live pass

## Summary

This round formalized a bidirectional UART test between `ESP32JTAG` Port B and
`STM32F103C6T6 USART1`:

- `STM32 PA9/USART1_TX -> ESP32JTAG UART RX`
- `ESP32JTAG UART TX -> STM32 PA10/USART1_RX`
- common ground

The validated behavior is:

- STM32 emits `AEL_READY ...` once after boot
- STM32 emits `AEL_IDLE count=... baud=115200 8N1` while idle
- when the ESP32JTAG websocket bridge sends a line, STM32 replies with
  `AEL_ECHO:<payload>`

## Live Evidence

Firmware build:

```bash
make -C firmware/targets/stm32f103c6_uart_roundtrip clean all
```

Live flash:

```bash
arm-none-eabi-gdb -q --nx --batch \
  -ex 'set pagination off' \
  -ex 'set confirm off' \
  -ex 'target extended-remote 192.168.2.98:4242' \
  -ex 'file firmware/targets/stm32f103c6_uart_roundtrip/build/stm32f103c6_uart_roundtrip_app.elf' \
  -ex 'monitor swdp_scan' \
  -ex 'attach 1' \
  -ex 'load' \
  -ex 'attach 1' \
  -ex 'monitor reset run' \
  -ex 'detach'
```

Passing live roundtrip check:

```bash
python3 experiments/esp32jtag/uart_roundtrip_check.py \
  --endpoint wss://192.168.2.98/ws \
  --payload 'PING 424242'
```

Observed result:

```text
Connecting: wss://192.168.2.98/ws
RX: AEL_IDLE count=10 baud=115200 8N1
TX: PING 424242
RX: AEL_ECHO:PING 424242
PASS: roundtrip echo observed
```

Final instrument-side UART state sample:

```json
{"status":"ok","uart_port_num":1,"gpio_txd":43,"gpio_rxd":44,"uart_port_sel":1,"portb_cfg":1,"tasks_expected":true,"tasks_started":true,"total_rx_messages":3389,"total_rx_bytes":150230,"last_rx_len":35,"last_rx_age_ms":49,"last_rx_ascii":"AEL_IDLE count=5","last_rx_hex":"41 45 4C 5F 49 44 4C 45 20 63 6F 75 6E 74 3D 35"}
```

## Root Cause And Repair

The first live roundtrip attempt did not fail because of bench wiring. It
failed because the STM32 receive loop used a blocking `delay_ms(1)` while idle.
At `115200`, an entire incoming line can arrive during that sleep window, while
`USART1` only buffers a byte or two. The result was partial echoes such as
`AEL_ECHO:P4` or `AEL_ECHO:P2`.

The repair was:

- keep `USART1` at `115200 8N1`
- remove the blocking idle sleep from the receive path
- drive `idle_ms` and `rx_gap_ms` from non-blocking SysTick polling instead
- update the websocket helper to match against the cumulative UART stream and
  drain stale text before sending the test payload

## Bench Dependency

This validation depends on the already-committed `esp32jtag_firmware` fix:

- `9a1610f` `Fix esp32jtag UART RX pin mapping`

Without that board-profile correction, the ESP32JTAG UART bridge does not use
the real `GPIO43/GPIO44` Port B pins and this cross-instrument test is invalid.

## Why Closeout Exists

This work would be incomplete if it stopped at the new target, helper, and live
pass. The reusable outcome is the validated pattern:

- exact wiring contract
- exact firmware dependency on the ESP32JTAG side
- actual failure mode for blocking UART receive loops on small STM32 parts
- repeatable live command used to prove the roundtrip

That is why this test is only considered closed after both this report and the
matching skill capture landed.
