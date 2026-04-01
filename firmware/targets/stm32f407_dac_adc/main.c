/*
 * STM32F407 — DAC1 → ADC1 Single-Pin Self-Loopback
 *
 * PA4 = DAC1_OUT = ADC1_IN4 (analog, same physical pin).
 * No external wiring required.
 *
 * Protocol:
 *   For each of 4 DAC output levels (0, 33%, 66%, 100%):
 *     - Set DAC1_DHR12R1 to target
 *     - Wait for settling
 *     - Read ADC1_IN4 (single conversion)
 *     - Verify ADC reading within ±10% of expected
 *
 * DAC output: 12-bit, full scale = VDD = 3.3V
 *   0%   → DAC=0,    expect ADC ~0
 *   33%  → DAC=1365, expect ADC ~1365 ±410
 *   66%  → DAC=2730, expect ADC ~2730 ±410
 *   100% → DAC=4095, expect ADC ~4095 ±410
 *
 * Clock: 16 MHz HSI. No PLL.
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = last ADC reading
 *   FAIL: error_code = step (1-4), detail0 = actual ADC reading
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---------------------------------------------------------------- */
#define RCC_BASE    0x40023800u
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30u))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40u))
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x44u))

/* ---- GPIOA -------------------------------------------------------------- */
#define GPIOA_BASE  0x40020000u
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))

/* ---- DAC (base 0x40007400) ---------------------------------------------- */
#define DAC_BASE    0x40007400u
#define DAC_CR      (*(volatile uint32_t *)(DAC_BASE + 0x00u))
#define DAC_DHR12R1 (*(volatile uint32_t *)(DAC_BASE + 0x08u))
#define DAC_DOR1    (*(volatile uint32_t *)(DAC_BASE + 0x2Cu))
#define DAC_CR_EN1  (1u << 0)

/* ---- ADC1 (base 0x40012000) --------------------------------------------- */
#define ADC1_BASE  0x40012000u
#define ADC1_SR    (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR1   (*(volatile uint32_t *)(ADC1_BASE + 0x04u))
#define ADC1_CR2   (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_SMPR2 (*(volatile uint32_t *)(ADC1_BASE + 0x10u))
#define ADC1_SQR3  (*(volatile uint32_t *)(ADC1_BASE + 0x34u))
#define ADC1_DR    (*(volatile uint32_t *)(ADC1_BASE + 0x4Cu))
/* ADC common (base 0x40012300) */
#define ADC_CCR    (*(volatile uint32_t *)0x40012304u)

#define ADC_SR_EOC  (1u << 1)
#define ADC_CR2_ADON (1u << 0)
#define ADC_CR2_SWSTART (1u << 30)

/* SysTick */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)

#define TOLERANCE  410u   /* ±10% of 4096 */
#define SETTLE_MS  5u
#define TIMEOUT    500000u

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0u; i < ms; i++) {
        SYST_CVR = 0u;
        while (!(SYST_CSR & (1u << 16))) {}
    }
}

static uint32_t adc_read(void)
{
    ADC1_SR  &= ~ADC_SR_EOC;
    ADC1_CR2 |= ADC_CR2_SWSTART;
    uint32_t t = TIMEOUT;
    while (!(ADC1_SR & ADC_SR_EOC)) { if (--t == 0u) return 0xFFFFu; }
    return ADC1_DR & 0xFFFu;
}

int main(void)
{
    /* SysTick: 1ms at 16 MHz */
    SYST_RVR = 15999u;
    SYST_CVR = 0u;
    SYST_CSR = (1u << 2) | (1u << 0);

    /* Enable GPIOA, DAC, ADC1 clocks */
    RCC_AHB1ENR |= (1u << 0);              /* GPIOA */
    (void)RCC_AHB1ENR;
    RCC_APB1ENR |= (1u << 29);             /* DAC */
    (void)RCC_APB1ENR;
    RCC_APB2ENR |= (1u << 8);              /* ADC1 */
    (void)RCC_APB2ENR;

    /* PA4: analog mode (11b) for both DAC output and ADC input */
    GPIOA_MODER |= (3u << 8);

    /* DAC: enable CH1, no trigger (software), no output buffer
     * BOFF1=1 (bit1) disables output buffer → better accuracy at full scale */
    DAC_CR = (1u << 1) | DAC_CR_EN1;      /* BOFF1=1, EN1=1 */
    delay_ms(1u);                          /* DAC startup */

    /* ADC: prescaler /4 (ADCPRE=01 → 16/4=4 MHz), 12-bit
     * CH4, sample time = 480 cycles (longest, for DAC settling) */
    ADC_CCR    = (1u << 16);              /* ADCPRE = /4 */
    ADC1_CR1   = 0u;                      /* 12-bit, no scan */
    ADC1_CR2   = ADC_CR2_ADON;
    ADC1_SMPR2 = (7u << 12);             /* CH4: SMP=7 (480 cycles) */
    ADC1_SQR3  = 4u;                     /* sequence: CH4 */
    delay_ms(1u);                         /* ADC stabilise */

    ael_mailbox_init();

    /* Test 4 levels */
    static const uint32_t dac_vals[4] = { 0u, 1365u, 2730u, 4095u };
    uint32_t last_adc = 0u;

    for (uint32_t step = 0u; step < 4u; step++) {
        uint32_t dv = dac_vals[step];
        DAC_DHR12R1 = dv;
        delay_ms(SETTLE_MS);

        uint32_t av = adc_read();
        last_adc = av;
        AEL_MAILBOX->detail0 = av;

        /* Check within ±TOLERANCE of DAC value */
        int32_t diff = (int32_t)av - (int32_t)dv;
        if (diff < 0) { diff = -diff; }
        if ((uint32_t)diff > TOLERANCE) {
            ael_mailbox_fail(step + 1u, av);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = last_adc;

    while (1) {}
}
