/*
 * stm32h750_timer_mailbox — TIM6 basic timer IRQ self-test
 *
 * Verifies: TIM6 peripheral clock, APB1L bus, NVIC routing (IRQ 54),
 * and interrupt delivery to TIM6_DAC_IRQHandler.
 *
 * Setup: TIM6 at APB1L clock (64 MHz), PSC=63999 → 1 kHz, ARR=99 → 100 Hz.
 * Waits for 5 interrupts (≈50 ms), then writes PASS.
 *
 * TIM6_BASE = 0x40001000 (RM0433 §43, APB1L domain D2)
 * RCC_APB1LENR bit 4 = TIM6EN (RM0433 §8.7.38)
 * IRQ 54 = TIM6_DAC (RM0433 Table 159), NVIC_ISER[1] bit 22
 *
 * Error codes:
 *   0xE001 = TIM6 interrupt never fired (timeout waiting for irq_count ≥ 5)
 */

#include <stdint.h>
#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ────────────────────────────────────────────────────────── */
#define RCC_BASE         0x58024400u
#define RCC_APB1LENR     (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))
#define RCC_APB1LENR_TIM6EN (1u << 4)

/* ── TIM6 (RM0433 §43, APB1L, base 0x40001000) ──────────────────── */
#define TIM6_BASE  0x40001000u
#define TIM6_CR1   (*(volatile uint32_t *)(TIM6_BASE + 0x00u))
#define TIM6_DIER  (*(volatile uint32_t *)(TIM6_BASE + 0x0Cu))
#define TIM6_SR    (*(volatile uint32_t *)(TIM6_BASE + 0x10u))
#define TIM6_EGR   (*(volatile uint32_t *)(TIM6_BASE + 0x14u))
#define TIM6_PSC   (*(volatile uint32_t *)(TIM6_BASE + 0x28u))
#define TIM6_ARR   (*(volatile uint32_t *)(TIM6_BASE + 0x2Cu))

/* ── NVIC ─────────────────────────────────────────────────────── */
/* IRQ 54 = TIM6_DAC → ISER[1] bit 22 (54 - 32 = 22) */
#define NVIC_ISER1  (*(volatile uint32_t *)0xE000E104u)
#define NVIC_TIM6_BIT  (1u << 22u)

/* ── SysTick (for timeout) ───────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

/* ── Shared state ────────────────────────────────────────────── */
static volatile uint32_t irq_count;

void TIM6_DAC_IRQHandler(void)
{
    if (TIM6_SR & 1u) {     /* UIF */
        TIM6_SR = 0u;        /* clear UIF */
        irq_count++;
    }
}

/* SysTick delay for timeout measurement */
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

    ael_mailbox_init();

    /* Enable TIM6 APB1L clock */
    RCC_APB1LENR |= RCC_APB1LENR_TIM6EN;
    (void)RCC_APB1LENR;

    irq_count = 0u;

    /*
     * TIM6 configuration:
     *   APB1 clock = 64 MHz HSI (PPRE1=1, no prescaler)
     *   Timer input clock = 64 MHz (PPRE1 = 1 → TIM clock = PCLK1)
     *   PSC=63999 → TIM tick = 64 MHz / 64000 = 1000 Hz (1 ms/tick)
     *   ARR=99   → overflow every 100 ticks = 100 ms → 10 Hz interrupt rate
     */
    TIM6_CR1  = 0u;
    TIM6_PSC  = 63999u;
    TIM6_ARR  = 99u;
    TIM6_EGR  = 1u;          /* UG: preload PSC/ARR */
    TIM6_SR   = 0u;           /* clear any pending UIF */
    TIM6_DIER = 1u;           /* UIE: enable update interrupt */

    /* Enable TIM6_DAC in NVIC (IRQ 54 → ISER[1] bit 22) */
    NVIC_ISER1 = NVIC_TIM6_BIT;

    /* Enable counter */
    TIM6_CR1 = 1u;            /* CEN */

    /* Enable global interrupts */
    __asm__ volatile ("cpsie i");

    /* Wait for 5 interrupts (≈500 ms). Timeout: 2 s (generous). */
    uint32_t timeout_ms = 2000u;
    while ((irq_count < 5u) && (timeout_ms-- > 0u)) {
        delay_ticks(1u);
    }

    if (irq_count < 5u) {
        ael_mailbox_fail(0xE001u, irq_count);
        while (1) {}
    }

    ael_mailbox_pass();
    /* Store interrupt count in detail0 */
    AEL_MAILBOX->detail0 = irq_count;
    while (1) {
        delay_ticks(1u);
        AEL_MAILBOX->detail0 = irq_count;
    }
}
