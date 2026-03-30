/*
 * stm32u585_rng_mailbox — AEL hardware accelerator test
 * STM32U585CIU6 True Random Number Generator (RNG)
 *
 * Pass criteria:
 *   - No error flags (SEIS/CEIS) during generation
 *   - All 16 values are non-zero
 *   - Not all 16 values are identical
 *
 * FAIL codes:
 *   0xE001 — DRDY timeout
 *   0xE002 — SEIS or CEIS error flag set
 *   0xE003 — one or more values is zero
 *   0xE004 — all values are identical
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* RNG — AHB2, bit 18 of RCC_AHB2ENR1 */
#define RNG_BASE        0x420C0800u
#define RNG_CR          (*(volatile uint32_t *)(RNG_BASE + 0x00u))
#define RNG_SR          (*(volatile uint32_t *)(RNG_BASE + 0x04u))
#define RNG_DR          (*(volatile uint32_t *)(RNG_BASE + 0x08u))

#define RNG_CR_RNGEN    (1u << 2u)
/* CED=0 (clock error detection enabled) — leave bit5 clear */

#define RNG_SR_DRDY     (1u << 0u)
#define RNG_SR_CEIS     (1u << 5u)
#define RNG_SR_SEIS     (1u << 6u)

#define NUM_SAMPLES     16u
#define DRDY_TIMEOUT    1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable RNG clock */
    RCC_AHB2ENR1 |= (1u << 18u);
    /* Small delay for clock to settle */
    volatile uint32_t dummy = RCC_AHB2ENR1;
    (void)dummy;

    /* 2. Enable RNG (RNGEN=1, CED=0) */
    RNG_CR = RNG_CR_RNGEN;

    uint32_t samples[NUM_SAMPLES];

    /* 3. Collect 16 random values */
    for (uint32_t i = 0u; i < NUM_SAMPLES; i++) {
        /* Wait for DRDY */
        uint32_t timeout = DRDY_TIMEOUT;
        while (!(RNG_SR & RNG_SR_DRDY)) {
            if (--timeout == 0u) {
                ael_mailbox_fail(0xE001u, i);
                while (1) {}
            }
        }

        /* Check error flags */
        if (RNG_SR & (RNG_SR_SEIS | RNG_SR_CEIS)) {
            ael_mailbox_fail(0xE002u, RNG_SR);
            while (1) {}
        }

        samples[i] = RNG_DR;
    }

    /* 4. Verify no value is zero */
    for (uint32_t i = 0u; i < NUM_SAMPLES; i++) {
        if (samples[i] == 0u) {
            ael_mailbox_fail(0xE003u, i);
            while (1) {}
        }
    }

    /* 5. Verify not all values are identical */
    uint32_t first = samples[0];
    uint32_t all_same = 1u;
    for (uint32_t i = 1u; i < NUM_SAMPLES; i++) {
        if (samples[i] != first) {
            all_same = 0u;
            break;
        }
    }
    if (all_same) {
        ael_mailbox_fail(0xE004u, first);
        while (1) {}
    }

    /* 6. PASS — store first sample in detail0 */
    AEL_MAILBOX->detail0 = samples[0];
    ael_mailbox_pass();

    while (1) {}
    return 0;
}
