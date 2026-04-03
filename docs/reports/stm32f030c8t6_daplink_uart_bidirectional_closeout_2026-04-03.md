# STM32F030C8T6 DAPLink UART Bidirectional Pack Closeout

**Date:** 2026-04-03
**Board:** `stm32f030c8t6_daplink_uart`
**Pack:** `packs/stm32f030c8t6_daplink_uart_bidirectional.json`
**Instrument:** `daplink_f030_c8_local` @ `127.0.0.1:3333`
**Status:** validated side-pack rerun completed, `5/5 PASS`

## Summary

This pack extends the earlier DAPLink UART observed side-pack into a true
bidirectional UART validation path for `STM32F030C8T6`.

Entry point:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f030c8t6_daplink_uart_bidirectional.json --board stm32f030c8t6_daplink_uart --stop-on-fail
```

Fixture contract:

- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- `NRST -> DAPLink NRST`
- `GND -> probe GND`

Shared bench jumpers remain:

- `PA0 <-> PA1`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA7 <-> PA6`
- `PC13 -> onboard LED`

## Validated Coverage

Validated by the final serialized rerun:

- `stm32f030c8t6_uart_tx_probe`
- `stm32f030c8t6_uart_multibyte_observed`
- `stm32f030c8t6_uart_banner`
- `stm32f030c8t6_uart_rx_probe`
- `stm32f030c8t6_uart_echo`

Final all-pass pack run:

- `pack_runs/2026-04-03_14-55-33_stm32f030c8t6_daplink_uart_bidirectional_stm32f030c8t6_daplink_uart`

Representative rerun artifacts:

- `runs/2026-04-03_14-55-33_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_tx_probe`
- `runs/2026-04-03_14-55-47_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_multibyte_observed`
- `runs/2026-04-03_14-56-01_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_banner`
- `runs/2026-04-03_14-56-10_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_rx_probe`
- `runs/2026-04-03_14-56-19_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_echo`

Earlier targeted passes that stabilized the new host-driven checks:

- `runs/2026-04-03_14-54-40_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_rx_probe`
- `runs/2026-04-03_14-55-11_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_echo`

## Important Implementation Detail

The new host-driven UART checks are not fake local loopback tests. They use a
new AEL host-serial exchange step that:

1. waits for DUT ready text
2. writes a payload from the host through `/dev/ttyACM0`
3. waits for the expected DUT response

That is what lets this pack prove:

- host-observed TX on `PA9`
- host-driven RX on `PA10`
- full request/response echo across the DAPLink UART bridge

## Bench-Specific Lesson

The first `uart_rx_probe` attempt failed for a non-hardware reason.

Failure:

- DUT printed ready once, then blocked forever in RX wait
- the runner began observing after post-flash settle
- the single ready line was already gone

Fix:

- changed `uart_rx_probe` and `uart_echo` firmware to emit periodic ready text
- used non-blocking RX polling instead of a one-shot blocking wait

This is the right design pattern for UART tests that depend on host traffic
after the generic flash settle window.

## Relation To Other Packs

Canonical local-loopback Golden Suite remains:

- `packs/stm32f030c8t6_golden.json`

Earlier TX-only observed side-pack remains:

- `packs/stm32f030c8t6_daplink_uart_observed.json`

This new pack is the stronger side-pack when the bench is wired for the DAPLink
UART bridge and you want true bidirectional proof.

## Deferred Scope

Still not included here:

- `stm32f030c8t6_uart_dma_observed`

That work is documented separately in:

- [stm32f030c8t6_uart_dma_investigation_2026-04-03.md](/home/ali/work/ai-embedded-lab/docs/reports/stm32f030c8t6_uart_dma_investigation_2026-04-03.md)

## Closeout Decision

This pack is now a reusable validated bidirectional DAPLink UART side-pack for
`STM32F030C8T6`.

The correct state is:

- board profile: already formalized
- pack: formalized and rerun
- live validation: complete for all 5 included tests
- reusable skill capture: completed
- `uart_dma_observed`: explicitly deferred
