# Memo: FreeRTOS Integration Strategy — Lessons from Zephyr

**Date:** 2026-04-05  
**Status:** Strategy guide — pre-implementation reference  
**Source:** chat_ael-arch-review.txt

---

## Purpose

This memo defines how AEL should approach FreeRTOS integration after the successful Zephyr path.

The goal is not to force FreeRTOS into a Zephyr-shaped model. The goal is to identify which lessons from the Zephyr path are reusable, and how AEL can apply the same AI-driven principles to FreeRTOS projects.

---

## Executive Summary

**The core lesson from Zephyr:**
> AEL should not try to replace upstream ecosystems. AEL should wrap them in AI-driven closed-loop orchestration, observation, verification, and recovery.

FreeRTOS should be approached the same way:
- do not rewrite it,
- do not normalize it completely before use,
- do not force it into a Zephyr-like structure.

**The first FreeRTOS milestone should be narrow, explicit, and repeatable:**
- one board,
- one known FreeRTOS demo,
- one build path,
- one flash path,
- one UART-observable result,
- one AEL verification rule.

---

## Why FreeRTOS Matters

FreeRTOS is a strong next integration target because it is **different enough from Zephyr** to test whether AEL's abstractions are genuinely reusable.

| | Zephyr | FreeRTOS |
|--|--------|---------|
| Board support | Unified, large BSP library | Vendor-specific, variable |
| Workspace tool | `west` (standard) | None (project-specific) |
| Build system | CMake (standard) | Make, CMake, IAR, Keil — varies |
| Sample library | Large, curated | Vendor demos, variable quality |
| Flash/debug | west runners (standard) | Vendor tools, project-specific |

**If AEL can integrate FreeRTOS cleanly, the backend model is becoming genuinely multi-ecosystem.**

---

## The Key Distinction: Human-Driven vs AI-Driven

### FreeRTOS upstream development is human-driven
Typical FreeRTOS workflow:
1. Developer builds a demo,
2. flashes the board,
3. opens a terminal,
4. reads output,
5. inspects behavior,
6. decides if correct.

### AEL is AI-driven
AEL's role is to replace steps 3-6 with:
- automatic observation,
- automatic verification,
- automatic pass/fail reporting,
- automatic recovery (future).

> FreeRTOS remains the upstream development ecosystem. AEL adds the machine-operable validation loop.

This is identical to how AEL treats Zephyr. The principle generalizes.

---

## Lessons from Zephyr That Apply Directly

### 1. Start with a Narrow Pilot
Do not begin by trying to "support FreeRTOS in general."

Start with: one board + one demo + one UART output + one AEL verification rule.

### 2. Reuse Upstream Structure
Do not immediately rewrite or restructure the FreeRTOS project.
- Understand it as it already exists,
- reuse its build path,
- reuse its flash path,
- add AEL observe/verify around it.

### 3. Prefer UART-Observable First
UART is:
- common across all boards,
- easy to capture,
- easy to verify (substring/regex),
- easy to generalize.

This aligned with what worked in Zephyr and should be the default first observation mode.

### 4. Keep the Backend Thin
A `FreeRTOSBackend` should not duplicate AEL's observation/verification logic. It should mainly:
- detect project structure,
- drive build/flash,
- connect to AEL observe/verify,
- normalize outputs.

---

## What Should NOT Be Done at the Beginning

| Trap | Reason |
|------|--------|
| Support all FreeRTOS projects at once | FreeRTOS appears in many forms — broad support is too early |
| Start with non-observable demos | First pilot must produce a clear machine-readable result |
| Force a uniform workspace model | FreeRTOS may not naturally have one — adapt to reality |
| Overdesign the backend before a pilot | Real pilots expose constraints that theory misses |

---

## Recommended First Pattern

```
Board:       one board with a known working FreeRTOS demo
Project:     upstream vendor demo or known sample (not a custom write)
Build path:  whatever the project already uses (make, cmake, etc.)
Flash path:  whatever the board already supports (gdbmi, pyocd, J-Link, etc.)
Observation: UART serial output
Verification: substring/regex match on known output patterns
AEL role:    trigger build → flash → observe → verify → report PASS/FAIL
```

Good initial UART observable patterns for FreeRTOS:
- startup banner (scheduler started),
- task startup messages (`Task X started`),
- periodic status lines,
- synchronization sequences,
- known PASS marker lines.

---

## What a Thin `FreeRTOSBackend` Should Implement

Follows the same `AELBackend` contract (`ael/backends/base.py`):

```python
class FreeRTOSBackend(AELBackend):

    def detect_project_type(self, project_dir: Path) -> bool:
        # Does this look like a FreeRTOS project?
        # Check for FreeRTOS.h, FreeRTOSConfig.h, tasks.c, etc.
        ...

    def build(self, **kwargs) -> Path:
        # Call the project's existing build path (make/cmake)
        # Return path to ELF artifact
        ...

    def flash(self, artifact: Path = None, **kwargs) -> None:
        # Use the board's known tool path (gdbmi, pyocd, J-Link, etc.)
        # Same flash adapter infrastructure as native AEL
        ...

    def start_debugserver(self, **kwargs):
        # Optional for first pilot; defer if not needed
        ...

    def observe(self, port, baud, duration_s, expect_patterns) -> dict:
        # Reuse AEL's existing observe_uart layer directly
        ...

    def verify(self, observation, expectations) -> dict:
        # Reuse AEL's existing verify layer directly
        ...
```

**Goal:** not a universal backend on day one. Prove that AEL can absorb a FreeRTOS project the same way it absorbed a Zephyr project.

---

## Integration Categories (Phased)

| Category | When | Complexity |
|----------|------|-----------|
| 1. Vendor-provided demos | Phase 1 (first pilot) | Low — known board, known output |
| 2. Existing customer/user projects | Phase 3+ | Higher — more variable |
| 3. AEL-maintained minimal harness projects | Phase 4+ | Medium — AEL writes thin wrapper firmware |

**Start with Category 1.**

---

## Observation and Verification Strategy

### Phase 1 — UART substring match
- capture serial output for N seconds,
- match against expected patterns (same as `zephyr_uart_observe`),
- report PASS/FAIL.

No upstream modification needed if output is already readable.

### Phase 2 — Structured output (if needed)
- add machine-readable PASS/FAIL markers in firmware,
- or use AEL mailbox pattern (if bare-metal-style firmware).

### Phase 3 — Richer observation
- GPIO-based observation,
- RTT (if J-Link available),
- timing-based assertions.

**Phase 1 is sufficient for the first milestone.**

---

## How FreeRTOS Fits the Broader Architecture

FreeRTOS should not be a special exception. It should validate that the five-layer architecture holds:

| Layer | FreeRTOS treatment |
|-------|--------------------|
| Core Loop | Same detect→build→flash→observe→verify→recover |
| Backend | `FreeRTOSBackend` adapts ecosystem; thin wrapper |
| Tool/Provider | Vendor tools, make, cmake, gdbmi — invoked by backend |
| Observation/Verification | Reuse AEL UART observe + verify layers directly |
| Onboarding | Board profile + project profile (build_cmd, flash_cmd, console, expected) |

---

## Recommended Roadmap

### Phase 1 — Pick One FreeRTOS Pilot
- one board (prefer STM32 — already wired, gdbmi proven),
- one UART-observable vendor demo (STM32Cube FreeRTOS example, or FreeRTOS demo repo),
- known `make`/`cmake` build path,
- known `gdbmi` flash path.

**Deliverable:** one test plan JSON + AEL closed-loop PASS.

### Phase 2 — Connect to Existing AEL Observation/Verification
- serial capture (reuse `observe_uart` adapter directly),
- substring/regex match,
- PASS/FAIL result in standard AEL format.

**Deliverable:** `tests/plans/stm32f103rct6_freertos_<demo>.json` or similar.

### Phase 3 — Define Minimal Board/Project Profile
Capture:
- board name and instrument,
- console path and baud,
- build command,
- flash command,
- expected output patterns.

**Deliverable:** `FreeRTOSBackend.detect_project_type()` + onboarding template.

### Phase 4 — Add a Second FreeRTOS Project or Board
Tests whether Phase 1 was a case or a pattern.

**Deliverable:** two boards/demos both PASS → FreeRTOS integration promoted from pilot to pattern.

### Phase 5 — Only Then Generalize
Once two or more pilots succeed:
- refine backend detection,
- formalize provider abstraction,
- define failure taxonomy,
- create onboarding templates.

---

## Final Conclusion

The right first milestone is not "support FreeRTOS broadly."

The right first milestone is:
> **Demonstrate one reliable, UART-observable, AEL-driven FreeRTOS closed loop on one known board.**

If that succeeds, FreeRTOS becomes the next proof that AEL's architecture is genuinely multi-ecosystem.

The method is the same as Zephyr:
- do not replace the upstream ecosystem,
- do not begin by rewriting it,
- start with a narrow observable pilot,
- reuse upstream tools where practical,
- let AEL provide the AI-driven closed loop.
