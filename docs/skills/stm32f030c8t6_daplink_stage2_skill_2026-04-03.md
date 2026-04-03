# STM32F030C8T6 DAPLink Stage 2 Skill

## Use When

Use this skill when building or repairing the Stage 2 suite for
`STM32F030C8T6` on the local DAPLink bench.

Fixture shape:

- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PA9 <-> PA10`
- SWD via DAPLink
- `NRST -> DAPLink NRST`

## Core Rules

1. Do not skip pre-Stage2 connectivity.

   Stage 2 failures are only meaningful after the five local jumper probes are
   already green.

2. Do not run multiple live tests at once on the same board.

   Parallel host builds are fine, but only one flash-and-run operation should
   touch this board at a time. Concurrent mailbox runs produce misleading
   evidence.

3. Keep UART local for the canonical Stage 2 path.

   `PA9 <-> PA10` is the canonical loopback route here. Do not mix in DAPLink
   UART while validating the canonical Stage 2 pack.

4. Reuse the jumper roles consistently.

   - Do not treat the jumper nets as interchangeable; keep the net meanings fixed.
   - `PA0 <-> PA1`: ADC loopback and EXTI
   - `PB0 <-> PB1`: GPIO loopback and `TIM3_CH3` PWM proof
   - `PB8 <-> PB9`: capture and PWM capture
   - `PA7 <-> PA6`: SPI1 `MOSI -> MISO`
   - `PA9 <-> PA10`: UART loopback

5. Defer rather than overclaim.

   `uart_dma` is not part of the canonical pack until it passes on this exact
   MCU and wiring.

## Canonical Validated Tests

- `stm32f030c8t6_gpio_loopback`
- `stm32f030c8t6_exti_trigger`
- `stm32f030c8t6_adc_loopback`
- `stm32f030c8t6_capture_mailbox`
- `stm32f030c8t6_pwm_capture`
- `stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`
- `stm32f030c8t6_spi1_loopback_mailbox`
- `stm32f030c8t6_uart_loopback_mailbox`
- `stm32f030c8t6_uart_multibyte`

## Troubleshooting Order

1. Confirm SWD access and the exact target family.
2. Run the pre-Stage2 pack.
3. Run the Stage 2 tests serially.
4. If a test fails, classify it as:
   - wiring
   - mailbox/flash transport
   - peripheral configuration
5. Spend up to 15 minutes on local debugging.
6. If still unresolved, compare against ST official docs/examples for the exact
   MCU for another bounded repair step.
7. If still unresolved, defer that test and keep the rest of the pack honest.

## Lessons To Keep

- A single-board staged suite needs strict serialization for trusted evidence.
- Local jumper contracts are enough to build a useful Stage 2 suite without an
  external observer.
- New Stage 2 work is not done at code plus one passing run; it closes only
  after evidence, closeout, and reusable skill capture land.
