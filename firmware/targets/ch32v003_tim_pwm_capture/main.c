/*
 * CH32V003 TIM1 PWM output + EXTI capture loopback
 *
 * PD2 = TIM1_CH1 PWM output (AF push-pull, no remap)
 * PD4 = GPIO input + EXTI4 rising-edge counter (loopback from PD2)
 * Wire: PD2 ↔ PD4
 *
 * PWM: ~1 kHz, 50% duty  (PSC=7, ARR=999, CCR1=500, APB2=8 MHz)
 * EXTI4 counts rising edges → detail0 = edge_count << 1
 *
 * PASS after 5 edges confirmed in loopback verification.
 *
 * Clock: 24 MHz HSI, APB2 = 8 MHz
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

/* ── TIM1 PWM init ────────────────────────────────────────────────────── */
static void tim1_pwm_init(void)
{
    /* Clocks: GPIOD + TIM1 on APB2 */
    RCC->APB2PCENR |= RCC_IOPDEN | RCC_TIM1EN;

    /* PD2 = TIM1_CH1 AF push-pull 50 MHz (CFGLR bits[11:8] = 0xB) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 8)) | (0xBu << 8);

    /* Reset TIM1 */
    RCC->APB2PRSTR |=  RCC_TIM1RST;
    RCC->APB2PRSTR &= ~RCC_TIM1RST;

    /* PSC=7 → timer clock = 8 MHz / 8 = 1 MHz */
    TIM1->PSC   = 7;
    /* ARR=999 → period = 1000 µs = 1 kHz */
    TIM1->ATRLR = 999;
    /* CCR1=500 → 50% duty */
    TIM1->CH1CVR = 500;

    /* CHCTLR1: OC1M=PWM1 (0b110<<4), OC1PE=1 (bit3) */
    TIM1->CHCTLR1 = (0x6u << 4) | TIM_OC1PE;
    /* CCER: CC1E=1 (enable CH1 output), CC1P=0 (active high) */
    TIM1->CCER = TIM_CC1E;
    /* BDTR: MOE=1 (main output enable — required for TIM1) */
    TIM1->BDTR = TIM_MOE;
    /* Generate update event to load PSC/ARR into shadow registers */
    TIM1->SWEVGR = 0x0001u;   /* UG bit */
    /* CTLR1: enable counter */
    TIM1->CTLR1 = 0x0001u;    /* CEN bit */
}

/* ── EXTI4 on PD4 ─────────────────────────────────────────────────────── */
static void exti4_init(void)
{
    /* PD4 = floating input (CFGLR bits[19:16] = 0x4) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 16)) | (0x4u << 16);

    /* AFIO clock already enabled; EXTI4 source = PD (3) → bits[19:16] */
    RCC->APB2PCENR |= RCC_AFIOEN;
    AFIO->EXTICR = (AFIO->EXTICR & ~(0xFu << 16)) | (0x3u << 16);

    EXTI->RTENR  |= (1u << 4);
    EXTI->INTENR |= (1u << 4);
    EXTI->INTFR   = (1u << 4);  /* clear pending */
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    ael_mailbox_init();
    tim1_pwm_init();
    exti4_init();

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    /* ── Wait for 5 rising edges to verify loopback ─────────────────── */
    uint32_t edge_count = 0;
    uint32_t timeout    = 0;
    while (edge_count < 5 && timeout < 5000000u) {
        if (EXTI->INTFR & (1u << 4)) {
            EXTI->INTFR = (1u << 4);
            edge_count++;
        }
        timeout++;
    }

    if (edge_count >= 5)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(0, edge_count);

    /* ── Liveness: keep counting edges, update detail0 ─────────────── */
    while (1) {
        if (EXTI->INTFR & (1u << 4)) {
            EXTI->INTFR = (1u << 4);
            edge_count++;
            *detail0 = edge_count << 1;
        }
    }

    return 0;
}
