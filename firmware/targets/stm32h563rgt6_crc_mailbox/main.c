/*
 * stm32h563rgt6_crc_mailbox
 *
 * STM32H563 hardware CRC-32 unit test.
 * CRC-32/MPEG-2: poly=0x04C11DB7, init=0xFFFFFFFF, no reflection.
 *
 * Verifies:
 *   1. Result differs from reset value (0xFFFFFFFF) → not stuck
 *   2. Second run with same data produces identical result → deterministic
 *
 * CRC clock: AHB1 bit12 (CRCEN).
 * CRC base:  0x40023000 (confirmed from RM0481 SVD).
 *
 * FAIL codes:
 *   0xE001 — result == 0xFFFFFFFF (CRC unit stuck)
 *   0xE002 — result1 != result2 (non-deterministic)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))

/* CRC (AHB1, bit12) */
#define CRC_BASE        0x40023000u
#define CRC_DR          (*(volatile uint32_t *)(CRC_BASE + 0x00u))
#define CRC_CR          (*(volatile uint32_t *)(CRC_BASE + 0x08u))

#define CRC_CR_RESET    (1u << 0)

static const uint32_t test_data[4] = {
    0xABCD1234u,
    0x56789EF0u,
    0x11223344u,
    0x55667788u,
};

static uint32_t run_crc(void)
{
    CRC_CR = CRC_CR_RESET;
    for (uint32_t i = 0u; i < 4u; i++) {
        CRC_DR = test_data[i];
    }
    return CRC_DR;
}

int main(void)
{
    ael_mailbox_init();

    /* Enable CRC clock (AHB1 bit12) */
    RCC_AHB1ENR |= (1u << 12);
    (void)RCC_AHB1ENR;

    uint32_t result1 = run_crc();

    if (result1 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, result1);
        while (1) {}
    }

    uint32_t result2 = run_crc();

    if (result1 != result2) {
        ael_mailbox_fail(0xE002u, result2);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = result1;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
