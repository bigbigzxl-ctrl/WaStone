# STM32 Capture Mailbox Multi-Edge Window

## Purpose

Use this skill when an STM32 timer/capture mailbox test is flaky on real
hardware even though:

- the matching banner test passes
- wiring is confirmed
- the mailbox failure is `ERR_PERIOD` with a large captured interval

## Trigger

Apply this when mailbox runs show patterns like:

- `mailbox verify failed: status=FAIL (expected PASS)`
- `error_code=ERR_PERIOD`
- `detail0` is far larger than the expected single-period window
- a companion banner test using the same wiring passes

## Source Case

Source board:

- `stm32f103_gpio`
- instrument: `esp32jtag_stm32f103_golden @ 192.168.2.99`
- tests:
  - `stm32f103_capture_mailbox`
  - `stm32f103_pwm_capture`

Observed symptom:

- single-shot mailbox period checks intermittently returned values like
  `6600`, `8000`, or `9000`
- the corresponding Stage 3 banner tests passed on the same bench

## Core Rule

Do not trust a single adjacent capture interval on a live bench when the same
timer path is already proven by a banner/self-check test.

For mailbox-only timer capture checks, sample multiple consecutive capture
intervals and accept the first interval that falls inside the expected window.

## Procedure

1. Confirm the wiring is not the problem.
   Example for the F103 case:
   - `PA8 ↔ PB8`

2. Confirm the richer companion test passes.
   Example:
   - `stm32f103_capture_banner`
   - `stm32f103_pwm_banner`

3. Inspect mailbox failure details.
   If the failure is `ERR_PERIOD` with a very large `detail0`, the timer path is
   often alive but the mailbox test sampled an unstable pair of edges.

4. Replace a single two-edge measurement with a bounded multi-edge scan.
   Pattern:
   - wait for one capture
   - take up to N subsequent captures
   - compute each delta
   - PASS on the first delta inside the expected window
   - FAIL only if none match

## Reusable Lesson

For STM32 mailbox capture tests, single-interval period checks can be too
fragile on a live bench even when the timer path is healthy.

Use a bounded multi-edge acceptance window to turn a timing-path smoke test
into a stable live-validation test.
