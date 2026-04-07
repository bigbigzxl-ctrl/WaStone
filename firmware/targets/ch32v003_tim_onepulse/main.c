/*
 * CH32V003 TIM One-Pulse Mode test — zero wiring
 *
 * TIM2 in One-Pulse Mode (OPM): counter runs once then stops.
 * PSC=7 → 1 MHz tick. ARR=1000 → 1 ms pulse.
 * After starting the timer, wait > 2 ms, then verify CEN is cleared.
 *
 * OPM works as follows:
 *   1. Set OPM bit (CTLR1 bit3) to enable one-pulse mode.
 *   2. Load PSC/ARR via UG event.
 *   3. Set CEN (start counting).
 *   4. When counter reaches ARR, overflow occurs, UIF set, CEN auto-cleared.
 *
 * PASS if CTLR1.CEN == 0 after the delay (timer stopped automatically).
 * detail0 = CNT value at check time (should be 0 or near 0 after OPM).
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable TIM2 clock (APB1) */
    RCC->APB1PCENR |= RCC_TIM2EN;

    /* Reset TIM2 */
    RCC->APB1PRSTR |=  RCC_TIM2RST;
    RCC->APB1PRSTR &= ~RCC_TIM2RST;

    /* PSC=7 → timer clock = APB1(8 MHz)/8 = 1 MHz (1 µs tick) */
    TIM2->PSC   = 7u;
    /* ARR = 1000 → 1000 µs = 1 ms pulse */
    TIM2->ATRLR = 1000u;

    /* UG: load PSC/ARR into shadow registers */
    TIM2->SWEVGR = 0x0001u;

    /* Wait for UG to be processed (UIF set) then clear it */
    for (volatile uint32_t i = 0; i < 100u; i++);
    TIM2->INTFR = 0u;  /* clear UIF */

    /* CTLR1: OPM (bit3) | CEN (bit0) — start one-pulse */
    TIM2->CTLR1 = (1u << 3) | (1u << 0);

    /* Wait 3 ms (much longer than 1 ms ARR) */
    for (volatile uint32_t i = 0; i < 60000u; i++);  /* ~3 ms @ 24 MHz / 2 cycles/iter */

    /* Check: CEN should be cleared after one pulse */
    uint32_t ctlr1 = TIM2->CTLR1;
    uint32_t cnt   = TIM2->CNT;
    uint32_t uif   = TIM2->INTFR & 0x1u;

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = cnt << 1;

    /* CEN should be 0 (timer stopped); UIF should be 1 (overflow happened) */
    if ((ctlr1 & 0x1u) == 0u && uif)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(ctlr1, cnt);

    /* Liveness */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 ^= 2u;
    }

    return 0;
}
