/*
 * stm32u585_pwm_capture_mailbox — TIM1 PWM → TIM3 input capture
 * STM32U585CIU6, MSI 4MHz
 *
 * Wire: PA8 (TIM1_CH1, AF1) → PB4 (TIM3_CH1, AF2)
 *
 * TIM1 generates 100Hz PWM on PA8:
 *   PSC=3, ARR=9999 → 4MHz/4/10000 = 100Hz
 *   CCR1=5000 → 50% duty
 *
 * TIM3 captures rising edges on PB4:
 *   PSC=3, ARR=0xFFFF (free-running)
 *   Expected period = 10000 counts (between 2 rising edges)
 *   Accept range: [9000, 11000] counts (±10%)
 *
 * FAIL codes:
 *   0xE001 — first capture (CC1IF) timeout
 *   0xE002 — second capture timeout
 *   0xE003 — measured period out of range (detail0 = measured period)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB1ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

/* GPIOB */
#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_OSPEEDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_AFRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))

/* TIM1 (APB2) */
#define TIM1_BASE       0x40012C00u
#define TIM1_CR1        (*(volatile uint32_t *)(TIM1_BASE + 0x00u))
#define TIM1_CCMR1      (*(volatile uint32_t *)(TIM1_BASE + 0x18u))
#define TIM1_CCER       (*(volatile uint32_t *)(TIM1_BASE + 0x20u))
#define TIM1_PSC        (*(volatile uint32_t *)(TIM1_BASE + 0x28u))
#define TIM1_ARR        (*(volatile uint32_t *)(TIM1_BASE + 0x2Cu))
#define TIM1_CCR1       (*(volatile uint32_t *)(TIM1_BASE + 0x34u))
#define TIM1_BDTR       (*(volatile uint32_t *)(TIM1_BASE + 0x44u))
#define TIM1_EGR        (*(volatile uint32_t *)(TIM1_BASE + 0x14u))

/* TIM3 (APB1) */
#define TIM3_BASE       0x40000400u
#define TIM3_CR1        (*(volatile uint32_t *)(TIM3_BASE + 0x00u))
#define TIM3_CCMR1      (*(volatile uint32_t *)(TIM3_BASE + 0x18u))
#define TIM3_CCER       (*(volatile uint32_t *)(TIM3_BASE + 0x20u))
#define TIM3_CNT        (*(volatile uint32_t *)(TIM3_BASE + 0x24u))
#define TIM3_PSC        (*(volatile uint32_t *)(TIM3_BASE + 0x28u))
#define TIM3_ARR        (*(volatile uint32_t *)(TIM3_BASE + 0x2Cu))
#define TIM3_CCR1       (*(volatile uint32_t *)(TIM3_BASE + 0x34u))
#define TIM3_SR         (*(volatile uint32_t *)(TIM3_BASE + 0x10u))
#define TIM3_EGR        (*(volatile uint32_t *)(TIM3_BASE + 0x14u))

#define TIM_SR_CC1IF    (1u << 1u)

#define TIMEOUT         5000000u   /* ~1.25s at 4MHz */
#define PERIOD_EXPECTED 10000u
#define PERIOD_MARGIN   1000u      /* ±10% */

int main(void)
{
    ael_mailbox_init();

    /* Clocks */
    RCC_AHB2ENR1 |= (1u << 0u) | (1u << 1u);  /* GPIOAEN | GPIOBEN */
    RCC_APB2ENR  |= (1u << 11u);               /* TIM1EN */
    RCC_APB1ENR1 |= (1u << 1u);                /* TIM3EN */
    volatile uint32_t dummy = RCC_APB1ENR1;
    (void)dummy;

    /* PA8: AF1 (TIM1_CH1) — in AFRH */
    GPIOA_MODER   &= ~(3u << 16u);
    GPIOA_MODER   |=  (2u << 16u);
    GPIOA_OTYPER  &= ~(1u <<  8u);
    GPIOA_OSPEEDR |=  (3u << 16u);
    GPIOA_AFRH    &= ~(0xFu << 0u);
    GPIOA_AFRH    |=  (1u   << 0u);   /* AF1 for PA8 in AFRH[3:0] */

    /* PB4: AF2 (TIM3_CH1) — in AFRL */
    GPIOB_MODER   &= ~(3u << 8u);
    GPIOB_MODER   |=  (2u << 8u);
    GPIOB_OTYPER  &= ~(1u << 4u);
    GPIOB_OSPEEDR |=  (3u << 8u);
    GPIOB_AFRL    &= ~(0xFu << 16u);
    GPIOB_AFRL    |=  (2u   << 16u);  /* AF2 for PB4 in AFRL[19:16] */

    /* TIM1: PWM mode 1 on CH1
     * PSC=3 → 1MHz tick, ARR=9999 → 100Hz, CCR1=5000 → 50% duty */
    TIM1_PSC   = 3u;
    TIM1_ARR   = 9999u;
    TIM1_CCR1  = 5000u;
    /* CCMR1: OC1M=110 (PWM mode 1), OC1PE=1 */
    TIM1_CCMR1 = (6u << 4u) | (1u << 3u);
    /* CCER: CC1E=1 (enable CH1 output) */
    TIM1_CCER  = (1u << 0u);
    /* BDTR: MOE=1 (main output enable — required for advanced timer TIM1) */
    TIM1_BDTR  = (1u << 15u);
    /* EGR: UG=1 — force update event to load PSC shadow register immediately */
    TIM1_EGR   = 1u;
    /* CR1: CEN=1, ARPE=1 */
    TIM1_CR1   = (1u << 7u) | (1u << 0u);

    /* TIM3: input capture on CH1 (TI1FP1)
     * PSC=3 → 1MHz tick, ARR=0xFFFF (free-running) */
    TIM3_PSC   = 3u;
    TIM3_ARR   = 0xFFFFu;
    /* CCMR1: CC1S=01 (TI1FP1 mapped to TIM3_CH1 input) */
    TIM3_CCMR1 = (1u << 0u);
    /* CCER: CC1E=1, CC1P=0 (rising edge) */
    TIM3_CCER  = (1u << 0u);
    /* EGR: UG=1 — force update event to load PSC shadow register immediately */
    TIM3_EGR   = 1u;
    TIM3_SR    = 0u;  /* clear UIF from EGR write and any other flags */
    /* CR1: CEN=1 */
    TIM3_CR1   = (1u << 0u);

    /* Wait for first capture */
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (TIM3_SR & TIM_SR_CC1IF) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE001u, 0u);
            while (1) {}
        }
    }
    uint32_t t1 = TIM3_CCR1;   /* reading CCR1 clears CC1IF */

    /* Wait for second capture */
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (TIM3_SR & TIM_SR_CC1IF) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE002u, t1);
            while (1) {}
        }
    }
    uint32_t t2 = TIM3_CCR1;

    /* Period = t2 - t1 (handles 16-bit wraparound) */
    uint32_t period = (t2 - t1) & 0xFFFFu;

    AEL_MAILBOX->detail0 = period;

    if (period < (PERIOD_EXPECTED - PERIOD_MARGIN) ||
        period > (PERIOD_EXPECTED + PERIOD_MARGIN)) {
        ael_mailbox_fail(0xE003u, period);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
