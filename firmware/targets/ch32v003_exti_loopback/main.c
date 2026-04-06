/*
 * CH32V003 EXTI loopback test: PC0 (output) → PC1 (EXTI1 input)
 *
 * PC0 = push-pull output (drives the edge)
 * PC1 = floating input + EXTI line 1 rising-edge trigger
 * Wired: PC0 ↔ PC1 (loopback wire)
 *
 * Test flow:
 *   1. Drive PC0 HIGH → poll EXTI->INTFR bit 1 → verify rising edge caught
 *   2. Drive PC0 LOW  → verify INDR bit 1 = 0
 *   3. PASS if all 5 initial cycles catch the edge; FAIL otherwise
 *   4. Loop forever, toggling and counting edges in detail0 for liveness
 *
 * detail0 = edge_count << 1  (toggle_count = edge_count for detail0_increment)
 *
 * Clock: 24 MHz HSI, APB2 = 8 MHz
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

/* ── EXTI helpers ─────────────────────────────────────────────────────── */
static void exti_init(void)
{
    /* Clocks: AFIO + GPIOC */
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPCEN;

    /* PC0 = push-pull output 50 MHz (CFGLR bits[3:0] = 0x3), drive LOW */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 0)) | (0x3u << 0);
    GPIOC->BSHR  = (1u << (0 + 16));  /* PC0 LOW */

    /* PC1 = floating input (CFGLR bits[7:4] = 0x4) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 4)) | (0x4u << 4);

    /* AFIO EXTICR: EXTI1 source = PC (value 2) → bits[3:2]
     * CH32V003 uses 2 bits per EXTI line: bit_pos = PinSource<<1
     * EXTI1 → shift=2, PC port value=2 → set bits[3:2]=0b10 */
    AFIO->EXTICR = (AFIO->EXTICR & ~(0x3u << 2)) | (0x2u << 2);

    /* EXTI line 1: rising-edge trigger, enable interrupt mask for INTFR latching */
    EXTI->RTENR  |= (1u << 1);
    EXTI->INTENR |= (1u << 1);

    /* Clear any pending flag */
    EXTI->INTFR = (1u << 1);
}

/* Drive PC0 HIGH, poll EXTI INTFR bit1 for rising edge (timeout loops).
 * Returns 1 if edge detected, 0 on timeout. Clears flag. */
static uint8_t wait_rising_edge(void)
{
    /* Clear stale flag before driving edge */
    EXTI->INTFR = (1u << 1);

    GPIOC->BSHR = (1u << 0);  /* PC0 HIGH */
    for (volatile uint32_t i = 0; i < 50000u; i++) {
        if (EXTI->INTFR & (1u << 1)) {
            EXTI->INTFR = (1u << 1);  /* clear */
            return 1;
        }
    }
    return 0;
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    ael_mailbox_init();
    exti_init();

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    /* ── initial loopback verification (5 cycles) ──────────────────────── */
    uint32_t err = 0u;
    for (uint8_t i = 0; i < 5; i++) {
        if (!wait_rising_edge()) { err |= (1u << i); }
        /* Drive LOW and verify INDR */
        GPIOC->BSHR = (1u << (0 + 16));
        for (volatile uint32_t j = 0; j < 1000u; j++);
        if (GPIOC->INDR & (1u << 1)) { err |= (1u << (i + 8)); }
    }

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, 0);

    /* ── liveness loop: keep toggling, update detail0 ───────────────────── */
    uint32_t edge_count = 0;
    while (1) {
        if (wait_rising_edge()) {
            edge_count++;
            *detail0 = edge_count << 1;
        }
        /* Drive LOW, short settle */
        GPIOC->BSHR = (1u << (0 + 16));
        for (volatile uint32_t i = 0; i < 500000u; i++);
    }

    return 0;
}
