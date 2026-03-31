/*
 * STM32F407VET6 — TIM2 CH1 PWM output on PA5
 *
 * System: 168 MHz PLL (HSI × 168/16 via N=336, M=16, P=2).
 * TIM2 clock = 2 × APB1 = 2 × 42 MHz = 84 MHz.
 * TIM2 CH1 output on PA5 (AF1).
 *
 * PWM frequency = 84 MHz / (PSC+1) / (ARR+1)
 * Sweep table (PSC=0 throughout):
 *   ARR=83999 →  1.000 kHz
 *   ARR= 8399 → 10.000 kHz
 *   ARR=  839 → 100.00 kHz
 *   ARR=   83 →  1.000 MHz
 * Each frequency held for ~3 s (DWT at 168 MHz).
 * PASS written to mailbox at startup; LA confirms each frequency on ch1.
 *
 * Wiring: LA P0.1 → PA5
 * Mailbox: 0x2001FC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* ── RCC ──────────────────────────────────────────────────────────────────── */
#define RCC_BASE    0x40023800U
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_PLLCFGR (*(volatile uint32_t *)(RCC_BASE + 0x04U))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40U))
#define FLASH_ACR   (*(volatile uint32_t *)(0x40023C00U))
#define PWR_CR      (*(volatile uint32_t *)(0x40007000U))

/* ── GPIOA ────────────────────────────────────────────────────────────────── */
#define GPIOA_BASE  0x40020000U
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_AFRL  (*(volatile uint32_t *)(GPIOA_BASE + 0x20U))

/* ── TIM2 ─────────────────────────────────────────────────────────────────── */
#define TIM2_BASE   0x40000000U
#define TIM2_CR1    (*(volatile uint32_t *)(TIM2_BASE + 0x00U))
#define TIM2_EGR    (*(volatile uint32_t *)(TIM2_BASE + 0x14U))
#define TIM2_CCMR1  (*(volatile uint32_t *)(TIM2_BASE + 0x18U))
#define TIM2_CCER   (*(volatile uint32_t *)(TIM2_BASE + 0x20U))
#define TIM2_PSC    (*(volatile uint32_t *)(TIM2_BASE + 0x28U))
#define TIM2_ARR    (*(volatile uint32_t *)(TIM2_BASE + 0x2CU))
#define TIM2_CCR1   (*(volatile uint32_t *)(TIM2_BASE + 0x34U))

/* ── DWT (for hold timing) ────────────────────────────────────────────────── */
#define CoreDebug_DEMCR (*(volatile uint32_t *)(0xE000EDFCU))
#define DWT_CTRL        (*(volatile uint32_t *)(0xE0001000U))
#define DWT_CYCCNT      (*(volatile uint32_t *)(0xE0001004U))

static void clock_168mhz(void)
{
    RCC_APB1ENR |= (1U << 28);                           /* PWREN */
    PWR_CR      |= (3U << 14);                           /* VOS Scale 1 */
    FLASH_ACR    = (5U) | (1U<<8) | (1U<<9) | (1U<<10); /* 5WS + caches */
    RCC_CFGR     = (0x5U << 10) | (0x4U << 13);         /* APB1/4 APB2/2 */
    RCC_PLLCFGR  = (16U<<0)|(336U<<6)|(0U<<16)|(7U<<24);/* M16 N336 P2 Q7 */
    RCC_CR |= (1U << 24);
    while (!(RCC_CR & (1U << 25)));
    RCC_CFGR |= (2U << 0);
    while ((RCC_CFGR & (3U << 2)) != (2U << 2));
}

static void hold_3s(void)
{
    uint32_t start = DWT_CYCCNT;
    while ((DWT_CYCCNT - start) < 168000000U * 3U);
}

/* Sweep: ARR values for PSC=0, TIM2_CLK=84MHz */
static const uint32_t arr_table[] = { 83999U, 8399U, 839U, 83U };
/* Expected frequencies:              1 kHz,  10 kHz, 100 kHz, 1 MHz */

int main(void)
{
    clock_168mhz();

    /* Enable DWT cycle counter */
    CoreDebug_DEMCR |= (1U << 24);
    DWT_CYCCNT       = 0U;
    DWT_CTRL        |= (1U << 0);

    /* Enable GPIOA (AHB1 bit 0) + TIM2 (APB1 bit 0) */
    RCC_AHB1ENR |= (1U << 0U);
    RCC_APB1ENR |= (1U << 0U);
    (void)RCC_APB1ENR;

    /*
     * PA5 → TIM2_CH1 AF1: MODER[11:10]=10 (AF), AFRL[23:20]=1 (AF1)
     */
    GPIOA_MODER &= ~(3U << 10U);
    GPIOA_MODER |=  (2U << 10U);
    GPIOA_AFRL  &= ~(0xFU << 20U);
    GPIOA_AFRL  |=  (1U  << 20U);

    /*
     * TIM2 PWM mode 1 on CH1:
     *   CCMR1[6:4] OC1M=110 (PWM1), CCMR1[3] OC1PE=1
     *   CCER[0] CC1E=1 (enable output)
     */
    TIM2_CCMR1 = (6U << 4U) | (1U << 3U);  /* OC1M=PWM1, OC1PE */
    TIM2_CCER  = (1U << 0U);               /* CC1E */
    TIM2_PSC   = 0U;

    ael_mailbox_init();
    ael_mailbox_pass();   /* PASS immediately — LA confirms frequency externally */

    uint32_t step = 0U;
    while (1) {
        uint32_t arr = arr_table[step % 4U];
        TIM2_ARR  = arr;
        TIM2_CCR1 = arr / 2U;           /* 50% duty cycle */
        TIM2_EGR  = 1U;                 /* UG: load PSC/ARR immediately */
        TIM2_CR1  = (1U << 0U);         /* CEN */
        AEL_MAILBOX->detail0 = step;
        hold_3s();
        step++;
    }
}
