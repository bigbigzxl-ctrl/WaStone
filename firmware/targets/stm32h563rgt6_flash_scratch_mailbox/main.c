/*
 * stm32h563rgt6_flash_scratch_mailbox — Flash sector erase + write self-test
 *
 * Tests Flash erase and program operations using the last sector as scratch space.
 * STM32H563RGT6 has 1 MB Flash (128 sectors × 8 KB each).
 * Scratch sector = sector 127 at 0x080FE000–0x080FFFFF.
 *
 * Sequence:
 *   1. Unlock Flash (write KEY1=0x45670123, KEY2=0xCDEF89AB to FLASH_KEYR)
 *   2. Wait until BSY=0 (FLASH_SR.BSY1)
 *   3. Erase sector 127: set CR.SER=1, CR.SNB=127, CR.STRT=1
 *   4. Wait BSY=0, check for errors in FLASH_SR
 *   5. Verify sector is erased (first word reads 0xFFFFFFFF)
 *   6. Write 4 words of test pattern using double-word (64-bit) writes
 *   7. Wait BSY=0 after each write
 *   8. Verify written data reads back correctly
 *   9. Lock Flash (set FLASH_CR.LOCK=1)
 *
 * FLASH_R_BASE = 0x40022000
 * Scratch sector base = 0x080FE000
 *
 * FLASH registers (H563, non-secure):
 *   FLASH_ACR    +0x000  Access Control
 *   FLASH_KEYR   +0x004  Unlock key register
 *   FLASH_CR     +0x028  Control register
 *   FLASH_SR     +0x020  Status register
 *   FLASH_CCR    +0x024  Clear status register
 *
 * FLASH_SR bits:
 *   bit0  = BSY1  (busy)
 *   bit16 = PGSERR (programming sequence error)
 *   bit17 = PGAERR (alignment error)  -- actually may differ on H5
 *   bit18 = WRPERR (write protection error)
 *   bit19 = EOP    (end of program)
 *
 * FLASH_CR bits:
 *   bit0  = LOCK
 *   bit1  = PG    (programming)
 *   bit2  = SER   (sector erase)
 *   bit3  = BER   (bank erase) — not used here
 *   bit7  = STRT  (start erase)
 *   bits[13:8] = SNB (sector number, 0–127)
 *
 * FAIL codes:
 *   0xE001 — Flash already busy at entry
 *   0xE002 — Sector erase error (SR has error flags after erase)
 *   0xE003 — Sector not erased (first word != 0xFFFFFFFF)
 *   0xE004 — Flash program error (SR has error flags after write)
 *   0xE005 — Data readback mismatch, detail0 = first mismatched word
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define FLASH_R_BASE    0x40022000u
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_R_BASE + 0x000u))
#define FLASH_KEYR      (*(volatile uint32_t *)(FLASH_R_BASE + 0x004u))
#define FLASH_SR        (*(volatile uint32_t *)(FLASH_R_BASE + 0x020u))
#define FLASH_CCR       (*(volatile uint32_t *)(FLASH_R_BASE + 0x024u))
#define FLASH_CR        (*(volatile uint32_t *)(FLASH_R_BASE + 0x028u))

#define FLASH_KEY1      0x45670123u
#define FLASH_KEY2      0xCDEF89ABu

#define FLASH_CR_LOCK   (1u << 0)
#define FLASH_CR_PG     (1u << 1)
#define FLASH_CR_SER    (1u << 2)
#define FLASH_CR_STRT   (1u << 7)
#define FLASH_CR_SNB_Pos 8u

#define FLASH_SR_BSY1   (1u << 0)
#define FLASH_SR_ERRORS (0x1Fu << 16)   /* PGSERR|PGAERR|WRPERR etc. */

#define SCRATCH_SECTOR   127u
#define SCRATCH_BASE     0x080FE000u

#define TIMEOUT 10000000u

static void flash_wait_bsy(void)
{
    for (volatile uint32_t i = 0u; i < TIMEOUT; i++) {
        if (!(FLASH_SR & FLASH_SR_BSY1)) return;
    }
}

int main(void)
{
    ael_mailbox_init();

    /* 1. Check not busy */
    if (FLASH_SR & FLASH_SR_BSY1) {
        ael_mailbox_fail(0xE001u, FLASH_SR);
        while (1) {}
    }

    /* 2. Unlock Flash */
    FLASH_KEYR = FLASH_KEY1;
    FLASH_KEYR = FLASH_KEY2;
    (void)FLASH_CR;   /* confirm LOCK is now 0 */

    /* Clear any pending errors */
    FLASH_CCR = FLASH_SR_ERRORS | (1u << 16);  /* clear all error flags */

    /* 3. Erase sector 127 */
    FLASH_CR = FLASH_CR_SER | (SCRATCH_SECTOR << FLASH_CR_SNB_Pos);
    FLASH_CR |= FLASH_CR_STRT;
    flash_wait_bsy();

    /* 4. Check for erase errors */
    uint32_t sr = FLASH_SR;
    if (sr & FLASH_SR_ERRORS) {
        FLASH_CR = FLASH_CR_LOCK;
        ael_mailbox_fail(0xE002u, sr);
        while (1) {}
    }

    /* 5. Verify sector erased */
    volatile uint32_t *scratch = (volatile uint32_t *)SCRATCH_BASE;
    if (scratch[0] != 0xFFFFFFFFu) {
        FLASH_CR = FLASH_CR_LOCK;
        ael_mailbox_fail(0xE003u, scratch[0]);
        while (1) {}
    }

    /* 6. Write 2 double-words (4 × 32-bit = 2 × 64-bit) using PG mode
     * H563 Flash programming is 128-bit (4 words at once) aligned */
    FLASH_CR = FLASH_CR_PG;

    /* Write first 128-bit word (4 × 32-bit) */
    scratch[0] = 0xDEADBEEFu;
    scratch[1] = 0xCAFEBABEu;
    scratch[2] = 0xA5A5A5A5u;
    scratch[3] = 0x12345678u;
    flash_wait_bsy();

    /* 7. Check for program errors */
    sr = FLASH_SR;
    if (sr & FLASH_SR_ERRORS) {
        FLASH_CR = FLASH_CR_LOCK;
        ael_mailbox_fail(0xE004u, sr);
        while (1) {}
    }

    /* 8. Lock Flash */
    FLASH_CR = FLASH_CR_LOCK;

    /* 9. Verify written data */
    static const uint32_t expected[4] = {
        0xDEADBEEFu, 0xCAFEBABEu, 0xA5A5A5A5u, 0x12345678u
    };
    for (uint32_t i = 0u; i < 4u; i++) {
        if (scratch[i] != expected[i]) {
            ael_mailbox_fail(0xE005u, scratch[i]);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = scratch[0];   /* 0xDEADBEEF */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
