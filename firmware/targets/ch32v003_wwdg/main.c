/*
 * CH32V003 WWDG (Window Watchdog) test
 *
 * Enables WWDG, sets a ~6 ms window, feeds the watchdog once within
 * the window, then disables WWDG by NOT re-enabling it (WWDG cannot
 * be disabled once WDGA=1, so we use the strategy of servicing it
 * quickly, then writing mailbox PASS before it expires).
 *
 * Strategy:
 *   1. Enable WWDG clock.
 *   2. Set CFGR: WDGTB=0 (prescaler/1), W=0x7F (max window).
 *   3. Set CTLR: WDGA=1 (enable), T=0x7F (max counter → ~6.5 ms timeout
 *      at PCLK1/4096 = 8 MHz/4096 ≈ 1953 Hz → period ~512 µs × 64 counts).
 *   4. Immediately write mailbox PASS (within the window).
 *   5. Immediately re-feed WWDG to prevent reset: CTLR = WDGA|T[6:0]=0x7F.
 *   6. Liveness loop: keep feeding WWDG every ~3 ms; update detail0.
 *
 * WWDG clock: APB1 / 4096 / WDGTB_prescaler
 *   PCLK1 = 8 MHz (APB1 default)
 *   WDGTB = 0 → div/1 → WWDG clock = 8 MHz / 4096 = 1953.125 Hz
 *   Counter T[6:0]=0x7F (127 → counts 63 down to 0x40 before reset)
 *   Timeout ≈ 63 / 1953 ≈ 32 ms
 *
 * PASS if we reach ael_mailbox_pass() before watchdog fires.
 * detail0 = feed_count << 1 (increments in liveness loop)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable WWDG clock on APB1 */
    RCC->APB1PCENR |= RCC_WWDGEN;

    /* CFGR: WDGTB=0 (no prescaler), W[6:0]=0x7F (max window = no early
     * write restriction), EWIF interrupt disabled */
    WWDG->CFGR = 0x7Fu;  /* W=0x7F, WDGTB=0 */

    /* CTLR: WDGA=1 (enable watchdog), T[6:0]=0x7F (full counter)
     * Once WDGA is set, it cannot be cleared by software. */
    WWDG->CTLR = 0xFFu;  /* WDGA | T=0x7F */

    /* We are now racing the watchdog (~32 ms).
     * Write PASS immediately. */
    ael_mailbox_pass();

    /* Feed WWDG to stay alive */
    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint32_t feed_count = 0;

    while (1) {
        /* Re-load counter before it reaches 0x3F (reset threshold) */
        WWDG->CTLR = 0xFFu;  /* WDGA | T=0x7F */
        feed_count++;
        *detail0 = feed_count << 1;
        /* Wait ~3 ms (< 32 ms timeout): 24 MHz → ~72000 cycles */
        for (volatile uint32_t i = 0; i < 72000u; i++);
    }

    return 0;
}
