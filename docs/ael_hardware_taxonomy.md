# AEL Hardware Taxonomy

Version: draft-0.1
Date: 2026-03-30
Status: Proposed

---

## Purpose

This document defines the canonical classification hierarchy for hardware platforms in AEL.

The goal is to make these questions answerable from one formal source:

- "What Golden Suites do we have?"
- "List all STM suites"
- "List all STM32F4 Golden Suites"
- "List all ESP32 suites"
- "Which suites are only candidate/testing/pre-release?"

This taxonomy is separate from suite maturity:

- taxonomy answers **what the hardware is**
- suite labels answer **how mature the validation path is**

---

## Classification Levels

Every DUT should be classifiable along these fields:

| Field | Purpose | Example |
|-------|---------|---------|
| `platform_class` | Top-level hardware kind | `mcu`, `fpga` |
| `vendor` | Silicon or platform vendor | `st`, `espressif`, `raspberry_pi`, `xilinx` |
| `family` | Major product family used in user queries | `stm32`, `esp32`, `rp2040` |
| `series` | Family subdivision used for broad grouping | `stm32f1`, `stm32f4`, `stm32g4`, `stm32h7`, `esp32c3`, `esp32s3` |
| `line` | Specific device line | `stm32f103`, `stm32f401`, `stm32f411` |
| `part_number` | Exact shipping part or board anchor MCU | `stm32f103c8t6`, `stm32f401rct6` |

These fields form a narrowing path:

`platform_class -> vendor -> family -> series -> line -> part_number`

---

## Field Definitions

### `platform_class`

The broadest hardware grouping.

Allowed values for now:

- `mcu`
- `fpga`

Reserved for future use:

- `soc`
- `instrument`
- `hybrid`

Rule:

- This field must be stable and sparse.
- It is for top-level filtering only.

---

### `vendor`

The vendor namespace used for broad grouping.

Examples:

- `st`
- `espressif`
- `raspberry_pi`
- `xilinx`
- `intel`
- `lattice`

Rule:

- Use a short normalized lowercase token.
- Do not encode product family here.

Correct:

- `vendor: st`

Incorrect:

- `vendor: stm32`

---

### `family`

The major family name users are likely to ask for directly.

Examples:

- `stm32`
- `esp32`
- `rp2040`
- `zynq`
- `cyclone`

Rule:

- `family` is the first product grouping below vendor.
- It should match common user phrasing.

Examples:

- user asks "STM series" -> map to `vendor=st`
- user asks "STM32 series" -> map to `family=stm32`
- user asks "ESP32" -> map to `family=esp32`

---

### `series`

The broad technical subdivision within a family.

For STM32:

- `stm32f0`
- `stm32f1`
- `stm32f4`
- `stm32g4`
- `stm32h7`
- `stm32u5`

For ESP32:

- `esp32`
- `esp32c3`
- `esp32c5`
- `esp32c6`
- `esp32s3`

Rule:

- `series` is the best level for queries like "show me all STM32F4 suites".
- This field should stay broad enough to contain multiple lines or parts.

---

### `line`

The specific device line within a series.

Examples:

- `stm32f103`
- `stm32f401`
- `stm32f411`
- `stm32g431`
- `stm32h750`

Rule:

- `line` is narrower than `series`, broader than a specific part number.
- Use this field when multiple package or flash/RAM variants share one logical bring-up path.

---

### `part_number`

The exact MCU part used as the canonical identity for the DUT or suite.

Examples:

- `stm32f103c8t6`
- `stm32f401rct6`
- `esp32c3`
- `rp2040`

Rule:

- This should match the silicon anchor, not the board marketing name.
- Board-level naming still lives in DUT metadata such as `id`, `name`, and board config.

---

## Recommended Mappings

### STM32 examples

| Board / DUT | platform_class | vendor | family | series | line | part_number |
|------------|----------------|--------|--------|--------|------|-------------|
| STM32F103C8T6 Bluepill | `mcu` | `st` | `stm32` | `stm32f1` | `stm32f103` | `stm32f103c8t6` |
| STM32F401RCT6 | `mcu` | `st` | `stm32` | `stm32f4` | `stm32f401` | `stm32f401rct6` |
| STM32F411CEU6 | `mcu` | `st` | `stm32` | `stm32f4` | `stm32f411` | `stm32f411ceu6` |
| STM32G431CBU6 | `mcu` | `st` | `stm32` | `stm32g4` | `stm32g431` | `stm32g431cbu6` |
| STM32H750VBT6 | `mcu` | `st` | `stm32` | `stm32h7` | `stm32h750` | `stm32h750vbt6` |

### ESP32 examples

| Board / DUT | platform_class | vendor | family | series | line | part_number |
|------------|----------------|--------|--------|--------|------|-------------|
| ESP32-C3 DevKit | `mcu` | `espressif` | `esp32` | `esp32c3` | `esp32c3` | `esp32c3` |
| ESP32-C5 DevKit | `mcu` | `espressif` | `esp32` | `esp32c5` | `esp32c5` | `esp32c5` |
| ESP32-S3 DevKit | `mcu` | `espressif` | `esp32` | `esp32s3` | `esp32s3` | `esp32s3` |

### RP examples

| Board / DUT | platform_class | vendor | family | series | line | part_number |
|------------|----------------|--------|--------|--------|------|-------------|
| RP2040 Pico | `mcu` | `raspberry_pi` | `rp2040` | `rp2040` | `rp2040` | `rp2040` |
| RP2350 Pico 2 | `mcu` | `raspberry_pi` | `rp2350` | `rp2350` | `rp2350` | `rp2350` |

---

## Query Mapping Rules

These are the intended resolution rules for future inventory filtering.

### User asks for "STM series"

Interpret as:

- `vendor=st`

Return:

- all ST-managed suites, including STM32 and any future ST FPGA/SoC entries if applicable

### User asks for "STM32 series"

Interpret as:

- `vendor=st`
- `family=stm32`

Return:

- all STM32 suites regardless of series

### User asks for "STM32F4"

Interpret as:

- `vendor=st`
- `family=stm32`
- `series=stm32f4`

Return:

- all F4 suites, such as F401/F411/F407

### User asks for "STM32F103"

Interpret as:

- `vendor=st`
- `family=stm32`
- `series=stm32f1`
- `line=stm32f103`

Return:

- all F103-derived suites, including different part numbers or benches if present

---

## Separation From Suite Status

Taxonomy fields must not encode suite maturity.

Do not use:

- `family: stm32_golden`
- `series: stm32f4_candidate`

Suite maturity belongs in suite-level labeling only:

- `golden`
- `pre_release`
- `testing`
- `candidate`
- `legacy`

This keeps filtering orthogonal:

- hardware class filter
- maturity filter

Example:

- `vendor=st`
- `family=stm32`
- `series=stm32f4`
- `suite_label=golden`

---

## Manifest Placement

Recommended future placement for DUT manifests:

```yaml
classification:
  platform_class: mcu
  vendor: st
  family: stm32
  series: stm32f1
  line: stm32f103
  part_number: stm32f103c8t6
```

Rule:

- taxonomy should live under one dedicated `classification:` block
- inventory should read from that block first
- fallback inference from `family`, `mcu`, or filename should be temporary only

---

## Planned Implementation Phases

### Phase 1

- define taxonomy fields and examples
- do not change runtime behavior yet

### Phase 2

- add `classification:` to completed canonical DUT manifests first
- add validation for allowed keys and normalized values

### Phase 3

- add `ael inventory suites` and filtering by:
  - `platform_class`
  - `vendor`
  - `family`
  - `series`
  - `line`
  - `part_number`
  - `suite_label`

### Phase 4

- make user-facing inventory answers rely on taxonomy-backed filtering instead of repo search heuristics

Current implementation status:

- `classification:` blocks exist on the canonical DUT manifests migrated so far
- `ael inventory suites` supports filtering by:
  - `--platform-class`
  - `--vendor`
  - `--family`
  - `--series`
  - `--line`
  - `--part-number`
  - `--label`

Example:

```bash
python3 -m ael inventory suites --vendor st --family stm32 --series stm32f4 --label golden --format text
```

---

## Non-Goals

This taxonomy does not currently define:

- package-only distinctions beyond `part_number`
- board vendor families as taxonomy families
- per-bench variants as taxonomy branches

Those remain separate concepts:

- board identity
- bench identity
- suite maturity
- connection contract
