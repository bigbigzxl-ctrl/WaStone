/*
 * stm32h563rgt6_lptim_multi_mailbox — LPTIM3/4/5/6 counter self-test
 *
 * Tests four additional LPTIM instances (LPTIM3–6) which are on APB3
 * alongside LPTIM1. These instances share the same IP but have different
 * clock sources and can operate independently in low-power modes.
 *
 * Each LPTIM is started in continuous count mode (internal PCLK3).
 * After a spin delay, two CNT reads are compared to verify advancement.
 *
 * LPTIM3_BASE = 0x44004800
 * LPTIM4_BASE = 0x44004C00
 * LPTIM5_BASE = 0x44005000
 * LPTIM6_BASE = 0x44005400
 *
 * RCC_APB3ENR = RCC_BASE + 0x0A8
 *   bit12 = LPTIM3EN
 *   bit13 = LPTIM4EN
 *   bit14 = LPTIM5EN
 *   bit15 = LPTIM6EN
 *
 * LPTIM register offsets (same as LPTIM1):
 *   ISR  +0x00
 *   CFGR +0x0C
 *   CR   +0x10  bit0=ENABLE, bit2=CNTSTRT
 *   ARR  +0x18
 *   CNT  +0x1C
 *
 * FAIL codes:
 *   0xE001 — LPTIM3 CNT did not advance
 *   0xE002 — LPTIM4 CNT did not advance
 *   0xE003 — LPTIM5 CNT did not advance
 *   0xE004 — LPTIM6 CNT did not advance
 *
 * detail0: [31:24]=LPTIM6_CNT[7:0], [23:16]=LPTIM5_CNT[7:0],
 *          [15:8]=LPTIM4_CNT[7:0],  [7:0]=LPTIM3_CNT[7:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))

#define LPTIM3_BASE     0x44004800u
#define LPTIM4_BASE     0x44004C00u
#define LPTIM5_BASE     0x44005000u
#define LPTIM6_BASE     0x44005400u

/* LPTIM register offsets */
#define LPTIM_CFGR_OFF  0x0Cu
#define LPTIM_CR_OFF    0x10u
#define LPTIM_ARR_OFF   0x18u
#define LPTIM_CNT_OFF   0x1Cu

#define LPTIM_CR_ENABLE  (1u << 0)
#define LPTIM_CR_CNTSTRT (1u << 2)

static inline volatile uint32_t *lptim_reg(uint32_t base, uint32_t off)
{
    return (volatile uint32_t *)(base + off);
}

static void lptim_start(uint32_t base)
{
    *lptim_reg(base, LPTIM_CFGR_OFF) = 0u;
    *lptim_reg(base, LPTIM_CR_OFF)   = LPTIM_CR_ENABLE;
    for (volatile uint32_t d = 0u; d < 50u; d++) {}
    *lptim_reg(base, LPTIM_ARR_OFF)  = 0xFFFFu;
    for (volatile uint32_t d = 0u; d < 200u; d++) {}
    *lptim_reg(base, LPTIM_CR_OFF)   = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}

static uint32_t lptim_cnt(uint32_t base)
{
    return *lptim_reg(base, LPTIM_CNT_OFF);
}

int main(void)
{
    ael_mailbox_init();

    /* Enable LPTIM3–6 clocks (APB3ENR bits 12–15) */
    RCC_APB3ENR |= (1u << 12) | (1u << 13) | (1u << 14) | (1u << 15);
    (void)RCC_APB3ENR;

    /* Start all four LPTIMs */
    lptim_start(LPTIM3_BASE);
    lptim_start(LPTIM4_BASE);
    lptim_start(LPTIM5_BASE);
    lptim_start(LPTIM6_BASE);

    /* Spin to let counters advance */
    for (volatile uint32_t d = 0u; d < 50000u; d++) {}

    uint32_t cnt3a = lptim_cnt(LPTIM3_BASE);
    uint32_t cnt4a = lptim_cnt(LPTIM4_BASE);
    uint32_t cnt5a = lptim_cnt(LPTIM5_BASE);
    uint32_t cnt6a = lptim_cnt(LPTIM6_BASE);

    for (volatile uint32_t d = 0u; d < 50000u; d++) {}

    uint32_t cnt3b = lptim_cnt(LPTIM3_BASE);
    uint32_t cnt4b = lptim_cnt(LPTIM4_BASE);
    uint32_t cnt5b = lptim_cnt(LPTIM5_BASE);
    uint32_t cnt6b = lptim_cnt(LPTIM6_BASE);

    if (cnt3a == cnt3b) { ael_mailbox_fail(0xE001u, cnt3a); while (1) {} }
    if (cnt4a == cnt4b) { ael_mailbox_fail(0xE002u, cnt4a); while (1) {} }
    if (cnt5a == cnt5b) { ael_mailbox_fail(0xE003u, cnt5a); while (1) {} }
    if (cnt6a == cnt6b) { ael_mailbox_fail(0xE004u, cnt6a); while (1) {} }

    /* detail0: last CNT reads from each LPTIM */
    AEL_MAILBOX->detail0 = ((cnt6b & 0xFFu) << 24)
                         | ((cnt5b & 0xFFu) << 16)
                         | ((cnt4b & 0xFFu) << 8)
                         | (cnt3b  & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
