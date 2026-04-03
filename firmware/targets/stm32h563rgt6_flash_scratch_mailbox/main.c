/*
 * stm32h563rgt6_flash_scratch_mailbox — Flash sector erase + write self-test
 *
 * Tests Flash erase and program operations using the last sector as scratch space.
 * STM32H563RGT6 has 1 MB Flash in DUAL-BANK mode: 2 banks × 64 sectors × 8 KB.
 * Scratch sector = bank 2 sector 63 at 0x080FE000–0x080FFFFF.
 * In dual-bank mode, SNB field (bits[12:6]) is LOCAL sector number (0-63),
 * and FLASH_CR.BKSEL (bit31) selects bank 0 or bank 1.
 *
 * Sequence:
 *   1. Unlock Flash (write KEY1=0x45670123, KEY2=0xCDEF89AB to FLASH_KEYR)
 *   2. Wait until BSY=0 (FLASH_SR.BSY)
 *   3. Erase bank2 sector 63: set CR.SER=1, CR.SNB=63, CR.BKSEL=1, CR.STRT=1
 *   4. Wait BSY=0, check for errors in FLASH_SR
 *   5. Verify sector is erased (first word reads 0xFFFFFFFF)
 *   6. Write 4 words of test pattern using 128-bit writes (4 words at once)
 *   7. Wait BSY=0 after write
 *   8. Verify written data reads back correctly
 *   9. Lock Flash (set FLASH_CR.LOCK=1)
 *
 * FLASH_R_BASE = 0x40022000
 * Scratch sector base = 0x080FE000
 *
 * FLASH registers (H563, non-secure):
 *   FLASH_ACR    +0x000  Access Control
 *   FLASH_NSKEYR +0x004  Non-Secure Key Register (NSKEYR)
 *   FLASH_NSSR   +0x020  Non-Secure Status Register
 *   FLASH_NSCR   +0x028  Non-Secure Control Register
 *   FLASH_NSCCR  +0x030  Non-Secure Clear Control Register (NOT 0x024 which is SECSR)
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
 *   bit5  = STRT  (start erase) — CMSIS confirmed
 *   bits[12:6] = SNB (sector number, 0–63 within selected bank) — CMSIS confirmed
 *   bit31 = BKSEL (bank select: 0=bank1, 1=bank2)
 *
 * FAIL codes:
 *   0xE001 — Unlock failed (LOCK still set after keys)
 *   0xE002 — Sector erase error (SR has error flags after erase)
 *   0xE003 — Sector not erased (first word != 0xFFFFFFFF)
 *   0xE004 — Flash program error (SR has error flags after write)
 *   0xE005 — Data readback mismatch, detail0 = first mismatched word
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define FLASH_R_BASE    0x40022000u
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_R_BASE + 0x000u))
#define FLASH_KEYR      (*(volatile uint32_t *)(FLASH_R_BASE + 0x004u))  /* NSKEYR */
#define FLASH_SR        (*(volatile uint32_t *)(FLASH_R_BASE + 0x020u))  /* NSSR */
#define FLASH_CR        (*(volatile uint32_t *)(FLASH_R_BASE + 0x028u))  /* NSCR */
#define FLASH_CCR       (*(volatile uint32_t *)(FLASH_R_BASE + 0x030u))  /* NSCCR (NOT 0x024 = SECSR) */

#define FLASH_KEY1      0x45670123u
#define FLASH_KEY2      0xCDEF89ABu

#define FLASH_CR_LOCK   (1u << 0)
#define FLASH_CR_PG     (1u << 1)
#define FLASH_CR_SER    (1u << 2)
#define FLASH_CR_STRT   (1u << 5)      /* CMSIS: START at bit5, not bit7 */
#define FLASH_CR_SNB_Pos 6u             /* CMSIS: SNB at bits[12:6], LOCAL sector within bank */
#define FLASH_CR_BKSEL  (1u << 31)     /* Bank selector: 0=bank1, 1=bank2 */

#define FLASH_SR_BSY1   (1u << 0)
#define FLASH_SR_ERRORS (0x7Fu << 17)  /* WRPERR|PGSERR|STRBERR|INCERR|OBKERR|OBKWERR|OPTCHANGEERR (bits17-23) */

/* H563 dual-bank: bank2 sector 63 = global sector 127 = 0x080FE000 */
#define SCRATCH_SECTOR   63u
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

    /* 1. Wait until not busy (in case a previous operation is completing) */
    flash_wait_bsy();

    /* 2. Unlock Flash if still locked */
    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
        (void)FLASH_SR;  /* AHB read barrier: ensure key writes are committed */
        if (FLASH_CR & FLASH_CR_LOCK) {
            /* Unlock failed — report FLASH_CR value as diagnostic */
            ael_mailbox_fail(0xE001u, FLASH_CR);
            while (1) {}
        }
    }

    /* Clear any pending status flags (EOP bit16 + errors bits17-23)
     * NSCCR is not protected by LOCK so this is always safe. */
    FLASH_CCR = (0xFFu << 16);

    /* 3. Erase bank2 sector 63 (global sector 127): single atomic write.
     *    H563 dual-bank: BKSEL=1 selects bank2, SNB=63 is sector within bank2. */
    FLASH_CR = FLASH_CR_SER | (SCRATCH_SECTOR << FLASH_CR_SNB_Pos) | FLASH_CR_STRT | FLASH_CR_BKSEL;
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
