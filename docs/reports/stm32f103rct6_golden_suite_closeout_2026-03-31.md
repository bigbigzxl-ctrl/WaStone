# STM32F103RCT6 Golden Suite Closeout

Date: 2026-03-31

## Outcome

`STM32F103RCT6` is now formalized as a canonical `golden` suite.

Canonical pack:

- `packs/stm32f103rct6_golden.json`

Canonical DUT:

- `assets_golden/duts/stm32f103rct6/manifest.yaml`

Inventory now resolves it as:

- `suite_label: golden`
- `suite_tier: canonical_golden`
- `selected_instrument: daplink_f103_rct6`
- `selected_instrument_type: daplink`

## Canonical Fixture

The golden suite is defined on the current local DAPLink/OpenOCD fixture:

- DAPLink SWD
- DAPLink UART `<->` `PA9/PA10`
- `PC8 <-> PC9`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`

`PC7` remains a non-canonical legacy LED path only.

## Golden Stage Shape

Stage 0

- `stm32f103rct6_pc8_pc9_probe`
- `stm32f103rct6_minimal_runtime_mailbox`

Stage 1

- `stm32f103rct6_timer_mailbox`
- `stm32f103rct6_systick_mailbox`
- `stm32f103rct6_internal_temp_mailbox`

Stage 2

- `stm32f103rct6_gpio_loopback`
- `stm32f103rct6_exti_trigger`
- `stm32f103rct6_adc_loopback`
- `stm32f103rct6_spi_loopback`
- `stm32f103rct6_iwdg`
- `stm32f103rct6_capture_mailbox`
- `stm32f103rct6_pwm_capture`
- `stm32f103rct6_uart_multibyte`
- `stm32f103rct6_uart_dma`

Stage 3

- `stm32f103rct6_uart_banner`

## Promotion Rationale

Promotion is based on completed live validation of the canonical non-visual
suite on the local DAPLink fixture.

Key live-pass items include:

- `pc8_pc9_probe`
- `minimal_runtime_mailbox`
- `timer_mailbox`
- `systick_mailbox`
- `internal_temp_mailbox`
- `gpio_loopback`
- `exti_trigger`
- `adc_loopback`
- `spi_loopback`
- `iwdg`
- `capture_mailbox`
- `pwm_capture`
- `uart_multibyte`
- `uart_dma`
- `uart_banner`

The earlier candidate closeout remains the detailed validation record:

- `docs/reports/stm32f103rct6_staged_candidate_live_validation_closeout_2026-03-31.md`

## Important Design Choice

`PC7` was intentionally removed from the canonical Stage 0 proof path.

Why:

- code does drive `PC7`
- a dedicated `PC7 -> PC6` probe failed
- a dedicated `PC8 -> PC9` probe passed

Conclusion:

- `GPIOC` is healthy
- the board-specific `PC7` LED path is ambiguous hardware, not a reliable
  canonical proof signal

So the golden suite now uses a machine-verifiable `PC8 -> PC9` GPIO probe for
Stage 0 board-health instead of depending on the unreliable LED path.
