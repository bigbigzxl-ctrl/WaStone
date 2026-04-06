/*
 * CH32V003 SysTick test
 *
 * Verifies that the SysTick counter advances over time.
 * No pins required.
 *
 * SysTick at 0xE000F000:
 *   CTLR bit0=ENABLE, bit2=HCLK_SEL(1=HCLK 24MHz, 0=HCLK/8 3MHz)
 *   CNT: 32-bit up-counter
 *   CMP: compare register (set to max for free-run)
 *
 * Test: enable SysTick, read CNT twice with a delay, verify it moved.
 * PASS if CNT advanced; FAIL otherwise.
 * detail0 = SysTick->CNT >> 14  (changes every ~0.7ms at 24MHz → liveness)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Configure SysTick: HCLK source (bit2=1), enable (bit0=1)
     * CMP = 0xFFFFFFFF for free-running counter */
    SysTick->CMP  = 0xFFFFFFFFu;
    SysTick->CNT  = 0;
    SysTick->CTLR = 0x5u;   /* bit0=ENABLE, bit2=HCLK_SEL */

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    /* Read CNT twice with a busy delay */
    uint32_t t1 = SysTick->CNT;
    for (volatile uint32_t i = 0; i < 500000u; i++);
    uint32_t t2 = SysTick->CNT;

    if (t2 != t1)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(0, 0);

    /* Liveness: detail0 updates from SysTick->CNT upper bits */
    while (1) {
        /* Shift by 13: at 24MHz, changes ~2929 times/sec → noticeable */
        *detail0 = (SysTick->CNT >> 13) << 1;
    }

    return 0;
}
