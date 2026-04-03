# STM32F030C8T6 DAPLink Pre-Stage2 Connectivity Skill

## When To Use

Use this pattern when bringing up a new `STM32F030C8T6` board on a local
`DAPLink` bench and you need a fast pre-Stage2 proof that the required loopback
wiring is actually present before running richer peripheral tests.

## Bench Contract

- DAPLink SWD connected and functional
- `NRST -> DAPLink NRST`
- `GND -> probe GND`
- local jumpers:
  - `PA0 <-> PA1`
  - `PB0 <-> PB1`
  - `PB8 <-> PB9`
  - `PA7 <-> PA6`
  - `PA9 <-> PA10`

For this pre-Stage2 pattern, keep UART local on `PA9 <-> PA10`. Do not mix
in host-observed DAPLink UART unless you are deliberately running a separate
UART pack.

## Required Software Pieces

- board profile:
  [stm32f030c8t6_daplink.yaml](/home/ali/work/ai-embedded-lab/configs/boards/stm32f030c8t6_daplink.yaml)
- local DAPLink instrument instance:
  [daplink_f030_c8_local.yaml](/home/ali/work/ai-embedded-lab/configs/instrument_instances/daplink_f030_c8_local.yaml)
- pack:
  [stm32f030c8t6_pre_stage2_connectivity.json](/home/ali/work/ai-embedded-lab/packs/stm32f030c8t6_pre_stage2_connectivity.json)

## Test Set

- `stm32f030c8t6_pb0_pb1_probe`
- `stm32f030c8t6_pb8_pb9_probe`
- `stm32f030c8t6_pa0_pa1_adc_probe`
- `stm32f030c8t6_pa7_pa6_spi_probe`
- `stm32f030c8t6_uart_loopback_probe`

## Execution Rule

Run the pack first. If one path fails, stop reasoning from assumptions and
classify the failure by layer:

1. Did flash and mailbox transport succeed?
2. Did the failure happen on the first sampled transition or after several good
   transitions?
3. Is this test trying to prove wiring only, or wiring plus a peripheral block?

If the answer is "wiring only", reduce the pre-Stage2 probe to the simplest
possible GPIO-level proof. Save the richer peripheral proof for Stage 2.

## Important Local Rule

On this bench, mailbox verify must use the instrument's configured `gdb_cmd`.
Do not assume `arm-none-eabi-gdb` exists. The local DAPLink setup was only
stable after mailbox verify respected `gdb-multiarch` from the instrument
instance.

## Debug Order

1. Confirm DAPLink SWD access works.
2. Run the four simplest local loopback checks first:
   `PB0/PB1`, `PB8/PB9`, `PA0/PA1`, `PA9/PA10`.
3. Treat `PA7/PA6` as a wiring gate before treating it as an SPI problem.
4. If `PA7/PA6` fails with an immediate first-high miss, suspect the jumper or
   physical routing before suspecting host tooling.
5. Only after pre-Stage2 is green should you move on to richer Stage 2 tests.

## Why This Skill Exists

This step is easy to miss because a new pack often feels "done" once code
exists and several tests pass. That is wrong. For reusable bring-up work, the
real deliverable is:

- pack shape
- live evidence
- explicit blocked path
- a written rule for how to classify the next failure

Without this skill capture, the same UART-routing confusion and pre-Stage2 vs
Stage-2 boundary mistakes will repeat on the next board.
