# AEL Architecture Review: Multi-Ecosystem Expansion
## West, FreeRTOS, and Embedded Linux

**Date:** 2026-04-05  
**Status:** Reference — basis for incremental restructuring  
**Source:** chat_ael-arch-review.txt

---

## Purpose

This memo reviews the AEL architecture after the successful Zephyr integration and determines whether the codebase is ready for broader architectural reassessment. The goal is to define how AEL should evolve to support:

- native bare-metal embedded projects (existing),
- Zephyr and west-backed projects (proven),
- future FreeRTOS projects,
- and eventually embedded Linux systems.

The central question: is AEL now ready to become a **multi-ecosystem AI-driven embedded validation system**?

---

## Executive Summary

**Yes, this is the right time for an architecture review.**

AEL has crossed an important threshold:

- native bare-metal workflows already exist,
- Zephyr is integrated through a backend structure and proven end-to-end,
- real cross-layer experience with orchestration, observation, verification, and backend coexistence has been accumulated.

AEL is no longer a single-path MCU automation system. It is becoming a framework for integrating multiple embedded development ecosystems.

If the architecture is not reviewed now, future additions (west reuse, FreeRTOS, Linux) will accumulate as isolated special cases, leading to fragmentation and technical debt.

**The correct action is not a large rewrite. It is an architecture review followed by careful, incremental restructuring.**

---

## What Has Already Been Proven

### 1. Native AEL Workflows Remain Valid
Bare-metal validation flows work. These remain the core value baseline.

### 2. Zephyr Integration Is Proven
Proven capabilities:
- `blinky`, `hello_world`, `synchronization`, `philosophers` — all PASS,
- UART observation works (multiple boards: STM32F4 Discovery, STM32F103RCT6),
- debugserver + GDB attach works,
- AEL native flows coexist on the same board,
- `AELBackend` contract exists (`ael/backends/base.py`),
- `ZephyrBackend` implements that contract (`ael/backends/zephyr_backend.py`),
- `observe()` and `verify()` are wired to AEL's existing layers,
- Hybrid Mode: Zephyr and bare-metal tests interleaved in one `ael pack` run — all PASS.

### 3. The AEL vs Ecosystem Distinction Is Clear

**External ecosystems (Zephyr, FreeRTOS, Linux) are human-driven:**
- board support, samples, build/flash/debug tools,
- workflows where a human reads logs, observes LEDs, and judges success.

**AEL is AI-driven:**
- orchestrate the run,
- observe target behavior,
- verify pass/fail,
- recover and retry automatically.

> AEL does not replace external ecosystems. AEL turns them from human-operated workflows into machine-operable closed loops.

This distinction is fundamental and must guide all future architecture decisions.

---

## Why the Architecture Should Be Reviewed Now

If the review happens too early → too speculative.  
If it happens too late → code paths for Zephyr, west, FreeRTOS, Linux accumulate as one-off exceptions.

**The review should answer:**

1. What is truly core to AEL?
2. What should AEL own directly?
3. What should AEL reuse from upstream ecosystems?
4. What is a backend?
5. What is a tool/provider?
6. What is common across all ecosystems?
7. What differs by ecosystem?
8. How should project and board onboarding be modeled?

---

## What AEL Should Treat as Core

These should remain central AEL responsibilities — the AI-driven parts:

- **orchestration** — pipeline execution, step ordering, pack runs,
- **observation** — UART, mailbox, GPIO, RTT, SSH, logs,
- **verification** — pattern match, pass/fail, result interpretation,
- **recovery and retry logic** — failure taxonomy, retry policies,
- **test plans and packs** — schema, execution contract,
- **board capability knowledge** — board profiles, wiring, features,
- **learned skills** — Civilization Engine patterns, experience records.

AEL should **not** rebuild what external ecosystems already provide well:
- vendor/ecosystem build systems (west build, make, cmake),
- ecosystem-native flash tools (west flash, pyocd, J-Link),
- upstream sample libraries.

Instead: **reuse and wrap in closed-loop logic.**

---

## The Five Layers That Need Review

### Layer 1 — Core Loop

```
detect → build → flash/deploy → observe → verify → recover
```

Questions:
- Which parts are universal?
- Which parts are ecosystem-specific?
- Which parts should be abstracted vs delegated?

**Current evidence:** `observe` and `verify` are more reusable; `build` and `flash/deploy` are more ecosystem-dependent. This is a strong hint for where the architecture boundary should sit.

### Layer 2 — Backend

`AELBackend` (`ael/backends/base.py`) + `ZephyrBackend` (`ael/backends/zephyr_backend.py`) already exist.

Review questions:
- Is the current contract broad enough for FreeRTOS and Linux?
- Are method names and semantics stable?
- Are result structures normalized?
- Should backend methods return richer, standardized data?

Current contract methods: `detect_project_type()`, `build()`, `flash()`, `start_debugserver()`, `observe()`, `verify()`.

### Layer 3 — Tool / Provider

This layer should become a more **explicit concept** in the architecture.

Current tools used implicitly:
- `west` (Zephyr build/flash/debug),
- OpenOCD (GDB server),
- pyOCD (direct flash),
- GDB (flash+attach),
- Make/CMake/Ninja,
- BMDA.

The distinction should be:
- **backend** = ecosystem integration logic,
- **tool/provider** = concrete mechanism invoked by that backend.

### Layer 4 — Observation / Verification

AEL's strongest emerging layer. Already reused across bare-metal and Zephyr.

Review questions:
- Can UART-observable validation be standardized further?
- Can mailbox-observable validation be standardized?
- Can GPIO/LED observation be standardized?
- Will embedded Linux use serial console, SSH, logs, or network probing?

**This layer should be strengthened and kept universal — not fragmented by ecosystem.**

### Layer 5 — Onboarding

Becoming increasingly important. AEL needs a repeatable way to onboard:
- a new board,
- a new project,
- a new ecosystem,
- a new observation path.

Candidates for formalization:
- board profiles (exists: `configs/bench_profiles/`),
- project type detection (`detect_project_type()`),
- wiring checklists,
- console configuration,
- flash/debug configuration,
- sample/test templates (exists: `tests/plans/templates/`),
- expected outputs.

---

## How Embedded Linux Should Be Included

Embedded Linux introduces different assumptions that affect architecture shape:
- boot/deploy = images (not MCU firmware),
- deployment = SD card, eMMC, network boot, U-Boot,
- observation = serial console, SSH, system logs, network probes,
- verification = system-level, not sample-level,
- recovery = reboot flows, bootloader logic, filesystem resets.

**Linux should be used as a future constraint to judge whether the architecture is too MCU-specific — even before it is implemented.**

The architecture does not need to be rewritten for Linux now. It does need to be reviewed with Linux in mind.

---

## What Should Be Avoided

| Trap | Why |
|------|-----|
| Large-scale rewrite | Destabilizes working code and proven paths |
| Backend proliferation without contract review | Adds backends before checking if the abstraction is sound |
| Making `west` the center of the system | `west` is an opportunistic provider, not a universal foundation |
| Mixing ecosystem integration with full board verification | Current work = define structure, not complete all validation packs |

---

## Recommended Outputs of the Review

1. **Architecture Review Memo** (this document) — layers, boundaries, future ecosystem implications.
2. **Backend Contract Review** — focused review of `AELBackend`, method semantics, return schemas, FreeRTOS/Linux compatibility.
3. **Integration Taxonomy** — classification of project types: native AEL, west-backed RTOS, vendor-demo RTOS, embedded Linux.
4. **Roadmap** — staged, reality-based.

---

## Recommended Roadmap

### Phase 1 — Generalize the Zephyr UART Pattern *(current state)*
Convert the F103/F407 Zephyr pilot into a reusable pattern:
- board profile approach proven,
- UART-observable closed loop proven,
- multi-board validated (2 boards, 7 tests PASS),
- Hybrid Mode proven (bare-metal + Zephyr coexist in one pack run).

**Status: COMPLETE.**

### Phase 2 — Review and Refine Backend/Tool Boundaries
- Clarify `AELBackend` contract for FreeRTOS/Linux compatibility,
- make the tool/provider layer an explicit concept,
- normalize observation and verification data structures,
- document onboarding schemas.

### Phase 3 — Narrow FreeRTOS Pilot
- one board, one UART-observable FreeRTOS demo,
- reuse existing project structure, existing build/flash tools,
- wrap with AEL observe/verify,
- prove the backend model generalizes beyond Zephyr.

### Phase 4 — Embedded Linux as Structural Constraint
- not necessarily implemented immediately,
- used to ensure the architecture is not too narrowly MCU-specific.

---

## One-Sentence Conclusion

AEL should keep its core identity as an AI-driven orchestration, observation, verification, and recovery system; opportunistically reuse upstream ecosystem tooling such as `west`; and evolve its architecture so that Zephyr, FreeRTOS, and eventually embedded Linux can all be integrated without fragmenting the codebase.
