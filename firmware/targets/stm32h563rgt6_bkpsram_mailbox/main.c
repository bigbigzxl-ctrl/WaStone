/*
 * stm32h563rgt6_bkpsram_mailbox — Backup SRAM write/read self-test
 *
 * Backup SRAM (BKPSRAM) is 4 KB of SRAM in the backup power domain.
 * It retains data across all reset types (including system reset and
 * power-on reset) as long as VBAT is present. Similar purpose to TAMP
 * BKPxR registers but with much larger capacity (4 KB vs 32×4 B).
 *
 * Steps:
 *   1. Enable BKPSRAM AHB clock (RCC offset 0x0E0 bit28 = BKPRAMEN)
 *   2. Unlock backup domain (PWR_DBPCR.DBP = bit0)
 *   3. Write test pattern to first 4 words of BKPSRAM
 *   4. Read back and verify all 4 words
 *
 * BKPSRAM_BASE = AHB1PERIPH_BASE + 0x16400 = 0x40036400
 *
 * RCC register at offset 0x0E0 from RCC_BASE contains BKPRAMEN (bit28),
 * DCACHE1EN (bit30), RAMCFGEN (bit17) — confirmed by passing tests.
 *
 * Backup domain unlock: PWR_DBPCR at PWR_BASE+0x024 (CMSIS offset 0x24).
 *
 * FAIL codes:
 *   0xE001 — BKPSRAM word[0] readback mismatch
 *   0xE002 — BKPSRAM word[1] readback mismatch
 *   0xE003 — BKPSRAM word[2] readback mismatch
 *   0xE004 — BKPSRAM word[3] readback mismatch
 *
 * detail0: word[0] readback (expected 0xDEADBEEF)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
/* RCC register at +0x0E0 holds BKPRAMEN(28), DCACHE1EN(30), RAMCFGEN(17) */
#define RCC_AHB1ENR2    (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))

#define PWR_BASE        0x44020800u
#define PWR_DBPCR       (*(volatile uint32_t *)(PWR_BASE + 0x024u))
#define PWR_DBPCR_DBP   (1u << 0)

#define BKPSRAM_BASE    0x40036400u

static const uint32_t patterns[4] = {
    0xDEADBEEFu,
    0xCAFEBABEu,
    0xA5A5A5A5u,
    0x12345678u
};

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable BKPSRAM clock (bit28 of RCC+0x0E0) */
    RCC_AHB1ENR2 |= (1u << 28);
    (void)RCC_AHB1ENR2;

    /* 2. Unlock backup domain */
    PWR_DBPCR |= PWR_DBPCR_DBP;
    (void)PWR_DBPCR;

    /* 3. Write test patterns */
    volatile uint32_t *bkp = (volatile uint32_t *)BKPSRAM_BASE;
    for (uint32_t i = 0u; i < 4u; i++) {
        bkp[i] = patterns[i];
    }

    /* 4. Read back and verify */
    for (uint32_t i = 0u; i < 4u; i++) {
        uint32_t rb = bkp[i];
        if (rb != patterns[i]) {
            ael_mailbox_fail(0xE001u + i, rb);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = bkp[0];   /* 0xDEADBEEF */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
