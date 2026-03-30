/*
 * stm32h750_blinky_visual — LED blink health gate
 *
 * Blinks PA8 at 2 Hz (250 ms on / 250 ms off) to provide a visual
 * "alive" indicator, then writes PASS to the AEL mailbox.
 * The test verifies: flash OK, CPU running, SysTick functional, GPIOA driveable.
 *
 * Clock: 64 MHz HSI (H750 default, no PLL). SysTick RVR=63999 → 1 ms/tick.
 * All register addresses from RM0433.
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC (RM0433 §8) ──────────────────────────────────────────────── */
#define RCC_BASE          0x58024400u
#define RCC_AHB4ENR       (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_AHB4ENR_GPIOAEN (1u << 0)

/* ── GPIOA (RM0433 §10, base 0x58020000) ─────────────────────────── */
#define GPIOA_BASE  0x58020000u
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_ODR   (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

/* ── SysTick ─────────────────────────────────────────────────────── */
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

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Enable GPIOA clock */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    (void)RCC_AHB4ENR;

    /* PA8 output push-pull */
    GPIOA_MODER &= ~(0x3u << 16u);
    GPIOA_MODER |=  (0x1u << 16u);
    GPIOA_ODR   &= ~(1u << 8u);

    ael_mailbox_init();

    /* Blink 4 times (2 s) before writing PASS — visible "alive" signal */
    for (uint32_t i = 0u; i < 4u; i++) {
        GPIOA_ODR |=  (1u << 8u);   /* ON */
        delay_ticks(250u);
        GPIOA_ODR &= ~(1u << 8u);   /* OFF */
        delay_ticks(250u);
    }

    ael_mailbox_pass();

    /* Continue blinking so the LED blink pattern remains visible */
    while (1) {
        GPIOA_ODR |=  (1u << 8u);
        delay_ticks(250u);
        GPIOA_ODR &= ~(1u << 8u);
        delay_ticks(250u);
    }
}
