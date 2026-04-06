/*
 * CH32V003 Flash read/write test
 *
 * Uses the 64-byte fast-program mode to write then read back
 * 64 bytes of data to the last page of flash (0x08003FC0).
 * No pins required.
 *
 * Sequence:
 *   1. Unlock (KEYR + MODEKEYR)
 *   2. BufReset
 *   3. BufLoad 16 words × 4 bytes = 64 bytes
 *   4. ErasePage_Fast at target address
 *   5. ProgramPage_Fast at target address
 *   6. Lock
 *   7. Read back and verify
 *   8. PASS or FAIL
 *
 * detail0 = verify_pass_count << 1  (liveness: re-runs every ~2s)
 *
 * Clock: 24 MHz HSI, APB2 = 8 MHz
 */

#define AEL_MAILBOX_ADDR  0x20000600u

/* Target: last 64-byte page in 16KB flash */
#define FLASH_TEST_ADDR  ((uint32_t)0x08003FC0u)

#define CR_STRT      0x00000040u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
/* ch32v003fun.h defines FLASH_KEY1/2, CR_PAGE_PG/ER, CR_BUF_LOAD/RST, SR_BSY */
#include <stdint.h>

void SystemInit(void) {}

static void flash_unlock(void)
{
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    FLASH->MODEKEYR = FLASH_KEY1;
    FLASH->MODEKEYR = FLASH_KEY2;
}

static void flash_lock(void)
{
    FLASH->CTLR |= 0x00000080u;  /* LOCK bit */
}

static void flash_buf_reset(void)
{
    FLASH->CTLR |= CR_PAGE_PG;
    FLASH->CTLR |= CR_BUF_RST;
    while (FLASH->STATR & SR_BSY);
    FLASH->CTLR &= ~CR_PAGE_PG;
}

static void flash_buf_load(uint32_t addr, uint32_t data)
{
    FLASH->CTLR |= CR_PAGE_PG;
    *(__IO uint32_t *)(addr) = data;
    FLASH->CTLR |= CR_BUF_LOAD;
    while (FLASH->STATR & SR_BSY);
    FLASH->CTLR &= ~CR_PAGE_PG;
}

static void flash_erase_page(uint32_t addr)
{
    FLASH->CTLR |= CR_PAGE_ER;
    FLASH->ADDR  = addr;
    FLASH->CTLR |= CR_STRT;
    while (FLASH->STATR & SR_BSY);
    FLASH->CTLR &= ~CR_PAGE_ER;
}

static void flash_program_page(uint32_t addr)
{
    FLASH->CTLR |= CR_PAGE_PG;
    FLASH->ADDR  = addr;
    FLASH->CTLR |= CR_STRT;
    while (FLASH->STATR & SR_BSY);
    FLASH->CTLR &= ~CR_PAGE_PG;
}

/* Write 64 bytes (16 words) to flash at FLASH_TEST_ADDR.
 * Returns 0 on verify success, error bitmask otherwise. */
static uint32_t flash_test(uint32_t seed)
{
    flash_unlock();
    flash_buf_reset();

    /* Load 16 words (64 bytes) into buffer */
    for (uint8_t i = 0; i < 16u; i++) {
        flash_buf_load(FLASH_TEST_ADDR + (uint32_t)i * 4u,
                       seed ^ ((uint32_t)i * 0x01010101u));
    }

    /* Erase then program */
    flash_erase_page(FLASH_TEST_ADDR);
    flash_program_page(FLASH_TEST_ADDR);
    flash_lock();

    /* Verify */
    uint32_t err = 0u;
    for (uint8_t i = 0; i < 16u; i++) {
        uint32_t expected = seed ^ ((uint32_t)i * 0x01010101u);
        uint32_t actual   = *(__IO uint32_t *)(FLASH_TEST_ADDR + (uint32_t)i * 4u);
        if (actual != expected) err |= (1u << (i & 31u));
    }
    return err;
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    ael_mailbox_init();

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    uint32_t err = flash_test(0xDEADBEEFu);

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, 0);

    /* Liveness: re-run every ~2 s with a different seed */
    uint32_t pass_count = 0;
    uint32_t seed = 0xA5A5A5A5u;
    while (1) {
        for (volatile uint32_t i = 0; i < 4000000u; i++);
        uint32_t e = flash_test(seed);
        seed = seed * 1664525u + 1013904223u;  /* LCG */
        if (e == 0u) {
            pass_count++;
            *detail0 = pass_count << 1;
        }
    }

    return 0;
}
