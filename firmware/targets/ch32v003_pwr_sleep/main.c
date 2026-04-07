/*
 * CH32V003 PWR AWU test — WFI + EXTI9 event wakeup
 *
 * AWU does not set AWUF in run mode; it fires only via the EXTI event line.
 * Key: EXTI->EVENR bit9 must be set to route AWU wakeup signal to WFI.
 *
 * Flow: configure AWU → enable EXTI9 event → __WFI() → AWU fires in ~100 ms
 * → CPU wakes → write PASS → busy liveness loop (no more WFI, so debug
 * halt always succeeds).
 *
 * LSI ~128 kHz, AWUPSC=0x08 (/512) → 250 Hz, AWUWR=25 → ~100 ms wakeup.
 * detail0 tracks SysTick upper bits for liveness.
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable LSI oscillator */
    RCC->RSTSCKR |= (1u << 0);  /* LSION */
    /* Wait for LSI ready */
    { uint32_t t; do { t = RCC->RSTSCKR; } while (!(t & (1u << 1))); }

    /* Enable PWR clock on APB1 */
    RCC->APB1PCENR |= RCC_PWREN;

    /* Configure AWU: AWUPSC=0x08 (/512 per RM → 250 Hz), AWUWR=25 → ~100 ms */
    PWR->AWUPSC = 0x08u;
    PWR->AWUWR  = 25u;

    /* Clear AWUF then enable AWU */
    PWR->AWUCSR = 0u;
    PWR->AWUCSR |= (1u << 1);  /* AWUEN */

    /* Enable AWU event on EXTI line 9 — required for WFI to exit on AWU */
    EXTI->EVENR |= (1u << 9);

    /* Enable SysTick for liveness counter (free-running, HCLK) */
    SysTick->CMP  = 0xFFFFFFFFu;
    SysTick->CNT  = 0;
    SysTick->CTLR = 0x5u;  /* ENABLE + HCLK */

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    /* Enter sleep — WFI exits when AWU fires via EXTI9 event (~100 ms) */
    __WFI();

    /* Woke up from AWU */
    PWR->AWUCSR &= ~(1u << 0);  /* clear AWUF */

    ael_mailbox_pass();

    /* Busy liveness: CPU stays awake — debug halt always works */
    while (1) {
        *detail0 = (SysTick->CNT >> 13) << 1;
    }

    return 0;
}
