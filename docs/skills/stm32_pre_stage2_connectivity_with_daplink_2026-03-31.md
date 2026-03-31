# STM32 Pre-Stage2 Connectivity With DAPLink

Date: 2026-03-31

## When To Use

Use this pattern when:

- a canonical STM32 golden suite runs on local `DAPLink + OpenOCD`
- Stage 2 depends on jumper continuity
- you want to fail early on wiring faults instead of misclassifying them as feature failures

## Core Rule

If a pack defines `pre_stage2_connectivity`, run those tests before Stage 2.

If a historical pack does not define it, skip it without error.

## Recommended Probe Types

Prefer small mailbox or observed-UART probes that only prove connectivity:

- digital jumper probe
- analog threshold probe
- timer-edge jumper probe
- UART TX observed probe

Avoid mixing connectivity proof with richer feature semantics in this layer.

## Reference Mapping

`STM32F103RCT6` reference implementation:

- `PB0 <-> PB1` -> `stm32f103rct6_pb0_pb1_probe`
- `PB8 <-> PB9` -> `stm32f103rct6_pb8_pb9_probe`
- `PA0 <-> PA1` -> `stm32f103rct6_pa0_pa1_adc_probe`
- `PB15 <-> PB14` -> `stm32f103rct6_pb15_pb14_probe`
- `PA9 -> DAPLink RX` -> `stm32f103rct6_uart_tx_probe`

Pack reference:

- `packs/stm32f103rct6_golden.json`

## DAPLink / OpenOCD Execution Rule

For local `DAPLink + OpenOCD` fixtures, full live pack runs must use host execution rather than sandbox-local execution.

Reason:

- sandbox-local runs may build successfully
- but the local GDB client can fail to attach to the host OpenOCD server even when the probe and target are healthy

Working pattern:

1. start `OpenOCD` on the host
2. point the board/instrument at the local `gdb_remote`
3. run `ael pack` on the host

## UART Banner Rule

If the fixture does not expose a separate logic-analyzer verify pin, convert banner proofs to UART-only observed tests.

Do not keep stale `pin: "sig"` checks on a serial-only DAPLink fixture.

## Practical Lesson

`Pre-Stage2` should answer:

- is the wire there
- is the host serial path alive
- is the loop path electrically present

It should not answer:

- is the peripheral perfectly characterized
- is timing accurate beyond coarse acceptance
- is the visual LED behavior correct

This separation makes Stage 2 failures easier to trust.
