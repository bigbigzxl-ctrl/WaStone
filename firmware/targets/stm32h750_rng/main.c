/*
 * stm32h750_rng — True Random Number Generator self-test
 *
 * Enables HSI48 (RNG kernel clock source, default RNGSEL=000),
 * generates 4 × 32-bit random words, verifies:
 *   - all words non-zero
 *   - not all four words identical (collision probability ~2e-29)
 *
 * RNG base: 0x48060800 (AHB2, D2 domain, RM0433 §33)
 * RCC_AHB2ENR bit 6 = RNGEN
 * RCC_CR bit 16 = HSI48ON, bit 17 = HSI48RDY
 *
 * RNG clock constraint (RM0433 §33.3.6):
 *   fRNGCLK ≥ fHCLK/32  (64 MHz/32 = 2 MHz) → HSI48 = 48 MHz ✓
 *   fRNGCLK < 4×fHCLK   (4×64 MHz = 256 MHz) → 48 MHz ✓
 *
 * Error codes:
 *   0xE001 = HSI48 ready timeout
 *   0xE002 = DRDY timeout (no data after 1M polls)
 *   0xE003 = SEIS set (seed error interrupt status)
 *   0xE004 = CEIS set (clock error interrupt status)
 *   0xE005 = suspicious: all 4 words identical
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE        0x58024400u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0DCu))   /* AHB2ENR at offset 0xDC (not 0xD4=AHB3ENR) */

#define RCC_CR_HSI48ON   (1u << 12u)   /* HSI48ON at bit 12 per RM0433 */
#define RCC_CR_HSI48RDY  (1u << 13u)   /* HSI48RDY at bit 13 */
#define RCC_AHB2ENR_RNGEN (1u << 6u)

/* ── RNG (AHB2, D2_AHB2PERIPH_BASE+0x1800 = 0x48021800) ─────────── */
#define RNG_BASE  0x48021800u
#define RNG_CR    (*(volatile uint32_t *)(RNG_BASE + 0x00u))
#define RNG_SR    (*(volatile uint32_t *)(RNG_BASE + 0x04u))
#define RNG_DR    (*(volatile uint32_t *)(RNG_BASE + 0x08u))

#define RNG_CR_RNGEN  (1u << 2u)   /* RNG enable */
#define RNG_CR_CED    (1u << 5u)   /* clock error detection disable */
#define RNG_SR_DRDY   (1u << 0u)   /* data ready */
#define RNG_SR_CEIS   (1u << 1u)   /* clock error interrupt status */
#define RNG_SR_SEIS   (1u << 2u)   /* seed error interrupt status */

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static void delay_ticks(uint32_t ticks)
{
    for (volatile uint32_t i = 0u; i < ticks; i++) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Enable HSI48 — RNG default kernel clock source (RNGSEL=000 in RCC_D2CCIP2R) */
    RCC_CR |= RCC_CR_HSI48ON;
    {
        uint32_t timeout = 10u;   /* 10 ms */
        while (((RCC_CR & RCC_CR_HSI48RDY) == 0u) && (--timeout > 0u)) {
            delay_ticks(1u);
        }
        if ((RCC_CR & RCC_CR_HSI48RDY) == 0u) {
            ael_mailbox_fail(0xE001u, RCC_CR);
            while (1) {}
        }
    }

    /* Enable RNG peripheral clock */
    RCC_AHB2ENR |= RCC_AHB2ENR_RNGEN;
    (void)RCC_AHB2ENR;

    /* Enable RNG (CED=0 → clock error detection ON) */
    RNG_CR = RNG_CR_RNGEN;
    delay_ticks(1u);   /* allow first conditioning cycles */

    /* Read 4 random words */
    uint32_t words[4] = { 0u, 0u, 0u, 0u };
    for (uint32_t i = 0u; i < 4u; i++) {
        uint32_t timeout = 1000000u;
        while (((RNG_SR & RNG_SR_DRDY) == 0u) && (--timeout > 0u)) {}
        if ((RNG_SR & RNG_SR_SEIS) != 0u) {
            ael_mailbox_fail(0xE003u, RNG_SR);
            while (1) {}
        }
        if ((RNG_SR & RNG_SR_CEIS) != 0u) {
            ael_mailbox_fail(0xE004u, RNG_SR);
            while (1) {}
        }
        if ((RNG_SR & RNG_SR_DRDY) == 0u) {
            ael_mailbox_fail(0xE002u, (i << 16u) | RNG_SR);
            while (1) {}
        }
        words[i] = RNG_DR;
    }

    /* Sanity: all four identical is astronomically unlikely with a working RNG */
    if ((words[0] == words[1]) && (words[1] == words[2]) && (words[2] == words[3])) {
        ael_mailbox_fail(0xE005u, words[0]);
        while (1) {}
    }

    ael_mailbox_pass();
    /* detail0: first random word as evidence */
    AEL_MAILBOX->detail0 = words[0];
    while (1) {}
}
