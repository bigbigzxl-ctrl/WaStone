/*
 * stm32h750_wwdg — Window Watchdog self-test
 *
 * WWDG1: APB3, base 0x50003000 (RM0433 §49).
 * RCC_APB3ENR bit 6 = WWDG1EN. Clock = PCLK3 = 64 MHz / 1 = 64 MHz.
 *
 * WWDG configuration:
 *   Prescaler WDGTB = /8  → fWWDG = 64 MHz / 4096 / 8 = ~1953 Hz
 *   Counter T[6:0] = 127 (max, 0x7F)
 *   Window  W[6:0] = 80  (0x50) — refresh allowed when CNT ≤ 80
 *   Timeout ≈ (127 - 63) / 1953 ≈ 32.7 ms (never reached in this test)
 *
 * Strategy: start WWDG, refresh it 5 times within the window,
 * then verify we're still alive (mailbox PASS before it resets us).
 * We deliberately do NOT let it expire — this tests the feed path.
 *
 * WWDG registers (RM0433 §49.4):
 *   CR  offset 0x00: bit7=WDGA (activate), bits[6:0]=T (counter)
 *   CFR offset 0x04: bits[8:7]=WDGTB (prescaler), bits[6:0]=W (window)
 *   SR  offset 0x08: bit0=EWIF (early wakeup interrupt flag)
 *
 * Note: WWDG reset is in RCC_RSR (bit 30 = WWDG1RSTF on H750).
 * We check this to confirm the test did NOT reset (a reset means we
 * missed the window or were too early — both are faults).
 *
 * Error codes:
 *   0xE001 = WWDG1RSTF set — board was reset by WWDG (missed feed)
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE        0x58024400u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))
#define RCC_RSR         (*(volatile uint32_t *)(RCC_BASE + 0x0D0u))
#define RCC_APB3ENR_WWDG1EN (1u << 6u)
#define RCC_RSR_WWDG1RSTF   (1u << 30u)
#define RCC_RSR_RMVF        (1u << 16u)   /* clear reset flags */

/* ── WWDG1 (APB3, 0x50003000) ───────────────────────────────────── */
#define WWDG1_BASE  0x50003000u
#define WWDG1_CR    (*(volatile uint32_t *)(WWDG1_BASE + 0x00u))
#define WWDG1_CFR   (*(volatile uint32_t *)(WWDG1_BASE + 0x04u))
#define WWDG1_SR    (*(volatile uint32_t *)(WWDG1_BASE + 0x08u))

#define WWDG_CR_WDGA  (1u << 7u)   /* activate watchdog (write-once) */
#define WWDG_CR_T_MAX 0x7Fu        /* max counter value */

/* CFR: WDGTB[8:7] prescaler bits; W[6:0] window */
#define WWDG_CFR_WDGTB_DIV8  (0x3u << 7u)   /* /8 prescaler */
#define WWDG_WINDOW           0x50u           /* allow refresh when T ≤ 0x50 */

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static void delay_ticks(uint32_t ticks)
{
    for (volatile uint32_t i = 0u; i < ticks; i++) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Check if we got here via a WWDG reset (should not happen) */
    if ((RCC_RSR & RCC_RSR_WWDG1RSTF) != 0u) {
        uint32_t rsr = RCC_RSR;
        RCC_RSR |= RCC_RSR_RMVF;   /* clear reset flags */
        ael_mailbox_fail(0xE001u, rsr);
        while (1) {}
    }
    /* Clear any prior reset flags for clean state */
    RCC_RSR |= RCC_RSR_RMVF;

    /* Enable WWDG1 clock */
    RCC_APB3ENR |= RCC_APB3ENR_WWDG1EN;
    (void)RCC_APB3ENR;

    /*
     * Configure CFR before activating:
     *   WDGTB = /8 → fWWDG ≈ 1953 Hz → period ≈ 512 µs per count
     *   Window W = 0x50 = 80 (refresh allowed when T[6:0] ≤ 80)
     */
    WWDG1_CFR = WWDG_CFR_WDGTB_DIV8 | WWDG_WINDOW;

    /*
     * Activate WWDG with T = 0x7F (127 counts until underflow at 63).
     * Writing WDGA=1 is irreversible — watchdog is now running.
     * Timeout = (127 - 63) / 1953 ≈ 32.7 ms.
     */
    WWDG1_CR = WWDG_CR_WDGA | WWDG_CR_T_MAX;

    /*
     * Refresh 5 times. Each refresh resets T to 0x7F.
     * Wait 5 ms between refreshes — well within the 32.7 ms window.
     * The window check: T must be ≤ W (0x50=80) when we write CR.
     * After ~5 ms: T decremented by ~5ms/0.512ms ≈ 10 counts → T ≈ 117.
     * 117 > 80 → writing now would be TOO EARLY (outside window → reset!).
     *
     * Correct approach: wait until T has counted down below window (T ≤ 80),
     * then refresh. At 1953 Hz, 127→80 takes (127-80)/1953 ≈ 24 ms.
     * So we wait 25 ms before each refresh.
     */
    for (uint32_t i = 0u; i < 5u; i++) {
        delay_ticks(25u);   /* 25 ms: T counts from 127 → ~78, inside window */
        WWDG1_CR = WWDG_CR_WDGA | WWDG_CR_T_MAX;   /* refresh: reload T=127 */
    }

    /* Still alive — WWDG did not expire and did not reset us */
    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 5u;   /* 5 successful refreshes */
    while (1) {}
}
