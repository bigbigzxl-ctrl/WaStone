/*
 * CH32V003 PWR sleep + AWU (Auto Wakeup Unit) test
 *
 * Enters sleep mode (WFI), wakes up via AWU (LSI-based auto-wakeup),
 * then writes PASS. Repeats in liveness loop.
 *
 * AWU configuration:
 *   LSI (~128 kHz internal RC oscillator)
 *   AWUPSC = 0b1011 (11 → prescaler /2048) → AWU clock ≈ 62.5 Hz
 *   AWUWR  = 0x3F   (63 counts)            → wakeup ≈ 63/62.5 ≈ 1 s
 *   (use smaller values to keep test fast)
 *
 * Faster: AWUPSC=0b1000 (prescaler /512) → ~250 Hz; AWUWR=25 → ~100 ms
 *
 * Registers:
 *   RCC->RSTSCKR: LSION (bit0), LSIRDY (bit1)
 *   RCC->APB1PCENR: PWREN (bit28)
 *   PWR->AWUPSC: bits[3:0] = prescaler select
 *   PWR->AWUWR:  bits[5:0] = window register (comparator value)
 *   PWR->AWUCSR: bit1 = AWUEN (enable AWU), bit0 = AWUF (wakeup flag, clear by write 0)
 *   EXTI->EVENR: bit9 = AWU event line (must enable for WFI wakeup)
 *
 * PASS after first wakeup from sleep.
 * detail0 = wakeup_count << 1 (increments in liveness loop)
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
    while (!(RCC->RSTSCKR & (1u << 1)));  /* LSIRDY */

    /* Enable PWR clock on APB1 */
    RCC->APB1PCENR |= RCC_PWREN;

    /* Configure AWU:
     *   AWUPSC = 0b1000 → prescaler /512 → AWU clock = 128 kHz / 512 = 250 Hz
     *   AWUWR  = 25     → wakeup period ≈ 25 / 250 = 100 ms */
    PWR->AWUPSC = 0x08u;  /* prescaler /512 */
    PWR->AWUWR  = 25u;    /* ~100 ms */

    /* Clear wakeup flag, then enable AWU */
    PWR->AWUCSR = 0u;             /* clear AWUF */
    PWR->AWUCSR |= (1u << 1);    /* AWUEN */

    /* Enable AWU event on EXTI line 9 (required for WFI to exit on AWU) */
    EXTI->EVENR |= (1u << 9);

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint32_t wakeup_count = 0;

    /* Enter sleep once — WFI exits when AWU fires */
    __WFI();

    /* First wakeup — clear flag */
    PWR->AWUCSR &= ~(1u << 0);  /* clear AWUF */
    wakeup_count++;
    *detail0 = wakeup_count << 1;

    ael_mailbox_pass();

    /* Liveness: keep sleeping and waking */
    while (1) {
        PWR->AWUCSR &= ~(1u << 0);  /* clear AWUF before sleep */
        __WFI();
        PWR->AWUCSR &= ~(1u << 0);  /* clear AWUF after wake */
        wakeup_count++;
        *detail0 = wakeup_count << 1;
    }

    return 0;
}
