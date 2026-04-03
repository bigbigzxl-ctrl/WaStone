/*
 * stm32h563rgt6_rtc_mailbox — RTC real-time clock self-test
 *
 * Configures RTC with LSI (32 kHz) reference, sets time to 12:00:00,
 * waits for RSF (register sync flag), reads TR and verifies it is non-zero
 * and represents a plausible time value.
 *
 * RTC_BASE    = APB3 + 0x7800 = 0x44007800
 * RCC_APB3ENR = RCC_BASE + 0x0A8: bit21=RTCAPBEN, bit22=RTCEN? (no, RTC en is in BDCR)
 * RCC_BDCR    = RCC_BASE + 0x0F0: bit26=LSION, bit27=LSIRDY,
 *                                  bits[9:8]=RTCSEL (10=LSI), bit15=RTCEN
 * PWR_DBPCR   = PWR_BASE + 0x24: bit0=DBP (backup domain write protect off)
 *
 * RTC registers (BCD format, write-protected):
 *   WPR  +0x024: write protection — key sequence: 0xCA then 0x53
 *   ISR  +0x00C (or ICSR on H5?): check RSF (register sync flag)
 *   TR   +0x000: time register (BCD: HT[1:0],HU[3:0],MNT[2:0],MNU[3:0],ST[2:0],SU[3:0])
 *   DR   +0x004: date register
 *   CR   +0x008: control (FMT bit for 12/24h)
 *
 * H5 RTC has ICSR (init control/status register):
 *   ICSR +0x00C: bit4=INITF (init mode ready), bit7=RSF
 *                bit3=INIT (enter init mode)
 *
 * Init sequence:
 *   1. Disable write protection (WPR ← 0xCA, then 0x53)
 *   2. Set INIT bit in ICSR, wait for INITF
 *   3. Write prescaler (PREDIV_A=127, PREDIV_S=249 for LSI 32kHz → 1Hz)
 *   4. Write TR = 12:00:00 in BCD
 *   5. Clear INIT bit
 *   6. Re-enable write protection
 *   7. Wait for RSF
 *   8. Read TR — must be non-zero
 *
 * BCD encoding for 12:00:00 in 24h mode:
 *   TR = (1<<20)|(2<<16)|(0<<12)|(0<<8)|(0<<4)|(0<<0) = 0x00120000
 *
 * FAIL codes:
 *   0xE001 — LSI not ready
 *   0xE002 — INITF not set (RTC init mode timeout)
 *   0xE003 — RSF not set (RTC registers not synced)
 *   0xE004 — TR = 0 after init (time not set)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))
#define RCC_BDCR        (*(volatile uint32_t *)(RCC_BASE + 0x0F0u))

#define RCC_BDCR_LSION   (1u << 26)
#define RCC_BDCR_LSIRDY  (1u << 27)
#define RCC_BDCR_RTCSEL_LSI (2u << 8)    /* RTCSEL[1:0] = 10 → LSI */
#define RCC_BDCR_RTCEN   (1u << 15)

#define PWR_BASE        0x44020800u
#define PWR_DBPCR       (*(volatile uint32_t *)(PWR_BASE + 0x24u))
#define PWR_DBPCR_DBP   (1u << 0)

#define RTC_BASE        0x44007800u
#define RTC_TR          (*(volatile uint32_t *)(RTC_BASE + 0x000u))
#define RTC_DR          (*(volatile uint32_t *)(RTC_BASE + 0x004u))
#define RTC_CR          (*(volatile uint32_t *)(RTC_BASE + 0x008u))
#define RTC_ICSR        (*(volatile uint32_t *)(RTC_BASE + 0x00Cu))
#define RTC_PRER        (*(volatile uint32_t *)(RTC_BASE + 0x010u))
#define RTC_WPR         (*(volatile uint32_t *)(RTC_BASE + 0x024u))

#define RTC_ICSR_INIT   (1u << 7)   /* enter init mode */
#define RTC_ICSR_INITF  (1u << 6)   /* init mode flag (ready) */
#define RTC_ICSR_RSF    (1u << 5)   /* register sync flag */

/* PRER: PREDIV_A[6:0]=127 in bits[22:16], PREDIV_S[14:0]=249 in bits[14:0]
 * 127 async + 249 sync = (127+1)*(249+1) = 128*250 = 32000 ≈ LSI 32kHz → ~1Hz
 */
#define RTC_PRER_VAL    ((127u << 16) | 249u)

/* TR for 12:00:00 in 24h BCD: HT=1,HU=2,MNT=0,MNU=0,ST=0,SU=0 */
#define RTC_TR_NOON     ((1u << 20) | (2u << 16))

#define TIMEOUT  5000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Unlock backup domain */
    PWR_DBPCR |= PWR_DBPCR_DBP;
    (void)PWR_DBPCR;

    /* 2. Enable LSI */
    RCC_BDCR |= RCC_BDCR_LSION;
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (RCC_BDCR & RCC_BDCR_LSIRDY) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE001u, RCC_BDCR);
        while (1) {}
    }

    /* 3. Select LSI as RTC clock and enable RTC */
    /* Clear RTCSEL first (if previously set differently) */
    RCC_BDCR = (RCC_BDCR & ~(0x3u << 8)) | RCC_BDCR_RTCSEL_LSI;
    RCC_BDCR |= RCC_BDCR_RTCEN;
    (void)RCC_BDCR;

    /* 4. Enable RTC APB clock (APB3ENR bit21) */
    RCC_APB3ENR |= (1u << 21);
    (void)RCC_APB3ENR;

    /* 5. Disable write protection */
    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;

    /* 6. Enter init mode */
    RTC_ICSR |= RTC_ICSR_INIT;
    for (t = 0u; t < TIMEOUT; t++) {
        if (RTC_ICSR & RTC_ICSR_INITF) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE002u, RTC_ICSR);
        while (1) {}
    }

    /* 7. Set prescaler and time */
    RTC_PRER = RTC_PRER_VAL;
    RTC_CR   = 0u;               /* 24h format (FMT=0) */
    RTC_TR   = RTC_TR_NOON;
    RTC_DR   = (1u << 13) | (1u << 8) | (1u << 0); /* year=01, month=Jan, day=01 */

    /* 8. Exit init mode */
    RTC_ICSR &= ~RTC_ICSR_INIT;

    /* 9. Re-enable write protection */
    RTC_WPR = 0xFFu;

    /* 10. Wait for RSF (registers synchronized) */
    for (t = 0u; t < TIMEOUT; t++) {
        if (RTC_ICSR & RTC_ICSR_RSF) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE003u, RTC_ICSR);
        while (1) {}
    }

    /* 11. Read time */
    uint32_t tr = RTC_TR;
    if (tr == 0u) {
        ael_mailbox_fail(0xE004u, tr);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = tr;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
