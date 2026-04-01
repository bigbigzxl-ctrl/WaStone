/*
 * STM32F407 — Internal Flash R/W Verification Test
 *
 * Uses Sector 7 (0x08060000, 128 KB) — last sector of STM32F407VET6 (512 KB).
 * Firmware occupies only the first few KB, so sector 7 is safe.
 *
 * Test flow:
 *   1. Unlock flash (KEYR sequence)
 *   2. Erase sector 7 (SNB=7, SER=1, STRT=1, poll BSY)
 *   3. Verify erased: first 8 words must be 0xFFFFFFFF
 *   4. Write 8 known words (PSIZE=word, PG=1)
 *   5. Read back and verify
 *   6. Lock flash
 *
 * No wiring required.
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = 8 (words verified)
 *   FAIL: error_code=1 flash unlock failed (BSY at start)
 *         error_code=2 erase timeout / BSY stuck
 *         error_code=3 erase error (OPERR/WRPERR in SR)
 *         error_code=4 sector not erased (expected 0xFFFFFFFF)
 *         error_code=5 write timeout / BSY stuck
 *         error_code=6 write error in SR
 *         error_code=7 readback mismatch; detail0=word index
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- FLASH registers ---- */
#define FLASH_BASE   0x40023C00u
#define FLASH_KEYR   (*(volatile uint32_t *)(FLASH_BASE + 0x04u))
#define FLASH_SR     (*(volatile uint32_t *)(FLASH_BASE + 0x0Cu))
#define FLASH_CR     (*(volatile uint32_t *)(FLASH_BASE + 0x10u))

#define FLASH_SR_BSY     (1u << 16)
#define FLASH_SR_OPERR   (1u << 1)
#define FLASH_SR_WRPERR  (1u << 4)
#define FLASH_SR_ERRORS  ((1u<<1)|(1u<<4)|(1u<<5)|(1u<<6)|(1u<<7))

#define FLASH_CR_PG      (1u << 0)
#define FLASH_CR_SER     (1u << 1)
#define FLASH_CR_PSIZE_W (2u << 8)   /* word (32-bit) programming */
#define FLASH_CR_STRT    (1u << 16)
#define FLASH_CR_LOCK    (1u << 31)

#define FLASH_KEY1  0x45670123u
#define FLASH_KEY2  0xCDEF89ABu

/* Sector 7: last 128 KB of STM32F407VET6 (512 KB) */
#define TEST_SECTOR   7u
#define TEST_ADDR     0x08060000u

static const uint32_t PATTERN[8] = {
    0xDEADBEEFu, 0xCAFEBABEu, 0x12345678u, 0xABCDEF01u,
    0x55AA55AAu, 0xAA55AA55u, 0xFEEDFACEu, 0xBAADF00Du
};

static uint32_t flash_wait(void)
{
    uint32_t t = 5000000u;
    while (FLASH_SR & FLASH_SR_BSY) {
        if (--t == 0u) return 0u;
    }
    return 1u;
}

int main(void)
{
    ael_mailbox_init();

    /* ---- Step 1: Unlock flash ---- */
    if (FLASH_SR & FLASH_SR_BSY) {
        ael_mailbox_fail(1u, FLASH_SR);
        while (1) {}
    }
    FLASH_KEYR = FLASH_KEY1;
    FLASH_KEYR = FLASH_KEY2;
    /* CR.LOCK should now be 0 */

    /* ---- Step 2: Erase sector 7 ---- */
    /* Clear any error flags */
    FLASH_SR = FLASH_SR_ERRORS;
    FLASH_CR = FLASH_CR_PSIZE_W | FLASH_CR_SER | (TEST_SECTOR << 3);
    FLASH_CR |= FLASH_CR_STRT;

    if (!flash_wait()) {
        ael_mailbox_fail(2u, FLASH_SR);
        while (1) {}
    }
    if (FLASH_SR & FLASH_SR_ERRORS) {
        ael_mailbox_fail(3u, FLASH_SR);
        while (1) {}
    }
    FLASH_CR = 0u; /* clear SER */

    /* ---- Step 3: Verify erased ---- */
    volatile uint32_t *p = (volatile uint32_t *)TEST_ADDR;
    for (uint32_t i = 0u; i < 8u; i++) {
        if (p[i] != 0xFFFFFFFFu) {
            ael_mailbox_fail(4u, i);
            while (1) {}
        }
    }

    /* ---- Step 4: Write pattern ---- */
    FLASH_CR = FLASH_CR_PSIZE_W | FLASH_CR_PG;
    for (uint32_t i = 0u; i < 8u; i++) {
        p[i] = PATTERN[i];
        if (!flash_wait()) {
            ael_mailbox_fail(5u, i);
            while (1) {}
        }
        if (FLASH_SR & FLASH_SR_ERRORS) {
            ael_mailbox_fail(6u, FLASH_SR);
            while (1) {}
        }
    }
    FLASH_CR = 0u;

    /* ---- Step 5: Lock flash ---- */
    FLASH_CR |= FLASH_CR_LOCK;

    /* ---- Step 6: Verify readback ---- */
    for (uint32_t i = 0u; i < 8u; i++) {
        if (p[i] != PATTERN[i]) {
            ael_mailbox_fail(7u, i);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 8u;
    while (1) {}
}
