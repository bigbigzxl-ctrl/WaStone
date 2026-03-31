# STM32F103RCT6 Staged Candidate Live Validation Closeout

Date: 2026-03-31

## Scope

This closeout records live validation for the current
`packs/stm32f103rct6_staged_candidate.json` suite on the local DAPLink fixture.

Validated physical setup:

- DAPLink SWD
- DAPLink UART `<->` `PA9/PA10`
- `PC7 -> LED`
- `PB0 -> PB1`
- `PB8 -> PB9`
- `PA0 -> PA1`
- `PB15 -> PB14`

Validated flash/runtime path:

- `OpenOCD`
- `interface/cmsis-dap.cfg`
- forced backend: `cmsis_dap_backend hid`
- stable low-speed setting: `adapter speed 50`

## Result

The staged candidate is functionally validated on the current DAPLink fixture
for all non-visual tests.

Live-pass evidence collected:

- `stm32f103rct6_minimal_runtime_mailbox`
- `stm32f103rct6_timer_mailbox`
- `stm32f103rct6_systick_mailbox`
- `stm32f103rct6_internal_temp_mailbox`
- `stm32f103rct6_gpio_loopback`
- `stm32f103rct6_exti_trigger`
- `stm32f103rct6_adc_loopback`
- `stm32f103rct6_spi_loopback`
- `stm32f103rct6_iwdg`
- `stm32f103rct6_capture_mailbox`
- `stm32f103rct6_pwm_capture`
- `stm32f103rct6_uart_multibyte`
- `stm32f103rct6_uart_dma`
- `stm32f103rct6_uart_banner`

Auxiliary bench proof:

- `stm32f103rct6_pb8_pb9_probe`

Representative PASS mailbox reads:

- timer: `ae100001 00000002 00000000 00000014`
- exti: `ae100001 00000002 00000000 0000000a`
- capture: `ae100001 00000002 00000000 00000c1d`
- pwm_capture: `ae100001 00000002 00000000 04030a87`
- uart_multibyte: `ae100001 00000002 00000000 00000009`
- uart_dma: `ae100001 00000002 00000000 00000009`

Representative UART-observe proofs:

- multibyte: `AEL_UART_MB 55 AA 12 34`
- dma: `AEL_UART_DMA A1 B2 C3 D4 55 66 77 88`
- banner: `AEL_READY STM32F103RCT6 UART`

## Key Findings

1. `PB8 -> PB9` is now usable again after wiring repair.
   The dedicated `stm32f103rct6_pb8_pb9_probe` now passes and the repaired wire
   is good enough to carry both `capture` and `pwm_capture`.

2. The original `exti_trigger` route was a bad canonical choice for this bench.
   The suite now uses `PA0 -> PA1` for `EXTI1`, which reuses an already-proven
   wire and avoids depending on the repaired timer path for EXTI coverage.

3. `uart_multibyte` and `uart_dma` should remain host-observed on this fixture.
   The current DAPLink setup exposes `PA9/PA10` as an external UART path, not a
   board-local `PA9 -> PA10` loopback path. The suite therefore validates
   `USART1 TX` and `DMA1 -> USART1 TX` honestly through UART observation plus
   mailbox PASS, rather than pretending board-local RX loopback exists.

4. `OpenOCD sleep` uses milliseconds, not seconds.
   Early mailbox reads were caused by using `sleep 2` and `sleep 5` when the
   intended waits were 2 s and 5 s. On this path, use `sleep 2000` and similar.

5. This DAPLink probe should be treated as single-session only.
   Parallel OpenOCD sessions intermittently destabilize the probe and cause
   `CMSIS-DAP failed to connect in mode (1)`. Serialized sessions are stable.

## Candidate Status

The suite is now strong enough to keep as a staged candidate with broad live
coverage, but this closeout does not promote it to `golden` yet.

Reason:

- `stm32f103rct6_pc7_blinky_visual` is still a human-observed visual proof and
  was not machine-verified in this closeout.
- the formal board/instrument metadata still points at legacy/non-DAPLink paths
  and has not yet been updated to a canonical local DAPLink/OpenOCD execution
  route.

## Recommended Next Step

Before promoting `stm32f103rct6` to `golden`:

1. formalize the canonical local DAPLink/OpenOCD execution path in metadata
2. rerun the full candidate suite on that canonical path
3. explicitly confirm `pc7_blinky_visual`
4. then update lifecycle/labels from `candidate` to `golden`
