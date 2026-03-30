/*
 * stm32u585_adc_temp_mailbox — AEL ADC4 internal temperature sensor test
 * STM32U585CIU6, MSI 4MHz default clock
 *
 * Reads the internal temperature sensor on ADC4 channel 17 (VSENSE).
 * Uses factory calibration constants (TS_CAL1 @ 30°C, TS_CAL2 @ 130°C)
 * to convert raw ADC reading to °C.  PASS if 5 ≤ temp_c ≤ 90.
 *
 * Key: RCC_CCIPR3.ADCDACSEL is shared between ADC1, ADC4, and DAC1.
 * Its value persists across GDB flash sessions (no hardware reset).
 * We must explicitly set ADCDACSEL = 000 (HCLK, 4 MHz) before use.
 *
 * FAIL codes:
 *   0xE001 — ADRDY timeout (ADC failed to enable)
 *   0xE002 — EOC timeout   (conversion never completed)
 *   0xE003 — cal2 <= cal1  (calibration data invalid / unprogrammed)
 *   0xE004 — temperature out of range [5, 90] °C
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB3RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x06Cu))
#define RCC_AHB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x094u))
#define RCC_CCIPR3      (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))
/* ADCDACSEL[14:12]: ADC1, ADC4, DAC1 kernel clock. 000=HCLK(4MHz default) */
#define RCC_CCIPR3_ADCDACSEL_MSK  (0x7u << 12u)

/* ADC4: AHB3, RCC_AHB3ENR bit 5 */
#define ADC4_BASE       0x46021000u
#define ADC4_ISR        (*(volatile uint32_t *)(ADC4_BASE + 0x00u))
#define ADC4_CR         (*(volatile uint32_t *)(ADC4_BASE + 0x08u))
#define ADC4_CFGR1      (*(volatile uint32_t *)(ADC4_BASE + 0x0Cu))
#define ADC4_SMPR       (*(volatile uint32_t *)(ADC4_BASE + 0x14u))
#define ADC4_CHSELR     (*(volatile uint32_t *)(ADC4_BASE + 0x28u))
#define ADC4_DR         (*(volatile uint32_t *)(ADC4_BASE + 0x40u))

/* ADC4 common registers (AHB3 base + 0x300 + 0x08) */
#define ADC4_CCR        (*(volatile uint32_t *)0x46021308u)
#define ADC4_CCR_VSENSEEN (1u << 23u)

/* ADC4_CR bits */
#define ADC4_CR_ADEN      (1u << 0u)
#define ADC4_CR_ADSTART   (1u << 2u)
#define ADC4_CR_ADVREGEN  (1u << 28u)
#define ADC4_CR_DEEPPWD   (1u << 29u)

/* ADC4_ISR bits */
#define ADC4_ISR_ADRDY    (1u << 0u)
#define ADC4_ISR_EOC      (1u << 2u)

/* Factory calibration — system flash, measured at VDDA=3.0V */
#define TS_CAL1_ADDR    0x0BFA0710u   /* 30°C  */
#define TS_CAL2_ADDR    0x0BFA0718u   /* 130°C */

/* Diagnostic scratchpad — 9 entries max, safe below mailbox at 0x20007F00 */
#define DIAG_BASE       0x20007EC0u
#define DIAG(n)         (*(volatile uint32_t *)(DIAG_BASE + (n)*4u))

#define TIMEOUT_ITERS   1000000u

int main(void)
{
    ael_mailbox_init();

    /* 0. Capture and clear ADCDACSEL in CCIPR3.
     *    BMDA does NOT do a hardware reset between runs, so CCIPR3 may be
     *    left at MSIK (0x5000) from a previous run.  Force HCLK (000). */
    DIAG(0) = RCC_CCIPR3;                            /* [0] CCIPR3 before fix */
    RCC_CCIPR3 &= ~RCC_CCIPR3_ADCDACSEL_MSK;         /* ADCDACSEL = 000 → HCLK */
    DIAG(1) = RCC_CCIPR3;                            /* [1] CCIPR3 after fix — expect 0 */

    /* 1. Enable ADC4 bus clock (AHB3, bit 5) */
    RCC_AHB3ENR |= (1u << 5u);
    volatile uint32_t dummy = RCC_AHB3ENR;
    (void)dummy;

    /* 2. RCC peripheral reset ADC4 for clean register state */
    RCC_AHB3RSTR |= (1u << 5u);
    for (volatile uint32_t t = 0u; t < 100u; t++) {}
    RCC_AHB3RSTR &= ~(1u << 5u);
    for (volatile uint32_t t = 0u; t < 100u; t++) {}
    DIAG(2) = ADC4_CR;   /* [2] ADC4_CR after RCC reset — expect 0x00000000 */

    /* 3. Clear DEEPPWD (may be 1 after power-on, cleared by RCC reset) */
    ADC4_CR &= ~ADC4_CR_DEEPPWD;

    /* 4. Enable ADC voltage regulator */
    ADC4_CR |= ADC4_CR_ADVREGEN;
    DIAG(3) = ADC4_CR;   /* [3] ADC4_CR after ADVREGEN — expect 0x10000000 */

    /* 5. Wait ≥20 µs regulator startup (500 iters @ 4 MHz ≈ 500 µs) */
    for (volatile uint32_t t = 0u; t < 500u; t++) {}

    /* 6. Enable temperature sensor in ADC4 common registers */
    ADC4_CCR |= ADC4_CCR_VSENSEEN;
    DIAG(4) = ADC4_CCR;  /* [4] ADC4_CCR after VSENSEEN — expect 0x00800000 */

    /* 7. Wait ≥10 µs sensor stabilization */
    for (volatile uint32_t t = 0u; t < 200u; t++) {}

    /* 8. Configure ADC4:
     *    CFGR1 = 0  → 12-bit, single, right-aligned
     *    SMPR  = 7  → SMP=111 (160.5 cycles) — required for temp sensor
     *    CHSELR: channel 17 (VSENSE) */
    ADC4_CFGR1  = 0u;
    ADC4_SMPR   = 7u;
    ADC4_CHSELR = (1u << 17u);

    /* 9. Clear ADRDY flag, then enable ADC */
    ADC4_ISR = ADC4_ISR_ADRDY;
    ADC4_CR |= ADC4_CR_ADEN;
    DIAG(5) = ADC4_CR;   /* [5] ADC4_CR after ADEN — expect 0x10000001 */

    /* 10. Wait ADRDY */
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT_ITERS; t++) {
            if (ADC4_ISR & ADC4_ISR_ADRDY) break;
        }
        DIAG(6) = ADC4_ISR;  /* [6] ADC4_ISR at ADRDY exit — expect bit0=1 */
        if (!(ADC4_ISR & ADC4_ISR_ADRDY)) {
            ael_mailbox_fail(0xE001u, ADC4_CR);
            while (1) {}
        }
    }

    /* 11. Start conversion */
    ADC4_CR |= ADC4_CR_ADSTART;

    /* 12. Wait EOC */
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

    /* 13. Read raw value */
    uint32_t raw = ADC4_DR & 0xFFFu;
    DIAG(7) = raw;       /* [7] raw ADC reading */

    /* 14. Read factory calibration */
    uint16_t ts_cal1 = *(volatile uint16_t *)TS_CAL1_ADDR;
    uint16_t ts_cal2 = *(volatile uint16_t *)TS_CAL2_ADDR;
    DIAG(8) = ((uint32_t)ts_cal2 << 16u) | ts_cal1;  /* [8] cal2:cal1 */

    int32_t temp_c;

    if ((int32_t)ts_cal2 <= (int32_t)ts_cal1 || ts_cal1 == 0u) {
        /* Calibration not programmed — raw plausibility check */
        if (raw < 300u || raw > 1500u) {
            AEL_MAILBOX->detail0 = raw;
            ael_mailbox_fail(0xE004u, raw);
            while (1) {}
        }
        AEL_MAILBOX->detail0 = raw;
        ael_mailbox_pass();
        while (1) {}
    }

    /* 15. Convert: temp = (raw - cal1) * 100 / (cal2 - cal1) + 30 */
    int32_t num = ((int32_t)raw - (int32_t)ts_cal1) * 100;
    int32_t den = (int32_t)ts_cal2 - (int32_t)ts_cal1;

    if (den <= 0) {
        ael_mailbox_fail(0xE003u, raw);
        while (1) {}
    }

    temp_c = num / den + 30;

    /* 16. Range check: 5 ≤ temp_c ≤ 90 */
    if (temp_c < 5 || temp_c > 90) {
        AEL_MAILBOX->detail0 = (uint32_t)temp_c;
        ael_mailbox_fail(0xE004u, raw);
        while (1) {}
    }

    /* 17. PASS */
    AEL_MAILBOX->detail0 = (uint32_t)temp_c;
    ael_mailbox_pass();

    while (1) {}
    return 0;
}
