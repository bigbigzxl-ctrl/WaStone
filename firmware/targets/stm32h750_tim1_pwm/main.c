/*
 * stm32h750_tim1_pwm — TIM1 hardware PWM output + software period capture
 *
 * TIM1 (APB2 advanced-control timer, base 0x40010000):
 *   PA8 = TIM1_CH1 (AF1) — 1 kHz, 50% duty PWM output
 *   PSC = 63, ARR = 999 → 64 MHz / 64 / 1000 = 1 kHz
 *   CCR1 = 500 (50% duty)
 *   MOE = 1 required (TIM1 main output enable in BDTR)
 *
 * Bench wire PB8↔PB9 is already present.
 * PA8 drives PWM; PB9 reads it (PA8↔PB8↔PB9 chain — PA8 is a new driver
 * replacing PB8 GPIO, so we repurpose PB8 from GPIO output to floating input
 * and use PB9 as the capture input via the existing bench wire).
 *
 * Wait: actually PB8 and PA8 are separate pins. The bench wire is PB8↔PB9.
 * We output PWM on PA8 (TIM1_CH1). We need to verify PA8's output.
 * But PA8 is not connected to the instrument.
 *
 * Alternative: Use TIM1_CH1 = PA8, and measure on PB8 which is wired to PB9.
 * Set PB8 as GPIO input and connect PA8→PB8 externally? No, that requires
 * a new wire.
 *
 * Better approach: Use TIM1 CH1 on PA8, and capture the period on PB9
 * by reading PB9 in software — but PA8 is not connected to PB9.
 *
 * Simplest clean option: output PWM on PB8 using TIM1_CH1N (complementary
 * output) — but TIM1_CH1N is not on PB8.
 *
 * Actually, check: TIM1 channels on available H750 pins:
 *   TIM1_CH1  = PA8 (AF1)
 *   TIM1_CH1N = PA7 (AF1), PB13 (AF1), PE8 (AF1)
 *   TIM1_CH2  = PA9 (AF1) — already used by UART
 *   TIM1_CH3  = PA10 (AF1) — already used by UART
 *
 * PB8 is the output side of our bench wire (PB8→PB9).
 * We can use TIM1 in a self-measuring mode without needing the wire:
 *   Output PWM on PA8, then use TIM2 free-running counter to measure
 *   the PA8 signal directly by polling GPIOA IDR bit 8 (input mode).
 *
 * Reconfigure PA8 sequence:
 *   1. Set PA8 as AF1 (TIM1_CH1) to output PWM
 *   2. Also poll PA8 IDR in a loop to measure the period
 *      (TIM1 output drives PA8 and we can read back the same pin in IDR)
 *   Actually on STM32, the IDR of a pin in AF mode reads the actual pin
 *   state — so we can output PWM on PA8 AF1 and still read IDR to verify.
 *
 * RCC: APB2ENR bit 0 = TIM1EN. APB2 clock = 64 MHz (no APB prescaler).
 * TIM1 tick clock = HCLK × 2 if APB prescaler ≠ 1, but APB2 = HCLK/1 here,
 * so TIM1 clock = APB2 = 64 MHz (no doubling needed).
 *
 * TIM2 (free-running): APB1LENR bit 0 = TIM2EN, TIM2 at 64 MHz.
 *
 * Error codes:
 *   0xE001 = rising edge timeout on PA8 (edge 1)
 *   0xE002 = rising edge timeout on PA8 (edge 2)
 *   0xE003 = period out of range (detail0 = measured counts)
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE         0x58024400u
#define RCC_AHB4ENR      (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_APB2ENR      (*(volatile uint32_t *)(RCC_BASE + 0x0F0u))
#define RCC_APB1LENR     (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))
#define RCC_AHB4ENR_GPIOAEN  (1u << 0u)
#define RCC_APB2ENR_TIM1EN   (1u << 0u)
#define RCC_APB1LENR_TIM2EN  (1u << 0u)

/* ── GPIOA ───────────────────────────────────────────────────────── */
#define GPIOA_BASE    0x58020000u
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRH    (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))
#define GPIOA_IDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x10u))

/* ── TIM1 (APB2, 0x40010000) ─────────────────────────────────────── */
#define TIM1_BASE   0x40010000u
#define TIM1_CR1    (*(volatile uint32_t *)(TIM1_BASE + 0x00u))
#define TIM1_EGR    (*(volatile uint32_t *)(TIM1_BASE + 0x14u))
#define TIM1_CCMR1  (*(volatile uint32_t *)(TIM1_BASE + 0x18u))
#define TIM1_CCER   (*(volatile uint32_t *)(TIM1_BASE + 0x20u))
#define TIM1_PSC    (*(volatile uint32_t *)(TIM1_BASE + 0x28u))
#define TIM1_ARR    (*(volatile uint32_t *)(TIM1_BASE + 0x2Cu))
#define TIM1_CCR1   (*(volatile uint32_t *)(TIM1_BASE + 0x34u))
#define TIM1_BDTR   (*(volatile uint32_t *)(TIM1_BASE + 0x44u))

#define TIM_CR1_ARPE    (1u << 7u)   /* auto-reload preload enable */
#define TIM_CR1_CEN     (1u << 0u)
#define TIM_EGR_UG      (1u << 0u)   /* generate update event (load PSC/ARR) */
#define TIM_CCMR1_OC1M_PWM1  (0x6u << 4u)   /* OC1M = 110: PWM mode 1 */
#define TIM_CCMR1_OC1PE      (1u << 3u)     /* OC1 preload enable */
#define TIM_CCER_CC1E   (1u << 0u)   /* CH1 output enable */
#define TIM_BDTR_MOE    (1u << 15u)  /* main output enable (required for TIM1) */

/* ── TIM2 (APB1L, 0x40000000) — free-running 32-bit counter ─────── */
#define TIM2_BASE   0x40000000u
#define TIM2_CR1    (*(volatile uint32_t *)(TIM2_BASE + 0x00u))
#define TIM2_PSC    (*(volatile uint32_t *)(TIM2_BASE + 0x28u))
#define TIM2_ARR    (*(volatile uint32_t *)(TIM2_BASE + 0x2Cu))
#define TIM2_CNT    (*(volatile uint32_t *)(TIM2_BASE + 0x24u))
#define TIM2_EGR    (*(volatile uint32_t *)(TIM2_BASE + 0x14u))

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0u)
#define SYST_CSR_CLKSOURCE (1u << 2u)
#define SYST_CSR_COUNTFLAG (1u << 16u)

static void delay_ticks(uint32_t ticks)
{
    for (volatile uint32_t i = 0u; i < ticks; i++) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    /* SysTick: 64 MHz, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Enable GPIOA, TIM1, TIM2 clocks */
    RCC_AHB4ENR  |= RCC_AHB4ENR_GPIOAEN;
    (void)RCC_AHB4ENR;
    RCC_APB2ENR  |= RCC_APB2ENR_TIM1EN;
    (void)RCC_APB2ENR;
    RCC_APB1LENR |= RCC_APB1LENR_TIM2EN;
    (void)RCC_APB1LENR;

    /*
     * PA8 = TIM1_CH1 (AF1): alternate function, high speed.
     * MODER[17:16] = 10, AFRH[3:0] = 1 (AF1).
     */
    GPIOA_MODER &= ~(0x3u << 16u);
    GPIOA_MODER |=  (0x2u << 16u);
    GPIOA_OSPEEDR |= (0x3u << 16u);
    GPIOA_AFRH &= ~(0xFu << 0u);
    GPIOA_AFRH |=  (0x1u << 0u);   /* AF1 */
    (void)GPIOA_MODER;

    /*
     * TIM1 configuration:
     *   PSC = 63 → timer clock = 64 MHz / 64 = 1 MHz
     *   ARR = 999 → 1 MHz / 1000 = 1 kHz PWM
     *   CCR1 = 500 → 50% duty
     *   CCMR1: OC1M=PWM mode 1, OC1PE=1
     *   CCER: CC1E=1 (CH1 output enable)
     *   BDTR: MOE=1 (main output enable — mandatory for TIM1/TIM8)
     */
    TIM1_PSC   = 63u;
    TIM1_ARR   = 999u;
    TIM1_CCR1  = 500u;
    TIM1_CCMR1 = TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;
    TIM1_CCER  = TIM_CCER_CC1E;
    TIM1_BDTR  = TIM_BDTR_MOE;
    TIM1_CR1   = TIM_CR1_ARPE;
    TIM1_EGR   = TIM_EGR_UG;        /* load PSC/ARR/CCR1 immediately */
    TIM1_CR1  |= TIM_CR1_CEN;       /* start */

    /*
     * TIM2 free-running 32-bit counter at 64 MHz (PSC=0, ARR=0xFFFFFFFF).
     * Used to measure PA8 PWM period.
     */
    TIM2_PSC = 0u;
    TIM2_ARR = 0xFFFFFFFFu;
    TIM2_EGR = TIM_EGR_UG;
    TIM2_CR1 = TIM_CR1_CEN;

    delay_ticks(2u);   /* let TIM1 run for 2 ms before sampling */

    /*
     * Measure period of PA8 output by reading GPIOA IDR bit 8.
     * On STM32, IDR reads the actual pin voltage even in AF mode.
     * Wait for rising edge 1 → record T1.
     * Wait for falling edge → ignore.
     * Wait for rising edge 2 → record T2.
     * Period = T2 - T1 (in 64 MHz counts).
     * Expected: ~64000 counts = 1 ms = 1 kHz.
     * Accept range: 57600..70400 (±10%).
     */
    uint32_t t1 = 0u, t2 = 0u;
    uint32_t timeout = 10000000u;

    /* Wait for low (to guarantee we catch the next rising edge) */
    while (((GPIOA_IDR >> 8u) & 1u) != 0u) {
        if (--timeout == 0u) { ael_mailbox_fail(0xE001u, 0u); while (1) {} }
    }
    /* Wait for rising edge 1 */
    timeout = 10000000u;
    while (((GPIOA_IDR >> 8u) & 1u) == 0u) {
        if (--timeout == 0u) { ael_mailbox_fail(0xE001u, 1u); while (1) {} }
    }
    t1 = TIM2_CNT;

    /* Wait for falling edge */
    timeout = 10000000u;
    while (((GPIOA_IDR >> 8u) & 1u) != 0u) {
        if (--timeout == 0u) { ael_mailbox_fail(0xE001u, 2u); while (1) {} }
    }
    /* Wait for rising edge 2 */
    timeout = 10000000u;
    while (((GPIOA_IDR >> 8u) & 1u) == 0u) {
        if (--timeout == 0u) { ael_mailbox_fail(0xE002u, TIM2_CNT); while (1) {} }
    }
    t2 = TIM2_CNT;

    uint32_t period = t2 - t1;   /* counts @ 64 MHz */

    /* Accept 1 kHz ± 10%: 900 Hz..1100 Hz → 58182..71111 counts */
    if ((period < 57600u) || (period > 70400u)) {
        ael_mailbox_fail(0xE003u, period);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = period;   /* measured period in 64 MHz counts */
    while (1) {}
}
