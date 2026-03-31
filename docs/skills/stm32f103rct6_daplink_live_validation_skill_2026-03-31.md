# STM32F103RCT6 DAPLink Live Validation Skill

## Use When

Use this skill when validating `STM32F103RCT6` firmware on the repaired local
DAPLink fixture using direct `OpenOCD` access instead of the repo's older bench
adapters.

Fixture shape:

- DAPLink SWD
- DAPLink UART `<->` `PA9/PA10`
- `PC7 -> LED`
- `PB0 -> PB1`
- `PB8 -> PB9`
- `PA0 -> PA1`
- `PB15 -> PB14`

## Core Rules

1. Use `OpenOCD` with:

   - `-f interface/cmsis-dap.cfg`
   - `-c "cmsis_dap_backend hid; adapter speed 50"`
   - `-f target/stm32f1x.cfg`

2. Do not run multiple `OpenOCD` sessions in parallel against this DAPLink.
   The probe can fall into:

   - `CMSIS-DAP failed to connect in mode (1)`

   if two sessions race.

3. `sleep` inside `OpenOCD` uses milliseconds.

   Examples:

   - `sleep 2000` = 2 seconds
   - `sleep 500` = 0.5 seconds

4. Before trusting `PB8 -> PB9` timer-path tests, run the probe firmware once:

   - `stm32f103rct6_pb8_pb9_probe`

5. On this fixture, UART multibyte and UART DMA should be host-observed TX
   proofs, not fake board-local loopback tests.

## Working Flash Pattern

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c "program <firmware>.elf verify reset exit"
```

## Working Mailbox Read Pattern

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c init \
  -c "reset run" \
  -c "sleep 2000" \
  -c halt \
  -c "mdw 0x2000BC00 4" \
  -c exit
```

Expected PASS shape:

- `magic = 0xAE100001`
- `status = 0x00000002`
- `error_code = 0`

## Bench-Specific Lessons

### EXTI

Do not anchor `exti_trigger` to `PB8/PB9` on this fixture. Reuse the already
validated `PA0 -> PA1` path and validate `EXTI1`.

### Capture / PWM

Use the repaired `PB8 -> PB9` wire for timer-edge tests. A dedicated
`pb8_pb9_probe` should PASS before trusting:

- `stm32f103rct6_capture_mailbox`
- `stm32f103rct6_pwm_capture`

### UART

Use the DAPLink serial path as the canonical UART proof path:

- `stm32f103rct6_uart_banner`
- `stm32f103rct6_uart_multibyte`
- `stm32f103rct6_uart_dma`

Representative expected strings:

- `AEL_READY STM32F103RCT6 UART`
- `AEL_UART_MB 55 AA 12 34`
- `AEL_UART_DMA A1 B2 C3 D4 55 66 77 88`

## Outcome Pattern

If all non-visual tests pass but metadata still points at a legacy instrument
path, record the suite as a strongly validated `candidate`, not `golden`, until
the canonical board/instrument metadata is corrected.
