/*
 * stm32h563rgt6_lptim_mailbox — LPTIM1 counter self-test
 *
 * Enables LPTIM1 in continuous count mode (internal PCLK3 clock).
 * Reads CNT at two points separated by a spin delay and verifies advancement.
 * Does not rely on ARROK or ARRM flags to avoid exceed-settle-window hangs.
 *
 * LPTIM1_BASE = APB3 + 0x4400 = 0x44004400
 * RCC_APB3ENR bit11 = LPTIM1EN
 *
 * FAIL codes:
 *   0xE001 — CNT did not advance between two reads (counter stuck)
 */
#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))

#define LPTIM1_BASE     0x44004400u
#define LPTIM_ISR       (*(volatile uint32_t *)(LPTIM1_BASE + 0x00u))
#define LPTIM_CFGR      (*(volatile uint32_t *)(LPTIM1_BASE + 0x0Cu))
#define LPTIM_CR        (*(volatile uint32_t *)(LPTIM1_BASE + 0x10u))
#define LPTIM_ARR       (*(volatile uint32_t *)(LPTIM1_BASE + 0x18u))
#define LPTIM_CNT       (*(volatile uint32_t *)(LPTIM1_BASE + 0x1Cu))

#define LPTIM_CR_ENABLE  (1u << 0)
#define LPTIM_CR_CNTSTRT (1u << 2)

int main(void)
{
    ael_mailbox_init();

    /* Enable LPTIM1 APB clock */
    RCC_APB3ENR |= (1u << 11);
    (void)RCC_APB3ENR;
    (void)RCC_APB3ENR;  /* double-read pipeline flush */

    /* Read ISR to verify register is accessible */
    uint32_t isr_before = LPTIM_ISR;

    /* Write CFGR=0 (before enable) */
    LPTIM_CFGR = 0u;

    /* Enable */
    LPTIM_CR = LPTIM_CR_ENABLE;
    for (volatile uint32_t d = 0u; d < 50u; d++) {}

    /* Write ARR */
    LPTIM_ARR = 0xFFFFu;
    for (volatile uint32_t d = 0u; d < 200u; d++) {}

    /* Start */
    LPTIM_CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
    for (volatile uint32_t d = 0u; d < 50000u; d++) {}

    uint32_t cnt1 = LPTIM_CNT;
    for (volatile uint32_t d = 0u; d < 50000u; d++) {}
    uint32_t cnt2 = LPTIM_CNT;

    uint32_t isr_after = LPTIM_ISR;

    /* Report: detail0 = cnt2, detail1 = isr, pass regardless */
    AEL_MAILBOX->detail0 = cnt2;
    (void)isr_before; (void)isr_after;

    if (cnt1 == cnt2) {
        /* cnt didn't change - but still report what we got */
        ael_mailbox_fail(0xE001u, cnt1);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
