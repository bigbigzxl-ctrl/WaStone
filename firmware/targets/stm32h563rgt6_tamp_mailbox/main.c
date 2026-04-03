/*
 * stm32h563rgt6_tamp_mailbox — TAMP backup register read/write self-test
 *
 * Verifies that the TAMP backup registers (BKPxR) are accessible and
 * can store/retrieve a pattern across the backup power domain.
 *
 * Steps:
 *   1. Enable RTC/TAMP APB clock (RCC_APB3ENR bit21 = RTCAPBEN)
 *   2. Unlock backup domain access (PWR_DBPCR.DBP = bit0)
 *   3. Write test pattern 0xDEADBEEF to BKP0R, 0x12345678 to BKP1R
 *   4. Read back and verify both values
 *
 * TAMP_BASE   = 0x44007C00 (secure alias 0x54007C00, using non-secure)
 * BKP0R       = TAMP_BASE + 0x100
 * BKP1R       = TAMP_BASE + 0x104
 *
 * PWR_BASE    = 0x44020800
 * PWR_DBPCR   = PWR_BASE + 0x24  (backup domain protection control)
 *   bit0 = DBP (disable backup domain write protection)
 *
 * RCC_BASE    = 0x44020C00
 * RCC_APB3ENR = RCC_BASE + 0x0A8
 *   bit21 = RTCAPBEN
 *
 * FAIL codes:
 *   0xE001 — BKP0R readback mismatch, detail0 = actual value
 *   0xE002 — BKP1R readback mismatch, detail0 = actual value
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))

#define PWR_BASE        0x44020800u
#define PWR_DBPCR       (*(volatile uint32_t *)(PWR_BASE + 0x024u))
#define PWR_DBPCR_DBP   (1u << 0)

#define TAMP_BASE       0x44007C00u
#define TAMP_BKP0R      (*(volatile uint32_t *)(TAMP_BASE + 0x100u))
#define TAMP_BKP1R      (*(volatile uint32_t *)(TAMP_BASE + 0x104u))

#define PATTERN0        0xDEADBEEFu
#define PATTERN1        0x12345678u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable RTC/TAMP APB clock */
    RCC_APB3ENR |= (1u << 21);
    (void)RCC_APB3ENR;

    /* 2. Unlock backup domain write access */
    PWR_DBPCR |= PWR_DBPCR_DBP;
    (void)PWR_DBPCR;

    /* 3. Write test patterns to backup registers */
    TAMP_BKP0R = PATTERN0;
    TAMP_BKP1R = PATTERN1;

    /* 4. Read back and verify */
    uint32_t rb0 = TAMP_BKP0R;
    uint32_t rb1 = TAMP_BKP1R;

    if (rb0 != PATTERN0) {
        ael_mailbox_fail(0xE001u, rb0);
        while (1) {}
    }
    if (rb1 != PATTERN1) {
        ael_mailbox_fail(0xE002u, rb1);
        while (1) {}
    }

    /* detail0: BKP0R value (should be 0xDEADBEEF) */
    AEL_MAILBOX->detail0 = rb0;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
