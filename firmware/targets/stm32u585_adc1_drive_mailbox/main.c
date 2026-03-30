/*
 * stm32u585_adc1_drive_mailbox — ADC1 with GPIO-driven input
 * STM32U585CIU6, MSI 4MHz
 *
 * Wire: PB1 (GPIO output) → PB0 (ADC1_IN15 input)
 *
 * Drives PB1 HIGH (≈VDD=3.3V) → ADC1_IN15 should read ~4095
 * Drives PB1 LOW  (≈0V)       → ADC1_IN15 should read ~0
 *
 * ADC1 clock: CKMODE=01 (synchronous HCLK/1 = 4MHz)
 *   This bypasses CCIPR3 / kernel clock issues.
 *
 * FAIL codes:
 *   0xE001 — ADVREGEN wait failed (unexpected)
 *   0xE002 — ADC calibration timeout
 *   0xE003 — ADRDY timeout
 *   0xE004 — EOC timeout
 *   0xE005 — HIGH reading too low  (detail0 = raw)
 *   0xE006 — LOW  reading too high (detail0 = raw)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* GPIOB */
#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_OSPEEDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14u))

/* ADC1 (AHB2) */
#define ADC1_BASE       0x42028000u
#define ADC1_ISR        (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR         (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_CFGR       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC1_SMPR1      (*(volatile uint32_t *)(ADC1_BASE + 0x14u))
#define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x30u))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x40u))

/* ADC1/ADC2 common registers */
#define ADC_COMMON_BASE  0x42028300u
#define ADC_CCR         (*(volatile uint32_t *)(ADC_COMMON_BASE + 0x08u))

/* ADC_CCR bits */
/* CKMODE[17:16]: 01 = synchronous HCLK/1 */
#define ADC_CCR_CKMODE_HCLK1   (1u << 16u)

/* ADC1 CR bits */
#define ADC_CR_ADEN      (1u << 0u)
#define ADC_CR_ADDIS     (1u << 1u)
#define ADC_CR_ADSTART   (1u << 2u)
#define ADC_CR_ADCAL     (1u << 31u)
#define ADC_CR_ADVREGEN  (1u << 28u)
#define ADC_CR_DEEPPWD   (1u << 29u)

/* ADC1 ISR bits */
#define ADC_ISR_ADRDY    (1u << 0u)
#define ADC_ISR_EOC      (1u << 2u)

/* ADC1 RCC: AHB2ENR1 bit 10 = ADC12EN */
#define RCC_AHB2ENR1_ADC12EN  (1u << 10u)

#define TIMEOUT         1000000u

/* Read one sample from ADC1 channel `ch` */
static uint32_t adc1_read(uint32_t ch)
{
    /* CFGR: 12-bit, right-aligned, single */
    ADC1_CFGR = 0u;
    /* SMPR1: SMP for ch (ch ≤ 9): 3 bits at [ch*3+2 : ch*3]
     * ch=15 is in SMPR2... but let's use SMPR1 for channels 0-9
     * ch 15 is in SMPR2 (channels 10-18 in SMPR2)
     * Use SMP=111 (160.5 cycles) for all */
    /* SQR1: L=0 (1 conversion), SQ1=ch at [10:6] */
    ADC1_SQR1 = (ch & 0x1Fu) << 6u;

    /* SMPR2 for channels 10-18 */
    /* ADC1_SMPR2 at ADC1_BASE + 0x18 */
    volatile uint32_t *smpr2 = (volatile uint32_t *)(ADC1_BASE + 0x18u);
    /* Channel 15: bits[17:15] of SMPR2 = SMP for ch15 (= ch-10 = 5) */
    /* SMP bits at [3*(ch-10)+2 : 3*(ch-10)] */
    uint32_t smp_shift = 3u * (ch - 10u);
    *smpr2 |= (7u << smp_shift);  /* SMP=111 */

    /* Start conversion */
    ADC1_CR |= ADC_CR_ADSTART;

    /* Wait EOC */
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (ADC1_ISR & ADC_ISR_EOC) break;
    }
    if (t >= TIMEOUT) {
        return 0xFFFFFFFFu;
    }

    return ADC1_DR & 0xFFFu;
}

int main(void)
{
    ael_mailbox_init();

    /* Clocks: GPIOBEN + ADC12EN */
    RCC_AHB2ENR1 |= (1u << 1u) | RCC_AHB2ENR1_ADC12EN;
    volatile uint32_t dummy = RCC_AHB2ENR1;
    (void)dummy;

    /* PB0: analog input (MODER=11) — no pull */
    GPIOB_MODER |= (3u << 0u);

    /* PB1: GPIO output push-pull */
    GPIOB_MODER  &= ~(3u << 2u);
    GPIOB_MODER  |=  (1u << 2u);
    GPIOB_OTYPER &= ~(1u << 1u);
    GPIOB_OSPEEDR|=  (3u << 2u);

    /* ADC setup: synchronous clock (CKMODE=01), no CCIPR3 needed */
    ADC_CCR = ADC_CCR_CKMODE_HCLK1;

    /* Exit deep power-down */
    ADC1_CR = 0u;   /* clear DEEPPWD if set */

    /* Enable voltage regulator */
    ADC1_CR = ADC_CR_ADVREGEN;
    /* Wait ≥20µs (500 iters @ 4MHz) */
    for (volatile uint32_t t = 0u; t < 500u; t++) {}

    /* Run calibration (single-ended) */
    ADC1_CR |= ADC_CR_ADCAL;
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (!(ADC1_CR & ADC_CR_ADCAL)) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE002u, ADC1_CR);
            while (1) {}
        }
    }

    /* Enable ADC */
    ADC1_ISR = ADC_ISR_ADRDY;
    ADC1_CR |= ADC_CR_ADEN;
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (ADC1_ISR & ADC_ISR_ADRDY) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE003u, ADC1_ISR);
            while (1) {}
        }
    }

    /* Test 1: drive PB1 HIGH → read PB0 (ADC1_IN15), expect ~4095 */
    GPIOB_ODR |= (1u << 1u);
    for (volatile uint32_t t = 0u; t < 5000u; t++) {}

    uint32_t raw_high = adc1_read(15u);
    if (raw_high == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE004u, 0u);
        while (1) {}
    }
    if (raw_high < 3500u) {
        AEL_MAILBOX->detail0 = raw_high;
        ael_mailbox_fail(0xE005u, raw_high);
        while (1) {}
    }

    /* Test 2: drive PB1 LOW → read PB0, expect ~0 */
    GPIOB_ODR &= ~(1u << 1u);
    for (volatile uint32_t t = 0u; t < 5000u; t++) {}

    uint32_t raw_low = adc1_read(15u);
    if (raw_low == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE004u, 1u);
        while (1) {}
    }
    if (raw_low > 500u) {
        AEL_MAILBOX->detail0 = raw_low;
        ael_mailbox_fail(0xE006u, raw_low);
        while (1) {}
    }

    /* detail0 = [high_raw:16 | low_raw:16] */
    AEL_MAILBOX->detail0 = (raw_high << 16u) | raw_low;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
