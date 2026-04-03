# STM32H563RGT6 Zero-Wiring Tests — CRS / TAMP / FLASH_OPTSR / RAMCFG
**Date:** 2026-04-03  
**Tests:** `stm32h563rgt6_crs_mailbox`, `stm32h563rgt6_tamp_mailbox`, `stm32h563rgt6_flash_optsr_mailbox`, `stm32h563rgt6_ramcfg_mailbox`  
**Final Result:** All 4 PASS (first run, no rework required)

---

## Background and Motivation

After the STM32H563RGT6 golden pack reached 28 tests, a coverage audit was performed to identify which STM32H5-specific peripherals were not yet validated. Four peripherals were identified as high-value, zero-wiring candidates:

| Priority | Peripheral | Why high value |
|----------|-----------|----------------|
| 1 | **CRS** (Clock Recovery System) | H5-family exclusive; validates HSI48 oscillator + factory calibration |
| 2 | **TAMP** (Tamper + Backup registers) | Backup power domain; BKPxR survive resets; RTC companion |
| 3 | **FLASH OPTSR** | H563-specific option byte format (OPTSR_CUR replaces OPTR); reads production state |
| 4 | **RAMCFG** | New IP introduced in H5; controls SRAM ECC — not present on any earlier STM32 |

All four tests require zero external wiring (pure self-test through register read/write). This was the primary selection criterion: the bench wiring was already fixed for the loopback suite and should not be disturbed.

---

## Infrastructure Setup

Before writing firmware, build scaffolding was created for all four targets by copying the Makefile, startup.c, and linker script from `stm32h563rgt6_rtc_mailbox` (an established, working target with identical toolchain and memory map):

```
firmware/targets/stm32h563rgt6_crs_mailbox/
firmware/targets/stm32h563rgt6_tamp_mailbox/
firmware/targets/stm32h563rgt6_flash_optsr_mailbox/
firmware/targets/stm32h563rgt6_ramcfg_mailbox/
```

Each directory received:
- `Makefile` — identical to rtc_mailbox, only `TARGET` variable changed
- `startup.c` — standard Cortex-M33 vector table + HardFault_Handler (mandatory per AEL pattern `3f13ca66`)
- `stm32h563rgt6.ld` — 512 KB Flash, 320 KB SRAM, mailbox at `0x20007F00`

Test plan JSONs were also created for each test under `tests/plans/`, templated from `stm32h563rgt6_rtc_mailbox.json`:
- `settle_s: 3.0` (standard H563 settle window)
- `addr: "0x20007F00"` (AEL mailbox address)
- `skip_attach: true`, `halt_before_read: false` (standard for pyocd-based DAPLink flow)
- `preflight.enabled: false` (no instrument preflight needed — DAPLink is local USB)

---

## Test 1 — CRS (Clock Recovery System)

### What is CRS?

CRS is a peripheral unique to certain STM32 families (H5, G0, G4, L5, U5) that trims the HSI48 48 MHz RC oscillator to a reference clock source (USB SOF, LSE, or GPIO). The purpose is to keep HSI48 within ±500 ppm — necessary for USB full-speed operation without an external crystal.

On STM32H563:
- `CRS_BASE = 0x40006000` (APB1 + 0x6000)
- `RCC_CRRCR` (offset +0x14 from RCC_BASE) holds the 10-bit `HSI48CAL` factory calibration value
- `CRS_CR` holds a 6-bit `TRIM` field (bits[13:8]) — the active trim applied at runtime

### Experiment Design

The test validates three things:
1. **HSI48 oscillator is functional** — enable it and wait for `HSI48RDY` (RCC_CR bit13)
2. **Factory calibration is present** — `CRS_CR.TRIM` (bits[13:8]) must be non-zero
3. **HSI48 calibration register is accessible** — `RCC_CRRCR` bits[9:0] = `HSI48CAL` must be non-zero

Step 5 additionally enables the CRS synchronisation counter (`CRS_CR.CEN = bit5`) and reads the ISR register to verify the peripheral is clocked.

```
detail0 = (ISR[7:0] << 24) | (TRIM << 16) | HSI48CAL
```

**FAIL codes:**
- `0xE001` — HSI48 not ready after 1,000,000 iterations
- `0xE002` — TRIM = 0 (CRS peripheral not clocked, or factory cal absent)
- `0xE003` — HSI48CAL = 0 (RCC_CRRCR not accessible / HSI48 not running)

### Key Register Details

The most non-obvious aspect is the clock enable sequence. CRS is on APB1, so:
```c
RCC_APB1LENR |= (1u << 24);   // CRSEN = bit24
```
This is `RCC_APB1LENR` (the Low half of APB1ENR), not `APB1HENR`. Getting this wrong would leave `CRS_CR` reading 0x00000000 and TRIM = 0, triggering `0xE002`.

The RCC_CRRCR offset is `+0x014` from `RCC_BASE = 0x44020C00`. This is distinct from the standard H7 layout; it was confirmed via STM32H563 reference manual section "RCC register map."

### Result: PASS (first run)

No rework required. `detail0` reports non-zero TRIM and HSI48CAL values, confirming factory calibration is intact and CRS peripheral is accessible.

---

## Test 2 — TAMP Backup Registers (BKPxR)

### What are TAMP Backup Registers?

The TAMP (Tamper and Backup) peripheral manages 32 general-purpose 32-bit backup registers (`BKP0R`–`BKP31R`). These registers are in the backup power domain and retain their values across system resets as long as `VBAT` or `VDD` remains powered. They are commonly used to store RTC calibration data, boot counters, or persistent flags.

On STM32H563:
- `TAMP_BASE = 0x44007C00`
- `BKP0R` at `TAMP_BASE + 0x100`
- `BKP1R` at `TAMP_BASE + 0x104`

### Experiment Design

The backup domain is write-protected by default. Two unlock steps are required:
1. Enable the RTC/TAMP APB clock: `RCC_APB3ENR |= (1u << 21)` (RTCAPBEN)
2. Disable backup domain write protection: `PWR_DBPCR |= (1u << 0)` (DBP bit)

Without step 2, writes to `BKPxR` are silently ignored — the registers read back as 0 regardless.

The test pattern chosen:
- `BKP0R = 0xDEADBEEF`
- `BKP1R = 0x12345678`

These patterns are distinguishable, non-trivial, and immediately recognisable in a mailbox dump. After writing, both are read back and compared.

```
detail0 = BKP0R readback (expected: 0xDEADBEEF)
```

**FAIL codes:**
- `0xE001` — BKP0R readback mismatch (detail0 = actual value)
- `0xE002` — BKP1R readback mismatch (detail0 = actual value)

### PWR_DBPCR Address

`PWR_BASE = 0x44020800`, `PWR_DBPCR` at offset `+0x00C`.

This was verified against the STM32H5 reference manual. On earlier STM32 families (F4, F7) this register is called `PWR_CR1` with a different bit position. Getting the address wrong would cause silent failure (writes ignored, readback = 0 → `0xE001`).

### Result: PASS (first run, with one DAPLink transient)

The DAPLink logged `Flash: attempt 1 (normal) -> FAIL` but recovered automatically on retry. This is a known DAPLink transient behavior during sequential batch flashing — not a firmware issue. The test itself passed on first attempt.

---

## Test 3 — FLASH OPTSR (Option Status Register)

### Background: H563 uses OPTSR, not OPTR

A critical difference from earlier STM32 families: the STM32H563 does **not** have the traditional `FLASH_OPTR` register. Instead it uses:
- `FLASH_OPTSR_CUR` at `FLASH_R_BASE + 0x050` — current (active) option byte status
- `FLASH_OPTSR_PRG` at `FLASH_R_BASE + 0x054` — programmed (pending) option bytes
- `FLASH_OPTSR2_CUR` at `FLASH_R_BASE + 0x070` — second option status register (TZEN etc.)

`FLASH_R_BASE = 0x40022000` on H563.

This distinction matters because any code ported from STM32F4/F7/H7 that reads `FLASH_OPTR` would be reading a wrong or reserved address. There is no `OPTR` register on H563.

### What does OPTSR_CUR contain?

The most important field is `PRODUCT_STATE` at bits[15:8]:

| Value | Meaning |
|-------|---------|
| `0x17` | Open (equivalent to RDP Level 0 — fully unlocked) |
| `0x51` | Provisioned |
| `0x6F` | iRoT Provisioned |
| `0xB4` | Locked (equivalent to RDP Level 1) |
| `0x8F` | **Closed** (equivalent to RDP Level 2 — permanently locked, flash unreadable) |

The test reads `OPTSR_CUR`, checks accessibility (non-zero), then checks `PRODUCT_STATE ≠ 0x8F`. A Closed device would be unrecoverable in the field and would typically prevent firmware flashing entirely — so this check is a production safety guard.

`OPTSR2_CUR` bits[31:24] hold `TZEN` (TrustZone enable, `0xB4` = enabled). This is captured in the upper half of `detail0`.

### Experiment Design

```c
uint32_t optsr  = FLASH_OPTSR_CUR;    // @ 0x40022050
uint32_t optsr2 = FLASH_OPTSR2_CUR;   // @ 0x40022070

// Fail if register unreadable
if (optsr == 0u) → 0xE001

// Fail if device is Closed (permanently locked)
if ((optsr >> 8) & 0xFF == 0x8F) → 0xE002

// detail0: upper half from OPTSR2, lower half from OPTSR
detail0 = (optsr2 & 0xFFFF0000) | (optsr & 0xFFFF)
```

No clock enable is required — the Flash controller is always accessible.

**FAIL codes:**
- `0xE001` — OPTSR_CUR reads 0 (Flash controller inaccessible)
- `0xE002` — PRODUCT_STATE = 0x8F (device is Closed)

### Result: PASS (first run)

The development board under test is in `Open` state (PRODUCT_STATE = 0x17), as expected. `detail0` encodes both OPTSR registers, giving a snapshot of the device's security configuration.

---

## Test 4 — RAMCFG (RAM Configuration)

### What is RAMCFG?

RAMCFG is a new IP block introduced in the STM32H5 series (not present on H7, F7, or any earlier family). It provides hardware ECC (Error Correcting Code) control for the internal SRAMs:
- `RAMCFG_M1CR` / `RAMCFG_M1ISR` — SRAM1 control/status
- `RAMCFG_M2CR` / `RAMCFG_M2ISR` — SRAM2 control/status
- `RAMCFG_M3CR` / `RAMCFG_M3ISR` — SRAM3 control/status

`RAMCFG_BASE = 0x40026000` (AHB1 peripheral).

The ECC mechanism can detect and correct single-bit errors (SEDC, reported in ISR bit0) and detect (but not correct) double-bit errors (DEDF, ISR bit1). A DEDF generates a bus fault.

### RCC_AHB1ENR Bit Position

RAMCFG requires its AHB1 clock to be enabled: `RCC_AHB1ENR |= (1u << 17)` (RAMCFGEN = bit17). `RCC_AHB1ENR` is at `RCC_BASE + 0x0E0`.

If this bit is wrong, reading `RAMCFG_M1CR` would return `0xFFFFFFFF` (AHB bus error response) — the test checks for this explicitly as `0xE001`.

### Experiment Design

The test performs:
1. Enable RAMCFG clock (AHB1ENR bit17)
2. Read `M1CR` — verify it's not `0xFFFFFFFF` (bus fault indicator)
3. Enable ECC: `RAMCFG_M1CR = 0x1` (ECCE bit0)
4. Write `0xA5A5A5A5` to a test word in SRAM1 (`0x20007EF0` — just below the mailbox at `0x20007F00`)
5. Read the word back
6. Read `M1ISR` — DEDF (bit1) must be 0
7. Re-read `M1CR` to capture the enabled state
8. Disable ECC (`M1CR = 0`) to restore original state

```
detail0 = (M1ISR[7:0] << 8) | M1CR[7:0]
```

**FAIL codes:**
- `0xE001` — M1CR = 0xFFFFFFFF (RAMCFG AHB clock not enabled or bus error)
- `0xE002` — DEDF bit set in M1ISR (double-bit ECC error detected after write)

### Test Word Address Selection

The test word was placed at `0x20007EF0` — 16 bytes below the AEL mailbox (`0x20007F00`). This was chosen deliberately:
- Above the stack (grows down from `0x20030000`)
- Below the mailbox (avoids overwriting the result)
- Within SRAM1 (`0x20000000`–`0x2002FFFF`)

The stack pointer at firmware entry is at the top of SRAM and grows downward; the mailbox is a fixed structure at the top of SRAM1. Writing to `0x20007EF0` is safe from both directions.

### Result: PASS (first run)

`M1ISR` shows no ECC errors, confirming SRAM1 hardware is functional with ECC enabled. ECC was restored to disabled state after the test to avoid interfering with subsequent runs.

---

## Common Debug: `ael run` Instrument Resolution Issue

During the first attempt to run the tests, an unexpected failure occurred at the `preflight` stage:

```
Preflight: post-fail ping 192.168.2.98 -> FAIL
Preflight: LA self-test error: ... No route to host (192.168.2.98)
```

This was puzzling because the STM32H563RGT6 uses a local DAPLink (USB, `127.0.0.1:3333`) — not a WiFi instrument. The root cause: calling `ael run --test <name> --board <board>` without specifying `--controller` caused AEL to fall back to a "legacy shared probe config" that resolved to the wrong instrument (the ESP32JTAG at 192.168.2.98).

The board config (`configs/boards/stm32h563rgt6.yaml`) does declare `instrument_instance: daplink_h563rgt6_local`, but this was not being picked up when only `--board` was given without the explicit `--controller` argument.

**Fix:** Always provide the full path to the instrument instance config:
```bash
python3 -m ael run \
  --test stm32h563rgt6_crs_mailbox \
  --board stm32h563rgt6 \
  --controller configs/instrument_instances/daplink_h563rgt6_local.yaml
```

With this argument, AEL correctly reports:
```
Using control instrument instance: daplink_h563rgt6_local (daplink) @ 127.0.0.1:3333
```

This is not a firmware issue — all four firmware files were correct on the first compilation. The only problem was in the run invocation.

---

## Summary Table

| Test | Peripheral | FAIL codes defined | First run result | detail0 semantics |
|------|-----------|-------------------|-----------------|-------------------|
| `crs_mailbox` | CRS + HSI48 | 0xE001–0xE003 | **PASS** | [ISR<<24 \| TRIM<<16 \| HSI48CAL] |
| `tamp_mailbox` | TAMP BKP0R/BKP1R | 0xE001–0xE002 | **PASS** | BKP0R readback (0xDEADBEEF) |
| `flash_optsr_mailbox` | FLASH OPTSR_CUR | 0xE001–0xE002 | **PASS** | [OPTSR2[31:16] \| OPTSR[15:0]] |
| `ramcfg_mailbox` | RAMCFG SRAM1 ECC | 0xE001–0xE002 | **PASS** | [M1ISR<<8 \| M1CR] |

All four tests pass on first hardware run with no firmware rework. The only issue encountered was the `ael run` invocation missing `--controller`.

---

## H563-Specific Gotchas Documented

### CRS
- Clock enable is `RCC_APB1LENR bit24` (CRSEN) — **not** APB1HENR
- `RCC_CRRCR` is at `RCC_BASE + 0x014` (different from H7 layout)
- CRS is absent on all STM32F/L/G families except those with USB FS + HSI48

### TAMP
- Backup domain write protection is **on by default** — `PWR_DBPCR.DBP` must be set first
- `PWR_DBPCR` is at `PWR_BASE + 0x00C` (different from F4/F7 `PWR_CR1`)
- Clock enable is `RCC_APB3ENR bit21` (RTCAPBEN) — same bit as RTC, since TAMP shares the APB3 clock

### FLASH_OPTSR
- H563 has **no `FLASH_OPTR`** — use `FLASH_OPTSR_CUR` at `+0x050`
- `PRODUCT_STATE` field replaces the traditional `RDP[1:0]` field — it's 8 bits at [15:8], not 2 bits
- `0x8F` = Closed = equivalent to RDP Level 2 — device is permanently locked

### RAMCFG
- RAMCFG is AHB1, clock bit is `RCC_AHB1ENR bit17` (RAMCFGEN)
- `RCC_AHB1ENR` is at `RCC_BASE + 0x0E0`
- Not present on any STM32 family before H5 — no equivalent IP on H7/F7
- ECC enable/disable is one bit (`M1CR.ECCE = bit0`)
- DEDF (double-error) in M1ISR would indicate a real hardware fault in SRAM

---

## Civilization Engine Audit

**查询了什么:** `stm32h563rgt6`, `CRS`, `TAMP`, `RAMCFG`, `OPTSR`  
**命中了什么:** 无专项记录  
**是否复用:** 否（全新外设）  
**新增记录:**
- `672492ce` — stm32h563rgt6_crs_mailbox PASS (scope=board_family)
- `e466f63f` — stm32h563rgt6_tamp_mailbox PASS (scope=board_family)
- `460788c7` — stm32h563rgt6_flash_optsr_mailbox PASS (scope=board_family)
- `d09e576c` — stm32h563rgt6_ramcfg_mailbox PASS (scope=board_family)  

**升级资产:** 无（单板首次运行，尚不满足 pattern 升级条件）
