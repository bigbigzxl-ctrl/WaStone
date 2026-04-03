#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define FLASH_BASE_ADDR  0x40022000u
#define FLASH_ACR        (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x00u))
#define FLASH_KEYR       (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x04u))
#define FLASH_SR         (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x0Cu))
#define FLASH_CR         (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x10u))
#define FLASH_AR         (*(volatile uint32_t *)(FLASH_BASE_ADDR + 0x14u))

#define FLASH_SR_BSY     (1u << 0)
#define FLASH_SR_PGERR   (1u << 2)
#define FLASH_SR_WRPRTERR (1u << 4)
#define FLASH_SR_EOP     (1u << 5)
#define FLASH_SR_ERRORS  (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)

#define FLASH_CR_PG      (1u << 0)
#define FLASH_CR_PER     (1u << 1)
#define FLASH_CR_STRT    (1u << 6)
#define FLASH_CR_LOCK    (1u << 7)

#define FLASH_KEY1       0x45670123u
#define FLASH_KEY2       0xCDEF89ABu

#define TEST_PAGE_ADDR   0x0800FC00u

static const uint32_t k_pattern[8] = {
    0xDEADBEEFu, 0xCAFEBABEu, 0x12345678u, 0xABCDEF01u,
    0x55AA55AAu, 0xAA55AA55u, 0xFEEDFACEu, 0xBAADF00Du
};

static uint32_t flash_wait_ready(void)
{
    uint32_t timeout = 5000000u;
    while (FLASH_SR & FLASH_SR_BSY) {
        if (--timeout == 0u) {
            return 0u;
        }
    }
    return 1u;
}

static void flash_clear_status(void)
{
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_ERRORS;
}

int main(void)
{
    volatile uint32_t *words = (volatile uint32_t *)TEST_PAGE_ADDR;
    volatile uint16_t *halves = (volatile uint16_t *)TEST_PAGE_ADDR;

    ael_mailbox_init();

    if (!flash_wait_ready()) {
        ael_mailbox_fail(0xE2F1u, FLASH_SR);
        while (1) {}
    }

    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
    if (FLASH_CR & FLASH_CR_LOCK) {
        ael_mailbox_fail(0xE2F2u, FLASH_CR);
        while (1) {}
    }

    flash_clear_status();

    FLASH_CR = FLASH_CR_PER;
    FLASH_AR = TEST_PAGE_ADDR;
    FLASH_CR |= FLASH_CR_STRT;
    if (!flash_wait_ready()) {
        ael_mailbox_fail(0xE2F3u, FLASH_SR);
        while (1) {}
    }
    if (FLASH_SR & FLASH_SR_ERRORS) {
        ael_mailbox_fail(0xE2F4u, FLASH_SR);
        while (1) {}
    }
    FLASH_CR = 0u;

    for (uint32_t i = 0u; i < 8u; ++i) {
        if (words[i] != 0xFFFFFFFFu) {
            ael_mailbox_fail(0xE2F5u, i);
            while (1) {}
        }
    }

    flash_clear_status();
    FLASH_CR = FLASH_CR_PG;
    for (uint32_t i = 0u; i < 8u; ++i) {
        uint32_t value = k_pattern[i];
        halves[i * 2u] = (uint16_t)(value & 0xFFFFu);
        if (!flash_wait_ready()) {
            ael_mailbox_fail(0xE2F6u, (i << 16u) | 0u);
            while (1) {}
        }
        if (FLASH_SR & FLASH_SR_ERRORS) {
            ael_mailbox_fail(0xE2F7u, FLASH_SR);
            while (1) {}
        }
        flash_clear_status();

        halves[i * 2u + 1u] = (uint16_t)(value >> 16u);
        if (!flash_wait_ready()) {
            ael_mailbox_fail(0xE2F6u, (i << 16u) | 1u);
            while (1) {}
        }
        if (FLASH_SR & FLASH_SR_ERRORS) {
            ael_mailbox_fail(0xE2F7u, FLASH_SR);
            while (1) {}
        }
        flash_clear_status();
    }
    FLASH_CR = 0u;

    for (uint32_t i = 0u; i < 8u; ++i) {
        if (words[i] != k_pattern[i]) {
            ael_mailbox_fail(0xE2F8u, i);
            while (1) {}
        }
    }

    FLASH_CR = FLASH_CR_LOCK;

    AEL_MAILBOX->detail0 = 8u;
    ael_mailbox_pass();
    while (1) {}
}
