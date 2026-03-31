/*
 * STM32F407VET6 — fastest possible PC0 toggle
 *
 * 1. Configure PLL: HSI(16 MHz) → SYSCLK = 168 MHz
 * 2. Tight assembly loop: STR set / STR clear / B → ~6 cycles/period
 *    Expected toggle frequency: 168 MHz / 6 ≈ 28 MHz
 */

#include <stdint.h>

/* ── RCC ──────────────────────────────────────────────────────────────────── */
#define RCC_BASE        0x40023800U
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_PLLCFGR     (*(volatile uint32_t *)(RCC_BASE + 0x04U))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40U))

/* ── Flash ────────────────────────────────────────────────────────────────── */
#define FLASH_BASE      0x40023C00U
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_BASE + 0x00U))

/* ── PWR ──────────────────────────────────────────────────────────────────── */
#define PWR_BASE        0x40007000U
#define PWR_CR          (*(volatile uint32_t *)(PWR_BASE + 0x00U))

/* ── GPIOC ────────────────────────────────────────────────────────────────── */
#define GPIOC_BASE      0x40020800U
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00U))
#define GPIOC_OSPEEDR   (*(volatile uint32_t *)(GPIOC_BASE + 0x08U))
#define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x18U))

static void clock_168mhz(void)
{
    /* 1. Enable PWR, set VOS Scale 1 (required for 168 MHz) */
    RCC_APB1ENR |= (1U << 28);          /* PWREN */
    PWR_CR      |= (3U << 14);          /* VOS = 11 (Scale 1) */

    /* 2. Flash: 5 wait states + prefetch + instruction/data cache */
    FLASH_ACR = (5U) | (1U << 8) | (1U << 9) | (1U << 10);

    /* 3. AHB=SYSCLK, APB1=SYSCLK/4, APB2=SYSCLK/2 */
    RCC_CFGR = (0x0U << 4)  |          /* HPRE  = /1  → AHB  168 MHz */
               (0x5U << 10) |          /* PPRE1 = /4  → APB1  42 MHz */
               (0x4U << 13);           /* PPRE2 = /2  → APB2  84 MHz */

    /* 4. PLL: src=HSI, M=16, N=336, P=2, Q=7 → SYSCLK = 168 MHz */
    RCC_PLLCFGR = (16U  <<  0) |       /* PLLM = 16  → VCO_in = 1 MHz */
                  (336U <<  6) |       /* PLLN = 336 → VCO_out = 336 MHz */
                  (0U   << 16) |       /* PLLP = 0 → /2 → 168 MHz */
                  (0U   << 22) |       /* PLLSRC = HSI */
                  (7U   << 24);        /* PLLQ = 7 (USB: 48 MHz) */

    /* 5. Enable PLL, wait ready */
    RCC_CR |= (1U << 24);
    while (!(RCC_CR & (1U << 25)));

    /* 6. Switch to PLL, wait confirmed */
    RCC_CFGR |= (2U << 0);
    while ((RCC_CFGR & (3U << 2)) != (2U << 2));
}

int main(void)
{
    clock_168mhz();

    /* Enable GPIOC, set PC0 output push-pull */
    RCC_AHB1ENR |= (1U << 2);
    __asm__ volatile ("nop"); __asm__ volatile ("nop");
    GPIOC_MODER   = (GPIOC_MODER   & ~0x3U) | 0x1U;  /* PC0 = output */
    GPIOC_OSPEEDR = (GPIOC_OSPEEDR & ~0x3U) | 0x3U;  /* PC0 = very high speed (100 MHz) */

    /* Tight toggle loop — compiled with -O2, no loop counter, no branches
     * except the back-edge.  Expected: ~6 AHB cycles per ON/OFF period.
     * At 168 MHz → ~28 MHz square wave on PC0.
     */
    register volatile uint32_t *bsrr = &GPIOC_BSRR;
    register uint32_t set   = (1U << 0);
    register uint32_t clear = (1U << 16);
    while (1) {
        *bsrr = set;
        *bsrr = clear;
    }
}
