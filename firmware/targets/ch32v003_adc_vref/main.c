/*
 * CH32V003 ADC basic conversion test — zero wiring
 *
 * Strategy: PA1 = push-pull output LOW → ADC ch1 (PA1).
 * Ch8/Vrefint (TSVREFE) is not reliably accessible on CH32V003 — TSVREFE
 * is write-only (reads as 0), and ch8 reads 1023 regardless.
 * Instead drive PA1 LOW → read ch1 → expect near 0.
 *
 * 4 readings, average. PASS if avg < 100.
 * detail0 = result (upper bits toggle in liveness loop)
 *
 * ADC clock: APB2/8 = 24 MHz/8 = 3 MHz
 * Sample time: SMP1 = 7 (241.5 cycles)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

static void adc_init(void)
{
    /* Enable ADC1 + GPIOA clocks */
    RCC->APB2PCENR |= RCC_ADC1EN | RCC_IOPAEN;

    /* ADC prescaler: PCLK2/8 */
    RCC->CFGR0 = (RCC->CFGR0 & ~(0x3u << 14)) | (0x3u << 14);

    /* PA1 = push-pull output LOW (ADC ch1 input) */
    GPIOA->OUTDR &= ~(1u << 1);
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFu << 4)) | (0x1u << 4);

    /* Reset ADC */
    RCC->APB2PRSTR |=  RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

    /* ADON + EXTSEL=software */
    ADC1->CTLR2 = (1u << 0) | (0x7u << 17);

    /* CALVOL: 50% VDD */
    ADC1->CTLR1 = (1u << 25);
    ADC1->CTLR1 = (1u << 8) | (1u << 25);  /* SCAN | CALVOL */

    /* Calibration — use direct write, no |= so we control all bits */
    ADC1->CTLR2 = (1u << 0) | (0x7u << 17) | (1u << 3);   /* ADON|EXTSEL|RSTCAL */
    while (ADC1->CTLR2 & (1u << 3));
    ADC1->CTLR2 = (1u << 0) | (0x7u << 17) | (1u << 2);   /* ADON|EXTSEL|CAL */
    while (ADC1->CTLR2 & (1u << 2));

    /* SMP ch1 = 241.5 cycles (bits[5:3] = 7) */
    ADC1->SAMPTR2 = (7u << 3);

    /* Sequence: ch1 only */
    ADC1->RSQR3 = 1u;
    ADC1->RSQR1 = 0u;
}

static uint16_t adc_read(void)
{
    /* Direct write — no |=, no TSVREFE needed for ch1 */
    ADC1->CTLR2 = (1u << 0) | (0x7u << 17) | (1u << 20) | (1u << 22);
    while (!(ADC1->STATR & (1u << 1)));  /* poll EOC */
    return (uint16_t)(ADC1->RDATAR & 0x3FFu);
}

int main(void)
{
    ael_mailbox_init();
    adc_init();

    /* 4 readings, average — no delay loops (EOC polling is the delay) */
    uint32_t sum = 0;
    sum += adc_read();
    sum += adc_read();
    sum += adc_read();
    sum += adc_read();
    uint16_t result = (uint16_t)(sum / 4u);

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = (uint32_t)result << 1;

    if (result < 100u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail((uint32_t)result, 0);

    /* Liveness */
    while (1) {
        uint16_t v = adc_read();
        *detail0 = ((uint32_t)v << 1) ^ 2u;  /* toggle bit1 for liveness */
    }

    return 0;
}
