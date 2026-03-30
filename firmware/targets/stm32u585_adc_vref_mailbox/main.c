/*
 * stm32u585_adc_vref_mailbox — AEL ADC4 VREFINT channel test
 * STM32U585CIU6, MSI 4MHz default clock
 *
 * Reads the internal voltage reference (VREFINT) on ADC4 channel 19.
 * VREFINT nominal = 1.212 V.  At VDDA=3.3V with 12-bit ADC:
 *   raw_nominal ≈ 1212 × 4096 / 3300 ≈ 1505
 * Accept range: 1200–1800 (wide margin covers VDDA variation).
 *
 * detail0  = raw ADC reading
 * error_code stored on PASS = vref_cal value (factory, at 3.0V) for debug
 *
 * FAIL codes:
 *   0xE001 — ADRDY timeout (ADC failed to enable)
 *   0xE002 — EOC timeout   (conversion never completed)
 *   0xE003 — raw out of range [1200, 1800]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x094u))

/* ADC4: AHB3, RCC_AHB3ENR bit 5 */
#define ADC4_BASE       0x46021000u
#define ADC4_ISR        (*(volatile uint32_t *)(ADC4_BASE + 0x00u))
#define ADC4_CR         (*(volatile uint32_t *)(ADC4_BASE + 0x08u))
#define ADC4_CFGR1      (*(volatile uint32_t *)(ADC4_BASE + 0x0Cu))
#define ADC4_SMPR       (*(volatile uint32_t *)(ADC4_BASE + 0x14u))
#define ADC4_CHSELR     (*(volatile uint32_t *)(ADC4_BASE + 0x28u))
#define ADC4_DR         (*(volatile uint32_t *)(ADC4_BASE + 0x40u))

/* ADC4 common registers */
#define ADC4_CCR        (*(volatile uint32_t *)0x46021308u)

/* ADC4_CR bits */
#define ADC4_CR_ADEN      (1u << 0u)
#define ADC4_CR_ADDIS     (1u << 1u)
#define ADC4_CR_ADSTART   (1u << 2u)
#define ADC4_CR_ADVREGEN  (1u << 28u)
#define ADC4_CR_DEEPPWD   (1u << 29u)

/* ADC4_ISR bits */
#define ADC4_ISR_ADRDY    (1u << 0u)
#define ADC4_ISR_EOC      (1u << 2u)

/* ADC4_CCR bits */
#define ADC4_CCR_VREFEN   (1u << 22u)

/* Factory VREFINT calibration — system flash, measured at VDDA=3.0V */
#define VREFINT_CAL_ADDR  0x0BFA07A5u

#define TIMEOUT_ITERS   1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable ADC4 clock (AHB3, bit 5) */
    RCC_AHB3ENR |= (1u << 5u);
    volatile uint32_t dummy = RCC_AHB3ENR;
    (void)dummy;

    /* 1b. Force DEEPPWD=1 to reset all ADC registers to default, then clear it.
     *     This gives a clean slate regardless of any leftover state from a
     *     previous run (ADEN, ADSTART, ADVREGEN all get cleared by hardware). */
    ADC4_CR = ADC4_CR_DEEPPWD;
    for (volatile uint32_t t = 0u; t < 10u; t++) {}
    ADC4_CR = 0u;   /* clear DEEPPWD */

    /* 2. Enable ADC voltage regulator */
    ADC4_CR = ADC4_CR_ADVREGEN;

    /* 3. Wait ≥20 µs regulator startup */
    for (volatile uint32_t t = 0u; t < 500u; t++) {}

    /* 4. Enable VREFINT in common registers */
    ADC4_CCR |= ADC4_CCR_VREFEN;

    /* 5. Short stabilization delay */
    for (volatile uint32_t t = 0u; t < 200u; t++) {}

    /* 6. Configure ADC4
     *    CFGR1 = 0  → 12-bit, single, right-aligned
     *    SMPR  = 7  → SMP=111 (160.5 cycles)
     *    CHSELR: channel 19 (VREFINT)
     */
    ADC4_CFGR1  = 0u;
    ADC4_SMPR   = 7u;
    ADC4_CHSELR = (1u << 19u);

    /* 7. Enable ADC */
    ADC4_CR |= ADC4_CR_ADEN;

    /* 8. Wait ADRDY */
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

    /* 9. Start conversion */
    ADC4_CR |= ADC4_CR_ADSTART;

    /* 10. Wait EOC */
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

    /* 11. Read raw value */
    uint32_t raw = ADC4_DR & 0xFFFu;

    /* 12. Read factory calibration (for debug, stored in error_code on PASS) */
    uint16_t vref_cal = *(volatile uint16_t *)VREFINT_CAL_ADDR;

    /* 13. Range check:
     *     raw_nominal (VDDA=3.3V) ≈ 1505
     *     Accept [1200, 1800] — wide margin for VDDA tolerance  */
    if (raw <= 1200u || raw >= 1800u) {
        AEL_MAILBOX->detail0    = raw;
        AEL_MAILBOX->error_code = (uint32_t)vref_cal;
        AEL_MAILBOX->status     = AEL_STATUS_FAIL;
        while (1) {}
    }

    /* 14. PASS — raw in detail0, factory cal in error_code (debug, not an error) */
    AEL_MAILBOX->detail0    = raw;
    AEL_MAILBOX->error_code = (uint32_t)vref_cal;
    ael_mailbox_pass();

    while (1) {}
    return 0;
}
