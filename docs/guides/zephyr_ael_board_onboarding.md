# Zephyr + AEL Board Onboarding Guide

Bring any Zephyr-supported board under AEL closed-loop UART validation in six steps.

---

## Overview

AEL's Zephyr pipeline (`build.type = "zephyr"` + `flash.method = "zephyr_west"`) wraps the standard `west build → west flash → observe_uart → verify` workflow.  Once a board passes the smoke test here, every Zephyr upstream sample (synchronization, philosophers, …) can be added as an `ael run` test plan with no firmware changes.

**Validated boards:**

| Board | Zephyr board name | Console pin | Instrument | Date |
|-------|-------------------|-------------|------------|------|
| STM32F4 Discovery | `stm32f4_disco` | PA2 (USART2 TX) | ST-Link onboard | 2026-04-05 |

---

## Prerequisites

- Zephyr workspace initialised at `~/zephyrproject/` (run `west init` + `west update` once)
- Python ≥ 3.12 venv at `~/zephyr-venv/` (`uv venv --python 3.12 ~/zephyr-venv`)
- ARM GNU toolchain at `/nvme1t/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/` (or update `ZephyrBackend` defaults)
- A USB-UART adapter (3.3 V logic) connected to the board's console TX pin

---

## Step-by-Step Checklist

### 1 — Confirm Zephyr supports the board

```bash
ZEPHYR_BASE=~/zephyrproject/zephyr ~/zephyr-venv/bin/west boards | grep <chip_family>
```

Note the exact **Zephyr board name** (e.g. `stm32f4_disco`, `stm32_min_dev`).

---

### 2 — Find the console UART and physical TX pin

Every Zephyr board defines its console via the DTS `chosen` node:

```bash
# Find the chosen console peripheral
grep -A6 "chosen" ~/zephyrproject/zephyr/boards/<arch>/<board_dir>/<board>.dts

# Find the TX pin in the pinctrl binding
grep "usart1_tx\|usart2_tx\|lpuart1_tx" ~/zephyrproject/zephyr/boards/<arch>/<board_dir>/<board>.dts
```

**Common mappings (confirmed):**

| Board | Console UART | TX pin | Notes |
|-------|-------------|--------|-------|
| `stm32f4_disco` | USART2 | PA2 | PA9/PA10 occupied by ST-Link UART bridge — do NOT use |
| `stm32_min_dev` | USART1 | PA9 | Standard Blue Pill console |

> **Caution:** On STM32F4 Discovery, PA9/PA10 are bridged to the onboard ST-Link UART chip and are not wired to the MCU USART in the typical hardware configuration. Zephyr's DTS correctly maps the console to USART2/PA2.

---

### 3 — Identify the flash runner

```bash
cat ~/zephyrproject/zephyr/boards/<arch>/<board_dir>/board.cmake
```

The first `include(...)` after the `board_runner_args` lines shows the default runner.  Common values:

| runner | Instrument type |
|--------|----------------|
| `openocd` | ST-Link, CMSIS-DAP (DAPLink), J-Link via OpenOCD |
| `jlink` | Segger J-Link directly |
| `pyocd` | CMSIS-DAP via pyOCD |

> If the board uses `openocd` with `source [find interface/stlink.cfg]` but you have a CMSIS-DAP probe, provide a custom `openocd_config` override in the test plan (see Step 6).

---

### 4 — Register the board in AEL (if not already present)

Create `configs/boards/<board_id>.yaml` with the minimal Zephyr profile:

```yaml
board:
  name: "<Human-readable name>"
  target: "<board_id>"               # AEL internal target name
  instrument_instance: "<instrument_id>"
  kind: zephyr_mcu
  features:
    - programmable_via_swd
  flash:
    method: zephyr_west
    runner: openocd                  # or jlink / pyocd
    post_load_settle_s: 1.5
```

For the `bench_setup` section of the test plan, set `preflight.enabled = false` — Zephyr uses west/OpenOCD directly; the AEL instrument stack is not involved in flash.

---

### 5 — Wire the bench

```
DUT console TX pin  →  USB-UART adapter RXD
DUT GND             →  USB-UART adapter GND
Instrument (SWD)    →  DUT SWD header
```

Confirm the USB-UART adapter appears as `/dev/ttyUSBx` or `/dev/ttyACMx`:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

---

### 6 — Copy and fill the test plan template

Start from `tests/plans/templates/zephyr_uart_observe_template.json`.

**The six board-specific variables:**

| Field | Where to get it | Example |
|-------|----------------|---------|
| `build.zephyr_board` | Step 1 output | `stm32f4_disco` |
| `build.project_dir` | Path to firmware source | `/home/aes/zephyrproject/zephyr/samples/synchronization` or `firmware/targets/...` |
| `build.build_dir` | Artifact output dir (relative to repo) | `artifacts/build_<board>_<test>` |
| `flash.runner` | Step 3 output | `openocd` |
| `observe_uart.port` | Step 5 confirmed port | `/dev/ttyUSB0` |
| `observe_uart.expect_patterns` | Known console output | `["thread_a: Hello World", "thread_b: Hello World"]` |

If your board uses CMSIS-DAP but the board's `openocd.cfg` hardcodes ST-Link, add:

```json
"flash": {
  "method": "zephyr_west",
  "runner": "openocd",
  "openocd_config": "firmware/targets/<board_target>/openocd_cmsis_dap.cfg"
}
```

And create `firmware/targets/<board_target>/openocd_cmsis_dap.cfg`:

```tcl
source [find interface/cmsis-dap.cfg]
transport select swd
source [find target/stm32f1x.cfg]   # or stm32f4x.cfg, stm32h7x.cfg, etc.
```

---

### 7 — Run

```bash
python3 -m ael run --board <ael_board_id> --test tests/plans/<your_plan>.json
```

**PASS criteria:** `uart.verify` substage passes, all `expect_patterns` found.

---

## Pitfalls to Avoid

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| `west build` fails: Python < 3.12 | System Python is 3.10 | Use `~/zephyr-venv/` (Python 3.12) |
| `west flash` fails: "Address already in use :3333/:4242" | AEL pyocd / st-util holding port | `ZephyrBackend._release_port()` handles this automatically |
| UART captures 0 bytes | Wrong TX pin wired | Check DTS `chosen` node; on F4 Discovery use PA2 not PA9 |
| Patterns not found | Single-print firmware (like hello_world) printed before observe_uart opened | Use `firmware/templates/zephyr_hello_loop_template/` which prints every 500 ms |
| `preflight` stage fails | AEL trying to use instrument stack for Zephyr board | Add `"preflight": {"enabled": false}` to test plan |
| philosophers UART looks garbled | VT100 cursor codes (`\x1b[N;1H`) present | Normal — AEL substring match works through escape codes |

---

## Quick Reference: Confirmed Sample → expect_patterns Mapping

| Sample | expect_patterns |
|--------|----------------|
| `firmware/templates/zephyr_hello_loop_template` | `["AEL_ZEPHYR_IDLE"]` |
| `samples/synchronization` | `["thread_a: Hello World", "thread_b: Hello World"]` |
| `samples/philosophers` | `["Philosopher", "EATING", "THINKING"]` |
| `samples/hello_world` | `["Hello World!"]` — risk: single-print race; prefer hello_loop |
