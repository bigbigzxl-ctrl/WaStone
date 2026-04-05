/*
 * stm32h563rgt6_lptim2_mailbox — LPTIM2 counter self-test
 *
 * LPTIM2 sits on APB1H (not APB3 like LPTIM1/3-6), giving it access to
 * different low-power clock sources. This test verifies LPTIM2 is functional
 * using the same continuous-count approach as the other LPTIM tests.
 *
 * LPTIM2_BASE = APB1PERIPH_BASE + 0x9400 = 0x40009400
 * RCC_APB1HENR = RCC_BASE + 0x0A0, bit5 = LPTIM2EN
 *
 * FAIL codes:
 *   0xE001 — LPTIM2 CNT did not advance between two reads
 *
 * detail0: cnt2 (final CNT value)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1HENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A0u))

#define LPTIM2_BASE     0x40009400u
#define LPTIM_ISR       (*(volatile uint32_t *)(LPTIM2_BASE + 0x00u))
#define LPTIM_CFGR      (*(volatile uint32_t *)(LPTIM2_BASE + 0x0Cu))
#define LPTIM_CR        (*(volatile uint32_t *)(LPTIM2_BASE + 0x10u))
#define LPTIM_ARR       (*(volatile uint32_t *)(LPTIM2_BASE + 0x18u))
#define LPTIM_CNT       (*(volatile uint32_t *)(LPTIM2_BASE + 0x1Cu))

#define LPTIM_CR_ENABLE  (1u << 0)
#define LPTIM_CR_CNTSTRT (1u << 2)

int main(void)
{
    ael_mailbox_init();

    /* Enable LPTIM2 APB1H clock (bit5) */
    RCC_APB1HENR |= (1u << 5);
    (void)RCC_APB1HENR;
    (void)RCC_APB1HENR;

    LPTIM_CFGR = 0u;
    LPTIM_CR   = LPTIM_CR_ENABLE;
    for (volatile uint32_t d = 0u; d < 50u; d++) {}
    LPTIM_ARR  = 0xFFFFu;
    for (volatile uint32_t d = 0u; d < 200u; d++) {}
    LPTIM_CR   = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
    for (volatile uint32_t d = 0u; d < 50000u; d++) {}

    uint32_t cnt1 = LPTIM_CNT;
    for (volatile uint32_t d = 0u; d < 50000u; d++) {}
    uint32_t cnt2 = LPTIM_CNT;

    if (cnt1 == cnt2) {
        ael_mailbox_fail(0xE001u, cnt1);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = cnt2;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
