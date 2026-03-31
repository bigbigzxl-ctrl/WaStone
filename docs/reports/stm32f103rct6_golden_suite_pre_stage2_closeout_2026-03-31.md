# STM32F103RCT6 Golden Suite Pre-Stage2 Closeout

Date: 2026-03-31

## Outcome

`STM32F103RCT6` now includes an explicit `pre_stage2_connectivity` layer in its canonical golden pack:

- pack: `packs/stm32f103rct6_golden.json`
- board: `stm32f103rct6`
- control instrument: `daplink_f103_rct6`
- transport: local `DAPLink + OpenOCD gdb_remote`

The canonical pack completed live validation with `20/20 PASS`.

Primary evidence:

- pack result: `pack_runs/2026-03-31_08-15-39_stm32f103rct6_golden_stm32f103rct6/pack_result.json`

## Pre-Stage2 Connectivity Coverage

The new `pre_stage2_connectivity` section proves the current bench wiring before Stage 2 feature tests run:

- `stm32f103rct6_pb0_pb1_probe`
- `stm32f103rct6_pb8_pb9_probe`
- `stm32f103rct6_pa0_pa1_adc_probe`
- `stm32f103rct6_pb15_pb14_probe`
- `stm32f103rct6_uart_tx_probe`

This layer validates:

- digital GPIO jumper continuity on `PB0 <-> PB1`
- timer/PWM jumper continuity on `PB8 <-> PB9`
- analog loop path on `PA0 <-> PA1`
- SPI jumper continuity on `PB15 <-> PB14`
- host-observed UART TX path on `PA9 -> DAPLink RX`

Current canonical fixture:

- `DAPLink SWD`
- `DAPLink UART <-> PA9/PA10`
- `PC8 <-> PC9`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`

## Final Suite Shape

Stage 0:

- `stm32f103rct6_pc8_pc9_probe`
- `stm32f103rct6_minimal_runtime_mailbox`

Stage 1:

- `stm32f103rct6_timer_mailbox`
- `stm32f103rct6_systick_mailbox`
- `stm32f103rct6_internal_temp_mailbox`

Pre-Stage2 Connectivity:

- `stm32f103rct6_pb0_pb1_probe`
- `stm32f103rct6_pb8_pb9_probe`
- `stm32f103rct6_pa0_pa1_adc_probe`
- `stm32f103rct6_pb15_pb14_probe`
- `stm32f103rct6_uart_tx_probe`

Stage 2:

- `stm32f103rct6_gpio_loopback`
- `stm32f103rct6_exti_trigger`
- `stm32f103rct6_adc_loopback`
- `stm32f103rct6_spi_loopback`
- `stm32f103rct6_iwdg`
- `stm32f103rct6_capture_mailbox`
- `stm32f103rct6_pwm_capture`
- `stm32f103rct6_uart_multibyte`
- `stm32f103rct6_uart_dma`

Stage 3:

- `stm32f103rct6_uart_banner`

## Important Implementation Notes

- `pre_stage2_connectivity` is optional at pack level.
- If a pack defines it, `ael pack` now runs it before Stage 2.
- If a historical pack does not define it, AEL skips it without error.
- `stm32f103rct6_uart_banner` was converted to a UART-only proof because this DAPLink fixture does not expose a separate logic-analyzer verify pin.
- Local `DAPLink + OpenOCD` runs required host-level execution. Sandbox-local pack runs could build successfully but failed to use the localhost GDB server.

## Residual Limits

Still not auto-proven in the current canonical fixture:

- UART RX host-to-target path as a dedicated pre-stage2 probe
- visual LED behavior on `PC7`
- features that require extra external hardware, including `I2C`, `USB`, and `CAN`

These are known limits, not regressions in the canonical suite.
