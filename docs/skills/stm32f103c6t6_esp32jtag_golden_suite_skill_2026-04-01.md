# STM32F103C6T6 ESP32JTAG Golden Suite Skill

## Use When

Use this skill when building or repairing the `STM32F103C6T6` staged suite on
the ESP32JTAG STM32 bench at `192.168.2.98:4242`.

Fixture shape:

- SWD on `P3`
- `PC13 -> P0.0`
- `PC13 -> LED`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`
- `PA9 <-> PA10`

## Core Rules

1. Treat this as exact `STM32F103C6T6`, not generic `STM32F103`.

   `C6` density boundaries matter for both memory layout and peripheral count.

2. Do not assume `SPI2`.

   `STM32F103C6T6` exposes only one SPI. If the bench loopback is on
   `PB13/PB14/PB15`, that is not a valid canonical SPI path for this MCU.

3. Expect low-density identification on this board.

   Live identity for this board is `DBGMCU_IDCODE low bits = 0x412`.

4. If copied `C6` tests depend on interrupts, verify the startup vectors first.

   A truncated vector table will break timer, SysTick, and EXTI tests in ways
   that look like firmware logic bugs.

5. Keep `pre_stage2_connectivity`.

   Run the dedicated jumper probes before trusting the richer Stage 2 tests:

   - `PB0/PB1`
   - `PB8/PB9`
   - `PA0/PA1`
   - `PB15/PB14`

6. Reuse the canonical net roles.

   - `PB0/PB1`: simple GPIO loopback
   - `PB8/PB9`: software-driven capture and PWM timing
   - `PB0/PB1`: also the valid hardware timer-channel expansion path via
     `TIM3_CH3 -> PB0`
   - `PA0/PA1`: ADC loopback and EXTI
   - `PA9/PA10`: UART loopback
   - `PC13/P0.0`: LED-net observation only

## Working Validation Shape

Canonical full-pack entrypoint:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c6t6_golden.json --board stm32f103c6t6_bluepill_like --stop-on-fail
```

Canonical validated pack:

- `packs/stm32f103c6t6_golden.json`

Representative all-pass pack run:

- `pack_runs/2026-04-01_11-11-47_stm32f103c6t6_golden_stm32f103c6t6_bluepill_like`

## Troubleshooting Order

1. Confirm the target family over SWD.
2. Confirm the exact density before reusing another F103 test.
3. Run Stage 0 and Stage 1 first.
4. Run the pre-Stage2 connectivity probes.
5. Only then trust Stage 2 timing, ADC, or UART failures as real firmware
   issues.
6. If a test exceeds the bounded repair window, compare it against official ST
   docs/examples before spending more time on local guesses.
7. For this current bench, defer rather than fake coverage:

   - SPI1 needs `PA5/PA6/PA7` or remapped `PB3/PB4/PB5`
   - I2C needs a real slave/partner path on `PB6/PB7` or remapped `PB8/PB9`

## Lessons To Keep

- `STM32F103C6T6` is close to `C8T6`, but not interchangeable.
- A clean staged suite is smaller and more honest than claiming unsupported
  peripherals.
- New-pack work is not finished at code plus first pass; it finishes at code,
  live evidence, closeout, and reusable skill capture.
