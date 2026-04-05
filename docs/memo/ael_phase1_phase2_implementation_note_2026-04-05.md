# AEL Phase 1 + Phase 2 Implementation Note

**Date:** 2026-04-05  
**Scope:** Backend contract documentation + FreeRTOS pilot readiness  
**Commit scope:** `ael/backends/base.py`, `ael/adapter_registry.py`

---

## What Was Changed

### 1. `ael/backends/base.py` — contract documentation overhaul

**Problem:** The existing docstrings were too thin and contained one active error:
- `observe()` docstring said "Default implementation re-uses AEL's existing observe_uart layer" — but the actual code raises `NotImplementedError`. This is directly misleading.
- `flash(artifact)` did not explain when `artifact` is used vs ignored. ZephyrBackend ignores it entirely (uses build_dir), but this was not documented.
- `start_debugserver()` had no lifecycle contract. Callers had to read ZephyrBackend source to know they needed to call `proc.terminate()`.
- `build()` did not document whether to raise or return None on failure.
- No pressure-point notes for future ecosystems (Linux, FreeRTOS).

**What changed:** All six methods now have complete docstrings. Signatures are unchanged. No behavior was modified.

Key corrections:
- `observe()`: "NOT implemented by default — raises NotImplementedError. Every backend must override." (was: "Default implementation re-uses AEL layer")
- `verify()`: same correction.
- `flash(artifact)`: documents that `artifact` is an optional hint, not required. Backends with build state (e.g. Zephyr) may ignore it entirely.
- `start_debugserver()`: full lifecycle contract added (ready-marker wait, caller-terminate, port release precondition).
- Added module-level docstring explaining how Backends relate to the adapter layer (native bare-metal tests bypass Backends entirely).

### 2. `ael/adapter_registry.py` — `"build.make"` alias

**Added:**
```python
"build.make": _BuildAdapter("arm_debug"),  # alias: generic make-based ARM build
```

**Why:** The existing `"build.arm_debug"` build type routes to `build_stm32.run()`, which executes `make -C <dir> BUILD_DIR=... OUT_ELF=... OUT_BIN=...`. This logic is not STM32-specific — it is a generic make invocation for any ARM bare-metal project (including FreeRTOS STM32CubeMX projects).

The name "arm_debug" is internal and confusing for non-STM32 users. Adding `"build.make"` as an explicit alias lets new test plans use:
```json
"build": { "type": "make", "project_dir": "...", "artifact_stem": "..." }
```
without needing to know the internal adapter name. No existing test plans are affected.

---

## What Was Deliberately NOT Changed

### No `ael/providers/` directory created

The task prompt proposed creating `west_provider`, `openocd_provider`, `pyocd_provider`.

**Audit result:** No duplication currently justifies this.

- **west**: all west invocations are in `ZephyrBackend._run()`. Adding a `west_provider.py` wrapper would be pure indirection with no second consumer.
- **openocd**: there are two distinct paths — `flash_bmda_gdbmi.py` starts an OpenOCD GDB server manually with a hand-crafted command; `ZephyrBackend.flash()` delegates to `west flash --runner openocd` which constructs its own openocd invocation internally. These are genuinely different use cases, not duplication of the same logic.
- **pyocd**: one narrow path in `ZephyrBackend.flash()` (the `pyocd_direct` runner). A `pyocd_provider.py` with one caller would be over-engineering.

**Rule:** `providers/` should be created when a second consumer appears that would otherwise duplicate the same tool invocation logic. The right trigger is the FreeRTOS backend needing to reuse west — at that point `west_provider.py` becomes justified. Not before.

### No signature changes to `AELBackend` methods

The `build(**kwargs)`, `flash(artifact, **kwargs)`, etc. signatures were not changed. The pressure points (Linux image vs ELF, SSH vs serial, etc.) are documented as comments. Changing signatures now without a concrete second backend to validate them would be speculative.

### No new `test_kind` values defined in routing code

`test_kind` is a label field — it is only read by `default_verification.py`, `stage_explain.py`, and `inventory.py` for counting and display. It drives zero routing decisions. A FreeRTOS test plan can use `"test_kind": "uart_observe"` and the existing pipeline handles it correctly without any code changes.

---

## FreeRTOS Pilot Readiness

A minimal FreeRTOS UART observe pilot on STM32F103RCT6 can be written **today** with zero additional code changes.

### What already works

| Piece | Existing mechanism | Status |
|-------|--------------------|--------|
| Build | `build.type = "make"` → `build_stm32.run()` → `make -C <dir>` | ✓ ready (alias just added) |
| Flash | `flash.method = "gdbmi"` → standard GDB flash path | ✓ ready |
| UART observe | `observe_uart.enabled = true` → `check.uart_log` adapter | ✓ ready |
| Verification | `observe_uart.expect_patterns` substring match | ✓ ready |
| Reporting label | `test_kind = "uart_observe"` | ✓ ready (falls through gracefully) |

### What is missing (not AEL code — firmware work)

1. **FreeRTOS firmware** with UART output on the target board. Recommended: STM32CubeMX FreeRTOS example on STM32F103RCT6, USART1 PA9 115200, two tasks printing alternating messages.

2. **Makefile compatibility**: `build_stm32.run()` passes `BUILD_DIR=`, `OUT_ELF=`, `OUT_BIN=` as make variables. The project Makefile must honour these. STM32CubeMX-generated Makefiles do not by default — a thin wrapper Makefile or build_dir adaptation is needed.

3. **Test plan JSON** — template (no code needed):
```json
{
  "schema_version": "1.0",
  "test_kind": "uart_observe",
  "name": "stm32f103rct6_freertos_tasks_uart",
  "board": "stm32f103rct6",
  "build": {
    "type": "make",
    "project_dir": "firmware/targets/stm32f103rct6_freertos_tasks",
    "artifact_stem": "freertos_tasks"
  },
  "flash": {
    "method": "gdbmi"
  },
  "observe_uart": {
    "enabled": true,
    "port": "/dev/ttyACM0",
    "baud": 115200,
    "duration_s": 8,
    "expect_patterns": ["Task A", "Task B"]
  }
}
```

### Is a `FreeRTOSBackend` needed?

**No — not for the first pilot.**

The backend class exists for ecosystems with coordinated runtime state (workspace path, toolchain env, multiple related commands sharing context). FreeRTOS on STM32 with a simple make build and gdbmi flash has no such shared state — the existing adapter chain handles it.

A `FreeRTOSBackend` becomes justified when:
- project auto-detection is needed (`detect_project_type()`), OR
- two or more boards/projects have proven the pattern and a generalizing backend reduces duplication.

For now: firmware + test plan + `build.make` alias is all that is needed to run the first FreeRTOS pilot.
