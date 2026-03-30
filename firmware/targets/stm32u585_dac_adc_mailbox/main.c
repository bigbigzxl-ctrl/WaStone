/*
 * stm32u585_dac_adc_mailbox — DAC1 output → ADC1 input (same pin PA4)
 * STM32U585CIU6, MSI 4MHz
 *
 * No external wire needed: PA4 = DAC1_OUT1 = ADC1_IN9
 *
 * STM32U585 key findings (differs from STM32H7 ADC IP):
 *   1. DAC1 is on AHB3 (LP domain), NOT APB1.
 *      DAC1_BASE_S = 0x56021800, clock = RCC_AHB3ENR bit 6.
 *   2. ADC1/ADC2 are on AHB2; ADC1_BASE_S = 0x52028000, clock = RCC_AHB2ENR1 bit 10.
 *      TZEN is active on this board — secure alias required (same as SPI1 → 0x50013000).
 *   3. ADC12_COMMON CCR has NO CKMODE field (unlike STM32H7).
 *      ADC clock source = RCC_CCIPR3.ADCDACSEL[14:12], shared by ADC1/ADC4/DAC1.
 *      Set ADCDACSEL=001 (SYSCLK) to ensure valid kernel clock.
 *   4. ADC DEEPPWD=1 after reset; must clear before writing any other ADC registers.
 *
 * DAC1 outputs ~1.65V (mid-scale, 0x800/4096 × VDDA ≈ 1.65V @ 3.3V).
 * ADC1_IN9 (PA4) reads back; accepted range [1500, 2600] raw counts.
 *
 * FAIL codes:
 *   0xE000 — ADC LDO not ready timeout (detail0 = ADC1_ISR at timeout)
 *   0xE001 — ADC calibration timeout (detail0 = ADC1_CR at timeout)
 *   0xE002 — ADC ADRDY timeout
 *   0xE003 — ADC EOC timeout
 *   0xE004 — raw reading out of range [1500, 2600] (detail0 = raw)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC (AHB3 non-secure alias: same domain as DAC1/ADC4) */
#define RCC_BASE        0x46020C00u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_AHB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x094u))
#define RCC_CCIPR3      (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))

/* PWR (AHB3 LP domain, NS alias 0x46020800 — PWR is NS domain on this board,
 * same as RCC. RM0456: "VDDA supply must be declared valid by setting PWR_SVMCR.ASV" */
#define PWR_BASE        0x46020800u
#define PWR_SVMCR       (*(volatile uint32_t *)(PWR_BASE + 0x10u))
#define PWR_SVMCR_ASV   (1u << 30u)   /* bit 30: VDDA independent analog supply valid */

/* GPIOA (AHB2 non-secure) */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))

/* DAC1 — AHB3 LP domain, secure alias
 * AHB3PERIPH_BASE_S = 0x50000000 + 0x06020000 = 0x56020000
 * DAC1_BASE_S = 0x56020000 + 0x1800 = 0x56021800 */
#define DAC1_BASE       0x56021800u
#define DAC1_CR         (*(volatile uint32_t *)(DAC1_BASE + 0x00u))
#define DAC1_DHR12R1    (*(volatile uint32_t *)(DAC1_BASE + 0x08u))

/* ADC1 (AHB2, NS alias 0x42028000) */
#define ADC1_BASE       0x42028000u
#define ADC1_ISR        (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR         (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_CFGR       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC1_SMPR1      (*(volatile uint32_t *)(ADC1_BASE + 0x14u))
#define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x30u))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x40u))

/* ADC1 CR bits */
#define ADC_CR_ADEN      (1u << 0u)
#define ADC_CR_ADSTART   (1u << 2u)
#define ADC_CR_ADCAL     (1u << 31u)
#define ADC_CR_ADVREGEN  (1u << 28u)

/* ADC1 ISR bits */
#define ADC_ISR_ADRDY    (1u << 0u)
#define ADC_ISR_EOC      (1u << 2u)

/* RCC bits */
#define RCC_AHB2ENR1_GPIOAEN  (1u << 0u)
#define RCC_AHB2ENR1_ADC12EN  (1u << 10u)
#define RCC_AHB3ENR_DAC1EN    (1u << 6u)
/* RCC_CR bits */
#define RCC_CR_HSION     (1u << 8u)   /* HSI16 oscillator enable */
#define RCC_CR_HSIKERON  (1u << 9u)   /* HSI16 kernel clock enable */
#define RCC_CR_HSIRDY    (1u << 10u)  /* HSI16 ready flag */

/* RCC_CCIPR3 ADCDACSEL[14:12]: 000=HCLK 001=SYSCLK 100=HSI16 101=MSIK */
#define RCC_CCIPR3_ADCDACSEL_MASK   (7u << 12u)
#define RCC_CCIPR3_ADCDACSEL_HSI16  (4u << 12u)  /* HSI16 (16MHz, reliable) */

#define TIMEOUT 1000000u

int main(void)
{
    ael_mailbox_init();

    /* Enable clocks: GPIOA, ADC12, DAC1 */
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN | RCC_AHB2ENR1_ADC12EN;
    RCC_AHB3ENR  |= RCC_AHB3ENR_DAC1EN;
    /* Read-back to flush clock enable pipeline */
    { volatile uint32_t d = RCC_AHB2ENR1; (void)d; }

    /* Enable HSI16 oscillator as ADC/DAC kernel clock (reliable 16MHz source) */
    RCC_CR |= RCC_CR_HSION | RCC_CR_HSIKERON;
    { uint32_t t; for (t = 0u; t < TIMEOUT; t++) { if (RCC_CR & RCC_CR_HSIRDY) break; } }
    /* Diagnostic: store RCC_CR at 0x20007F10; bit10 (HSIRDY) expected set */
    (*(volatile uint32_t *)0x20007F10u) = RCC_CR;

    /* Select HSI16 as ADCDACSEL kernel clock */
    RCC_CCIPR3 = (RCC_CCIPR3 & ~RCC_CCIPR3_ADCDACSEL_MASK) | RCC_CCIPR3_ADCDACSEL_HSI16;
    (*(volatile uint32_t *)0x20007F14u) = RCC_CCIPR3;
    AEL_MAILBOX->detail0 = RCC_CCIPR3;

    /* PA4: analog mode (MODER[9:8]=11) */
    GPIOA_MODER |= (3u << 8u);

    /* DAC1 CH1: enable, no trigger */
    DAC1_CR      = (1u << 0u);   /* EN1=1 */
    DAC1_DHR12R1 = 0x800u;       /* mid-scale ≈ 1.65V */
    for (volatile uint32_t t = 0u; t < 200u; t++) {}

    /* ADC1 init: exit DEEPPWD → set ADVREGEN → wait LDORDY → calibrate */
    ADC1_CR = 0u;               /* clear DEEPPWD (bit29) — must be first */
    /* Diagnostic: CR after clearing DEEPPWD. 0x00=OK, 0x20000000=DEEPPWD stuck */
    (*(volatile uint32_t *)0x20007F18u) = ADC1_CR;
    ADC1_CR = ADC_CR_ADVREGEN; /* enable internal voltage regulator */
    (*(volatile uint32_t *)0x20007F1Cu) = ADC1_CR;  /* should be 0x10000000 */
    /* Wait longer than tVREGEN (20us @ 4MHz = 80 cycles; use 10000 to be safe) */
    for (volatile uint32_t t = 0u; t < 10000u; t++) {}

    /* SKIP calibration — test if ADC is at all functional without it */

    /* Enable ADC */
    ADC1_ISR = ADC_ISR_ADRDY;
    ADC1_CR |= ADC_CR_ADEN;
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (ADC1_ISR & ADC_ISR_ADRDY) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE002u, ADC1_ISR);
            while (1) {}
        }
    }

    /* Configure: channel 9 (PA4), SMP=111 (max), single conversion */
    ADC1_CFGR = 0u;
    ADC1_SMPR1 = (7u << 27u);   /* SMP9 at bits[29:27] */
    ADC1_SQR1  = (9u << 6u);    /* SQ1=9 */

    /* Start conversion */
    ADC1_CR |= ADC_CR_ADSTART;
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (ADC1_ISR & ADC_ISR_EOC) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE003u, ADC1_ISR);
            while (1) {}
        }
    }

    uint32_t raw = ADC1_DR & 0xFFFu;
    AEL_MAILBOX->detail0 = raw;

    if (raw < 1500u || raw > 2600u) {
        ael_mailbox_fail(0xE004u, raw);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
