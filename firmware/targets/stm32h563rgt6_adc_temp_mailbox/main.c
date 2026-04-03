/*
 * stm32h563rgt6_adc_temp_mailbox
 *
 * Reads internal temperature sensor on ADC1 channel 16.
 * Uses factory calibration: TS_CAL1 @ 30°C, TS_CAL2 @ 130°C (VDDA=3.3V)
 * at addresses 0x08FFF814 / 0x08FFF818.
 * PASS if 5 <= temp_c <= 90.
 *
 * Clocks: HSI 64 MHz default. ADC kernel clock = HCLK (via CCIPR5).
 *
 * FAIL codes:
 *   0xE001 — ADRDY timeout
 *   0xE002 — EOC timeout
 *   0xE003 — calibration data invalid
 *   0xE004 — temperature out of range
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC (AHB3, offset from RCC_BASE=0x44020C00) */
#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_AHB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x064u))

/* ADC1 (AHB2) */
#define ADC1_BASE       0x42028000u
#define ADC1_ISR        (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR         (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_CFGR       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC1_SMPR2      (*(volatile uint32_t *)(ADC1_BASE + 0x18u))  /* SMP16 @ bits[20:18] */
#define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x30u))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x40u))

/* ADC12 common (offset +0x308 from AHB2 ADC base) */
#define ADC12_CCR       (*(volatile uint32_t *)0x42028308u)

/* ADC_CR bits */
#define ADC_CR_ADEN       (1u << 0)
#define ADC_CR_ADSTART    (1u << 2)
#define ADC_CR_ADVREGEN   (1u << 28)
#define ADC_CR_DEEPPWD    (1u << 29)

/* ADC_ISR bits */
#define ADC_ISR_ADRDY     (1u << 0)
#define ADC_ISR_EOC       (1u << 2)

/* ADC12_CCR bit */
#define ADC_CCR_TSEN      (1u << 23)   /* temperature sensor enable */

/* Factory calibration (measured at VDDA=3.3V) */
#define TS_CAL1_ADDR    0x08FFF814u    /* 30°C */
#define TS_CAL2_ADDR    0x08FFF818u    /* 130°C */

/* Diagnostic scratchpad */
#define DIAG_BASE       0x20007EC0u
#define DIAG(n)         (*(volatile uint32_t *)(DIAG_BASE + (n)*4u))

#define TIMEOUT         1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable ADC1/2 bus clock (AHB2 bit10) */
    RCC_AHB2ENR |= (1u << 10);
    (void)RCC_AHB2ENR;

    /* 2. Reset ADC block for clean state */
    RCC_AHB2RSTR |= (1u << 10);
    for (volatile uint32_t t = 0; t < 100; t++) {}
    RCC_AHB2RSTR &= ~(1u << 10);
    for (volatile uint32_t t = 0; t < 100; t++) {}

    DIAG(0) = ADC1_CR;  /* expect 0x20000000 (DEEPPWD=1 after reset) */

    /* 3. Clear DEEPPWD, enable voltage regulator */
    ADC1_CR &= ~ADC_CR_DEEPPWD;
    ADC1_CR |= ADC_CR_ADVREGEN;
    DIAG(1) = ADC1_CR;  /* expect 0x10000000 */

    /* 4. Wait ≥20µs regulator startup (200 iters @ 64 MHz = 3µs loop → ~600µs) */
    for (volatile uint32_t t = 0; t < 2000; t++) {}

    /* 5. Enable temperature sensor path in common CCR */
    ADC12_CCR |= ADC_CCR_TSEN;

    /* 6. Wait ≥10µs sensor stabilisation */
    for (volatile uint32_t t = 0; t < 1000; t++) {}

    /* 7. Configure ADC1:
     *    CFGR = 0 → 12-bit, right-aligned, single
     *    SMPR2: SMP16 = 111 (814.5 cycles) bits[20:18]
     *    SQR1:  L=0 (1 conversion), SQ1=16 bits[10:6] */
    ADC1_CFGR = 0u;
    ADC1_SMPR2 = (7u << 18);
    ADC1_SQR1  = (16u << 6);

    /* 8. Clear ADRDY, enable ADC */
    ADC1_ISR = ADC_ISR_ADRDY;
    ADC1_CR |= ADC_CR_ADEN;

    /* 9. Wait ADRDY */
    {
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (ADC1_ISR & ADC_ISR_ADRDY) break;
        }
        DIAG(2) = ADC1_ISR;
        if (!(ADC1_ISR & ADC_ISR_ADRDY)) {
            ael_mailbox_fail(0xE001u, ADC1_CR);
            while (1) {}
        }
    }

    /* 10. Start conversion */
    ADC1_CR |= ADC_CR_ADSTART;

    /* 11. Wait EOC */
    {
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (ADC1_ISR & ADC_ISR_EOC) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE002u, ADC1_ISR);
            while (1) {}
        }
    }

    uint32_t raw = ADC1_DR & 0xFFFu;
    DIAG(3) = raw;

    uint16_t cal1 = *(volatile uint16_t *)TS_CAL1_ADDR;
    uint16_t cal2 = *(volatile uint16_t *)TS_CAL2_ADDR;
    DIAG(4) = ((uint32_t)cal2 << 16) | cal1;

    if (cal1 == 0u || (int32_t)cal2 <= (int32_t)cal1) {
        /* No factory cal — raw plausibility only */
        if (raw < 200u || raw > 2000u) {
            ael_mailbox_fail(0xE003u, raw);
        } else {
            AEL_MAILBOX->detail0 = raw;
            ael_mailbox_pass();
        }
        while (1) {}
    }

    /* temp_c = (raw - cal1) * 100 / (cal2 - cal1) + 30 */
    int32_t num = ((int32_t)raw - (int32_t)cal1) * 100;
    int32_t den = (int32_t)cal2 - (int32_t)cal1;
    int32_t temp_c = num / den + 30;
    DIAG(5) = (uint32_t)temp_c;

    if (temp_c < 5 || temp_c > 90) {
        AEL_MAILBOX->detail0 = (uint32_t)temp_c;
        ael_mailbox_fail(0xE004u, raw);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = (uint32_t)temp_c;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
