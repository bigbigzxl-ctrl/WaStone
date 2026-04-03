/*
 * stm32h563rgt6_dts_mailbox — DTS digital temperature sensor self-test
 *
 * Enables the DTS, triggers one measurement via TS1_START, waits for ITEF,
 * and verifies the raw frequency value is non-zero.
 *
 * DTS (RM0481 §57, stm32h563xx.h):
 *   DTS_BASE     = APB1 + 0x8C00 = 0x40008C00
 *   RCC_APB1HENR = RCC_BASE + 0xA0, bit3 = DTSEN
 *
 * DTS registers at DTS_BASE:
 *   CFGR1  +0x00: TS1_EN bit0, TS1_START bit4, INTRIG_SEL[11:8],
 *                 TS1_SMP_TIME[19:16], REFCLK_SEL bit20,
 *                 HSREF_CLK_DIV[30:24]
 *   T0VALR1 +0x08: factory calibration freq at T0
 *   RAMPVALR +0x10: TS1_RAMP_COEFF
 *   ITR1    +0x14: interrupt threshold
 *   DR      +0x1C: TS1_MFREQ[15:0] (measured frequency)
 *   SR      +0x20: ITEF bit0, AITEF bit4, RDY bit15
 *   ITENR   +0x24: TS1_ITEEN bit0 (interrupt enable for ITEF)
 *   ICIFR   +0x28: clear flags
 *
 * TS1_START (bit4) must be written to trigger a measurement.
 * ITENR.TS1_ITEEN (bit0) must be set for SR.ITEF to latch.
 *
 * FAIL codes:
 *   0xE001 — MFREQ = 0 after measurement (not in range)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1HENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A0u))
#define RCC_BDCR        (*(volatile uint32_t *)(RCC_BASE + 0x0F0u))
#define RCC_BDCR_LSION  (1u << 26)
#define RCC_BDCR_LSIRDY (1u << 27)

/* PWR — backup domain access unlock */
#define PWR_BASE        0x44020800u
#define PWR_DBPCR       (*(volatile uint32_t *)(PWR_BASE + 0x24u))
#define PWR_DBPCR_DBP   (1u << 0)

#define DTS_BASE        0x40008C00u
#define DTS_CFGR1       (*(volatile uint32_t *)(DTS_BASE + 0x00u))
#define DTS_T0VALR1     (*(volatile uint32_t *)(DTS_BASE + 0x08u))
#define DTS_DR          (*(volatile uint32_t *)(DTS_BASE + 0x1Cu))
#define DTS_SR          (*(volatile uint32_t *)(DTS_BASE + 0x20u))
#define DTS_ITENR       (*(volatile uint32_t *)(DTS_BASE + 0x24u))
#define DTS_ICIFR       (*(volatile uint32_t *)(DTS_BASE + 0x28u))

/* CFGR1 bits */
#define DTS_CFGR1_TS1_EN    (1u << 0)
#define DTS_CFGR1_TS1_START (1u << 4)   /* trigger one measurement */
#define DTS_CFGR1_SMP_MAX   (0xFu << 16)

/* SR bits */
#define DTS_SR_TS1_ITEF  (1u << 0)   /* end-of-measure flag (needs ITEEN) */
#define DTS_SR_TS1_AITEF (1u << 4)   /* async end-of-measure (always latches) */
#define DTS_SR_TS1_RDY   (1u << 15)  /* peripheral ready */

/* ITENR bits */
#define DTS_ITENR_TS1_ITEEN  (1u << 0)  /* enable ITEF latching in SR */

/* ICIFR bits (write 1 to clear) */
#define DTS_ICIFR_TS1_CITEF  (1u << 0)

#define TIMEOUT  10000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable DTS clock (APB1H bit3) */
    RCC_APB1HENR |= (1u << 3);
    (void)RCC_APB1HENR;

    /* 2. Unlock backup domain and enable LSI (32 kHz) as DTS reference.
     *    REFCLK_SEL=0 (default) → LS clock path → LSI/LSE.
     *    LSION = bit26 of RCC_BDCR on H5.
     */
    PWR_DBPCR |= PWR_DBPCR_DBP;
    (void)PWR_DBPCR;
    RCC_BDCR |= RCC_BDCR_LSION;
    uint32_t t;
    for (t = 0; t < TIMEOUT; t++) {
        if (RCC_BDCR & RCC_BDCR_LSIRDY) break;
    }

    /* 3. Configure DTS: SMP_TIME=15 (max), REFCLK_SEL=0 (LSI).
     *    Enable ITEF latching via ITENR before setting EN.
     */
    DTS_CFGR1 = DTS_CFGR1_SMP_MAX;   /* configure (no EN, no START) */
    DTS_ITENR = DTS_ITENR_TS1_ITEEN; /* allow ITEF to latch in SR */
    DTS_CFGR1 |= DTS_CFGR1_TS1_EN;  /* enable peripheral */

    /* 4. Wait for TS1_RDY (peripheral initialised). */
    for (t = 0; t < TIMEOUT; t++) {
        if (DTS_SR & DTS_SR_TS1_RDY) break;
    }

    /* 5. Trigger one measurement via TS1_START (self-clearing bit). */
    DTS_CFGR1 |= DTS_CFGR1_TS1_START;

    /* 6. Wait for ITEF (end-of-measure) or AITEF (async variant).
     *    With LSI=32kHz, SMP=15: measurement ≈ 31/32kHz ≈ 970 µs.
     */
    for (t = 0; t < TIMEOUT; t++) {
        if (DTS_SR & (DTS_SR_TS1_ITEF | DTS_SR_TS1_AITEF)) break;
    }

    /* 7. Read MFREQ */
    uint32_t mfreq = DTS_DR & 0xFFFFu;

    /* 8. Sanity-check: MFREQ must be non-zero and plausible.
     *    With LSI=32kHz ref, measurement at ~25°C gives MFREQ≈677.
     *    Accept any value in [100, 65535].
     */
    if (mfreq < 100u || mfreq > 65535u) {
        ael_mailbox_fail(0xE001u, mfreq);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = mfreq;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
