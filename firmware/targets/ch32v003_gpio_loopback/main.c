/*
 * CH32V003 GPIO loopback test: PC6 → PC7
 * PC4 driven in parallel with PC6 in blink loop (no PC4 loopback test).
 *
 * PC6 = output (driver + LED D1)
 * PC7 = input  (sense, wired to PC6)
 * PC4 = output (LED D2, blinks with PC6)
 *
 * detail0 layout: bits[15:1]=toggle_count, bit[0]=led_state (PC6)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    RCC->APB2PCENR |= RCC_IOPCEN;

    /* PC4 output, PC6 output, PC7 floating input */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xFu << 16) | (0xFu << 24) | (0xFu << 28)))
                 | (0x3u << 16)   /* PC4 push-pull output 50 MHz */
                 | (0x3u << 24)   /* PC6 push-pull output 50 MHz */
                 | (0x4u << 28);  /* PC7 floating input */

    /* ── loopback test PC6 → PC7 ────────────────────────────────────── */
    GPIOC->BSHR = (1u << 6);
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t high_read = (GPIOC->INDR >> 7) & 1u;

    GPIOC->BSHR = (1u << (6 + 16));
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t low_read = (GPIOC->INDR >> 7) & 1u;

    uint32_t err = 0u;
    if (!high_read) err |= 1u;
    if ( low_read)  err |= 2u;

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, (high_read << 1) | low_read);

    /* ── blink PC4 + PC6 in parallel, update detail0 ────────────────── */
    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint16_t toggle_count = 0;
    while (1)
    {
        for (volatile uint32_t i = 0; i < 1000000u; i++);
        GPIOC->OUTDR ^= (1u << 6) | (1u << 4);  /* single RMW — avoids stale read between two writes */
        toggle_count++;
        uint32_t led = (GPIOC->OUTDR >> 6) & 1u;
        *detail0 = ((uint32_t)toggle_count << 1) | led;
    }

    return 0;
}
