/*
 * stm32u585_adc4_internal_mailbox — ADC4 reads DAC1_OUT1 via internal channel
 * STM32U585CIU6, MSI 4MHz
 *
 * No external wire needed.
 * DAC1_OUT1 internally routed to ADC4:
 *   Rev.A silicon (REVID=0x1000): ADC4 channel 20
 *   Rev.B silicon (REVID=0x2000): ADC4 channel 21, ADC4_OR[0]=0
 *
 * Key change vs previous attempts:
 *   ADC4 uses SECURE alias 0x56021000 (previously used NS 0x46021000)
 *   ADC4_COMMON uses SECURE alias 0x56021308
 *   DAC1 uses SECURE alias 0x56021800 (same as before)
 *
 * DAC1 outputs 0x800 (~1.65 V). Expected ADC raw ≈ 2048.
 * Accept range: [1400, 2800].
 *
 * FAIL codes:
 *   0xE001 — ADRDY timeout (detail0 = ADC4_ISR; detail1 = REVID<<16 | ADC4_CR)
 *   0xE002 — EOC timeout
 *   0xE003 — raw out of range [1400, 2800] (detail0 = raw)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC (NS alias — works for clock enable) */
#define RCC_BASE        0x46020C00u
#define RCC_AHB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x094u))

/* DBGMCU — to read silicon revision */
#define DBGMCU_IDCODE   (*(volatile uint32_t *)0xE0044000u)

/* DAC1 — AHB3 SECURE alias */
#define DAC1_BASE       0x56021800u
#define DAC1_CR         (*(volatile uint32_t *)(DAC1_BASE + 0x00u))
#define DAC1_DHR12R1    (*(volatile uint32_t *)(DAC1_BASE + 0x08u))

/* ADC4 — AHB3 SECURE alias (was 0x46021000 before — key change) */
#define ADC4_BASE       0x56021000u
#define ADC4_ISR        (*(volatile uint32_t *)(ADC4_BASE + 0x00u))
#define ADC4_CR         (*(volatile uint32_t *)(ADC4_BASE + 0x08u))
#define ADC4_CFGR1      (*(volatile uint32_t *)(ADC4_BASE + 0x0Cu))
#define ADC4_SMPR       (*(volatile uint32_t *)(ADC4_BASE + 0x14u))
#define ADC4_CHSELR     (*(volatile uint32_t *)(ADC4_BASE + 0x28u))
#define ADC4_DR         (*(volatile uint32_t *)(ADC4_BASE + 0x40u))
#define ADC4_OR         (*(volatile uint32_t *)(ADC4_BASE + 0xD0u))

/* ADC4 COMMON — SECURE alias */
#define ADC4_CCR        (*(volatile uint32_t *)0x56021308u)

/* ADC4_CR bits */
#define ADC4_CR_ADEN      (1u << 0u)
#define ADC4_CR_ADSTART   (1u << 2u)
#define ADC4_CR_ADVREGEN  (1u << 28u)
#define ADC4_CR_DEEPPWD   (1u << 29u)

/* ADC4_ISR bits */
#define ADC4_ISR_ADRDY    (1u << 0u)
#define ADC4_ISR_EOC      (1u << 2u)

#define TIMEOUT_ITERS   2000000u
#define DAC_MID         0x800u   /* ~1.65 V at 3.3 V VDDA */

int main(void)
{
    ael_mailbox_init();

    /* Read silicon revision for channel selection */
    uint32_t revid = (DBGMCU_IDCODE >> 16u) & 0xFFFFu;
    /* Rev.A=0x1000: DAC1OUT1=ch20. Rev.B=0x2000: DAC1OUT1=ch21 + OR[0]=0 */
    uint32_t dac_ch = (revid == 0x1000u) ? 20u : 21u;

    /* Diagnostics scratch */
    (*(volatile uint32_t *)0x20007F10u) = revid;
    (*(volatile uint32_t *)0x20007F14u) = dac_ch;

    /* Enable ADC4 + DAC1 clocks */
    RCC_AHB3ENR |= (1u << 5u) | (1u << 6u);  /* ADC4EN | DAC1EN */
    { volatile uint32_t d = RCC_AHB3ENR; (void)d; }

    /* DAC1 CH1: enable, no trigger, output mid-scale */
    DAC1_CR      = (1u << 0u);   /* EN1=1 */
    DAC1_DHR12R1 = DAC_MID;
    for (volatile uint32_t t = 0u; t < 500u; t++) {}

    /* ADC4: clean init via DEEPPWD reset sequence */
    ADC4_CR = ADC4_CR_DEEPPWD;
    for (volatile uint32_t t = 0u; t < 20u; t++) {}
    ADC4_CR = 0u;                  /* clear DEEPPWD */

    (*(volatile uint32_t *)0x20007F18u) = ADC4_CR;  /* expect 0 */

    ADC4_CR = ADC4_CR_ADVREGEN;    /* enable LDO */
    for (volatile uint32_t t = 0u; t < 2000u; t++) {}

    (*(volatile uint32_t *)0x20007F1Cu) = ADC4_CR;  /* expect 0x10000000 */

    /* Configure: DAC1OUT1 internal channel, max sampling time */
    ADC4_CFGR1  = 0u;
    ADC4_SMPR   = 7u;              /* SMP=111 (max) */
    ADC4_CHSELR = (1u << dac_ch);  /* internal DAC1CH1 channel */
    if (dac_ch == 21u) {
        ADC4_OR = 0u;              /* OR[0]=0 → DAC1CH1 (not CH2) */
    }

    /* Enable ADC */
    ADC4_ISR = ADC4_ISR_ADRDY;
    ADC4_CR |= ADC4_CR_ADEN;

    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT_ITERS; t++) {
            if (ADC4_ISR & ADC4_ISR_ADRDY) break;
        }
        if (t >= TIMEOUT_ITERS) {
            ael_mailbox_fail(0xE001u, ADC4_ISR);
            while (1) {}
        }
    }

    /* Start conversion */
    ADC4_CR |= ADC4_CR_ADSTART;
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT_ITERS; t++) {
            if (ADC4_ISR & ADC4_ISR_EOC) break;
        }
        if (t >= TIMEOUT_ITERS) {
            ael_mailbox_fail(0xE002u, ADC4_ISR);
            while (1) {}
        }
    }

    uint32_t raw = ADC4_DR & 0xFFFu;
    AEL_MAILBOX->detail0 = raw;

    if (raw < 1400u || raw > 2800u) {
        ael_mailbox_fail(0xE003u, raw);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
