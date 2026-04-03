/*
 * stm32h563rgt6_dac_adc
 *
 * DAC1 channel 1 output → wire → ADC1 channel 1 input:
 *   PA4 (DAC1_OUT1, analog)  → wire → PA1 (ADC1_INP1, analog)
 *
 * Test sequence:
 *   A. DAC = 2048 (mid-scale, ~1.65V) → ADC expect 1800..2300
 *   B. DAC = 3900 (near full,  ~3.14V) → ADC expect > 3500
 *   C. DAC =  100 (near zero,  ~0.08V) → ADC expect < 400
 *
 * DAC MCR = 0 (normal mode, output buffer enabled, output on PA4).
 * ADC1 channel 1, 12-bit, 814.5-cycle sampling.
 * Clock: HSI 64 MHz default.
 *
 * FAIL codes:
 *   0xE001 — ADRDY timeout
 *   0xE002 — mid-scale read out of range, detail0 = raw
 *   0xE003 — high-scale read too low,    detail0 = raw
 *   0xE004 — low-scale  read too high,   detail0 = raw
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_AHB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x064u))

/* GPIOA (AHB2 bit0) */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))

/* DAC1 (AHB2 bit11) — base 0x42028400 */
#define DAC_BASE        0x42028400u
#define DAC_CR          (*(volatile uint32_t *)(DAC_BASE + 0x00u))
#define DAC_DHR12R1     (*(volatile uint32_t *)(DAC_BASE + 0x08u))
#define DAC_MCR         (*(volatile uint32_t *)(DAC_BASE + 0x3Cu))

/* ADC1 (AHB2 bit10) — base 0x42028000 */
#define ADC1_BASE       0x42028000u
#define ADC1_ISR        (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR         (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_CFGR       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC1_SMPR1      (*(volatile uint32_t *)(ADC1_BASE + 0x14u))  /* SMP0..SMP9 */
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

/* DAC_CR bit */
#define DAC_CR_EN1        (1u << 0)

#define TIMEOUT         1000000u

static uint32_t adc_convert(void)
{
    ADC1_ISR = ADC_ISR_EOC;
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

    /* 1. Enable GPIOA (bit0), ADC12 (bit10), DAC12 (bit11) clocks */
    RCC_AHB2ENR |= (1u << 0) | (1u << 10) | (1u << 11);
    (void)RCC_AHB2ENR;

    /* 2. Configure PA4 as analog (MODER bits[9:8] = 11) for DAC1_OUT1 */
    GPIOA_MODER |= (3u << 8);

    /* 3. Configure PA1 as analog (MODER bits[3:2] = 11) for ADC1_INP1 */
    GPIOA_MODER |= (3u << 2);

    /* 4. Configure DAC1: MCR=0 (normal mode, output buffer on PA4), enable CH1 */
    DAC_MCR = 0u;
    DAC_CR  = DAC_CR_EN1;
    for (volatile uint32_t t = 0; t < 2000; t++) {}  /* DAC startup */

    /* 5. Reset ADC block for clean state */
    RCC_AHB2RSTR |= (1u << 10);
    for (volatile uint32_t t = 0; t < 100; t++) {}
    RCC_AHB2RSTR &= ~(1u << 10);
    for (volatile uint32_t t = 0; t < 100; t++) {}

    /* 6. ADC1: clear DEEPPWD, enable voltage regulator */
    ADC1_CR &= ~ADC_CR_DEEPPWD;
    ADC1_CR |= ADC_CR_ADVREGEN;
    for (volatile uint32_t t = 0; t < 2000; t++) {}

    /* 7. Configure ADC1: channel 1 (PA1 = ADC1_INP1)
     *    SMPR1: SMP1 at bits[5:3] = 7 (814.5 cycles)
     *    SQR1:  L=0 (1 conversion), SQ1=1 at bits[10:6] */
    ADC1_CFGR = 0u;
    ADC1_SMPR1 = (7u << 3);   /* SMP1 */
    ADC1_SQR1  = (1u << 6);

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

    /* ---- Test A: mid-scale 2048 → expect 1800..2300 ---- */
    DAC_DHR12R1 = 2048u;
    for (volatile uint32_t t = 0; t < 20000; t++) {}   /* settle */
    uint32_t raw_mid = adc_convert();
    AEL_MAILBOX->detail0 = raw_mid;
    if (raw_mid < 1800u || raw_mid > 2300u) {
        ael_mailbox_fail(0xE002u, raw_mid);
        while (1) {}
    }

    /* ---- Test B: near full 3900 → expect > 3500 ---- */
    DAC_DHR12R1 = 3900u;
    for (volatile uint32_t t = 0; t < 20000; t++) {}
    uint32_t raw_high = adc_convert();
    if (raw_high < 3500u) {
        ael_mailbox_fail(0xE003u, raw_high);
        while (1) {}
    }

    /* ---- Test C: near zero 100 → expect < 400 ---- */
    DAC_DHR12R1 = 100u;
    for (volatile uint32_t t = 0; t < 20000; t++) {}
    uint32_t raw_low = adc_convert();
    if (raw_low > 400u) {
        ael_mailbox_fail(0xE004u, raw_low);
        while (1) {}
    }

    /* PASS: detail0 = mid raw in upper 16 bits, low raw in lower 16 */
    AEL_MAILBOX->detail0 = ((raw_mid & 0xFFFFu) << 16) | (raw_low & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
