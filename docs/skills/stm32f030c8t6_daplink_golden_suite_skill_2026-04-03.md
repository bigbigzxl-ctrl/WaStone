# STM32F030C8T6 DAPLink Golden Suite Skill

## Use When

Use this skill when building, repairing, or rerunning the canonical staged
Golden Suite for `STM32F030C8T6` on the local DAPLink bench.

## Canonical Entry Point

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_golden.json --board stm32f030c8t6_daplink --stop-on-fail
```

## Fixture Contract

- SWD via DAPLink
- `NRST -> DAPLink NRST`
- `GND -> probe GND`
- `PC13 -> onboard LED`
- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PA9 <-> PA10`

## Core Rules

1. Treat this as exact `STM32F030C8T6`, not generic F0.
2. Keep UART local in the canonical suite.
3. Run real-hardware tests serially on this single board.
4. Keep `pre_stage2_connectivity` as a real gate, not a formality.
5. If a test exceeds the bounded repair window, compare it against ST official
   docs/examples for the same MCU before spending more time.
6. Defer rather than overclaim unsupported or unstable coverage.

## Canonical Coverage

Stage 0:

- `stm32f030c8t6_pc13_blinky_visual`
- `stm32f030c8t6_minimal_runtime_mailbox`

Stage 1:

- `stm32f030c8t6_timer_mailbox`
- `stm32f030c8t6_systick_mailbox`
- `stm32f030c8t6_internal_temp_mailbox`
- `stm32f030c8t6_system_identity_mailbox`
- `stm32f030c8t6_reset_cause_mailbox`
- `stm32f030c8t6_sleep_wfi_mailbox`
- `stm32f030c8t6_adc_vref_mailbox`
- `stm32f030c8t6_iwdg`

Pre-Stage2:

- `stm32f030c8t6_pb0_pb1_probe`
- `stm32f030c8t6_pb8_pb9_probe`
- `stm32f030c8t6_pa0_pa1_adc_probe`
- `stm32f030c8t6_pa7_pa6_spi_probe`
- `stm32f030c8t6_uart_loopback_probe`

Stage 2:

- `stm32f030c8t6_gpio_loopback`
- `stm32f030c8t6_exti_trigger`
- `stm32f030c8t6_adc_loopback`
- `stm32f030c8t6_capture_mailbox`
- `stm32f030c8t6_pwm_capture`
- `stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`
- `stm32f030c8t6_spi1_loopback_mailbox`
- `stm32f030c8t6_uart_loopback_mailbox`
- `stm32f030c8t6_uart_multibyte`

Deferred:

- `stm32f030c8t6_uart_dma`
- host-observed DAPLink UART pack
- I2C

## Troubleshooting Order

1. Confirm exact MCU identity over SWD.
2. Prove Stage 0 / Stage 1 first.
3. Run the pre-Stage2 jumper pack.
4. Only then trust Stage 2 failures as likely firmware issues.
5. Keep full-pack reruns serialized and use the pack result as the final truth.

## Lessons To Keep

- A good Golden Suite is explicit about both coverage and deferrals.
- For this bench, local loopback is enough to produce a strong canonical pack.
- The work is only complete after code, live evidence, closeout, reusable
  skill capture, and a clean full-pack rerun all exist together.
