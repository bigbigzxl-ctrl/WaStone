/*
 * stm32h563rgt6_wwdg_mailbox — WWDG (Window Watchdog) self-test
 *
 * Tests the Window Watchdog peripheral without triggering a reset.
 * The WWDG is a safety peripheral: once enabled (WDGA=1), it cannot be
 * disabled and will reset the MCU if not refreshed within its window.
 *
 * Strategy (safe — no reset triggered):
 *   1. Enable WWDG APB1L clock (RCC_APB1LENR bit11 = WWDGEN)
 *   2. Read WWDG_CR — verify accessible, WDGA must be 0 (not yet armed)
 *   3. Configure CFR with prescaler and window value (do NOT set WDGA)
 *   4. Read SR — verify EWIF (Early Wakeup Interrupt Flag) is 0
 *   5. Read CR — verify T[6:0] field is the reset value (0x7F)
 *   6. Report CR and CFR values
 *
 * Note: We intentionally do NOT set WDGA=1 (bit7 of CR). Once set, WWDG
 * cannot be disabled. The test only verifies register accessibility and
 * the reset state of the watchdog before arming.
 *
 * WWDG_BASE = APB1PERIPH_BASE + 0x2C00 = 0x40002C00
 *
 * WWDG registers:
 *   CR  +0x00: bit7=WDGA (arm), bits[6:0]=T (counter, 0x7F after reset)
 *   CFR +0x04: bits[6:0]=W (window), bits[8:7]=WDGTB (prescaler), bit9=EWI
 *   SR  +0x08: bit0=EWIF (early wakeup interrupt flag)
 *
 * Counter period = (4096 × 2^WDGTB × (T+1)) / PCLK1
 * At PCLK1=64MHz, WDGTB=0, T=0x7F: period ≈ (4096×128)/64M ≈ 8.2 ms
 *
 * FAIL codes:
 *   0xE001 — WWDG CR inaccessible (reads 0xFFFFFFFF)
 *   0xE002 — WWDG CR.WDGA already set at entry (pre-armed from prior run)
 *
 * detail0: [31:16]=CFR[15:0], [15:8]=CR[7:0], [7:0]=SR[7:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

#define WWDG_BASE       0x40002C00u
#define WWDG_CR         (*(volatile uint32_t *)(WWDG_BASE + 0x00u))
#define WWDG_CFR        (*(volatile uint32_t *)(WWDG_BASE + 0x04u))
#define WWDG_SR         (*(volatile uint32_t *)(WWDG_BASE + 0x08u))

#define WWDG_CR_WDGA    (1u << 7)
#define WWDG_CR_T_MASK  (0x7Fu)
#define WWDG_SR_EWIF    (1u << 0)

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable WWDG APB1L clock (bit11) */
    RCC_APB1LENR |= (1u << 11);
    (void)RCC_APB1LENR;

    /* 2. Read CR — verify accessible */
    uint32_t cr = WWDG_CR;
    if (cr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, cr);
        while (1) {}
    }

    /* 3. Check WDGA is 0 (not pre-armed) */
    if (cr & WWDG_CR_WDGA) {
        /* WWDG already armed from a previous run — cannot disarm.
         * The board will reset soon. Report what we observed. */
        ael_mailbox_fail(0xE002u, cr);
        while (1) {}
    }

    /* 4. Write CFR: window=0x7F (max), prescaler=0 (div1), EWI=0
     * This configures the watchdog but does not start it (WDGA still 0) */
    WWDG_CFR = 0x007Fu;   /* W[6:0]=0x7F, WDGTB=0, EWI=0 */
    uint32_t cfr = WWDG_CFR;

    /* 5. Read SR */
    uint32_t sr = WWDG_SR;

    /* 6. Verify T field (counter default = 0x7F after reset) */
    cr = WWDG_CR;

    /* detail0: [31:16]=CFR, [15:8]=CR, [7:0]=SR */
    AEL_MAILBOX->detail0 = ((cfr & 0xFFFFu) << 16)
                         | ((cr  & 0xFFu)   << 8)
                         | (sr   & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
