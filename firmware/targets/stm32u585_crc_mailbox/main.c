/*
 * stm32u585_crc_mailbox — AEL hardware accelerator test
 * STM32U585CIU6 CRC-32 hardware unit
 *
 * Uses CRC-32/MPEG-2: poly=0x04C11DB7, init=0xFFFFFFFF, no reflection.
 * Verifies determinism: two runs with same input produce same result.
 * Also verifies result differs from the reset value (0xFFFFFFFF).
 *
 * FAIL codes:
 *   0xE001 — result1 == 0xFFFFFFFF (CRC unit appears untouched)
 *   0xE002 — result1 != result2 (non-deterministic, hardware fault)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))

/* CRC — AHB1, bit 12 of RCC_AHB1ENR */
#define CRC_BASE        0x40023000u
#define CRC_DR          (*(volatile uint32_t *)(CRC_BASE + 0x00u))
#define CRC_IDR         (*(volatile uint32_t *)(CRC_BASE + 0x04u))
#define CRC_CR          (*(volatile uint32_t *)(CRC_BASE + 0x08u))
#define CRC_INIT        (*(volatile uint32_t *)(CRC_BASE + 0x10u))
#define CRC_POL         (*(volatile uint32_t *)(CRC_BASE + 0x14u))

#define CRC_CR_RESET    (1u << 0u)

/* Test data — 4 words */
static const uint32_t test_data[4] = {
    0xABCD1234u,
    0x56789EF0u,
    0x11223344u,
    0x55667788u,
};

static uint32_t run_crc(void)
{
    /* Reset CRC unit (loads INIT value into DR) */
    CRC_CR = CRC_CR_RESET;

    /* Feed all 4 words */
    for (uint32_t i = 0u; i < 4u; i++) {
        CRC_DR = test_data[i];
    }

    return CRC_DR;
}

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable CRC clock */
    RCC_AHB1ENR |= (1u << 12u);
    /* Small delay for clock to settle */
    volatile uint32_t dummy = RCC_AHB1ENR;
    (void)dummy;

    /* 2. First run */
    uint32_t result1 = run_crc();

    /* 3. Verify result is not still the reset/init value */
    if (result1 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, result1);
        while (1) {}
    }

    /* 4. Second run — must be identical (determinism check) */
    uint32_t result2 = run_crc();

    if (result1 != result2) {
        ael_mailbox_fail(0xE002u, result2);
        while (1) {}
    }

    /* 5. PASS — store CRC result in detail0 */
    AEL_MAILBOX->detail0 = result1;
    ael_mailbox_pass();

    while (1) {}
    return 0;
}
