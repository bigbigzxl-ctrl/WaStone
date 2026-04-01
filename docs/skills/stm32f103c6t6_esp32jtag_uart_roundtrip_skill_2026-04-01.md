# STM32F103C6T6 ESP32JTAG UART Roundtrip Skill

## Use When

Use this skill when you need to prove real bidirectional UART communication
between `STM32F103C6T6 USART1` and the `ESP32JTAG` Port B web terminal.

Validated fixture:

- `STM32 PA9/USART1_TX -> ESP32JTAG UART RX`
- `ESP32JTAG UART TX -> STM32 PA10/USART1_RX`
- SWD connected through `P3`
- common ground

## Preconditions

1. The ESP32JTAG firmware must include the Port B UART pin fix.

   Required known-good firmware revision:

   - `9a1610f` in `/nvme1t/work/esp32jtag_firmware`

2. ESP32JTAG runtime settings must be `115200 8N1` with Port B in UART mode.

3. The STM32 target should use the repo target:

   - `firmware/targets/stm32f103c6_uart_roundtrip`

## Core Rule

Do not put blocking sleeps in the STM32 UART receive idle path.

On `STM32F103C6T6` at `115200`, a `delay_ms(1)` style loop is enough to lose
most of an incoming line because the peripheral only buffers a very small
amount of unread RX data. The symptom is partial echoes such as:

- `AEL_ECHO:P4`
- `AEL_ECHO:P2`

If you need idle timers, derive them from a non-blocking tick source while the
main loop keeps polling `RXNE`.

## Validation Shape

Build:

```bash
make -C firmware/targets/stm32f103c6_uart_roundtrip clean all
```

Flash:

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

Roundtrip proof:

```bash
python3 experiments/esp32jtag/uart_roundtrip_check.py \
  --endpoint wss://192.168.2.98/ws \
  --payload 'PING 424242'
```

Expected signal:

- STM32 emits `AEL_IDLE count=... baud=115200 8N1`
- helper sends `PING 424242`
- STM32 replies `AEL_ECHO:PING 424242`

## Troubleshooting Order

1. Confirm the ESP32JTAG firmware revision is at or after `9a1610f`.
2. Confirm `/get_credentials` reports `uartBaud=115200`, `uartDataBits=8`,
   `uartStopBits=1`, `uartParity=n`, `pbcfg=1`.
3. Confirm `/api/uart_debug` shows the STM32 idle banner arriving on
   `gpio_rxd=44`.
4. If STM32-to-ESP32JTAG traffic is visible but roundtrip echo is truncated,
   inspect the STM32 receive loop for blocking waits before blaming the bridge.
5. Treat websocket input as a byte stream, not record-framed messages.
   Helper-side matching should operate on cumulative received text.

## Keep

- This is a real cross-instrument UART test, not a board-local loopback.
- The strongest proof is a request/response payload match, not just idle text.
- New test formalization is not complete at code plus first pass; it closes at
  code, live evidence, closeout, and skill capture.
