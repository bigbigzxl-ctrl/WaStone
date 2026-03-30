# Default Verification Baseline

Default verification is now the system-owned regression baseline for the current
repo-native hardware line.

It selects DUT tests only. The DUT test plan remains the single source of truth
for:

- test identity
- bench setup and connections
- control instrument selection
- expected checks

## Current configured steps

The current baseline is one three-pack parallel batch defined in
[configs/default_verification_setting.yaml](/nvme1t/work/codex/ai-embedded-lab/configs/default_verification_setting.yaml).

### Required (3)

- DUT: `stm32f401rct6`
- Pack: `packs/stm32f401rct6_golden.json`
- Scope: full golden suite (`20` tests)

- DUT: `stm32f411ceu6`
- Pack: `packs/stm32f411ceu6_golden.json`
- Scope: full golden suite (`20` tests)

- DUT: `stm32g431cbu6`
- Pack: `packs/stm32g431cbu6_golden.json`
- Scope: full golden suite (`17` tests)

## Current validated result

This baseline is now intentionally narrowed to the three STM32 golden suites:
`STM32F401`, `STM32F411`, and `STM32G431`.

Current expanded task count: `57` tests total.

## Current baseline meaning

At the current project stage, this baseline should be treated as:

- the default regression health line for schema and execution-model changes
- the main repeated live-bench stability line for the current three-board STM32
  golden set
- the primary readiness signal for the F4/G4 STM32 golden path

## Notes

- Default verification does not define its own test names anymore.
- Default verification does not define a second setup for the same test.
- If setup changes are needed, update the DUT test plan, not the default verification config.
- Repeated reliability collection should prefer worker-level repeat commands when the goal is long-horizon stability measurement.
