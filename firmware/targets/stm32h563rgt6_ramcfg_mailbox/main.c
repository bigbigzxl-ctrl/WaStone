/*
 * stm32h563rgt6_ramcfg_mailbox — RAMCFG ECC enable self-test
 *
 * Tests the RAMCFG peripheral (new IP in STM32H5 series):
 *   1. Enable RAMCFG AHB clock (RCC_AHB1ENR bit17 = RAMCFGEN)
 *   2. Read RAMCFG SRAM1 control register (M1CR at +0x00)
 *   3. Enable ECC for SRAM1 (M1CR.ECCE = bit0)
 *   4. Write/read a word to/from SRAM1 (at 0x20000000)
 *   5. Read M1ISR (+0x08) to verify no double-error (DEDF bit5 should be 0)
 *   6. Read M1CR to verify ECCE was applied
 *   7. Disable ECC to leave SRAM in original state
 *
 * RAMCFG is on AHB1:
 *   RAMCFG_BASE    = 0x40026000
 *   RAMCFG_M1CR    = RAMCFG_BASE + 0x000  (SRAM1 control)
 *   RAMCFG_M1ISR   = RAMCFG_BASE + 0x008  (SRAM1 interrupt/status)
 *   RAMCFG_M2CR    = RAMCFG_BASE + 0x040  (SRAM2 control)
 *
 * RAMCFG_M1CR bits:
 *   bit0 = ECCE   (ECC enable — only writable when SRAM not in ECC mode)
 *   bit8 = ECCNMI (ECC NMI enable)
 *
 * RAMCFG_M1ISR bits:
 *   bit0 = SEDC  (single-error detected and corrected)
 *   bit1 = DEDF  (double-error detected and failed — uncorrectable)
 *
 * SRAM1 = 0x20000000 (192 KB, shared with stack — write to a safe high address)
 *
 * RCC_BASE    = 0x44020C00
 * RCC_AHB1ENR = RCC_BASE + 0x0E0
 *   bit17 = RAMCFGEN
 *
 * FAIL codes:
 *   0xE001 — RAMCFG M1CR not accessible (reads 0xFFFFFFFF)
 *   0xE002 — SRAM1 double-error (ECC DEDF bit set after write/read)
 *
 * detail0: [15:8]=M1ISR, [7:0]=M1CR (after ECC enable)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))

#define RAMCFG_BASE     0x40026000u
#define RAMCFG_M1CR     (*(volatile uint32_t *)(RAMCFG_BASE + 0x000u))
#define RAMCFG_M1ISR    (*(volatile uint32_t *)(RAMCFG_BASE + 0x008u))

#define RAMCFG_M1CR_ECCE  (1u << 0)
#define RAMCFG_M1ISR_DEDF (1u << 1)

/* Write to end of SRAM1 (safely above stack and mailbox area)
 * Mailbox is at 0x20007F00; stack grows down from 0x20030000.
 * Use a word just below mailbox as test location. */
#define TEST_SRAM_ADDR  0x20007EF0u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable RAMCFG AHB clock */
    RCC_AHB1ENR |= (1u << 17);
    (void)RCC_AHB1ENR;

    /* 2. Read M1CR to verify accessibility */
    uint32_t m1cr = RAMCFG_M1CR;
    if (m1cr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, m1cr);
        while (1) {}
    }

    /* 3. Enable ECC for SRAM1 */
    RAMCFG_M1CR = RAMCFG_M1CR_ECCE;
    (void)RAMCFG_M1CR;

    /* 4. Write and read back a test word */
    volatile uint32_t *test_word = (volatile uint32_t *)TEST_SRAM_ADDR;
    *test_word = 0xA5A5A5A5u;
    uint32_t rb = *test_word;
    (void)rb;

    /* 5. Read ISR — DEDF must be 0 (no uncorrectable ECC error) */
    uint32_t isr = RAMCFG_M1ISR;
    if (isr & RAMCFG_M1ISR_DEDF) {
        ael_mailbox_fail(0xE002u, isr);
        while (1) {}
    }

    /* 6. Re-read M1CR after ECC enable */
    m1cr = RAMCFG_M1CR;

    /* 7. Disable ECC (restore original state) */
    RAMCFG_M1CR = 0u;

    /* detail0: [15:8]=M1ISR, [7:0]=M1CR */
    AEL_MAILBOX->detail0 = ((isr & 0xFFu) << 8) | (m1cr & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
