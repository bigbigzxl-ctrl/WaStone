# STM32F030C8T6 Stage 2 Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink`
**Stage:** `2`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** stage-complete live validation, `9/9 PASS`

## Summary

`STM32F030C8T6` now has a validated Stage 2 suite on the local DAPLink bench.
This stage assumes the canonical local loopback contract proved earlier by the
pre-Stage2 pack:

- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PA9 <-> PA10`

With that contract in place, Stage 2 now covers richer GPIO, EXTI, ADC, timer
capture, hardware PWM, SPI1 loopback, and UART loopback behavior on real
hardware.

## Evidence

Validated Stage 2 run ids:

- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_gpio_loopback`
- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_exti_trigger`
- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_adc_loopback`
- `runs/2026-04-03_09-06-25_stm32f030c8t6_daplink_stm32f030c8t6_spi1_loopback_mailbox`
- `runs/2026-04-03_09-09-41_stm32f030c8t6_daplink_stm32f030c8t6_uart_loopback_mailbox`
- `runs/2026-04-03_09-10-30_stm32f030c8t6_daplink_stm32f030c8t6_uart_multibyte`
- `runs/2026-04-03_09-42-49_stm32f030c8t6_daplink_stm32f030c8t6_capture_mailbox`
- `runs/2026-04-03_09-43-24_stm32f030c8t6_daplink_stm32f030c8t6_pwm_capture`
- `runs/2026-04-03_09-43-59_stm32f030c8t6_daplink_stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`

## What Was Validated

Validated on real hardware:

- `stm32f030c8t6_gpio_loopback`
- `stm32f030c8t6_exti_trigger`
- `stm32f030c8t6_adc_loopback`
- `stm32f030c8t6_capture_mailbox`
- `stm32f030c8t6_pwm_capture`
- `stm32f030c8t6_tim3_pwm_pb0_pb1_mailbox`
- `stm32f030c8t6_spi1_loopback_mailbox`
- `stm32f030c8t6_uart_loopback_mailbox`
- `stm32f030c8t6_uart_multibyte`

## Key Findings

1. Single-board hardware runs must stay serial.

An earlier attempt accidentally launched multiple real-hardware runs against
the same board at once. Those results were discarded. The trusted evidence in
this closeout comes only from serialized reruns.

2. `PB8 <-> PB9` supports both simple capture and PWM timing proofs.

The same jumper pair is now proven useful for software-driven capture-style
checks and richer PWM timing validation without adding an external observer.

3. `PB0 <-> PB1` is the right timer-channel expansion path on this fixture.

`TIM3_CH3 -> PB0`, observed through `PB1`, gives a clean hardware PWM proof on
this MCU and current wiring.

4. Keep UART local for the canonical pack.

For this suite, `PA9 <-> PA10` remains the stable UART loopback contract.
Host-observed DAPLink UART belongs in a separate future pack, not in the
canonical local-loopback Golden Suite.

## Deferred Scope

Not part of Stage 2 closeout:

- `stm32f030c8t6_uart_dma`
- host-observed DAPLink UART pack
- I2C, because the current setup has no valid partner path

`uart_dma` exists as work-in-progress, but it is explicitly deferred until it
passes independently on this exact MCU and bench contract.

## Closeout Decision

This round is a valid Stage 2 closeout for `STM32F030C8T6`.

The correct current conclusion is:

- Stage 2 shape: complete
- live Stage 2 validation: complete
- trusted Stage 2 evidence: `9/9 PASS`
- `uart_dma`: deferred, not hidden
