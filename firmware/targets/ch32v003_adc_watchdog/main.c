/*
 * CH32V003 ADC Analog Watchdog test — zero wiring
 *
 * ADC channel 8 (Vrefint ≈ 372/1023 @ 3.3V supply).
 * AWD window: [LTR=500, HTR=900].
 * Vrefint ~372 < LTR=500 → outside window → AWD flag fires.
 *
 * PASS if STATR.AWD (bit0) is set after one conversion.
 * detail0 = ADC result (raw 10-bit, shifted left 1 for liveness bit).
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable ADC1 clock on APB2 */
    RCC->APB2PCENR |= RCC_ADC1EN;

    /* ADC prescaler: PCLK2/8 (bits[15:14]=0b11 in CFGR0) */
    RCC->CFGR0 = (RCC->CFGR0 & ~(0x3u << 14)) | (0x3u << 14);

    /* Reset ADC */
    RCC->APB2PRSTR |=  RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

    /* CTLR2: ADON | TSVREFE | EXTSEL=0b111 (software trigger) */
    ADC1->CTLR2 = (1u << 0) | (1u << 23) | (0x7u << 17);

    /* CTLR1: AWDEN(bit23) | AWDSGL(bit9) | AWDCH=8 (bits[4:0])
     * Single-channel watchdog on channel 8 (Vrefint) */
    ADC1->CTLR1 = (1u << 23) | (1u << 9) | 8u;

    /* Calibration reference voltage: 50% VDD */
    ADC1->CTLR1 |= (1u << 25);

    /* Reset calibration */
    ADC1->CTLR2 |= (1u << 3);
    while (ADC1->CTLR2 & (1u << 3));

    /* Start calibration */
    ADC1->CTLR2 |= (1u << 2);
    while (ADC1->CTLR2 & (1u << 2));

    /* Sample time: channel 8 → bits[26:24] in SAMPTR1 = 7 (241.5 cycles) */
    ADC1->SAMPTR1 = (7u << 24);

    /* Sequence: 1 conversion, channel 8 */
    ADC1->RSQR3 = 8u;
    ADC1->RSQR1 = 0u;

    /* AWD thresholds: HTR=900, LTR=500
     * Vrefint ~372 < LTR=500 → outside [500,900] → AWD fires */
    ADC1->WDHTR = 900u;
    ADC1->WDLTR = 500u;

    /* Trigger one conversion: EXTTRIG | SWSTART */
    ADC1->CTLR2 |= (1u << 20) | (1u << 22);

    /* Wait for EOC (bit1) */
    while (!(ADC1->STATR & (1u << 1)));

    /* Read result and AWD flag before clearing */
    uint32_t statr = ADC1->STATR;
    uint16_t result = (uint16_t)(ADC1->RDATAR & 0x3FFu);

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = (uint32_t)result << 1;

    if (statr & 0x1u)     /* AWD flag set → Vrefint outside [500,900] as expected */
        ael_mailbox_pass();
    else
        ael_mailbox_fail((uint32_t)result, statr);

    /* Liveness: toggle bit1 of detail0 */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 ^= 2u;
    }

    return 0;
}
