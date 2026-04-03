# STM32H563RGT6 DTS Mailbox — Debug Report

**Date:** 2026-04-03
**Board:** STM32H563RGT6 (Cortex-M33, AHB/APB busses)
**Test:** `stm32h563rgt6_dts_mailbox`
**Final outcome:** PASS (MFREQ ≈ 677 at ~25 °C)

---

## 1. Objective

Implement a bare-metal self-test for the STM32H563 DTS (Digital Temperature Sensor) peripheral.
The test must:
1. Enable and configure the DTS using the internal LSI oscillator as the reference clock.
2. Trigger a single measurement.
3. Read back the raw frequency value (MFREQ) from DTS_DR.
4. Verify MFREQ is in a plausible range ([100, 65535]).
5. Report PASS via the AEL mailbox.

---

## 2. Register Map (DTS_BASE = 0x40008C00)

| Register | Offset | Key bits |
|----------|--------|----------|
| CFGR1    | +0x00  | bit0=TS1_EN, bit4=TS1_START, bits[19:16]=SMP_TIME, bit20=REFCLK_SEL |
| T0VALR1  | +0x08  | Factory T0 calibration frequency |
| RAMPVALR | +0x10  | TS1_RAMP_COEFF (ramp coefficient) |
| ITR1     | +0x14  | Interrupt threshold |
| DR       | +0x1C  | bits[15:0] = TS1_MFREQ (measured frequency) |
| SR       | +0x20  | bit0=ITEF, bit4=AITEF, bit15=TS1_RDY |
| ITENR    | +0x24  | bit0=TS1_ITEEN (interrupt enable, required for ITEF to latch) |
| ICIFR    | +0x28  | Write-1-to-clear flags |

RCC: `RCC_APB1HENR` (RCC_BASE + 0x0A0), bit3 = DTSEN

---

## 3. Initial Implementation and First Failure

### 3.1 First attempt — CSI reference clock

The initial implementation tried to use CSI (4 MHz internal oscillator) as the DTS reference:
- Set `REFCLK_SEL = 1` (bit20 of CFGR1) to select the high-speed reference path.
- Enabled CSI via `RCC_CR.CSION`.

**Result:** DR read 0x0000 after waiting for TS1_RDY.

**Symptom:** TS1_RDY (SR bit15) set quickly, but DR never updated from zero.

**Hypothesis at this point:** Wrong reference clock, or measurement never actually triggered.

---

### 3.2 Second attempt — Switch to LSI reference

Switched to LSI (32 kHz) reference (REFCLK_SEL = 0, the default):
- Enabled PWR backup domain access: `PWR_DBPCR |= DBP`.
- Enabled LSI: `RCC_BDCR |= LSION (bit26)`.
- Waited for `LSIRDY (bit27)`.
- Set CFGR1 = SMP_TIME=15 (max sampling), REFCLK_SEL=0.
- Set TS1_EN (bit0).
- Polled `SR & (ITEF | TS1_RDY)` — exited when TS1_RDY set.
- Read DR.

**Result:** DR still 0x0000.

**New observation:** TS1_RDY set almost immediately (≪ 1 ms). The poll exited on TS1_RDY, not ITEF.

---

### 3.3 Third attempt — Interrupt enable bit in CFGR1?

Reading the RM0481 description of CFGR1, there was a mention of an "interrupt enable for end-of-measure". Assumed this was CFGR1 bit2 (ITEIE).

Added: `DTS_CFGR1 |= (1u << 2);` before enabling TS1_EN.

**Diagnostic readback:** After writing bit2, read CFGR1 back → bit2 = 0. The write had no effect.

**Conclusion:** Bit2 of CFGR1 is reserved on this device. The write was silently ignored.

---

### 3.4 Diagnostic: Verify LSIRDY and LSI state

To rule out the reference clock as the root cause, read back `RCC_BDCR` directly:

```
BDCR readback: LSIRDY=1, LSERDY=1
```

Both LSI and LSE were running. The reference clock was confirmed operational. The problem was elsewhere.

---

### 3.5 Diagnostic: Register offset scan

To verify correct register map (guard against documentation error or wrong base address), scanned DTS registers at offsets +0x00 through +0x30:

| Offset | Expected register | Observed value |
|--------|------------------|----------------|
| +0x00  | CFGR1            | Written/read correctly |
| +0x08  | T0VALR1          | Non-zero factory value |
| +0x10  | RAMPVALR         | 0x0848 (non-zero — confirms correct base) |
| +0x14  | ITR1             | 0x0000 |
| +0x1C  | DR               | 0x0000 |
| +0x20  | SR               | TS1_RDY=1, ITEF=0 |
| +0x24  | (unknown)        | 0x0000 |
| +0x28  | (unknown)        | 0x0000 |

RAMPVALR = 0x0848 at +0x10 confirmed the base address was correct (factory data would be 0x00000000 only on a blank device).

---

### 3.6 Root cause found — CMSIS header analysis

Consulted `/tmp/stm32h563xx.h` (ST CMSIS header for H563) for definitive bit definitions.

**Discovery 1:** `TS1_START` (bit4 of CFGR1) is the trigger for a **single measurement**. It is a self-clearing bit. Writing TS1_EN=1 does NOT automatically start a measurement — it only powers the peripheral. A measurement only begins when TS1_START is explicitly written.

All previous attempts omitted `TS1_START`. The peripheral was initialised and ready (TS1_RDY=1), but never actually measuring.

**Discovery 2:** `ITENR` is a **separate register at offset +0x24**, not a field inside CFGR1. It contains `TS1_ITEEN` (bit0). This bit must be set for SR.ITEF to latch when a measurement completes. Without it, ITEF never sets regardless of whether a measurement occurred.

---

## 4. Root Causes

| # | Root cause | Effect |
|---|-----------|--------|
| 1 | `TS1_START` (CFGR1 bit4) never written | DTS powered but never measuring; DR stays 0 |
| 2 | `ITENR.TS1_ITEEN` (+0x24, bit0) not set | ITEF flag never latches; poll loop exits on TS1_RDY instead |
| 3 | Polling on `ITEF \| TS1_RDY` | Loop exits immediately on TS1_RDY (peripheral init) before any measurement |

---

## 5. Fix — Correct Initialisation Sequence

```c
/* 1. Enable DTS clock */
RCC_APB1HENR |= (1u << 3);
(void)RCC_APB1HENR;

/* 2. Enable LSI reference */
PWR_DBPCR |= PWR_DBPCR_DBP;
RCC_BDCR |= RCC_BDCR_LSION;
while (!(RCC_BDCR & RCC_BDCR_LSIRDY)) {}

/* 3. Configure DTS — NO TS1_EN yet, NO TS1_START */
DTS_CFGR1 = DTS_CFGR1_SMP_MAX;           /* SMP_TIME=15, REFCLK_SEL=0 */

/* 4. Enable ITEF latching — SEPARATE register at +0x24 */
DTS_ITENR = DTS_ITENR_TS1_ITEEN;         /* bit0 */

/* 5. Enable peripheral */
DTS_CFGR1 |= DTS_CFGR1_TS1_EN;

/* 6. Wait for TS1_RDY (means: ready to accept trigger, NOT measurement done) */
while (!(DTS_SR & DTS_SR_TS1_RDY)) {}

/* 7. Trigger one measurement */
DTS_CFGR1 |= DTS_CFGR1_TS1_START;        /* self-clearing bit */

/* 8. Wait for ITEF (end-of-measure) — NOT TS1_RDY */
while (!(DTS_SR & DTS_SR_TS1_ITEF)) {}

/* 9. Read result */
uint32_t mfreq = DTS_DR & 0xFFFFu;
```

---

## 6. Test Result

After the fix, the test produced:

```
PASS  detail0 = 677
```

MFREQ = 677, consistent with the expected value for ~25 °C room temperature with LSI = 32 kHz reference and SMP_TIME = 15.

Range check `[100, 65535]` passed.

---

## 7. Key Lessons

### 7.1 TS1_RDY ≠ measurement complete

`TS1_RDY` (SR bit15) means the DTS peripheral has completed its power-on initialization and is ready to accept a measurement trigger. It does **not** mean a measurement has been taken. Polling on `ITEF | TS1_RDY` and exiting on whichever fires first will almost always exit on `TS1_RDY` before any measurement starts.

### 7.2 TS1_START must be explicitly written every measurement

The DTS does not auto-measure on enable. Each single-shot measurement requires an explicit `TS1_START` write. This is easy to miss because the peripheral signals "ready" (TS1_RDY) without it.

### 7.3 ITENR is a separate register

The interrupt enable for ITEF (`TS1_ITEEN`) lives in the **ITENR register at DTS_BASE+0x24**, not inside CFGR1. CFGR1 bit2 is reserved and writes to it are silently discarded. Without `ITENR.TS1_ITEEN = 1`, ITEF will not latch in SR even after a successful measurement — the measurement happens silently and the flag never signals completion.

### 7.4 Readback is essential for reserved bits

Writing to reserved bits (CFGR1 bit2) silently fails. Always read back after write to confirm the value stored.

### 7.5 RAMPVALR as a sanity check

Reading RAMPVALR (+0x10) is a quick way to verify the DTS base address is correct. A non-zero value (factory programmed ramp coefficient) means the peripheral is properly clocked and the base address is right.

---

## 8. Final Firmware Location

`firmware/targets/stm32h563rgt6_dts_mailbox/main.c`

Test plan: `tests/plans/stm32h563rgt6_dts_mailbox.json`

Included in: `packs/stm32h563rgt6_golden.json`
