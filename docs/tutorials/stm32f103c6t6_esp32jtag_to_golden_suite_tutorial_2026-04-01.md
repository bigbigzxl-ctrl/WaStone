# STM32F103C6T6: From ESP32JTAG Identification To Golden Suite

## Quick Start

If you only want the shortest validated path, use this setup.

### 1. Wire the board to the ESP32JTAG bench

- `SWDIO -> P3 SWDIO`
- `SWCLK -> P3 SWCLK`
- `GND -> probe GND`
- `PC13 -> P0.0`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`
- `PA9 <-> PA10`
- `PA7 <-> PA6`
- `PA5 -> P0.1`

Notes:

- `NRST` was not connected on this board during bring-up.
- `PC13 -> P0.0` is the machine-observed LED net.
- `PA7 -> PA6` is the final `SPI1` loopback path.
- `PA5 -> P0.1` is optional `SPI1 SCK` observation.

### 2. Identify the target before trusting the silk

Use the live SWD path through the ESP32JTAG monitor and GDB backend.

For this board, the important live facts were:

- target family reported as `STM32F1 L/M density M3`
- core type consistent with `Cortex-M3`
- peripheral base addresses matched STM32F1, not STM32F0
- later system identity confirmed low-density `STM32F103C6T6` class

This mattered because the first reported silk was wrong. The live target was
not an `STM32F030`; it behaved like an `STM32F1`.

### 3. Start with a minimal `PC13` blinky

The first useful firmware milestone is a tiny `PC13` LED blinker:

- build and flash succeeds
- the board executes code after reset
- the classic Blue Pill active-low LED path is alive

Validated visual plan:

- [stm32f103c6_pc13_blinky_visual.json](/nvme1t/work/codex/ai-embedded-lab/tests/plans/stm32f103c6_pc13_blinky_visual.json)

### 4. Add machine confirmation of the LED net

Human-visible blink is not enough for a reusable suite. We machine-confirmed
`PC13` by wiring:

- `PC13 -> P0.0`

Because the visible blink was too slow for the easiest capture path, the
working method was:

- temporarily switch `PC13` to a fast electrical probe waveform
- capture it on `P0.0`
- confirm edges electrically
- then restore the visible blinky firmware

### 5. Run the canonical pack

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c6t6_golden.json --board stm32f103c6t6_bluepill_like --stop-on-fail
```

Final canonical pack:

- [stm32f103c6t6_golden.json](/nvme1t/work/codex/ai-embedded-lab/packs/stm32f103c6t6_golden.json)

Final closeout:

- [stm32f103c6t6_golden_suite_closeout_2026-04-01.md](/nvme1t/work/codex/ai-embedded-lab/docs/reports/stm32f103c6t6_golden_suite_closeout_2026-04-01.md)

This document explains how we got there and what mattered.

### 6. Run the opt-in UART roundtrip variant when needed

If you also wire the ESP32JTAG UART transmit path into the STM32:

- `STM32 PA9 -> ESP32JTAG UART RX`
- `ESP32JTAG UART TX -> STM32 PA10`

then you can run the extended pack:

```bash
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103c6t6_golden_with_uart_roundtrip.json --board stm32f103c6t6_bluepill_like --stop-on-fail
```

Use this only when you explicitly want cross-instrument UART proof through the
ESP32JTAG web UART bridge. The normal canonical pack remains the default suite.

## Purpose

This tutorial records the real bring-up path for a Blue Pill-like
`STM32F103C6T6` board on the ESP32JTAG STM32 bench at `192.168.2.98:4242`.

It covers:

- how the MCU was identified over SWD
- how the first blinky firmware was built and validated
- how machine confirmation replaced pure visual trust
- how the staged golden suite was built up from simple checks to richer tests
- what failed during the process
- how those failures were diagnosed and fixed
- what reusable skills came out of the work

## End State

At the end of the process, the repo contains:

- canonical pack  
  [stm32f103c6t6_golden.json](/nvme1t/work/codex/ai-embedded-lab/packs/stm32f103c6t6_golden.json)
- golden DUT manifest  
  [manifest.yaml](/nvme1t/work/codex/ai-embedded-lab/assets_golden/duts/stm32f103c6t6/manifest.yaml)
- canonical board config  
  [stm32f103c6t6_bluepill_like.yaml](/nvme1t/work/codex/ai-embedded-lab/configs/boards/stm32f103c6t6_bluepill_like.yaml)
- closeout report  
  [stm32f103c6t6_golden_suite_closeout_2026-04-01.md](/nvme1t/work/codex/ai-embedded-lab/docs/reports/stm32f103c6t6_golden_suite_closeout_2026-04-01.md)
- reusable skill capture  
  [stm32f103c6t6_esp32jtag_golden_suite_skill_2026-04-01.md](/nvme1t/work/codex/ai-embedded-lab/docs/skills/stm32f103c6t6_esp32jtag_golden_suite_skill_2026-04-01.md)

Final live status:

- canonical suite: `24/24 PASS`
- later stability reruns: 3 additional full-pack passes

Representative final evidence:

- [pack_result.json](/nvme1t/work/codex/ai-embedded-lab/pack_runs/2026-04-01_12-41-58_stm32f103c6t6_golden_stm32f103c6t6_bluepill_like/pack_result.json)
- [pack_result.json](/nvme1t/work/codex/ai-embedded-lab/pack_runs/2026-04-01_13-12-52_stm32f103c6t6_golden_stm32f103c6t6_bluepill_like/pack_result.json)
- [pack_result.json](/nvme1t/work/codex/ai-embedded-lab/pack_runs/2026-04-01_13-21-21_stm32f103c6t6_golden_stm32f103c6t6_bluepill_like/pack_result.json)
- [pack_result.json](/nvme1t/work/codex/ai-embedded-lab/pack_runs/2026-04-01_13-32-07_stm32f103c6t6_golden_stm32f103c6t6_bluepill_like/pack_result.json)

## Part 1: Identify The MCU Before Believing The Silk

The first important lesson was simple: do not trust the board label until SWD
agrees.

The board was initially described as `STM32F030C8T6`. Live SWD attach through
the ESP32JTAG said otherwise:

- target type presented as `STM32F1 L/M density M3`
- core behavior matched `Cortex-M3`
- peripheral base addresses matched STM32F1-style layout

That immediately ruled out `STM32F030`, which is an STM32F0 / Cortex-M0 part.

Once the user corrected the silk to `STM32F103C6T6`, the live SWD result made
sense.

### Practical rule

Before building tests for an unfamiliar board:

1. attach over SWD
2. read live target family
3. confirm the core class
4. only then choose firmware and pin/peripheral assumptions

If step 4 happens before step 1, wrong-family assumptions spread everywhere.

## Part 2: Use The Smallest Possible First Firmware

The first firmware milestone was a simple `PC13` LED blink.

Why `PC13`:

- it matches the Blue Pill-style onboard LED path
- it proves flash, reset, startup, and GPIO basic health in one tiny target
- it is easy to inspect visually and electrically

The validated target is:

- [main.c](/nvme1t/work/codex/ai-embedded-lab/firmware/targets/stm32f103c6_gpio_no_external_capture/main.c)

The visible validation plan is:

- [stm32f103c6_pc13_blinky_visual.json](/nvme1t/work/codex/ai-embedded-lab/tests/plans/stm32f103c6_pc13_blinky_visual.json)

This step proved:

- the SWD flash path through ESP32JTAG worked
- the board ran flashed code
- `PC13` was the correct onboard LED net

## Part 3: Visual Blink Is Not Enough

The visible blink was useful, but a golden suite cannot stop at
"someone looked at the LED".

We first tried to reason about machine confirmation using the observed LED net.
The correct physical observation point was:

- `PC13 -> P0.0`

The first capture idea was too naive because the visible blink was too slow for
the easiest logic-analyzer path. The better method was:

1. temporarily flash a fast electrical probe on `PC13`
2. capture the waveform on `P0.0`
3. confirm repeated edges
4. restore the human-visible blink firmware

That temporary probe target became:

- [main.c](/nvme1t/work/codex/ai-embedded-lab/firmware/targets/stm32f103c6_pc13_fast_probe/main.c)

This is an important pattern:

- separate "human-visible behavior" from "machine-verifiable electrical proof"
- use the latter when formalizing a reusable suite

## Part 4: Build The Suite In Stages, Not As One Big Jump

The final suite shape was intentionally staged.

### Stage 0: Board alive

- `stm32f103c6_pc13_blinky_visual`
- `stm32f103c6_minimal_runtime_mailbox`

### Stage 1: Internal and self tests

- `stm32f103c6_timer_mailbox`
- `stm32f103c6_systick_mailbox`
- `stm32f103c6_internal_temp_mailbox`
- `stm32f103c6_system_identity_mailbox`
- `stm32f103c6_reset_cause_mailbox`
- `stm32f103c6_sleep_wfi_mailbox`
- `stm32f103c6_adc_vref_mailbox`
- `stm32f103c6_iwdg`

### Pre-Stage2 connectivity

- `stm32f103c6_pb0_pb1_probe`
- `stm32f103c6_pb8_pb9_probe`
- `stm32f103c6_pa0_pa1_adc_probe`
- `stm32f103c6_pb15_pb14_probe`

### Stage 2: Functional tests

- `stm32f103c6_gpio_loopback`
- `stm32f103c6_exti_trigger`
- `stm32f103c6_adc_loopback`
- `stm32f103c6_capture_mailbox`
- `stm32f103c6_pwm_capture`
- `stm32f103c6_tim3_pwm_pb0_pb1_mailbox`
- `stm32f103c6_spi1_loopback_mailbox`
- `stm32f103c6_uart_loopback_mailbox`
- `stm32f103c6_uart_multibyte`
- `stm32f103c6_uart_dma`

This structure matters because it narrows failure classification:

- Stage 0 tells you whether the board is alive at all
- Stage 1 checks internal primitives
- pre-Stage2 isolates wiring faults early
- only then do richer loopback/timing tests become trustworthy

## Part 5: The Canonical Bench Contract

The final canonical board contract became:

- SWD on `P3`
- no reset line
- verify view on `P0.0`
- `PC13 -> P0.0`
- `PC13 -> LED`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`
- `PA9 <-> PA10`
- `PA7 <-> PA6`
- `PA5 -> P0.1`
- common ground

Why these nets:

- `PB0/PB1`: GPIO loopback and the final valid hardware timer expansion path
- `PB8/PB9`: capture and PWM timing
- `PA0/PA1`: ADC and EXTI-style checks
- `PB15/PB14`: digital connectivity
- `PA9/PA10`: UART local loopback
- `PA7/PA6`: `SPI1` loopback
- `PA5/P0.1`: optional observation of `SPI1 SCK`

The board config is:

- [stm32f103c6t6_bluepill_like.yaml](/nvme1t/work/codex/ai-embedded-lab/configs/boards/stm32f103c6t6_bluepill_like.yaml)

## Part 6: Important Failures And How They Were Fixed

This section matters more than the happy path.

### Issue 1: Wrong MCU family assumption

Symptom:

- early assumptions treated the board like an `STM32F030`

Why it was wrong:

- live SWD identification showed `STM32F1` / `Cortex-M3`

Fix:

- stop trusting the original label
- switch the whole effort to `STM32F103C6T6`
- base all later work on live SWD identity, not the initial claim

Lesson:

- target-family detection comes before board-profile selection

### Issue 2: Machine LED confirmation was initially planned the wrong way

Symptom:

- the first machine-check idea assumed the slow visible blink would be easy to
  confirm directly

Why it was wrong:

- the blink period was poor for the easiest observation path

Fix:

- use a temporary fast-probe firmware on `PC13`
- capture edges electrically on `P0.0`
- then restore the visible blink firmware

Lesson:

- when human timing and machine timing conflict, add a temporary electrical
  proof target instead of forcing the visual test to do both jobs

### Issue 3: Copied startup code had an incomplete vector table

Symptom:

- interrupt-driven tests behaved inconsistently
- timer and SysTick logic looked broken even when register setup looked sane

Why it happened:

- the copied `stm32f103c6` startup file only exposed a tiny vector table

Fix:

- repair the shared startup base so the expected interrupt vectors were present

Relevant base:

- [startup_stm32f103.c](/nvme1t/work/codex/ai-embedded-lab/firmware/targets/stm32f103c6/startup_stm32f103.c)

Lesson:

- if interrupt-based tests fail in a suspiciously broad way, check startup and
  vectors before rewriting peripheral logic

### Issue 4: The first broader timer choice was wrong for this exact MCU

Symptom:

- the first hardware-timer expansion around `TIM4/PB8` did not make sense on
  the real board

Why it happened:

- this board identifies as low-density `STM32F103C6T6`
- a broader F103 timer assumption was copied too aggressively

Fix:

- stop trying to force `TIM4`
- move the hardware PWM expansion to `TIM3_CH3 -> PB0`
- observe it through the existing `PB0 <-> PB1` loopback

Final test:

- [stm32f103c6_tim3_pwm_pb0_pb1_mailbox.json](/nvme1t/work/codex/ai-embedded-lab/tests/plans/stm32f103c6_tim3_pwm_pb0_pb1_mailbox.json)

Lesson:

- "same family" is not enough; density and exact pin/timer availability matter

### Issue 5: `SPI2` assumptions were invalid for `STM32F103C6T6`

Symptom:

- the first SPI thinking followed larger F103 patterns on `PB13/PB14/PB15`

Why it was wrong:

- `STM32F103C6T6` exposes only one SPI
- `PB13/PB14/PB15` is not the canonical answer for this MCU on this bench

Fix:

- defer SPI until the bench added a valid `SPI1` path
- later add:
  - `PA7 -> PA6`
  - `PA5 -> P0.1`
- implement a true `SPI1` loopback target

Final SPI test:

- [stm32f103c6_spi1_loopback_mailbox.json](/nvme1t/work/codex/ai-embedded-lab/tests/plans/stm32f103c6_spi1_loopback_mailbox.json)

Lesson:

- do not claim SPI coverage until the physical loopback matches the actual SPI
  instance available on the exact MCU

### Issue 6: Full-pack rerun initially failed for environment reasons, not firmware reasons

Symptom:

- one early full-pack rerun failed to connect to the remote GDB endpoint

Why it happened:

- the pack was launched from a context that did not have the needed remote bench
  access

Fix:

- rerun with the correct remote access
- use only one active debug session at a time

Lesson:

- separate bench-access failures from device-firmware failures before editing
  code

## Part 7: The 15-Minute / Docs / 10-Minute Rule In Practice

During the suite expansion, each new test was treated with bounded repair time:

1. spend up to 15 minutes trying to complete the test
2. if still failing, stop and compare against official ST docs or examples
3. compare the assumptions against the real MCU
4. spend one shorter follow-up repair window
5. if still blocked, defer it honestly and move on

This rule paid off in two places:

- timer-channel expansion
- SPI coverage

In both cases, the real fix was not "try harder". The real fix was
"stop assuming a bigger STM32F103 than the one actually on the bench."

## Part 8: What Skills And Experience Came Out Of This

This work produced both code artifacts and operational skills.

### Technical skills reinforced

- identify unknown STM32-family hardware from live SWD instead of silk
- bring up a new board using the smallest possible `GPIO + startup + flash`
  target first
- convert a visual-only validation path into a machine-verifiable electrical
  proof
- use pre-connectivity probes to classify later failures cleanly
- port staged tests between similar MCUs without over-claiming compatibility
- keep exact-density constraints in mind for low-end STM32 parts

### Process skills reinforced

- build staged suites from easiest proof to richest proof
- fail honestly when bench wiring cannot support a claimed peripheral
- record reusable lessons in both closeout and skill docs
- commit independent completed intents separately

### Durable repo captures

- tutorial: this file
- closeout:
  [stm32f103c6t6_golden_suite_closeout_2026-04-01.md](/nvme1t/work/codex/ai-embedded-lab/docs/reports/stm32f103c6t6_golden_suite_closeout_2026-04-01.md)
- skill:
  [stm32f103c6t6_esp32jtag_golden_suite_skill_2026-04-01.md](/nvme1t/work/codex/ai-embedded-lab/docs/skills/stm32f103c6t6_esp32jtag_golden_suite_skill_2026-04-01.md)

## Part 9: What The Final Suite Covers And What It Does Not

What is covered well:

- board life
- startup/runtime mailbox path
- timer and SysTick basics
- internal temperature and `VREFINT`
- reset cause and sleep via `WFI`
- watchdog
- GPIO loopback
- EXTI
- ADC loopback
- capture and PWM timing
- hardware PWM on the valid low-density path
- `SPI1` loopback
- UART loopback, multibyte, DMA

What is intentionally not claimed:

- `I2C`
- host-external UART proof beyond local `PA9 <-> PA10`

Those are bench-scope limits, not hidden failures.

## Final Recommendation

If you repeat this process on another "Blue Pill-like" board, keep the order:

1. identify the MCU over SWD
2. start with a minimal blinky
3. add machine proof for the observed net
4. build Stage 0 and Stage 1
5. add dedicated connectivity probes
6. only then add richer Stage 2 tests
7. stop and correct wrong MCU assumptions as soon as they appear

That order is why this suite ended cleanly at a stable, repeatable
`24/24 PASS` instead of becoming a pile of half-true tests.
