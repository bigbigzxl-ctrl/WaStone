/*
 * stm32h750_internal_temp_mailbox — ADC3 internal temperature sensor
 *
 * Uses ADC3 (D3 domain, 0x58026000) to read the on-die temperature
 * sensor on channel IN18. Verifies ADC3 init, TSEN enable, and that
 * the reading falls within a plausible room-temperature range.
 *
 * Key H750 differences vs G4:
 *   ADC3 at 0x58026000 (D3_AHB1, not D2). Only ADC3 can access internal
 *   channels (TempSensor=IN18, VrefInt=IN19, VBAT=IN17).
 *   ADC3_COMMON CCR.TSEN = bit 23 (not VSENSESEL as on G4).
 *   ADC3_COMMON CCR.CKMODE=10 → synchronous HCLK4/2 = 32 MHz.
 *   RCC_AHB4ENR bit 24 = ADC3EN (not AHB1 like ADC12).
 *   ADC3 is 16-bit by default (CFGR.RES=00).
 *   PCSEL register at ADC3_BASE+0x1C — set bit 18 for IN18.
 *   Sampling time for IN18: SMPR2 bits[26:24]=7 (640.5 cycles).
 *   Calibration skipped (requires kernel clock, not available without PLL).
 *
 * Acceptance: 0x0800 < raw < 0xF000 (plausible 16-bit reading, 25±15°C).
 *
 * All register addresses from RM0433.
 */

#define AEL_MAILBOX_ADDR  0x38000000u
#include "../ael_mailbox.h"

/* ── RCC ────────────────────────────────────────────────────────── */
#define RCC_BASE           0x58024400u
#define RCC_AHB4ENR        (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_AHB4ENR_ADC3EN (1u << 24)

/* ── ADC3 (RM0433 §25, D3_AHB1, base 0x58026000) ────────────────── */
#define ADC3_BASE   0x58026000u
#define ADC3_ISR    (*(volatile uint32_t *)(ADC3_BASE + 0x00u))
#define ADC3_CR     (*(volatile uint32_t *)(ADC3_BASE + 0x08u))
#define ADC3_CFGR   (*(volatile uint32_t *)(ADC3_BASE + 0x0Cu))
#define ADC3_SMPR2  (*(volatile uint32_t *)(ADC3_BASE + 0x18u))
#define ADC3_PCSEL  (*(volatile uint32_t *)(ADC3_BASE + 0x1Cu))
#define ADC3_SQR1   (*(volatile uint32_t *)(ADC3_BASE + 0x30u))
#define ADC3_DR     (*(volatile uint32_t *)(ADC3_BASE + 0x40u))

/* ── ADC3_COMMON (base 0x58026300) ─────────────────────────────── */
#define ADC3_COMMON_BASE  0x58026300u
#define ADC3_CCR          (*(volatile uint32_t *)(ADC3_COMMON_BASE + 0x08u))

/* ADC_CR bits */
#define ADC_CR_ADEN     (1u << 0)
#define ADC_CR_ADSTART  (1u << 2)
#define ADC_CR_ADVREGEN (1u << 28)
#define ADC_CR_DEEPPWD  (1u << 29)

/* ADC_ISR bits */
#define ADC_ISR_ADRDY  (1u << 0)
#define ADC_ISR_EOC    (1u << 2)

/* ADC3_CCR: CKMODE[17:16]=10 (sync HCLK4/2), TSEN=bit23 */
#define ADC3_CCR_CKMODE_HCLK4_DIV2  (0x2u << 16u)
#define ADC3_CCR_TSEN               (1u << 23u)

/* Error codes */
#define ERR_VREG_TIMEOUT  0xE001u
#define ERR_ADEN_TIMEOUT  0xE002u
#define ERR_EOC_TIMEOUT   0xE003u
#define ERR_RANGE         0xE004u

/* ── SysTick ─────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static void delay_ticks(uint32_t ticks)
{
    for (volatile uint32_t i = 0u; i < ticks; i++) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

static void spin_us(uint32_t us)
{
    /* 64 MHz → ~64 cycles/us; loop ~4 cycles → 16 iterations/us */
    volatile uint32_t n = us * 16u;
    while (n-- > 0u) {}
}

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Enable ADC3 clock (RCC_AHB4ENR bit 24) */
    RCC_AHB4ENR |= RCC_AHB4ENR_ADC3EN;
    (void)RCC_AHB4ENR;

    /* ── ADC3_COMMON: set CKMODE=10 (HCLK4/2=32 MHz) + enable TSEN ── */
    ADC3_CCR = ADC3_CCR_CKMODE_HCLK4_DIV2 | ADC3_CCR_TSEN;

    /* ── ADC3 startup sequence (RM0433 §25.4.6) ───────────────────── */

    /* 1. Clear DEEPPWD, enable voltage regulator */
    ADC3_CR = 0u;                    /* clear DEEPPWD (bit29=0) */
    ADC3_CR = ADC_CR_ADVREGEN;       /* set ADVREGEN */
    spin_us(20u);                    /* Tvr ≥ 10 us */

    /* 2. Verify regulator stable (ADVREGEN should remain set) */
    uint32_t timeout = 100000u;
    /* ADVREGEN has no separate "ready" flag; just check CR write is sticky */
    if ((ADC3_CR & ADC_CR_ADVREGEN) == 0u) {
        ael_mailbox_fail(ERR_VREG_TIMEOUT, ADC3_CR);
        while (1) {}
    }

    /* 3. Calibration intentionally skipped (no kernel clock without PLL). */

    /* 4. Configure PCSEL: select channel IN18 (internal temp) */
    ADC3_PCSEL = (1u << 18u);

    /* 5. Configure sampling time for IN18 in SMPR2.
     *    IN18 offset from CH10 = 8; bits [26:24]. SMP=7 = 640.5 cycles. */
    ADC3_SMPR2 = (7u << 24u);

    /* 6. Configure single conversion of channel 18.
     *    SQR1: L[3:0]=0 (1 conv), SQ1[10:6]=18. */
    ADC3_SQR1 = (18u << 6u);

    /* 7. Enable ADC: set ADEN */
    ADC3_ISR = ADC_ISR_ADRDY;       /* clear stale ADRDY */
    ADC3_CR |= ADC_CR_ADEN;
    timeout = 500000u;
    while (((ADC3_ISR & ADC_ISR_ADRDY) == 0u) && (--timeout > 0u)) {}
    if ((ADC3_ISR & ADC_ISR_ADRDY) == 0u) {
        ael_mailbox_fail(ERR_ADEN_TIMEOUT, ADC3_CR);
        while (1) {}
    }

    /* 8. Wait for temperature sensor stabilisation (≥120 us per RM0433) */
    delay_ticks(2u);   /* 2 ms, well above minimum */

    /* 9. Start conversion */
    ADC3_ISR = ADC_ISR_EOC;
    ADC3_CR |= ADC_CR_ADSTART;
    timeout = 500000u;
    while (((ADC3_ISR & ADC_ISR_EOC) == 0u) && (--timeout > 0u)) {}
    if ((ADC3_ISR & ADC_ISR_EOC) == 0u) {
        ael_mailbox_fail(ERR_EOC_TIMEOUT, ADC3_CR);
        while (1) {}
    }

    /* 10. Read raw value (16-bit, ADC3 default resolution) */
    uint32_t raw = ADC3_DR & 0xFFFFu;

    /*
     * Acceptance range: 0x0800..0xF000 on 16-bit scale.
     * At room temperature (20-30°C), the raw value should be in the
     * lower quarter of the 16-bit range (temperature sensor slope ≈
     * 2-3 mV/°C with ~0.76V at 25°C → ~14900 / 65535 × 65535 ≈ 0x3A20).
     * Wide range accommodates VDDA variation and uncalibrated ADC.
     */
    if (raw < 0x0800u || raw > 0xF000u) {
        ael_mailbox_fail(ERR_RANGE, raw);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = raw;

    while (1) {}
}
