/*
 * CH32V003 ADC + DMA test — zero wiring
 *
 * Strategy (revised — Vrefint/ch8 not usable on CH32V003 due to TSVREFE
 * write-only bit being silently cleared by calibration, leaving ch8 floating
 * near VCC = 1023.  Instead, drive PA1 as push-pull output LOW and use
 * ADC ch1 (PA1) as the ADC input source.  PA1 driven LOW → ~0V → reading
 * should be 0..100.  This tests the full ADC + DMA path without internal
 * references.
 *
 * ADC ch1 (PA1) driven LOW, 16-entry SCAN sequence.
 * DMA1 Channel1 reads ADC1->RDATAR → adc_buf[16].
 * After 16 DMA transfers (TC flag), verify all samples < 200.
 *
 * PASS if all 16 samples are < 200.
 * detail0 = adc_buf[0] on fail.
 *
 * CH32V003 ADC+DMA notes:
 *   - DMA1 Ch1 is hardware-mapped to ADC1.
 *   - RDATAR at ADC1_BASE + 0x4C = 0x4001244C.
 *   - SCAN=1 (CTLR1 bit8) required for multi-entry regular sequence.
 *   - CTLR2.DMA=1 (bit8) set AFTER calibration (RDATAR ≈ 0x3FF during cal).
 *   - ch32v003fun.c startup BSS clearing is broken for initialized globals;
 *     use static (zero-init) arrays only.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

#define BUF_SIZE  16u

/* .bss — zero-init static: ch32v003fun.c BSS bug workaround */
static volatile uint32_t adc_buf[BUF_SIZE];

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOA (for PA1), DMA1, ADC1 clocks */
    RCC->APB2PCENR |= RCC_IOPAEN | RCC_ADC1EN;
    RCC->AHBPCENR  |= RCC_DMA1EN;

    /* ADC prescaler: PCLK2/8 */
    RCC->CFGR0 = (RCC->CFGR0 & ~(0x3u << 14)) | (0x3u << 14);

    /* PA1: push-pull output 10 MHz, drive LOW → ~0V ADC input */
    GPIOA->OUTDR &= ~(1u << 1);                                    /* PA1 = 0 */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFu << 4)) | (0x1u << 4);   /* PA1: MODE=01(10MHz), CNF=00(push-pull) */

    /* Reset ADC */
    RCC->APB2PRSTR |=  RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;

    /* CTLR2: ADON | EXTSEL=software (no TSVREFE needed for external ch) */
    ADC1->CTLR2 = (1u << 0) | (0x7u << 17);

    /* CTLR1: SCAN=1 for 16-entry regular sequence */
    ADC1->CTLR1 = (1u << 8);   /* SCAN */

    /* Calibration reference 50% VDD */
    ADC1->CTLR1 |= (1u << 25);

    /* Reset calibration */
    ADC1->CTLR2 |= (1u << 3);
    while (ADC1->CTLR2 & (1u << 3));

    /* Start calibration */
    ADC1->CTLR2 |= (1u << 2);
    while (ADC1->CTLR2 & (1u << 2));

    /* Sample time ch1 in SAMPTR2 bits[5:3] (SMP1 field) = 7 (241.5 cycles) */
    ADC1->SAMPTR2 = (7u << 3);

    /* 16-entry regular sequence: all SQ1-SQ16 = ch1 (PA1) */
    ADC1->RSQR3 = (1u)       | (1u << 5)  | (1u << 10) | (1u << 15) | (1u << 20) | (1u << 25);  /* SQ1-SQ6 = ch1 */
    ADC1->RSQR2 = (1u)       | (1u << 5)  | (1u << 10) | (1u << 15) | (1u << 20) | (1u << 25);  /* SQ7-SQ12 = ch1 */
    ADC1->RSQR1 = (15u << 20)| (1u)       | (1u << 5)  | (1u << 10) | (1u << 15);               /* L=15, SQ13-SQ16 = ch1 */

    /* DMA1 Channel1 configuration (ADC1 DMA):
     *   Source: ADC1->RDATAR = 0x4001244C (32-bit, no increment)
     *   Dest:   adc_buf[] (32-bit, memory increment)
     *   Count:  16
     *   Prio:   HIGH, MEM2MEM=0, CIRC=0, MINC=1, PINC=0
     */
    DMA1_Channel1->PADDR  = (uint32_t)&ADC1->RDATAR;    /* peripheral: RDATAR */
    DMA1_Channel1->MADDR  = (uint32_t)adc_buf;          /* memory: adc_buf */
    DMA1_Channel1->CNTR   = BUF_SIZE;
    DMA1_Channel1->CFGR   = (0x2u << 12)   /* PL=HIGH */
                           | (0x2u << 10)   /* MSIZE=32-bit */
                           | (0x2u << 8)    /* PSIZE=32-bit */
                           | (1u   << 7)    /* MINC=1 (memory increment) */
                           | (0u   << 6)    /* PINC=0 */
                           | (0u   << 5)    /* CIRC=0 */
                           | (0u   << 4)    /* DIR=0 (periph→memory) */
                           | (1u   << 0);   /* EN=1 */

    /* Enable ADC DMA requests (bit8 of CTLR2) — AFTER calibration */
    ADC1->CTLR2 |= (1u << 8);

    /* Start 16-conversion SCAN sequence: EXTTRIG | SWSTART */
    ADC1->CTLR2 |= (1u << 20) | (1u << 22);

    /* Wait for DMA Transfer Complete (TC flag for Ch1 in INTFR bit1) */
    while (!(DMA1->INTFR & (1u << 1)));

    /* Verify all 16 samples < 200 (PA1 driven LOW → should be ~0-50) */
    uint32_t bad = 0;
    for (uint32_t k = 0; k < BUF_SIZE; k++) {
        uint32_t v = adc_buf[k] & 0x3FFu;
        if (v >= 200u) { bad = k + 1u; break; }
    }

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = adc_buf[0] & 0x3FFu;

    if (bad == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(bad, adc_buf[0] & 0x3FFu);

    /* Liveness: count-DOWN (immune to WCH OpenOCD GPR corruption) */
    while (1) {
        for (volatile uint32_t i = 500000u; i > 0u; i--);
        *detail0 ^= 2u;
    }

    return 0;
}
