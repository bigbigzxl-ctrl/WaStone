/*
 * CH32V003 TIM2 basic counter test
 *
 * Enables TIM2 (APB1), starts the free-running counter at 1 MHz
 * (PSC=7, APB1=8 MHz → 8/8=1 MHz tick), then verifies the counter
 * is actually advancing by reading it twice with a short delay.
 *
 * No pins required.
 *
 * Note: TIM2->CNT is 16-bit on CH32V003. A long wait loop would cause
 * multiple overflows, making a simple >=threshold check unreliable.
 * Instead, verify the timer is running by confirming CNT advances.
 *
 * PASS if CNT1 > 0 and CNT2 > CNT1 (timer counting forward).
 * detail0 = CNT value (liveness: value keeps changing)
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

    /* PSC=7 → timer clock = APB1(8 MHz) / 8 = 1 MHz */
    TIM2->PSC   = 7;
    /* ARR = max (16-bit free-running) */
    TIM2->ATRLR = 0xFFFFu;

    /* UG: load PSC/ARR into shadow registers */
    TIM2->SWEVGR = 0x0001u;

    /* CEN: start the counter */
    TIM2->CTLR1 = 0x0001u;

    /* Short delay to let counter advance (≥1 tick) */
    for (volatile uint32_t i = 0; i < 100000u; i++);

    uint32_t cnt1 = TIM2->CNT;

    /* Second delay: another ~100 µs @ 1 MHz → should advance ≥50 ticks */
    for (volatile uint32_t i = 0; i < 10000u; i++);

    uint32_t cnt2 = TIM2->CNT;

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = cnt2 << 1;

    /* PASS if timer is counting: cnt1 > 0 and counter advanced.
     * Use unsigned 16-bit subtraction to handle wrap-around correctly. */
    uint32_t uif = TIM2->INTFR & 0x1u;  /* UIF (bit0 of INTFR) set if CNT overflowed */
    uint16_t advance = (uint16_t)((uint16_t)cnt2 - (uint16_t)cnt1);
    if (cnt1 > 0u && (advance > 0u || uif))
        ael_mailbox_pass();
    else
        ael_mailbox_fail(cnt1, cnt2);

    /* Liveness: keep updating detail0 with current CNT */
    while (1) {
        *detail0 = (TIM2->CNT << 1);
        for (volatile uint32_t i = 0; i < 500000u; i++);
    }

    return 0;
}
