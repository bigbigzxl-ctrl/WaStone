# STM32H563RGT6 PKA Mailbox — Investigation Report

**Date:** 2026-04-05  
**Status:** SUSPENDED — excluded from golden suite pending hardware investigation  
**Test plan:** `tests/plans/stm32h563rgt6_pka_mailbox.json`  
**Firmware:** `firmware/targets/stm32h563rgt6_pka_mailbox/`

---

## Summary

The PKA arithmetic-add self-test (0x1234 + 0x5678 = 0x68AC) was run **90 times** across
2026-04-03 and 2026-04-05. It produced:

| Outcome | Count | Notes |
|---------|-------|-------|
| True PASS (PKA firmware, mailbox status=PASS) | **1** | 2026-04-03 06:29:33, fresh board |
| False PASS (wrong firmware flashed) | 7 | `minimal_runtime_mailbox_app.elf` built instead of PKA firmware |
| FAIL — E001 INITOK timeout | ~75 | The dominant failure mode |
| FAIL — E002 PROCENDF timeout | ~6 | After some firmware iterations |
| FAIL — E003 operation error | 2 | Error flag set after START |
| FAIL — other / pipeline error | ~1 | Flash/attach failures |

---

## The One Genuine PASS

**Run:** `2026-04-03_06-29-33`

- Board had just been freshly connected (power-on state)
- PKA firmware (`stm32h563rgt6_pka_mailbox_app.elf`) was built and flashed for the first time
- Mailbox verified: `status=PASS, error_code=0x0, detail0=0`
- The result word (0x68AC) was not captured in `detail0` in that early firmware version

This was the only run where PKA SRAM ECC was in a clean, freshly-erased state at boot.

---

## The Seven False PASSes

Runs at 06:14:03, 06:14:59, 06:40:40, 11:04:40, 11:42:16, 13:10:41, 16:38-59 on 2026-04-03
all show `success=True` but used `stm32h563rgt6_minimal_runtime_mailbox_app.elf` — the default
board firmware — instead of the PKA test firmware. The framework built the wrong artifact
(a board-config build override took precedence over the test plan's build config during those
sessions). These are not PKA test results.

---

## Root Cause Analysis

### PKA internal SRAM and ECC

The STM32H563 PKA has 1334-word (5336 byte) ECC-protected internal SRAM. When PKA is enabled
(`CR.EN=1`), the hardware performs an internal erase/scan of this SRAM:

1. `CR.EN=1` → immediate: bit1 of PKA_SR fires ("erase started"), no INITOK yet
2. PKA scans RAM ECC asynchronously; any word with bad ECC fires `OPERRF` (bit21)
3. `OPERRF` is a sticky latch — it must be cleared via `CLRFR` for the scan to advance
4. After all 1334 words are scanned/erased clean, `INITOK` (bit0) fires

### Why it fails after the first run

The critical hardware constraint: **PKA internal SRAM ECC state persists across `NRST`
(hardware reset, connect-under-reset) and across AHB peripheral reset (`RCC_AHB2RSTR`).**
Only a true Power-On Reset (POR) is supposed to clear SRAM to a known state.

After the first EN=1 scan, the PKA SRAM is in a well-defined erased state. Subsequent
`monitor reset hardware` (which asserts nRESET via DAPLink) resets the CPU and peripherals
but does NOT clear PKA SRAM contents. The next run's `EN=1` scan therefore encounters the
same ECC state left by the previous run.

### Why it still fails after user power cycle (2026-04-05)

After the user performed a full power cycle, the PKA SRAM contents become undefined/random
(SRAM loses content on power loss). The EN=1 erase scan will encounter ECC errors (random
data with random ECC) and fire `OPERRF` repeatedly. The firmware correctly clears `OPERRF`
without toggling `EN` to allow the scan to continue. However, **INITOK never fires within our
timeout (`INITOK_TIMEOUT = 10,000,000` loop iterations)**.

Estimated worst-case erase time at 64 MHz with O2 optimization and peripheral reads:
~5 ns/cycle × 5 cycles/iteration × 10M iterations ≈ **250 ms**. The PKA erase may need
longer than 250 ms in the worst case (all 1334 words corrupted), or the loop overhead is
underestimated.

The last run after power cycle (`2026-04-05_03-39-00`) shows `E001, PKA_SR=0x2` at timeout:
bit1 is still set (erase running) but `OPERRF` is not visible (either cleared or not yet
re-asserted). INITOK has not fired.

### Firmware approach summary

Over ~90 runs, the following approaches were tested:

| Approach | Result |
|----------|--------|
| Toggle EN=0/1 on every OPERRF | FAIL — restarts erase from scratch, infinite loop |
| Wait for INITOK without clearing OPERRF | FAIL — OPERRF blocks scan |
| Use secure alias `0x520C2000` | FAIL — same behavior |
| IDX_OP2=538 (from CMSIS header) | FAIL — correct index is 546 (verified from passing ELF) |
| Clear OPERRF, keep EN=1, loop until INITOK | FAIL — timeout (current approach, conceptually correct) |
| Post-op: zero all 1334 PKA RAM words with EN=1 | Never reached (INITOK never fires) |

---

## Evidence of Hardware Issue

The failure pattern is consistent with a **hardware anomaly specific to this board**:

1. One genuine PASS on first power-up, then never again
2. Power cycle did not restore the passing state
3. The EN=1 erase scan appears to stall indefinitely
4. PKA_SR=0x2 (bit1 only) after timeout suggests the erase is "running" but frozen

Possible causes:
- This board's PKA peripheral has a silicon defect or requires a specific ECC initialization
  sequence not documented in RM0481
- The PKA SRAM may have developed a persistent ECC fault that blocks the erase scan
- There may be an undocumented errata for the H563 PKA that requires a different init sequence

---

## Next Steps

### Short-term (recommended)

1. **Board swap test**: Flash and run the PKA test on a second STM32H563RGT6 board (if
   available). If it passes reliably on a fresh board but not on this one, the issue is
   hardware-specific to this board instance.

2. **Check ST errata**: Review the STM32H563 errata document (DocID ES0616) for any PKA
   initialization workarounds. Specifically look for errata around PKA ECC scan, OPERRF,
   or EN toggling requirements.

3. **Increase timeout**: Raise `INITOK_TIMEOUT` significantly (e.g., 100M iterations ≈ 2.5s)
   to rule out pure timing as the cause. This can be tested without hardware changes.

4. **Use ST HAL reference**: The STM32H5 HAL PKA driver (`stm32h5xx_hal_pka.c`) may show the
   exact startup sequence ST uses, including any mandatory delays or register sequences not
   documented in the reference manual.

### Long-term

The PKA test is currently excluded from `packs/stm32h563rgt6_golden.json`. The test plan
and firmware are preserved at:
- `tests/plans/stm32h563rgt6_pka_mailbox.json`
- `firmware/targets/stm32h563rgt6_pka_mailbox/main.c`

Re-add to the golden pack once passing reliably across ≥3 consecutive GDB-reset runs.

---

## Civilization Engine Audit

查询了什么：`stm32h563rgt6`, `pka`, `HIGH_PRIORITY`  
命中了什么：无直接 PKA 相关记录  
是否复用：否（PKA 无先例）  
新增记录：无（本报告作为调查记录）  
升级资产：无
