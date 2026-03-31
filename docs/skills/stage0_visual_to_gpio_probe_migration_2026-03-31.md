# Stage 0 Visual To GPIO Probe Migration

## Use When

Use this pattern when a Stage 0 visual LED test is too board-specific or flaky
to serve as a canonical golden proof.

Typical trigger:

- firmware definitely toggles the advertised LED pin
- users still cannot trust the visible LED result
- the LED path may be inverted, buffered, transistor-driven, or otherwise
  ambiguous at the board level

## Core Rule

Do not let a flaky board LED path block golden-suite promotion if the MCU GPIO
itself can be machine-verified on a cleaner pair of pins.

Instead:

1. keep the old LED test only as a non-canonical diagnostic
2. introduce a same-bank GPIO probe using two clean pins
3. promote the probe to the canonical Stage 0 proof path

## Source Case

`STM32F103RCT6`

- `PC7` LED path was advertised by board silk and existing firmware
- firmware did drive `PC7`
- `PC7 -> PC6` probe failed
- `PC8 -> PC9` probe passed

Result:

- `PC7` stayed as a non-canonical legacy diagnostic
- `PC8 -> PC9` became the canonical machine-verifiable Stage 0 path

## Recommended Probe Shape

Use:

- one output pin
- one input pull-down pin
- repeated high/low toggles
- mailbox PASS only after both repeated highs and repeated lows are confirmed

This is stronger than a visual blink because it:

- proves the output pin toggles
- proves the input path can read both states
- removes human observation from the pass criterion

## Promotion Guidance

If the replacement GPIO probe passes and the rest of the suite is already
validated, it is reasonable to promote the suite using the GPIO probe as the
canonical Stage 0 signal, while explicitly documenting the old LED path as
non-canonical hardware.
