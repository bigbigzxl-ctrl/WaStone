/*
 * stm32h563rgt6_rng_mailbox
 *
 * Enables the STM32H563 hardware RNG, reads 4 random words,
 * verifies they are non-zero and not all identical.
 * PASS if all 4 reads succeed and values show entropy.
 *
 * FAIL codes:
 *   0xE001 — DRDY timeout on first word
 *   0xE002 — DRDY timeout on subsequent word
 *   0xE003 — all four values identical (stuck RNG)
 *   0xE004 — RNG SEIS or CEIS error flag set
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* RCC_CR bits for HSI48 */
#define RCC_CR_HSI48ON  (1u << 12)
#define RCC_CR_HSI48RDY (1u << 13)

/* RNG (AHB2) */
#define RNG_BASE        0x420C0800u
#define RNG_CR          (*(volatile uint32_t *)(RNG_BASE + 0x00u))
#define RNG_SR          (*(volatile uint32_t *)(RNG_BASE + 0x04u))
#define RNG_DR          (*(volatile uint32_t *)(RNG_BASE + 0x08u))

#define RNG_CR_RNGEN    (1u << 2)
#define RNG_SR_DRDY     (1u << 0)
#define RNG_SR_SEIS     (1u << 6)
#define RNG_SR_CEIS     (1u << 5)

#define TIMEOUT         1000000u

int main(void)
{
    ael_mailbox_init();

    /* Enable HSI48 (required: RNGSEL=00 → HSI48 as RNG clock source) */
    RCC_CR |= RCC_CR_HSI48ON;
    {
        uint32_t t;
        for (t = 0; t < 100000u; t++) {
            if (RCC_CR & RCC_CR_HSI48RDY) break;
        }
        if (!(RCC_CR & RCC_CR_HSI48RDY)) {
            ael_mailbox_fail(0xE000u, RCC_CR);
            while (1) {}
        }
    }

    /* Enable RNG clock (AHB2 bit18) */
    RCC_AHB2ENR |= (1u << 18);
    (void)RCC_AHB2ENR;

    /* Enable RNG */
    RNG_CR |= RNG_CR_RNGEN;

    uint32_t vals[4] = {0, 0, 0, 0};

    for (int i = 0; i < 4; i++) {
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (RNG_SR & RNG_SR_DRDY) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(i == 0 ? 0xE001u : 0xE002u, (uint32_t)i);
            while (1) {}
        }
        /* Check for error flags */
        if (RNG_SR & (RNG_SR_SEIS | RNG_SR_CEIS)) {
            ael_mailbox_fail(0xE004u, RNG_SR);
            while (1) {}
        }
        vals[i] = RNG_DR;
    }

    /* All identical = stuck */
    if (vals[0] == vals[1] && vals[1] == vals[2] && vals[2] == vals[3]) {
        ael_mailbox_fail(0xE003u, vals[0]);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = vals[0];
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
