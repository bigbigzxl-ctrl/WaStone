/*
 * STM32F407 — TIM4 PWM Output + Input Capture Loopback
 *
 * TIM4 CH3 output: PB8 (AF2) — 1 kHz, 50% duty PWM
 * TIM4 CH4 input:  PB9 (AF2) — input capture, measures period
 *
 * Wire required: PB8 ↔ PB9 (one jumper wire)
 *
 * Clock: 16 MHz HSI. APB1 prescaler = 1. TIM4 clock = 16 MHz.
 *   PSC = 15  →  1 MHz tick (1 µs resolution)
 *   ARR = 999 →  1 kHz period (1000 µs)
 *   CCR3= 500 →  50% duty
 *
 * Capture: CH4 captures rising edge of TI4 (= PB9).
 * Period measured = CCR4[n] - CCR4[n-1].
 * Accept range: [900, 1100] µs (±10%).
 * PASS after N_CAPTURES=10 consecutive valid captures.
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = last measured period (µs, should be ~1000)
 *   FAIL: error_code = ERR_CAP_TIMEOUT(1) or ERR_PERIOD(2)
 *         detail0   = measured period (µs, 0 on timeout)
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* HardFault: SYSRESETREQ instead of LOCKUP (pattern ef195d1e) */
void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---------------------------------------------------------------- */
#define RCC_BASE        0x40023800u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30u))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40u))

/* ---- GPIOB -------------------------------------------------------------- */
#define GPIOB_BASE  0x40020400u
#define GPIOB_MODER (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_AFRH  (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

/* ---- TIM4 (APB1, base 0x40000800) --------------------------------------- */
#define TIM4_BASE  0x40000800u
#define TIM4_CR1   (*(volatile uint32_t *)(TIM4_BASE + 0x00u))
#define TIM4_SMCR  (*(volatile uint32_t *)(TIM4_BASE + 0x08u))
#define TIM4_DIER  (*(volatile uint32_t *)(TIM4_BASE + 0x0Cu))
#define TIM4_SR    (*(volatile uint32_t *)(TIM4_BASE + 0x10u))
#define TIM4_EGR   (*(volatile uint32_t *)(TIM4_BASE + 0x14u))
#define TIM4_CCMR1 (*(volatile uint32_t *)(TIM4_BASE + 0x18u))
#define TIM4_CCMR2 (*(volatile uint32_t *)(TIM4_BASE + 0x1Cu))
#define TIM4_CCER  (*(volatile uint32_t *)(TIM4_BASE + 0x20u))
#define TIM4_CNT   (*(volatile uint32_t *)(TIM4_BASE + 0x24u))
#define TIM4_PSC   (*(volatile uint32_t *)(TIM4_BASE + 0x28u))
#define TIM4_ARR   (*(volatile uint32_t *)(TIM4_BASE + 0x2Cu))
#define TIM4_CCR3  (*(volatile uint32_t *)(TIM4_BASE + 0x3Cu))
#define TIM4_CCR4  (*(volatile uint32_t *)(TIM4_BASE + 0x40u))

#define TIM_SR_CC4IF  (1u << 4)
#define TIM_CR1_CEN   (1u << 0)
#define TIM_EGR_UG    (1u << 0)

#define ERR_CAP_TIMEOUT 1u
#define ERR_PERIOD      2u

#define N_CAPTURES   10u
#define CAP_TIMEOUT  5000000u
#define PERIOD_MIN   900u
#define PERIOD_MAX   1100u

int main(void)
{
    /* Enable GPIOB (bit1) and TIM4 (bit2) clocks */
    RCC_AHB1ENR |= (1u << 1);
    (void)RCC_AHB1ENR;
    RCC_APB1ENR |= (1u << 2);
    (void)RCC_APB1ENR;

    /* PB8, PB9: AF mode (10b).
     * MODER: PB8=[17:16], PB9=[19:18] */
    GPIOB_MODER &= ~((3u << 16) | (3u << 18));
    GPIOB_MODER |=  ((2u << 16) | (2u << 18));
    /* AFRH: AF2 for PB8 [3:0], AF2 for PB9 [7:4] */
    GPIOB_AFRH &= ~((0xFu << 0) | (0xFu << 4));
    GPIOB_AFRH |=  ((0x2u << 0) | (0x2u << 4));

    /* TIM4 config */
    TIM4_CR1  = 0u;
    TIM4_PSC  = 15u;      /* 16MHz / 16 = 1 MHz tick */
    TIM4_ARR  = 999u;     /* 1 kHz period */
    TIM4_CCR3 = 500u;     /* 50% duty on CH3 */

    /* CCMR2:
     * CH3 (bits [7:0]): CC3S=00 (output), OC3M=110 (PWM1), OC3PE=1
     * CH4 (bits[15:8]): CC4S=01 (IC4 mapped to TI4 = PB9 input) */
    TIM4_CCMR2 = (1u << 3)   /* OC3PE */
               | (6u << 4)   /* OC3M = PWM mode 1 */
               | (1u << 8);  /* CC4S = 01 → IC4 on TI4 */

    /* CCER: CC3E=1 (enable CH3 output), CC4E=1 (enable CH4 capture) */
    TIM4_CCER = (1u << 8) | (1u << 12);

    /* Update registers (PSC double-buffer, pattern 27de4499) */
    TIM4_EGR = TIM_EGR_UG;

    /* Start timer */
    TIM4_CR1 = TIM_CR1_CEN;

    ael_mailbox_init();

    /* Capture N_CAPTURES periods, verify each is within ±10% of 1000µs */
    uint32_t prev_ccr4 = 0u;
    uint32_t good       = 0u;
    uint32_t last_period = 0u;

    /* Discard first capture (no previous reference) */
    {
        uint32_t t = CAP_TIMEOUT;
        while (!(TIM4_SR & TIM_SR_CC4IF)) {
            if (--t == 0u) {
                ael_mailbox_fail(ERR_CAP_TIMEOUT, 0u);
                while (1) {}
            }
        }
        prev_ccr4 = TIM4_CCR4;  /* clears CC4IF on read */
    }

    for (uint32_t i = 0u; i < N_CAPTURES; i++) {
        uint32_t t = CAP_TIMEOUT;
        while (!(TIM4_SR & TIM_SR_CC4IF)) {
            if (--t == 0u) {
                ael_mailbox_fail(ERR_CAP_TIMEOUT, last_period);
                while (1) {}
            }
        }
        uint32_t cur = TIM4_CCR4;  /* clears CC4IF */
        /* Handle 16-bit counter wrap (TIM4 on F407 is 16-bit) */
        uint32_t period = (cur - prev_ccr4) & 0xFFFFu;
        prev_ccr4 = cur;
        last_period = period;
        AEL_MAILBOX->detail0 = period;

        if (period >= PERIOD_MIN && period <= PERIOD_MAX) {
            good++;
        }
    }

    if (good == N_CAPTURES) {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = last_period;
    } else {
        ael_mailbox_fail(ERR_PERIOD, last_period);
    }

    while (1) {}
}
