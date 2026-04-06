/*
 * CH32V003 ADC internal Vrefint test
 *
 * Reads ADC channel 8 (Vrefint ≈ 1.2 V).
 * Supply = 3.3 V → expected raw ≈ 372 (10-bit, 0–1023).
 * Accept 200–700 to accommodate supply tolerance.
 * TSVREFE (CTLR2 bit23) must be set to enable internal channels.
 *
 * ADC clock: APB2/8 = 24 MHz/8 = 3 MHz (< 14 MHz max)
 * Sample time: 241 cycles (slowest, best for Vrefint)
 *
 * PASS if result in [200, 700].
 * detail0 = result (liveness: value stays constant ≈ Vref)
 * detail0 bit0 = 0 (toggle_count=0 → use simple PASS check)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

static void adc_init(void)
{
    /* Enable ADC1 clock on APB2 */
    RCC->APB2PCENR |= RCC_ADC1EN;

    /* ADC prescaler: PCLK2/8 (bits[15:14]=0b11 in CFGR0) */
    RCC->CFGR0 = (RCC->CFGR0 & ~(0x3u << 14)) | (0x3u << 14);

    /* Reset ADC */
    RCC->APB2PRSTR |=  RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

    /* CTLR2: ADON=1, TSVREFE=1, EXTSEL=0b111 (software trigger, required for SWSTART)
     * ADC_ExternalTrigConv_None = 0x000E0000 = EXTSEL[19:17]=111 */
    ADC1->CTLR2 = (1u << 0) | (1u << 23) | (0x7u << 17);  /* ADON | TSVREFE | EXTSEL=SW */

    /* Calibration voltage: 50% VDD (CTLR1 bit25) */
    ADC1->CTLR1 |= (1u << 25);

    /* Reset calibration */
    ADC1->CTLR2 |= (1u << 3);   /* RSTCAL */
    while (ADC1->CTLR2 & (1u << 3));

    /* Start calibration */
    ADC1->CTLR2 |= (1u << 2);   /* CAL */
    while (ADC1->CTLR2 & (1u << 2));

    /* SAMPTR1: channel 8 → bits[26:24] = 7 (241.5 cycles) */
    ADC1->SAMPTR1 = (7u << 24);

    /* RSQR3: sequence 1 = channel 8 */
    ADC1->RSQR3 = 8u;

    /* RSQR1: L=0 (1 conversion) */
    ADC1->RSQR1 = 0;
}

static uint16_t adc_read(void)
{
    /* Trigger software start + external trigger enable */
    ADC1->CTLR2 |= (1u << 20) | (1u << 22);  /* EXTTRIG | SWSTART */

    /* Poll EOC (bit1 of STATR) */
    while (!(ADC1->STATR & (1u << 1)));

    return (uint16_t)(ADC1->RDATAR & 0x3FFu);
}

int main(void)
{
    ael_mailbox_init();
    adc_init();

    /* Take 4 readings, average them */
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
        sum += adc_read();
        for (volatile uint32_t j = 0; j < 10000u; j++);
    }
    uint16_t result = (uint16_t)(sum / 4u);

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = (uint32_t)result << 1;  /* bit0=0: toggle_count=0 */

    if (result >= 200u && result <= 700u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail((uint32_t)result, 0);

    /* Liveness: keep reading, update detail0 */
    while (1) {
        uint16_t v = adc_read();
        *detail0 = (uint32_t)v << 1;
        for (volatile uint32_t i = 0; i < 500000u; i++);
    }

    return 0;
}
