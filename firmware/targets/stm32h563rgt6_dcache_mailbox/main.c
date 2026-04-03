/*
 * stm32h563rgt6_dcache_mailbox — DCACHE1 data cache self-test
 *
 * Tests the data cache peripheral (new on H5 vs H7/F7):
 *   1. Enable DCACHE1 AHB1 clock (RCC_AHB1ENR bit30 = DCACHE1EN)
 *   2. Verify CR is accessible (not 0xFFFFFFFF)
 *   3. Enable DCACHE1 (CR.EN = bit0)
 *   4. Enable read-hit and read-miss monitors (CR bits 16,17)
 *   5. Perform several volatile SRAM reads to generate cache activity
 *   6. Read RHMONR (read-hit count) and RMMONR (read-miss count)
 *   7. Trigger full cache invalidation (CR.CACHEINV = bit1)
 *   8. Poll SR.BSYENDF (bit1) for invalidation complete
 *   9. Disable DCACHE1 (restore state)
 *
 * DCACHE1_BASE = AHB1PERIPH_BASE + 0x11400 = 0x40031400
 * RCC_AHB1ENR  = RCC_BASE + 0x0E0, bit30 = DCACHE1EN
 *
 * DCACHE registers (offsets from DCACHE1_BASE):
 *   CR       +0x00: bit0=EN, bit1=CACHEINV, bit16=RHITMEN, bit17=RMISSMEN
 *   SR       +0x04: bit0=BUSYF, bit1=BSYENDF
 *   FCR      +0x0C: bit1=CBSYENDF (clear BSYENDF)
 *   RHMONR   +0x10: read-hit monitor count
 *   RMMONR   +0x14: read-miss monitor count
 *
 * FAIL codes:
 *   0xE001 — DCACHE1 CR inaccessible (reads 0xFFFFFFFF)
 *   0xE002 — Invalidation did not complete (BSYENDF never set)
 *
 * detail0: [31:16]=RHMONR, [15:0]=RMMONR (after cache activity)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))

#define DCACHE1_BASE    0x40031400u
#define DCACHE_CR       (*(volatile uint32_t *)(DCACHE1_BASE + 0x00u))
#define DCACHE_SR       (*(volatile uint32_t *)(DCACHE1_BASE + 0x04u))
#define DCACHE_FCR      (*(volatile uint32_t *)(DCACHE1_BASE + 0x0Cu))
#define DCACHE_RHMONR   (*(volatile uint32_t *)(DCACHE1_BASE + 0x10u))
#define DCACHE_RMMONR   (*(volatile uint32_t *)(DCACHE1_BASE + 0x14u))

#define DCACHE_CR_EN        (1u << 0)
#define DCACHE_CR_CACHEINV  (1u << 1)
#define DCACHE_CR_RHITMEN   (1u << 16)
#define DCACHE_CR_RMISSMEN  (1u << 17)
#define DCACHE_CR_RHITMRST  (1u << 18)
#define DCACHE_CR_RMISSMRST (1u << 19)
#define DCACHE_SR_BSYENDF   (1u << 1)
#define DCACHE_FCR_CBSYENDF (1u << 1)

#define TIMEOUT 1000000u

/* Use a small array in SRAM to generate cache reads */
static volatile uint32_t test_buf[16];

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable DCACHE1 clock (AHB1ENR bit30) */
    RCC_AHB1ENR |= (1u << 30);
    (void)RCC_AHB1ENR;

    /* 2. Check accessibility */
    uint32_t cr = DCACHE_CR;
    if (cr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, cr);
        while (1) {}
    }

    /* 3. Enable DCACHE + monitors (reset monitor counters first) */
    DCACHE_CR = DCACHE_CR_EN | DCACHE_CR_RHITMEN | DCACHE_CR_RMISSMEN
              | DCACHE_CR_RHITMRST | DCACHE_CR_RMISSMRST;
    (void)DCACHE_CR;

    /* 4. Write test data to SRAM (cache-line fill) */
    for (uint32_t i = 0u; i < 16u; i++) {
        test_buf[i] = i * 0x11111111u;
    }

    /* 5. Read same data multiple times to generate hits */
    volatile uint32_t sink = 0u;
    for (uint32_t r = 0u; r < 4u; r++) {
        for (uint32_t i = 0u; i < 16u; i++) {
            sink += test_buf[i];
        }
    }
    (void)sink;

    /* 6. Read monitor counters */
    uint32_t rhit  = DCACHE_RHMONR;
    uint32_t rmiss = DCACHE_RMMONR;

    /* 7. Trigger full cache invalidation */
    DCACHE_FCR = DCACHE_FCR_CBSYENDF;       /* clear pending flag first */
    DCACHE_CR |= DCACHE_CR_CACHEINV;

    /* 8. Wait for BSYENDF */
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (DCACHE_SR & DCACHE_SR_BSYENDF) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE002u, DCACHE_SR);
        while (1) {}
    }

    /* 9. Disable DCACHE (restore state) */
    DCACHE_CR = 0u;

    /* detail0: [31:16]=rhit, [15:0]=rmiss */
    AEL_MAILBOX->detail0 = ((rhit & 0xFFFFu) << 16) | (rmiss & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
