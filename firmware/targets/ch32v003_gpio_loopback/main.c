/*
 * CH32V003 GPIO loopback test: PC6 → PC7
 *
 * PC6 = output (driver)
 * PC7 = input  (sense, connected to PC6 externally)
 *
 * Test sequence:
 *   1. Drive PC6 HIGH → read PC7, expect 1
 *   2. Drive PC6 LOW  → read PC7, expect 0
 *
 * Mailbox result:
 *   PASS (status=2)              both checks matched
 *   FAIL (status=3, error_code)  error_code bit 0 = HIGH check failed
 *                                error_code bit 1 = LOW  check failed
 *
 * After the loopback test, PC6 blinks at a visible rate so the
 * connected LED can be seen toggling.
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOC clock */
    RCC->APB2PCENR |= RCC_IOPCEN;

    /*
     * PC6 (CFGLR bits [27:24]): push-pull output, 50 MHz  → nibble 0x3
     * PC7 (CFGLR bits [31:28]): floating input             → nibble 0x4
     */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xFu << 24) | (0xFu << 28)))
                 | (0x3u << 24)
                 | (0x4u << 28);

    /* ── loopback test ───────────────────────────────────────────────── */

    /* Drive HIGH */
    GPIOC->BSHR = (1u << 6);
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t high_read = (GPIOC->INDR >> 7) & 1u;   /* expect 1 */

    /* Drive LOW */
    GPIOC->BSHR = (1u << (6 + 16));
    for (volatile uint32_t i = 0; i < 1000u; i++);
    uint32_t low_read = (GPIOC->INDR >> 7) & 1u;    /* expect 0 */

    uint32_t err = 0u;
    if (!high_read) err |= 1u;   /* bit 0: HIGH failed */
    if ( low_read)  err |= 2u;   /* bit 1: LOW  failed */

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, (high_read << 1) | low_read);

    /* Reconfigure PC7 as push-pull output 50 MHz (was input during loopback) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 28)) | (0x3u << 28);
    GPIOC->OUTDR &= ~(1u << 7);

    /* ── blink PC6 ───────────────────────────────────────────────────── */
    while (1)
    {
        for (volatile uint32_t i = 0; i < 2000000u; i++);
        GPIOC->OUTDR ^= (1u << 6);
    }

    return 0;
}
