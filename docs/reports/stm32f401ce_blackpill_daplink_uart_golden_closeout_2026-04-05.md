# STM32F401 Black Pill DAPLink UART Golden Pack Closeout

**Date:** 2026-04-05
**Board:** `stm32f401ce_blackpill_daplink`
**Pack:** `packs/stm32f401ce_blackpill_daplink_uart_golden.json`
**Instrument:** `daplink_stm32f401_blackpill_local` @ `127.0.0.1:3333`
**Status:** validated, `4/4 PASS`

## Summary

This work formalized a DAPLink-backed Black Pill side-pack for the current
bench where one local DAPLink provides both:

- SWD flash/debug
- CDC UART host exchange on `USART1`

Entry point:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f401ce_blackpill_daplink_uart_golden.json --board stm32f401ce_blackpill_daplink --stop-on-fail
```

Fixture contract:

- `SWDIO -> DAPLink SWDIO`
- `SWCLK -> DAPLink SWCLK`
- `PA9/USART1_TX -> DAPLink RX`
- `DAPLink TX -> PA10/USART1_RX`
- `PC13 -> onboard LED`
- `GND -> DAPLink GND`

## Validated Coverage

Validated by the final serialized pack rerun:

- `stm32f401ce_blackpill_daplink_led_blink`
- `stm32f401ce_blackpill_daplink_uart_banner`
- `stm32f401ce_blackpill_daplink_uart_ping_pong`
- `stm32f401ce_blackpill_daplink_uart_echo`

Authoritative pack artifact:

- `pack_runs/2026-04-05_19-55-01_stm32f401ce_blackpill_daplink_uart_golden_stm32f401ce_blackpill_daplink/pack_result.json`

Representative single-run artifacts used while stabilizing the pack:

- `runs/2026-04-05_19-54-38_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_daplink_led_blink/result.json`
- `runs/2026-04-05_19-53-56_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_daplink_uart_banner/result.json`
- `runs/2026-04-05_19-54-44_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_daplink_uart_ping_pong/result.json`
- `runs/2026-04-05_19-54-44_stm32f401ce_blackpill_daplink_stm32f401ce_blackpill_daplink_uart_echo/result.json`

## Important Implementation Detail

This pack is intentionally mixed-mode:

- Stage 0 is `program_only`
- Stages 1-3 are machine-verified UART checks

That split is deliberate. A DAPLink-only bench has no independent LED observer,
so a true LED verification step would be fake unless another instrument or
camera exists. The correct contract for this bench is:

- flash the LED heartbeat image and leave the board running
- use UART for machine-verifiable runtime proof

## Real Failure That Separated Design From Assumption

The first Stage 0 attempt failed because I modeled the LED step as if the bench
could observe `PC13`.

Observed failure:

- the board flashed successfully
- the run then failed in `check_signal`
- the hint was `verify GPIO mapping and wiring for verify pin`

Root cause:

- the DAPLink bench has SWD and CDC UART only
- there is no logic analyzer, meter, or LED observer in this pack
- `observe_led` was the wrong abstraction for this hardware contract

Fix:

- changed `stm32f401ce_blackpill_daplink_led_blink` to `program_only`
- kept the runtime proof on UART where the DAPLink actually has a measurable
  surface

This was the critical distinction between a real validated pack and a pack that
only looked automated on paper.

## Additional Bench Note

The pack repeatedly reported:

- `port 3333 taken by another instrument; using 3334 instead`

This did not block validation. It proved the newer local DAPLink runtime path
correctly allocates and reuses a conflict-free local OpenOCD port instead of
assuming fixed ownership of `127.0.0.1:3333`.

## Why This Closeout Could Have Been Missed

The easy failure mode here would have been:

- add the new board and pack files
- see the individual UART tests pass
- commit immediately
- skip the final pack rerun and skip recording the one meaningful modeling
  correction from live validation

That would have lost the reusable lesson. The pack only became trustworthy
after the full rerun and after the Stage 0 contract was corrected from
fake-observed to explicit visual/program-only.

## Closeout Decision

This pack is now a validated reusable DAPLink side-pack for the STM32F401
Black Pill bench with host UART wired through DAPLink CDC.

Current state:

- board/instrument binding: formalized
- pack: formalized and rerun
- live validation: complete for all 4 included tests
- closeout: completed
- reusable skill capture: completed

Not claimed here:

- canonical full-family Golden Suite status
- independent optical LED verification
- mailbox, timer, ADC, SPI, or wider peripheral coverage
