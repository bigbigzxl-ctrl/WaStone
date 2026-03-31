/*
 * STM32F407VET6 — ADC1 internal temperature sensor
 *
 * Reads the internal temperature sensor (ADC1 channel 16).
 * Expected range at ambient: ~15–55 °C.
 * PASS if converted temperature is in that range.
 * detail0 = raw ADC count (0–4095), detail1 (error_code) = raw when out of range.
 *
 * No external wiring needed.
 * Mailbox: 0x2001FC00
 * Clock: 16 MHz HSI (ADCCLK = PCLK2/2 = 8 MHz, within 0.6–36 MHz spec).
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* RCC */
#define RCC_BASE    0x40023800U
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x44U))

/* ADC common (F407 RM0090 §13.13) */
#define ADC_CCR     (*(volatile uint32_t *)(0x40012304U))
#define ADC_CCR_TSVREFE (1U << 23)   /* temperature sensor & Vrefint enable */

/* ADC1 */
#define ADC1_BASE   0x40012000U
#define ADC1_SR     (*(volatile uint32_t *)(ADC1_BASE + 0x00U))
#define ADC1_CR1    (*(volatile uint32_t *)(ADC1_BASE + 0x04U))
#define ADC1_CR2    (*(volatile uint32_t *)(ADC1_BASE + 0x08U))
#define ADC1_SMPR1  (*(volatile uint32_t *)(ADC1_BASE + 0x0CU))
#define ADC1_SQR1   (*(volatile uint32_t *)(ADC1_BASE + 0x2CU))
#define ADC1_SQR3   (*(volatile uint32_t *)(ADC1_BASE + 0x34U))
#define ADC1_DR     (*(volatile uint32_t *)(ADC1_BASE + 0x4CU))

#define ADC_SR_EOC      (1U << 1)
#define ADC_CR2_ADON    (1U << 0)
#define ADC_CR2_SWSTART (1U << 30)

/* ~1 ms delay at 16 MHz */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0U; i < ms; i++) {
        for (volatile uint32_t d = 0U; d < 4000U; d++) {
            __asm__ volatile ("nop");
        }
    }
}

int main(void)
{
    RCC_APB2ENR |= (1U << 8U);  /* ADC1EN */
    (void)RCC_APB2ENR;

    ael_mailbox_init();

    /* Enable temperature sensor */
    ADC_CCR |= ADC_CCR_TSVREFE;

    /*
     * Configure ADC1: 12-bit, single, channel 16 (temperature sensor).
     * SMPR1[20:18] = 111 → 480-cycle sampling (required for temp sensor).
     * SQR3[4:0] = 16 → first conversion is channel 16.
     * SQR1[23:20] = 0 → sequence length = 1.
     */
    ADC1_CR1  = 0U;
    ADC1_CR2  = 0U;
    ADC1_SQR1 = 0U;
    ADC1_SQR3 = 16U;
    ADC1_SMPR1 |= (7U << 18U);   /* ch16: 480 cycles */
    ADC1_CR2  = ADC_CR2_ADON;
    delay_ms(10U);                /* ADC stabilisation (tstab ≥ 3 µs) */

    /* Start conversion */
    ADC1_CR2 |= ADC_CR2_SWSTART;
    uint32_t timeout = 2000000U;
    while ((ADC1_SR & ADC_SR_EOC) == 0U) {
        if (--timeout == 0U) {
            ael_mailbox_fail(0xDEAD0001U, 0U);
            while (1) {}
        }
    }
    uint32_t raw = ADC1_DR & 0xFFFU;
    AEL_MAILBOX->detail0 = raw;

    /*
     * Convert to °C (RM0090 §13.10):
     *   V_sense = raw × 3.3 / 4096  (V)
     *   Temperature = ((V_sense − 0.76) / 0.0025) + 25
     *
     * Using integer arithmetic (×1000 to keep precision):
     *   V_mV = raw × 3300 / 4096
     *   T_mC = ((V_mV - 760) × 1000 / 2500) + 25000  (millidegrees)
     *
     * Expected range: 15–55 °C → 15000–55000 mC
     * Corresponding raw range: ~760–1007 counts (rough)
     */
    uint32_t v_mv = (raw * 3300U) / 4096U;
    int32_t  t_mc;
    if (v_mv >= 760U) {
        t_mc = (int32_t)(((v_mv - 760U) * 1000U) / 2500U) * 1000 + 25000;
    } else {
        t_mc = 25000 - (int32_t)(((760U - v_mv) * 1000U) / 2500U) * 1000;
    }

    /* Accept 10–70 °C (generous margin for bench conditions) */
    if (t_mc < 10000 || t_mc > 70000) {
        ael_mailbox_fail(0x01U, raw);
        while (1) {}
    }

    ael_mailbox_pass();
    uint32_t n = 0U;
    while (1) {
        for (volatile uint32_t d = 0U; d < 4000U; d++) { __asm__ volatile("nop"); }
        AEL_MAILBOX->detail0 = ++n;
    }
}
