# Zephyr + AEL Board Onboarding Guide

Bring any Zephyr-supported board under AEL closed-loop UART validation in six steps.

---

## Overview

AEL's Zephyr pipeline uses `build.type = "zephyr"` to build with `west build`, then flashes via the board's existing AEL instrument path (ST-Link, DAPLink, etc.) — the same GDB-based `load.gdbmi` stage used for bare-metal firmware.  Once a board passes the smoke test here, every Zephyr upstream sample (synchronization, philosophers, …) can be added as an `ael run` test plan with no firmware changes.

> **Flash path clarification:** `flash.method = "zephyr_west"` in a test plan is currently a no-op — the AEL pipeline uses the board profile's GDB flash path (`load.gdbmi`) to program the Zephyr ELF, not `west flash`. The `runner` and `openocd_config` fields in the test plan's `flash` section are therefore informational only. `west flash` is only called if you invoke `ZephyrBackend.flash()` directly (e.g. from a regression test or standalone script).

**Validated boards:**

| Board | Zephyr board name | Console pin | Instrument | UART path | Date |
|-------|-------------------|-------------|------------|-----------|------|
| STM32F4 Discovery | `stm32f4_disco` | PA2 (USART2 TX) | ST-Link onboard | USB-UART `/dev/ttyUSB0` | 2026-04-05 |
| STM32F103RCT6 | `stm32f103_mini` | PA9 (USART1 TX) | DAPLink CMSIS-DAP | DAPLink bridge `/dev/ttyACM0` | 2026-04-05 |

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

| Board | Console UART | TX pin | UART capture path | Notes |
|-------|-------------|--------|-------------------|-------|
| `stm32f4_disco` | USART2 | PA2 | USB-UART adapter RXD | PA9/PA10 occupied by ST-Link UART bridge — do NOT use |
| `stm32f103_mini` | USART1 | PA9 | DAPLink UART bridge (`/dev/ttyACM0`) | DAPLink has built-in USB-UART; no separate adapter needed |
| `stm32_min_dev` | USART1 | PA9 | USB-UART adapter RXD | Blue Pill variant (F103xb) |

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

If the board already has an AEL board profile (it was previously used for bare-metal tests), no changes are needed — the existing GDB flash path works for Zephyr ELFs too.

If registering a new board, create `configs/boards/<board_id>.yaml`. The `flash` section should match whatever GDB/SWD instrument is on the bench (same as bare-metal):

```yaml
board:
  name: "<Human-readable name>"
  target: "<board_id>"
  instrument_instance: "<instrument_id>"
  kind: bare_mcu                     # or zephyr_mcu; both work
  features:
    - programmable_via_swd
  flash:
    reset_strategy: none             # adjust per board
    post_load_settle_s: 1.5
    gdb_launch_cmds:
      - "file {firmware}"
      - "load"
      - "monitor reset run"
      - "detach"
```

Set `preflight.enabled = false` in every Zephyr test plan — preflight probes the AEL mailbox, which Zephyr firmware does not implement.

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

**The five board-specific variables that actually matter:**

| Field | Where to get it | Example |
|-------|----------------|---------|
| `build.zephyr_board` | Step 1 output | `stm32f103_mini` |
| `build.project_dir` | Path to firmware source | `/home/aes/zephyrproject/zephyr/samples/synchronization` or `firmware/targets/...` |
| `build.build_dir` | Artifact output dir (relative to repo) | `artifacts/build_<board>_<test>` |
| `observe_uart.port` | Step 5 confirmed port | `/dev/ttyUSB0` or `/dev/ttyACM0` |
| `observe_uart.expect_patterns` | Known console output | `["thread_a: Hello World", "thread_b: Hello World"]` |

> `flash.runner` and `flash.openocd_config` in the test plan are informational only — the AEL pipeline uses the board profile's GDB flash path, not `west flash`. You can omit them or leave them as documentation.

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
| `west flash` fails: "Address already in use :3333/:4242" | AEL pyocd / st-util holding port | Only relevant if calling `ZephyrBackend.flash()` directly; `ZephyrBackend._release_port()` handles it |
| `zephyr_board` ignored, build uses wrong board | `zephyr_board` not in strategy_resolver merge list | Fixed in `de4b4d7`; `zephyr_board` is now correctly propagated from test plan to build |
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
