/*
 * stm32h563rgt6_adc_loopback
 *
 * GPIO loopback into ADC:
 *   PC2 (push-pull output)  → wire → PC1 (ADC1_INP11 analog input)
 *
 * Wiring: PC1 ↔ PC2 (physically connected on bench).
 * PC2 drives HIGH/LOW; PC1 reads back via ADC1 channel 11.
 * (Original wiring was PC0→PC1; changed to PC2→PC1 to match bench.)
 *
 * Test:
 *   1. Drive PC2 HIGH → ADC read INP11, expect > 3500  (≈VDD)
 *   2. Drive PC2 LOW  → ADC read INP11, expect < 500   (≈GND)
 *
 * Clock: HSI 64 MHz default.
 * ADC kernel clock: default async (uses HCLK via ADCSEL).
 *
 * FAIL codes:
 *   0xE001 — ADRDY timeout
 *   0xE002 — HIGH read too low (< 3500), detail0 = raw
 *   0xE003 — LOW  read too high (> 500), detail0 = raw
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_AHB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x064u))

/* GPIOC (AHB2) */
#define GPIOC_BASE      0x42020800u
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00u))
#define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x18u))

/* ADC1 (AHB2) */
#define ADC1_BASE       0x42028000u
#define ADC1_ISR        (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR         (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_CFGR       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC1_SMPR2      (*(volatile uint32_t *)(ADC1_BASE + 0x18u))  /* SMP10..SMP18 */
#define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x30u))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x40u))

/* ADC_CR bits */
#define ADC_CR_ADEN       (1u << 0)
#define ADC_CR_ADSTART    (1u << 2)
#define ADC_CR_ADVREGEN   (1u << 28)
#define ADC_CR_DEEPPWD    (1u << 29)

/* ADC_ISR bits */
#define ADC_ISR_ADRDY     (1u << 0)
#define ADC_ISR_EOC       (1u << 2)

#define TIMEOUT         1000000u

static uint32_t adc_read_channel11(void)
{
    ADC1_ISR = ADC_ISR_EOC;       /* clear EOC */
    ADC1_CR |= ADC_CR_ADSTART;
    uint32_t t;
    for (t = 0; t < TIMEOUT; t++) {
        if (ADC1_ISR & ADC_ISR_EOC) break;
    }
    if (t >= TIMEOUT) return 0xFFFFFFFFu;
    return ADC1_DR & 0xFFFu;
}

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPIOC clock (AHB2 bit2) */
    RCC_AHB2ENR |= (1u << 2);
    (void)RCC_AHB2ENR;

    /* 2. Configure PC2 as push-pull output (MODER bits[5:4] = 01) */
    GPIOC_MODER = (GPIOC_MODER & ~(3u << 4)) | (1u << 4);
    /* 3. Configure PC1 as analog input (MODER bits[3:2] = 11) */
    GPIOC_MODER |= (3u << 2);

    /* 4. Enable ADC12 clock (AHB2 bit10) */
    RCC_AHB2ENR |= (1u << 10);
    (void)RCC_AHB2ENR;

    /* 5. Reset ADC block for clean state */
    RCC_AHB2RSTR |= (1u << 10);
    for (volatile uint32_t t = 0; t < 100; t++) {}
    RCC_AHB2RSTR &= ~(1u << 10);
    for (volatile uint32_t t = 0; t < 100; t++) {}

    /* 6. Clear DEEPPWD, enable voltage regulator */
    ADC1_CR &= ~ADC_CR_DEEPPWD;
    ADC1_CR |= ADC_CR_ADVREGEN;
    for (volatile uint32_t t = 0; t < 2000; t++) {}  /* ≥20µs regulator startup */

    /* 7. Configure ADC1: single, 12-bit, channel 11
     *    SMPR2: SMP11 at bits[5:3] = 7 (814.5 cycles)
     *    SQR1:  L=0 (1 conversion), SQ1=11 at bits[10:6] */
    ADC1_CFGR = 0u;
    ADC1_SMPR2 = (7u << 3);    /* SMP11 */
    ADC1_SQR1  = (11u << 6);

    /* 8. Clear ADRDY, enable ADC */
    ADC1_ISR = ADC_ISR_ADRDY;
    ADC1_CR |= ADC_CR_ADEN;

    /* 9. Wait ADRDY */
    {
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (ADC1_ISR & ADC_ISR_ADRDY) break;
        }
        if (!(ADC1_ISR & ADC_ISR_ADRDY)) {
            ael_mailbox_fail(0xE001u, ADC1_CR);
            while (1) {}
        }
    }

    /* 10. Drive PC2 HIGH, settle, read ADC */
    GPIOC_BSRR = (1u << 2);                       /* BS2 = set PC2 high */
    for (volatile uint32_t t = 0; t < 10000; t++) {}  /* ~150µs settle */

    uint32_t raw_high = adc_read_channel11();
    AEL_MAILBOX->detail0 = raw_high;

    if (raw_high < 3500u) {
        ael_mailbox_fail(0xE002u, raw_high);
        while (1) {}
    }

    /* 11. Drive PC2 LOW, settle, read ADC */
    GPIOC_BSRR = (1u << 18);                      /* BR2 = reset PC2 low */
    for (volatile uint32_t t = 0; t < 10000; t++) {}  /* ~150µs settle */

    uint32_t raw_low = adc_read_channel11();
    AEL_MAILBOX->detail0 = (raw_high << 16) | (raw_low & 0xFFFFu);

    if (raw_low > 500u) {
        ael_mailbox_fail(0xE003u, raw_low);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
