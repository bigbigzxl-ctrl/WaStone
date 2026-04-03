# STM32H563RGT6 FDCAN Mailbox Debug Report
**Date:** 2026-04-03  
**Test:** `stm32h563rgt6_fdcan_mailbox`  
**Peripheral:** FDCAN1 internal loopback  
**Final Result:** PASS (after 2 major iteration rounds)

---

## Objective

Implement a bare-metal FDCAN1 self-test for the STM32H563RGT6 using hardware loopback mode (CCCR.TEST=1, TEST.LBCK=1). The test sends a classic CAN 2.0 frame with 11-bit standard ID=0x123, DLC=4, data={0x11,0x22,0x33,0x44} and verifies the looped-back data in Rx FIFO0.

No external CAN transceiver or bus wiring required — pure internal loopback inside the FDCAN IP.

---

## Initial Implementation (Round 1)

### Approach

The initial firmware was written assuming the STM32H563 FDCAN peripheral behaves like the H7's FDCAN. This was a mistake. The key registers used were:

```c
// Assumed to exist — DOES NOT in H563:
RXF0C at FDCAN1_BASE + 0x0A0  // Rx FIFO0 Configuration
SIDFC at FDCAN1_BASE + 0x???  // Standard ID Filter Configuration

// Assumed message RAM layout (wrong — taken from H7 docs):
#define FLSSA_OFFSET  0x000u   // standard filter area (words)
#define RXF0_OFFSET   0x01Cu   // Rx FIFO0 area start (words) — WRONG
#define TXBUF_OFFSET  0x031u   // Tx buffer start (words) — WRONG

// Wrong TXBC write:
FDCAN_TXBC = (1u << 24) | (TXBUF_OFFSET);   // (1<<24) is TFQM, not buffer count

// Wrong IR bit:
#define FDCAN_IR_TC   (1u << 9)   // WRONG — bit9 is TCF, not TC

// Wrong RXGFC:
#define FDCAN_RXGFC_VAL  ((2u<<2) | (2u<<0))  // WRONG — ANFS at [5:4] not [1:0]
```

### Outcome

`FAIL 0xE002` — Tx timeout. IR=0 at timeout (TC never fired).

`detail0 = 0x00000000` — nothing appeared in IR at all.

---

## Diagnosis and Root Cause Analysis

### Problem 1 — Wrong IR.TC bit (most critical)

The first and most impactful bug: the code polled bit 9 of the IR register for TC (Transmission Complete), but on STM32H563 the TC flag is at **bit 7**.

```
FDCAN_IR_TC_Pos = 7U   →  (1u << 7) = 0x80
FDCAN_IR_TCF_Pos = 8U  →  (1u << 8) = 0x100   // Tx Cancellation Finished
```

Bit 9 is ECF (Error Counter overflow), not TC. This meant the polling loop always timed out regardless of whether transmission actually completed.

**Confirmed by:** searching FDCAN_IR bit definitions in `/tmp/stm32h563xx.h` (CMSIS header).

### Problem 2 — H563 FDCAN has a fixed/simplified message RAM (no RXF0C/SIDFC)

The STM32H563 uses a simplified "FDCAN lite" peripheral that differs substantially from the H7 FDCAN:

| Feature | H7 FDCAN | H563 FDCAN |
|---------|----------|------------|
| `SIDFC` register | Yes (configures std filter list base) | **No** |
| `XIDFC` register | Yes | **No** |
| `RXF0C` register | Yes (configures Rx FIFO0 base + size) | **No** (0x0A0 is RESERVED) |
| `TXBC.NDTB` field | Yes (number of dedicated Tx buffers) | **No** |
| `TXBC.TBSA` field | Yes (Tx buffer start address) | **No** |
| `RXESC`/`TXESC` | Yes (configures element data size) | **No** |
| `TXBC.TFQM` | Yes | Yes (bit24 only) |

**The H563 FDCAN message RAM layout is completely fixed:**

| Region | Byte Offset | Size |
|--------|-------------|------|
| Standard ID filter list | `0x000` | 28 × 4 B = 112 B |
| Extended ID filter list | `0x070` | 8 × 8 B = 64 B |
| Rx FIFO0 (RF0SA) | **`0x0B0`** | 3 × 72 B = 216 B |
| Rx FIFO1 | `0x188` | 3 × 72 B = 216 B |
| Tx Event FIFO | `0x260` | 3 × 8 B = 24 B |
| Tx FIFO/Queue (TFQSA) | **`0x278`** | 3 × 72 B = 216 B |

Each Rx/Tx element is always **18 words = 72 bytes** (FD-capable size), regardless of the actual DLC or frame format.

The initial code wrote the Tx element to `SRAMCAN_BASE + 0x31 * 4 = SRAMCAN_BASE + 0xC4`, which falls in the middle of the Rx FIFO0 region. The Rx element was read from `SRAMCAN_BASE + 0x70 + idx * 16`, which mapped to the extended filter list area. Both addresses were wrong.

**Source confirmed:** `stm32h5xx_hal_fdcan.c` from STMicroelectronics/stm32h5xx_hal_driver GitHub repository, which defines:
```c
#define SRAMCAN_FLSSA  ((uint32_t)0)
#define SRAMCAN_FLESA  ((uint32_t)(0 + 28*4))          // = 0x70
#define SRAMCAN_RF0SA  ((uint32_t)(0x70 + 8*8))        // = 0x0B0
#define SRAMCAN_RF1SA  ((uint32_t)(0x0B0 + 3*72))      // = 0x188
#define SRAMCAN_TEFSA  ((uint32_t)(0x188 + 3*72))      // = 0x260
#define SRAMCAN_TFQSA  ((uint32_t)(0x260 + 3*8))       // = 0x278
```

### Problem 3 — Wrong RXGFC ANFS bit position

The `RXGFC` register controls how non-matching frames are handled:

```
FDCAN_RXGFC_ANFE_Pos = 2U   →  bits[3:2]  (non-matching extended)
FDCAN_RXGFC_ANFS_Pos = 4U   →  bits[5:4]  (non-matching standard)
```

The initial code:
```c
#define FDCAN_RXGFC_VAL  ((2u<<2) | (2u<<0))   // WRONG: 2u<<0 hits bits[1:0]
```

Bits[1:0] of RXGFC are `RRFE` (Reject Remote Frames Extended) and `RRFS` (Reject Remote Frames Standard). Writing `2` there is setting a nonsensical value. The `ANFS` field was effectively 0 = **reject all non-matching standard frames**.

Since the test uses a standard 11-bit ID (0x123) with no matching filter entry, ANFS=0 meant every incoming frame was rejected instead of stored in Rx FIFO0. Even if transmission had completed, no data would have appeared in FIFO0.

The correct value:
```c
#define FDCAN_RXGFC_VAL  ((2u<<2) | (2u<<4))   // ANFE=2→FIFO0, ANFS=2→FIFO0
```

### Problem 4 — Spurious write to reserved register 0x0A0

The initial code wrote to `FDCAN1_BASE + 0x0A0`, expecting a `RXF0C` register. In H563, offset 0x0A0 is part of `RESERVED6` (0x0A0–0x0BC block). The write was silently ignored.

---

## Round 2 — Complete Rewrite

### Changes made

1. **IR_TC bit corrected:** `(1u << 7)` instead of `(1u << 9)`.

2. **TXBC set to 0:** No fields to write for H563 other than TFQM=0 (dedicated buffer mode).

3. **Tx element written to correct address:**
   ```c
   volatile uint32_t *tx = (volatile uint32_t *)(SRAMCAN_BASE + 0x278u);
   tx[0] = (0x123u << 18);   // T0: std 11-bit ID
   tx[1] = (4u << 16);       // T1: DLC=4
   tx[2] = 0x44332211u;      // data
   ```

4. **Rx element read from correct address:**
   ```c
   uint32_t get_idx = (rxf0s >> 8) & 0x3u;   // F0GI field
   volatile uint32_t *rx = (volatile uint32_t *)
       (SRAMCAN_BASE + 0x0B0u + get_idx * 72u);
   uint32_t rx_data = rx[2];   // data word at element offset +8
   ```

5. **RXGFC corrected:** `(2u<<2) | (2u<<4)`.

6. **Removed dead write** to `FDCAN1_BASE + 0x0A0` (reserved).

### Initialization sequence (final, correct)

```c
FDCAN_CCCR = FDCAN_CCCR_INIT;            // request init mode
// wait CCCR.INIT=1
FDCAN_CCCR |= FDCAN_CCCR_CCE;            // enable config access
// wait INIT+CCE both set
FDCAN_CCCR |= FDCAN_CCCR_TEST;           // unlock TEST register
FDCAN_TEST  = FDCAN_TEST_LBCK;           // internal loopback
FDCAN_NBTP  = 0x06030E07u;               // 250 kbit/s @ 64 MHz
FDCAN_RXGFC = (2u<<2) | (2u<<4);         // accept all → FIFO0
FDCAN_TXBC  = 0u;                         // dedicated mode, no addr config needed
FDCAN_CCCR &= ~FDCAN_CCCR_INIT;          // exit init
```

### Result

**PASS** — `detail0 = 0x44332211` (received data matches transmitted data).

---

## Key Lessons

### Lesson 1 — H563 FDCAN is NOT the same as H7 FDCAN
The H563 uses a simplified FDCAN peripheral with fixed message RAM layout. Any code ported from H7 examples must be rewritten. Do not use RXF0C, SIDFC, XIDFC, RXESC, or TXESC — they do not exist.

### Lesson 2 — Always verify IR bit positions from CMSIS, not documentation examples
The IR.TC bit is at position 7 on H563, not position 9. The CMSIS header `stm32h563xx.h` is the authoritative source:
```c
#define FDCAN_IR_TC_Pos  7U   →  (1u << 7) = 0x80
```

### Lesson 3 — H563 FDCAN message RAM layout comes from HAL source
The ST HAL driver (`stm32h5xx_hal_fdcan.c`) defines the fixed offsets explicitly. These are the only authoritative source for H563 message RAM layout since no `RXF0C`-style registers exist.

### Lesson 4 — RXGFC ANFE and ANFS have different bit positions
Despite their similar names, ANFE (extended) is at bits[3:2] while ANFS (standard) is at bits[5:4]. A shift of `<<0` when meaning `<<4` causes all standard frames to be rejected.

---

## Civilization Engine Audit

**查询了什么:** `stm32h563rgt6`, `fdcan`, `HIGH_PRIORITY`  
**命中了什么:** 无 FDCAN 专项记录  
**是否复用:** 否（无已有 pattern）  
**新增记录:** `584ea0e9` — `[HIGH_PRIORITY] STM32H563 FDCAN lite: fixed message RAM layout + IR_TC=bit7` (scope=pattern)  
**升级资产:** `584ea0e9` 升级为 scope='pattern' — 适用于所有未来 H563/H5 FDCAN 开发
