#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x40021000u
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))
#define RCC_BDCR        (*(volatile uint32_t *)(RCC_BASE + 0x20u))
#define RCC_CSR         (*(volatile uint32_t *)(RCC_BASE + 0x24u))
#define RCC_PWREN       (1u << 28)
#define RCC_BKPEN       (1u << 27)
#define RCC_LSION       (1u << 0)
#define RCC_LSIRDY      (1u << 1)
#define RCC_RTCSEL_LSI  (0x2u << 8)
#define RCC_RTCEN       (1u << 15)
#define RCC_BDRST       (1u << 16)

/* PWR */
#define PWR_BASE        0x40007000u
#define PWR_CR          (*(volatile uint32_t *)(PWR_BASE + 0x00u))
#define PWR_DBP         (1u << 8)

/* BKP */
#define BKP_BASE        0x40006C00u
#define BKP_DR1         (*(volatile uint32_t *)(BKP_BASE + 0x04u))
#define BKP_DR2         (*(volatile uint32_t *)(BKP_BASE + 0x08u))
#define BKP_DR3         (*(volatile uint32_t *)(BKP_BASE + 0x0Cu))
#define BKP_DR4         (*(volatile uint32_t *)(BKP_BASE + 0x10u))

/* RTC */
#define RTC_BASE        0x40002800u
#define RTC_CRL         (*(volatile uint32_t *)(RTC_BASE + 0x04u))
#define RTC_PRLH        (*(volatile uint32_t *)(RTC_BASE + 0x08u))
#define RTC_PRLL        (*(volatile uint32_t *)(RTC_BASE + 0x0Cu))
#define RTC_CNTH        (*(volatile uint32_t *)(RTC_BASE + 0x18u))
#define RTC_CNTL        (*(volatile uint32_t *)(RTC_BASE + 0x1Cu))
#define RTC_RTOFF       (1u << 5)
#define RTC_CNF         (1u << 4)
#define RTC_RSF         (1u << 3)

/* RTC prescaler: LSI ~40 kHz → PRL=399 → ~100 Hz ticks */
#define RTC_PRL_VALUE   399u

#define ERR_BKP_MISMATCH    0x01u
#define ERR_LSI_TIMEOUT     0x02u
#define ERR_RTC_CFG_TIMEOUT 0x03u
#define ERR_RTC_NO_TICK     0x04u

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static uint8_t wait_rtoff(void)
{
    for (uint32_t i = 0u; i < 1000000u; ++i) {
        if ((RTC_CRL & RTC_RTOFF) != 0u) {
            return 1u;
        }
    }
    return 0u;
}

int main(void)
{
    ael_mailbox_init();

    /* Enable PWR and BKP clocks, unlock backup domain */
    RCC_APB1ENR |= RCC_PWREN | RCC_BKPEN;
    (void)RCC_APB1ENR;
    PWR_CR |= PWR_DBP;
    delay_cycles(10000u);

    /* --- Test 1: backup registers --- */
    static const uint16_t bkp_pat[4] = {0x5A5Au, 0xA5A5u, 0x1234u, 0xABCDu};
    BKP_DR1 = bkp_pat[0];
    BKP_DR2 = bkp_pat[1];
    BKP_DR3 = bkp_pat[2];
    BKP_DR4 = bkp_pat[3];

    if (BKP_DR1 != bkp_pat[0] || BKP_DR2 != bkp_pat[1] ||
        BKP_DR3 != bkp_pat[2] || BKP_DR4 != bkp_pat[3]) {
        ael_mailbox_fail(ERR_BKP_MISMATCH, 0u);
        while (1) {}
    }

    /* --- Reset backup domain for clean RTC config --- */
    RCC_BDCR |= RCC_BDRST;
    delay_cycles(10000u);
    RCC_BDCR &= ~RCC_BDRST;
    delay_cycles(10000u);
    PWR_CR |= PWR_DBP; /* re-assert DBP after BDRST */
    delay_cycles(10000u);

    /* --- Test 2: RTC ticks with LSI --- */
    /* Enable LSI */
    RCC_CSR |= RCC_LSION;
    uint32_t timeout = 1000000u;
    while (((RCC_CSR & RCC_LSIRDY) == 0u) && timeout-- > 0u) {}
    if ((RCC_CSR & RCC_LSIRDY) == 0u) {
        ael_mailbox_fail(ERR_LSI_TIMEOUT, 0u);
        while (1) {}
    }

    /* Select LSI, enable RTC */
    RCC_BDCR = RCC_RTCSEL_LSI | RCC_RTCEN;
    delay_cycles(10000u);

    /* Enter RTC config mode */
    if (!wait_rtoff()) {
        ael_mailbox_fail(ERR_RTC_CFG_TIMEOUT, 1u);
        while (1) {}
    }
    RTC_CRL |= RTC_CNF;
    RTC_PRLH = 0u;
    RTC_PRLL = RTC_PRL_VALUE;   /* ~100 Hz ticks */
    RTC_CNTH = 0u;
    RTC_CNTL = 0u;
    RTC_CRL &= ~RTC_CNF;
    if (!wait_rtoff()) {
        ael_mailbox_fail(ERR_RTC_CFG_TIMEOUT, 2u);
        while (1) {}
    }

    /* Wait for RSF (registers synced after clock domain crossing) */
    RTC_CRL &= ~RTC_RSF;
    timeout = 1000000u;
    while (((RTC_CRL & RTC_RSF) == 0u) && timeout-- > 0u) {}

    uint32_t cnt0 = ((uint32_t)RTC_CNTH << 16u) | RTC_CNTL;

    /* ~100 ms wait — at 100 Hz ticks, expect ~10 ticks */
    delay_cycles(800000u);

    uint32_t cnt1 = ((uint32_t)RTC_CNTH << 16u) | RTC_CNTL;

    if (cnt1 == cnt0) {
        ael_mailbox_fail(ERR_RTC_NO_TICK, cnt0);
        while (1) {}
    }

    /* PASS: detail0 = ticks observed in ~100 ms */
    AEL_MAILBOX->detail0 = cnt1 - cnt0;
    ael_mailbox_pass();

    while (1) {
        AEL_MAILBOX->detail0 = ((uint32_t)RTC_CNTH << 16u) | RTC_CNTL;
        delay_cycles(40000u);
    }
}
