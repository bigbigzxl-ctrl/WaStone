/*
 * CH32V003 dual GPIO loopback test: PC6→PC7  AND  PC4→PC5
 *
 * PC6 = output (driver),  PC7 = input (sense, wired to PC6)
 * PC4 = output (driver),  PC5 = input (sense, wired to PC4)
 *
 * error_code bits:
 *   bit0 = PC6 high-read failed
 *   bit1 = PC6 low-read failed
 *   bit2 = PC4 high-read failed
 *   bit3 = PC4 low-read failed
 *
 * After the loopback tests, PC6 blinks and detail0 is updated every
 * toggle so the pipeline can confirm the MCU is still running.
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

    /* PC4 output, PC5 input, PC6 output, PC7 input */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xFu << 16) | (0xFu << 20) | (0xFu << 24) | (0xFu << 28)))
                 | (0x3u << 16)   /* PC4 push-pull output 50 MHz */
                 | (0x4u << 20)   /* PC5 floating input */
                 | (0x3u << 24)   /* PC6 push-pull output 50 MHz */
                 | (0x4u << 28);  /* PC7 floating input */

    /* ── loopback test PC6 → PC7 ────────────────────────────────────── */
    GPIOC->BSHR = (1u << 6);
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t pc6_high = (GPIOC->INDR >> 7) & 1u;

    GPIOC->BSHR = (1u << (6 + 16));
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t pc6_low  = (GPIOC->INDR >> 7) & 1u;

    /* ── loopback test PC4 → PC5 ────────────────────────────────────── */
    GPIOC->BSHR = (1u << 4);
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t pc4_high = (GPIOC->INDR >> 5) & 1u;

    GPIOC->BSHR = (1u << (4 + 16));
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t pc4_low  = (GPIOC->INDR >> 5) & 1u;

    uint32_t err = 0u;
    if (!pc6_high) err |= 1u;
    if ( pc6_low)  err |= 2u;
    if (!pc4_high) err |= 4u;
    if ( pc4_low)  err |= 8u;

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, (pc6_high << 3) | (pc6_low << 2) | (pc4_high << 1) | pc4_low);

    /* ── blink PC6, update detail0 ───────────────────────────────────── */
    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint16_t toggle_count = 0;
    while (1)
    {
        for (volatile uint32_t i = 0; i < 2000000u; i++);
        GPIOC->OUTDR ^= (1u << 6);
        toggle_count++;
        uint32_t led = (GPIOC->OUTDR >> 6) & 1u;
        *detail0 = ((uint32_t)toggle_count << 1) | led;
    }

    return 0;
}
