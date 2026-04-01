/*
 * STM32F407 — Hardware RNG Test
 *
 * STM32F407 has a True RNG peripheral (not on F401/F411).
 * RNG clock source: PLL48CLK (48 MHz from USB/SDIO PLL).
 * On F407, PLLQ output drives RNG. Requires PLL48CLK = 48 MHz.
 *
 * PLL config for 48 MHz on PLL48CLK (from 16 MHz HSI):
 *   PLLM=16, PLLN=192, PLLQ=4 → VCO=192 MHz, PLL48CLK=48 MHz
 *   PLLP=2  → main PLL output = 96 MHz (not used, just satisfies constraints)
 *   Flash WS=3 for 96 MHz, APB prescalers=1
 *
 * Test:
 *   - Collect N_SAMPLES random words
 *   - PASS if: no SEIS/CEIS error, at least 2 distinct values seen,
 *     no single value repeated > MAX_REPEAT times
 *
 * No external wiring required.
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = number of distinct values seen (should equal N_SAMPLES
 *         unless collision, but at least 2)
 *   FAIL: error_code = ERR_CLOCK(1) ERR_READY(2) ERR_SEIS(3) ERR_STUCK(4)
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---------------------------------------------------------------- */
#define RCC_BASE    0x40023800u
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00u))
#define RCC_PLLCFGR (*(volatile uint32_t *)(RCC_BASE + 0x04u))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x08u))
#define RCC_AHB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x34u))
#define FLASH_ACR   (*(volatile uint32_t *)0x40023C00u)
#define PWR_BASE    0x40007000u
#define PWR_CR      (*(volatile uint32_t *)(PWR_BASE + 0x00u))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40u))

/* ---- RNG (base 0x50060800) ---------------------------------------------- */
#define RNG_BASE  0x50060800u
#define RNG_CR    (*(volatile uint32_t *)(RNG_BASE + 0x00u))
#define RNG_SR    (*(volatile uint32_t *)(RNG_BASE + 0x04u))
#define RNG_DR    (*(volatile uint32_t *)(RNG_BASE + 0x08u))

#define RNG_CR_RNGEN (1u << 2)
#define RNG_SR_DRDY  (1u << 0)
#define RNG_SR_CEIS  (1u << 5)
#define RNG_SR_SEIS  (1u << 6)

/* Error codes */
#define ERR_CLOCK  1u
#define ERR_READY  2u
#define ERR_SEIS   3u
#define ERR_STUCK  4u

#define N_SAMPLES  32u
#define MAX_REPEAT 4u
#define TIMEOUT    2000000u

static void setup_pll48(void)
{
    /* Enable PWR and set VOS=Scale1 for higher freq */
    RCC_APB1ENR |= (1u << 28);
    PWR_CR |= (3u << 14);

    /* Flash: 3 wait states + cache/prefetch for up to 100 MHz */
    FLASH_ACR = (3u) | (1u << 8) | (1u << 9) | (1u << 10);

    /* PLL config: HSI (16 MHz) → PLLM=16, PLLN=192, PLLP=2, PLLQ=4
     * VCO = 16/16 * 192 = 192 MHz
     * PLLCLK = 192/2 = 96 MHz  (SYSCLK)
     * PLL48CLK = 192/4 = 48 MHz (RNG clock) */
    RCC_PLLCFGR = (16u <<  0)   /* PLLM */
                | (192u << 6)   /* PLLN */
                | (0u  << 16)   /* PLLP = /2 */
                | (0u  << 22)   /* PLLSRC = HSI */
                | (4u  << 24);  /* PLLQ */

    /* Enable PLL */
    RCC_CR |= (1u << 24);
    { uint32_t t = TIMEOUT; while (!(RCC_CR & (1u << 25)) && --t) {} }

    /* Switch SYSCLK to PLL */
    RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
    { uint32_t t = TIMEOUT; while (((RCC_CFGR >> 2) & 3u) != 2u && --t) {} }
}

int main(void)
{
    setup_pll48();

    /* Enable RNG clock (AHB2, bit 6) */
    RCC_AHB2ENR |= (1u << 6);
    (void)RCC_AHB2ENR;

    ael_mailbox_init();

    /* Enable RNG */
    RNG_CR = RNG_CR_RNGEN;

    /* Wait for first DRDY */
    {
        uint32_t t = TIMEOUT;
        while (!(RNG_SR & RNG_SR_DRDY)) {
            if (RNG_SR & (RNG_SR_SEIS | RNG_SR_CEIS)) {
                ael_mailbox_fail(ERR_SEIS, RNG_SR);
                while (1) {}
            }
            if (--t == 0u) {
                ael_mailbox_fail(ERR_READY, 0u);
                while (1) {}
            }
        }
    }

    /* Collect samples */
    uint32_t samples[N_SAMPLES];
    for (uint32_t i = 0u; i < N_SAMPLES; i++) {
        uint32_t t = TIMEOUT;
        while (!(RNG_SR & RNG_SR_DRDY)) {
            if (RNG_SR & (RNG_SR_SEIS | RNG_SR_CEIS)) {
                ael_mailbox_fail(ERR_SEIS, RNG_SR);
                while (1) {}
            }
            if (--t == 0u) {
                ael_mailbox_fail(ERR_READY, i);
                while (1) {}
            }
        }
        samples[i] = RNG_DR;
        AEL_MAILBOX->detail0 = i + 1u;
    }

    /* Check: no value repeated more than MAX_REPEAT times */
    for (uint32_t i = 0u; i < N_SAMPLES; i++) {
        uint32_t count = 0u;
        for (uint32_t j = 0u; j < N_SAMPLES; j++) {
            if (samples[j] == samples[i]) { count++; }
        }
        if (count > MAX_REPEAT) {
            ael_mailbox_fail(ERR_STUCK, samples[i]);
            while (1) {}
        }
    }

    /* Count distinct values */
    uint32_t distinct = 0u;
    for (uint32_t i = 0u; i < N_SAMPLES; i++) {
        uint32_t unique = 1u;
        for (uint32_t j = 0u; j < i; j++) {
            if (samples[j] == samples[i]) { unique = 0u; break; }
        }
        if (unique) { distinct++; }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = distinct;  /* should be close to N_SAMPLES */

    while (1) {}
}
