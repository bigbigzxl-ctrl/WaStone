/*
 * CH32V003 IWDG (Independent Watchdog) test
 *
 * Verifies that the IWDG can be started and fed indefinitely
 * without triggering a reset.
 * No pins required.
 *
 * IWDG:
 *   CTLR (key register): 0x5555=unlock, 0xAAAA=reload, 0xCCCC=start
 *   PSCR: prescaler (PR[2:0]): /4=0, /8=1, /16=2, /32=3, /64=4, /128=5, /256=6
 *   RLDR: reload value (12-bit)
 *   LSI ≈ 40 kHz
 *
 * Config: PR=6 (÷256) → ~156 Hz, RLDR=1200 → timeout ≈ 7.7 s
 * Feed every ~1 s (detail0 increments fast enough for liveness check)
 *
 * PASS immediately after starting the watchdog.
 * detail0 = feed_count << 1
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Unlock PSCR and RLDR */
    IWDG->CTLR = 0x5555u;
    /* Prescaler /256 */
    IWDG->PSCR = 6u;
    /* Reload = 1200 → timeout ≈ 7.7 s (gives plenty of margin) */
    IWDG->RLDR = 1200u;
    /* Reload then start (WCH HAL does NOT poll PVU/RVU — CH32V003 sequence) */
    IWDG->CTLR = 0xAAAAu;   /* reload */
    IWDG->CTLR = 0xCCCCu;  /* start watchdog */

    ael_mailbox_pass();

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint32_t feed_count = 0;

    while (1) {
        /* Feed every ~500ms (well under 7.7s timeout) */
        for (volatile uint32_t i = 0; i < 2000000u; i++);
        IWDG->CTLR = 0xAAAAu;   /* reload */
        feed_count++;
        *detail0 = feed_count << 1;
    }

    return 0;
}
