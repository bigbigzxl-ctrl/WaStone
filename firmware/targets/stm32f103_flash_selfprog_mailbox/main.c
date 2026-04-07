#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

/* Flash interface */
#define FLASH_BASE      0x40022000u
#define FLASH_KEYR      (*(volatile uint32_t *)(FLASH_BASE + 0x04u))
#define FLASH_SR        (*(volatile uint32_t *)(FLASH_BASE + 0x0Cu))
#define FLASH_CR        (*(volatile uint32_t *)(FLASH_BASE + 0x10u))
#define FLASH_AR        (*(volatile uint32_t *)(FLASH_BASE + 0x14u))

#define FLASH_KEY1      0x45670123u
#define FLASH_KEY2      0xCDEF89ABu

#define FLASH_CR_PG     (1u << 0)
#define FLASH_CR_PER    (1u << 1)
#define FLASH_CR_STRT   (1u << 6)
#define FLASH_CR_LOCK   (1u << 7)
#define FLASH_SR_BSY    (1u << 0)
#define FLASH_SR_PGERR  (1u << 2)
#define FLASH_SR_WRPERR (1u << 4)
#define FLASH_SR_EOP    (1u << 5)

/*
 * Scratch page: last 1 KB page of 64 KB flash (STM32F103C8T6).
 * Page 63: 0x08000000 + 63 * 0x400 = 0x0800FC00
 * Firmware is tiny (<4 KB) so this page is always free.
 */
#define SCRATCH_ADDR    0x0800FC00u
#define PAGE_SIZE       0x400u

/* Test patterns written as halfwords (flash programs 16-bit at a time) */
static const uint16_t wr_pat[] = {0x5A5Au, 0xA5A5u, 0x1234u, 0xABCDu,
                                   0xDEADu, 0xBEEFu, 0xCAFEu, 0xF00Du};
#define N_PAT (sizeof(wr_pat) / sizeof(wr_pat[0]))

#define ERR_UNLOCK_FAILED   0x01u
#define ERR_ERASE_FAILED    0x02u
#define ERR_ERASE_VERIFY    0x03u
#define ERR_PROGRAM_FAILED  0x04u
#define ERR_READBACK        0x05u

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static uint8_t flash_wait_bsy(void)
{
    for (uint32_t i = 0u; i < 2000000u; ++i) {
        if ((FLASH_SR & FLASH_SR_BSY) == 0u) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t flash_unlock(void)
{
    if ((FLASH_CR & FLASH_CR_LOCK) == 0u) {
        return 1u; /* already unlocked */
    }
    FLASH_KEYR = FLASH_KEY1;
    FLASH_KEYR = FLASH_KEY2;
    return (FLASH_CR & FLASH_CR_LOCK) == 0u;
}

static void flash_lock(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}

static uint8_t flash_erase_page(uint32_t addr)
{
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPERR; /* clear flags */
    FLASH_CR |= FLASH_CR_PER;
    FLASH_AR  = addr;
    FLASH_CR |= FLASH_CR_STRT;
    if (!flash_wait_bsy()) {
        return 0u;
    }
    FLASH_CR &= ~FLASH_CR_PER;
    return (FLASH_SR & (FLASH_SR_PGERR | FLASH_SR_WRPERR)) == 0u;
}

static uint8_t flash_write_hw(uint32_t addr, uint16_t data)
{
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPERR;
    FLASH_CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = data;
    if (!flash_wait_bsy()) {
        return 0u;
    }
    FLASH_CR &= ~FLASH_CR_PG;
    return (FLASH_SR & (FLASH_SR_PGERR | FLASH_SR_WRPERR)) == 0u;
}

int main(void)
{
    ael_mailbox_init();

    /* Step 1: unlock flash */
    if (!flash_unlock()) {
        ael_mailbox_fail(ERR_UNLOCK_FAILED, 0u);
        while (1) {}
    }

    /* Step 2: erase scratch page */
    if (!flash_erase_page(SCRATCH_ADDR)) {
        flash_lock();
        ael_mailbox_fail(ERR_ERASE_FAILED, 0u);
        while (1) {}
    }

    /* Step 3: verify erased (all 0xFFFF) */
    for (uint32_t i = 0u; i < N_PAT; ++i) {
        uint16_t v = *(volatile uint16_t *)(SCRATCH_ADDR + i * 2u);
        if (v != 0xFFFFu) {
            flash_lock();
            ael_mailbox_fail(ERR_ERASE_VERIFY, (uint32_t)v);
            while (1) {}
        }
    }

    /* Step 4: program patterns */
    for (uint32_t i = 0u; i < N_PAT; ++i) {
        if (!flash_write_hw(SCRATCH_ADDR + i * 2u, wr_pat[i])) {
            flash_lock();
            ael_mailbox_fail(ERR_PROGRAM_FAILED, i);
            while (1) {}
        }
    }

    /* Step 5: lock flash, then read back and verify */
    flash_lock();

    for (uint32_t i = 0u; i < N_PAT; ++i) {
        uint16_t v = *(volatile uint16_t *)(SCRATCH_ADDR + i * 2u);
        if (v != wr_pat[i]) {
            ael_mailbox_fail(ERR_READBACK, ((uint32_t)wr_pat[i] << 16u) | v);
            while (1) {}
        }
    }

    /* PASS: detail0 = bytes programmed */
    AEL_MAILBOX->detail0 = N_PAT * 2u;
    ael_mailbox_pass();

    while (1) {
        AEL_MAILBOX->detail0++;
        delay_cycles(40000u);
    }
}
