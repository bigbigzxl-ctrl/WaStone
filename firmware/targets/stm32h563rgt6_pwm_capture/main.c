/*
 * stm32h563rgt6_pwm_capture
 *
 * TIM3 CH1 generates PWM on PC6 (AF2) → wire → PC7 (AF2) captured by TIM3 CH2.
 * Same timer, PWM output on CH1, input capture on CH2.
 *
 * PWM parameters (TIM clock = APB1 × 1 = 64 MHz after reset):
 *   PSC = 63  → timer clock = 64MHz/64 = 1 MHz
 *   ARR = 999 → period = 1000 µs → 1 kHz
 *   CCR1 = 499 → 50% duty
 *
 * CH2 captures rising edge of TI2 (PC7 = TIM3_CH2_IN).
 * Wait for CC2 capture flag, read CCR2.
 * Expected CCR2 ≈ ARR+1 = 1000 counts (±20%).
 *
 * Clocks: TIM3 = APB1L bit1, GPIOC = AHB2 bit2.
 * HIGH_PRIORITY 27de4499: write EGR.UG=1 after setting PSC/ARR.
 *
 * FAIL codes:
 *   0xE001 — CC2IF timeout (no capture)
 *   0xE002 — CCR2 out of range [800, 1200], detail0 = CCR2
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))

/* GPIOC */
#define GPIOC_BASE      0x42020800u
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00u))
#define GPIOC_OSPEEDR   (*(volatile uint32_t *)(GPIOC_BASE + 0x08u))
#define GPIOC_AFRL      (*(volatile uint32_t *)(GPIOC_BASE + 0x20u))  /* PC0-PC7 */

/* TIM3 (APB1L bit1) */
#define TIM3_BASE       0x40000400u
#define TIM3_CR1        (*(volatile uint32_t *)(TIM3_BASE + 0x00u))
#define TIM3_EGR        (*(volatile uint32_t *)(TIM3_BASE + 0x14u))
#define TIM3_CCMR1      (*(volatile uint32_t *)(TIM3_BASE + 0x18u))
#define TIM3_CCER       (*(volatile uint32_t *)(TIM3_BASE + 0x20u))
#define TIM3_PSC        (*(volatile uint32_t *)(TIM3_BASE + 0x28u))
#define TIM3_ARR        (*(volatile uint32_t *)(TIM3_BASE + 0x2Cu))
#define TIM3_CCR1       (*(volatile uint32_t *)(TIM3_BASE + 0x34u))
#define TIM3_CCR2       (*(volatile uint32_t *)(TIM3_BASE + 0x38u))
#define TIM3_SR         (*(volatile uint32_t *)(TIM3_BASE + 0x10u))

#define TIM_CR1_CEN     (1u << 0)
#define TIM_CR1_ARPE    (1u << 7)
#define TIM_EGR_UG      (1u << 0)
#define TIM_SR_CC2IF    (1u << 2)

/* CCMR1:
 *   bits[1:0]  = CC1S=00 (CH1 as output)
 *   bits[6:4]  = OC1M=110 (PWM mode 1)
 *   bit3       = OC1PE=1 (output preload enable)
 *   bits[9:8]  = CC2S=01 (CH2 mapped to TI2=PC7)
 *   bits[11:10]= IC2PSC=0
 *   bits[15:12]= IC2F=0 (no filter)
 */
#define CCMR1_VAL  ((0u << 0) | (1u << 3) | (6u << 4) | (1u << 8))

/* CCER:
 *   bit0 = CC1E=1 (CH1 output enable)
 *   bit4 = CC2E=1 (CH2 capture enable)
 *   bit5 = CC2P=0 (rising edge)
 */
#define CCER_VAL  ((1u << 0) | (1u << 4))

#define TIMEOUT  5000000u   /* ~5s at 64 MHz */

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOC (AHB2 bit2), TIM3 (APB1L bit1) */
    RCC_AHB2ENR  |= (1u << 2);
    RCC_APB1LENR |= (1u << 1);
    (void)RCC_AHB2ENR;
    (void)RCC_APB1LENR;

    /* PC6=TIM3_CH1(AF2), PC7=TIM3_CH2(AF2)
     * MODER bits[15:12] = 1010 (AF=10 for PC6,PC7)
     * AFRL bits[31:24] = 0x22 (AF2 for PC7@bits[31:28], PC6@bits[27:24]) */
    GPIOC_MODER   = (GPIOC_MODER   & ~(0xFu << 12)) | (0xAu << 12);
    GPIOC_OSPEEDR = (GPIOC_OSPEEDR & ~(0xFu << 12)) | (0xAu << 12);
    GPIOC_AFRL    = (GPIOC_AFRL    & ~(0xFFu << 24)) | (0x22u << 24);

    /* Configure TIM3 */
    TIM3_CR1  = 0u;
    TIM3_CCMR1 = CCMR1_VAL;
    TIM3_CCER  = 0u;             /* disable before config */
    TIM3_PSC  = 63u;             /* 64MHz / 64 = 1MHz */
    TIM3_ARR  = 999u;            /* 1kHz */
    TIM3_CCR1 = 499u;            /* 50% duty */

    /* HIGH_PRIORITY 27de4499: write EGR.UG=1 after PSC to force reload */
    TIM3_EGR  = TIM_EGR_UG;
    TIM3_SR   = 0u;              /* clear update flag set by UG */

    TIM3_CCER = CCER_VAL;
    TIM3_CR1  = TIM_CR1_CEN | TIM_CR1_ARPE;

    /* Wait for first rising edge capture on TI2 (PC7) */
    uint32_t t;
    for (t = 0; t < TIMEOUT; t++) {
        if (TIM3_SR & TIM_SR_CC2IF) break;
    }
    if (!(TIM3_SR & TIM_SR_CC2IF)) {
        ael_mailbox_fail(0xE001u, TIM3_SR);
        while (1) {}
    }
    uint32_t first = TIM3_CCR2;   /* reading CCR2 clears CC2IF */

    /* Wait for second rising edge */
    for (t = 0; t < TIMEOUT; t++) {
        if (TIM3_SR & TIM_SR_CC2IF) break;
    }
    if (!(TIM3_SR & TIM_SR_CC2IF)) {
        ael_mailbox_fail(0xE001u, 2u);
        while (1) {}
    }
    uint32_t second = TIM3_CCR2;

    /* Period in timer counts.
     * Normal case: second > first.
     * Same value (both 0 when edge aligns with counter overflow):
     *   period = ARR+1 = 1000. */
    uint32_t period;
    if (second == first) {
        period = TIM3_ARR + 1u;           /* edge aligns with overflow */
    } else if (second > first) {
        period = second - first;
    } else {
        period = second + (TIM3_ARR + 1u) - first;
    }

    if (period < 800u || period > 1200u) {
        ael_mailbox_fail(0xE002u, period);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = period;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
