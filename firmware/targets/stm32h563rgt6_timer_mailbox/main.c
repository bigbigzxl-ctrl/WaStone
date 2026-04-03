/*
 * stm32h563rgt6_timer_mailbox — TIM6 basic timer IRQ self-test
 *
 * Verifies: TIM6 peripheral clock, APB1L bus, NVIC routing (IRQ 49),
 * and interrupt delivery to TIM6_IRQHandler.
 *
 * Setup: TIM6 at APB1L clock (64 MHz HSI), PSC=63999 → 1 kHz, ARR=99 → 100 Hz.
 * Waits for 5 interrupts (≈50 ms), then writes PASS.
 *
 * H563 specifics (RM0481):
 *   TIM6_BASE  = 0x40001000 (APB1L)
 *   RCC_BASE   = 0x44020C00
 *   RCC_APB1LENR = RCC_BASE + 0x09C, bit4 = TIM6EN
 *   IRQ 49 = TIM6  →  NVIC_ISER[1] bit 17 (49 - 32 = 17)
 *
 * FAIL codes:
 *   0xE001 — TIM6 interrupt never fired (timeout)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE         0x44020C00u
#define RCC_APB1LENR     (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))

/* ── TIM6 (APB1L, base 0x40001000) ──────────────────────────────── */
#define TIM6_BASE  0x40001000u
#define TIM6_CR1   (*(volatile uint32_t *)(TIM6_BASE + 0x00u))
#define TIM6_DIER  (*(volatile uint32_t *)(TIM6_BASE + 0x0Cu))
#define TIM6_SR    (*(volatile uint32_t *)(TIM6_BASE + 0x10u))
#define TIM6_EGR   (*(volatile uint32_t *)(TIM6_BASE + 0x14u))
#define TIM6_PSC   (*(volatile uint32_t *)(TIM6_BASE + 0x28u))
#define TIM6_ARR   (*(volatile uint32_t *)(TIM6_BASE + 0x2Cu))

/* ── NVIC ────────────────────────────────────────────────────────── */
/* IRQ 49 = TIM6 → ISER[1] bit 17 (49 - 32 = 17) */
#define NVIC_ISER1       (*(volatile uint32_t *)0xE000E104u)
#define NVIC_TIM6_BIT    (1u << 17u)

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

/* ── Shared state ────────────────────────────────────────────────── */
static volatile uint32_t irq_count;

void TIM6_IRQHandler(void)
{
    if (TIM6_SR & 1u) {   /* UIF */
        TIM6_SR = 0u;      /* clear UIF */
        irq_count++;
    }
}

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0u; i < ms; i++) {
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

    /* Enable TIM6 APB1L clock (bit4) */
    RCC_APB1LENR |= (1u << 4u);
    (void)RCC_APB1LENR;

    irq_count = 0u;

    /*
     * TIM6 configuration:
     *   APB1L clock = 64 MHz (HSI, PPRE1=1 at reset)
     *   PSC = 63999 → TIM tick = 64 MHz / 64000 = 1 kHz (1 ms/tick)
     *   ARR = 99    → overflow every 100 ms → 10 Hz
     */
    TIM6_CR1  = 0u;
    TIM6_PSC  = 63999u;
    TIM6_ARR  = 99u;
    TIM6_EGR  = 1u;   /* UG: force reload PSC/ARR into shadow registers */
    TIM6_SR   = 0u;   /* clear UIF set by UG */
    TIM6_DIER = 1u;   /* UIE: enable update interrupt */

    /* Enable TIM6 in NVIC (IRQ 49 → ISER[1] bit 17) */
    NVIC_ISER1 = NVIC_TIM6_BIT;

    /* Start counter */
    TIM6_CR1 = 1u;    /* CEN */

    /* Enable global interrupts */
    __asm__ volatile ("cpsie i");

    /* Wait for 5 interrupts (≈500 ms). Timeout: 2 s. */
    uint32_t timeout_ms = 2000u;
    while ((irq_count < 5u) && (timeout_ms-- > 0u)) {
        delay_ms(1u);
    }

    if (irq_count < 5u) {
        ael_mailbox_fail(0xE001u, irq_count);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = irq_count;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
