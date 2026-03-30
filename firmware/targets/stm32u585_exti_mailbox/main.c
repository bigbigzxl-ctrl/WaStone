/*
 * stm32u585_exti_mailbox — EXTI edge detection test
 * STM32U585CIU6, MSI 4MHz
 *
 * Wire: PA8 (output) → PB4 (EXTI line 4 input)
 *
 * PA8 toggles at ~100Hz; EXTI line 4 (PB4) should detect rising+falling edges.
 * Polls RPR1/FPR1 (no interrupt handler needed).
 * PASS if ≥10 edges detected within 500ms window.
 *
 * FAIL codes:
 *   0xE001 — fewer than 10 edges in window (edge_count in detail0)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

/* GPIOB */
#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))

/* EXTI — AHB3, secure alias required (CMSIS EXTI_BASE = EXTI_BASE_S = 0x56022000) */
#define EXTI_BASE       0x56022000u
#define EXTI_RTSR1      (*(volatile uint32_t *)(EXTI_BASE + 0x00u))
#define EXTI_FTSR1      (*(volatile uint32_t *)(EXTI_BASE + 0x04u))
#define EXTI_RPR1       (*(volatile uint32_t *)(EXTI_BASE + 0x0Cu))
#define EXTI_FPR1       (*(volatile uint32_t *)(EXTI_BASE + 0x10u))
/* EXTICR2 controls lines 4-7: bits [7:0] = port for line 4 */
#define EXTI_EXTICR2    (*(volatile uint32_t *)(EXTI_BASE + 0x64u))
/* IMR1: interrupt mask register — must be set for RPR1/FPR1 to be asserted (new IP) */
#define EXTI_IMR1       (*(volatile uint32_t *)(EXTI_BASE + 0x80u))

/* Port B code for EXTICR = 1 */
#define EXTI_PORTB      0x01u

/* Toggle interval: ~5ms at 4MHz ≈ 20000 loops of simple logic */
#define TOGGLE_ITERS    20000u
/* Window: 500ms ≈ 100 toggles = 100 edges */
#define WINDOW_TOGGLES  100u
#define MIN_EDGES       10u

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOA, GPIOB clocks */
    RCC_AHB2ENR1 |= (1u << 0u) | (1u << 1u);
    volatile uint32_t dummy = RCC_AHB2ENR1;
    (void)dummy;

    /* PA8: output push-pull */
    GPIOA_MODER   &= ~(3u << 16u);
    GPIOA_MODER   |=  (1u << 16u);
    GPIOA_OTYPER  &= ~(1u << 8u);
    GPIOA_OSPEEDR |=  (3u << 16u);
    GPIOA_ODR     &= ~(1u << 8u);

    /* PB4: input, no pull */
    GPIOB_MODER &= ~(3u << 8u);
    GPIOB_PUPDR &= ~(3u << 8u);

    /* EXTI line 4: port B, rising + falling */
    EXTI_EXTICR2 = (EXTI_EXTICR2 & ~0xFFu) | EXTI_PORTB;
    EXTI_RTSR1   |= (1u << 4u);
    EXTI_FTSR1   |= (1u << 4u);
    /* IMR1[4]=1: required for RPR1/FPR1 to be set (new EXTI IP: pending gated by IMR) */
    EXTI_IMR1    |= (1u << 4u);
    EXTI_RPR1     = (1u << 4u);   /* clear any pending */
    EXTI_FPR1     = (1u << 4u);
    /* Diagnostics at 0x20007F10: RTSR1 readback, EXTICR2 readback, GPIOB_IDR snapshot */
    (*(volatile uint32_t *)0x20007F10u) = EXTI_RTSR1;
    (*(volatile uint32_t *)0x20007F14u) = EXTI_EXTICR2;
    /* Toggle PA8 once and read PB4 IDR to verify wire */
    GPIOA_ODR |= (1u << 8u);
    for (volatile uint32_t t = 0u; t < 100u; t++) {}
    (*(volatile uint32_t *)0x20007F18u) = GPIOB_IDR; /* expect bit4=1 if wire connected */
    GPIOA_ODR &= ~(1u << 8u);

    uint32_t edge_count = 0u;
    uint32_t toggle_count = 0u;
    uint8_t  pa8_high = 0u;

    while (toggle_count < WINDOW_TOGGLES) {
        /* Toggle PA8 */
        pa8_high ^= 1u;
        if (pa8_high) {
            GPIOA_ODR |=  (1u << 8u);
        } else {
            GPIOA_ODR &= ~(1u << 8u);
        }

        /* Wait */
        for (volatile uint32_t t = 0u; t < TOGGLE_ITERS; t++) {}

        /* Check EXTI pending */
        if (EXTI_RPR1 & (1u << 4u)) {
            EXTI_RPR1 = (1u << 4u);
            edge_count++;
        }
        if (EXTI_FPR1 & (1u << 4u)) {
            EXTI_FPR1 = (1u << 4u);
            edge_count++;
        }

        toggle_count++;
    }

    AEL_MAILBOX->detail0 = edge_count;

    if (edge_count < MIN_EDGES) {
        ael_mailbox_fail(0xE001u, edge_count);
    } else {
        ael_mailbox_pass();
    }

    while (1) {}
    return 0;
}
