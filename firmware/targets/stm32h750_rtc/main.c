/*
 * stm32h750_rtc — RTC seconds counting self-test
 *
 * Configures RTC with LSI (~32 kHz) as clock source.
 * Prescalers: PREDIV_A=127 (async), PREDIV_S=249 (sync)
 *   → fCK_SPRE = 32000 / (128 × 250) ≈ 1 Hz
 * Sets time 00:00:00, waits 3 seconds, reads TR seconds field.
 * Accept seconds in [2, 5] (generous for ±20% LSI tolerance).
 *
 * RTC base: 0x58004000 (D3_APB1PERIPH_BASE + 0x4000)
 * LSI enabled via RCC_CSR (offset 0x74): LSION bit0
 * Backup domain access: PWR_CR1 (PWR_BASE+0x00) bit8 = DBP
 * RCC_BDCR (offset 0x70): RTCSEL=LSI (10 at bits[9:8]), RTCEN bit15
 * RCC_APB4ENR (offset 0xF4) bit16 = RTCAPBEN
 *
 * Write protection sequence: WPR = 0xCA then 0x53 to unlock.
 * Lock: WPR = 0xFF.
 *
 * Error codes:
 *   0xE001 = LSI ready timeout
 *   0xE002 = RTC init mode (INITF) timeout
 *   0xE003 = seconds out of expected range (detail0 = TR raw value)
 *
 * On PASS: detail0 = raw RTC_TR value (seconds in BCD at bits[6:0])
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE      0x58024400u
#define RCC_BDCR      (*(volatile uint32_t *)(RCC_BASE + 0x070u))
#define RCC_CSR       (*(volatile uint32_t *)(RCC_BASE + 0x074u))
#define RCC_APB4ENR   (*(volatile uint32_t *)(RCC_BASE + 0x0F4u))

#define RCC_CSR_LSION       (1u << 0u)
#define RCC_CSR_LSIRDY      (1u << 1u)
#define RCC_BDCR_BDRST      (1u << 16u)
#define RCC_BDCR_RTCEN      (1u << 15u)
#define RCC_BDCR_RTCSEL_LSI (2u << 8u)   /* RTCSEL bits[9:8] = 10 → LSI */
#define RCC_APB4ENR_RTCAPBEN (1u << 16u)

/* ── PWR ─────────────────────────────────────────────────────────── */
#define PWR_BASE      0x58024800u
#define PWR_CR1       (*(volatile uint32_t *)(PWR_BASE + 0x000u))
#define PWR_CR1_DBP   (1u << 8u)   /* disable backup protection */

/* ── RTC (D3_APB1PERIPH_BASE + 0x4000 = 0x58004000) ─────────────── */
#define RTC_BASE      0x58004000u
#define RTC_TR        (*(volatile uint32_t *)(RTC_BASE + 0x000u))
#define RTC_DR        (*(volatile uint32_t *)(RTC_BASE + 0x004u))
#define RTC_CR        (*(volatile uint32_t *)(RTC_BASE + 0x008u))
#define RTC_ISR       (*(volatile uint32_t *)(RTC_BASE + 0x00Cu))
#define RTC_PRER      (*(volatile uint32_t *)(RTC_BASE + 0x010u))
#define RTC_WPR       (*(volatile uint32_t *)(RTC_BASE + 0x024u))

/* ISR bits */
#define RTC_ISR_INITF  (1u << 6u)   /* initialization mode flag */
#define RTC_ISR_INIT   (1u << 7u)   /* initialization mode request */
#define RTC_ISR_RSF    (1u << 5u)   /* registers sync flag */

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0u)
#define SYST_CSR_CLKSOURCE (1u << 2u)
#define SYST_CSR_COUNTFLAG (1u << 16u)

static void delay_ms(uint32_t ms)
{
    for (uint32_t m = 0u; m < ms; m++) {
        SYST_CVR = 0u;
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

    /* Enable RTC APB bus clock */
    RCC_APB4ENR |= RCC_APB4ENR_RTCAPBEN;
    (void)RCC_APB4ENR;

    /* Unlock backup domain: PWR_CR1.DBP = 1 */
    PWR_CR1 |= PWR_CR1_DBP;
    (void)PWR_CR1;
    delay_ms(1u);

    /* Reset backup domain to clear any previous RTC config */
    RCC_BDCR |= RCC_BDCR_BDRST;
    (void)RCC_BDCR;
    delay_ms(1u);
    RCC_BDCR &= ~RCC_BDCR_BDRST;
    (void)RCC_BDCR;
    delay_ms(1u);

    /* Enable LSI */
    RCC_CSR |= RCC_CSR_LSION;
    uint32_t timeout = 1000000u;
    while ((RCC_CSR & RCC_CSR_LSIRDY) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE001u, RCC_CSR);
            while (1) {}
        }
    }

    /* Configure BDCR: select LSI as RTC clock, enable RTC */
    RCC_BDCR = RCC_BDCR_RTCSEL_LSI | RCC_BDCR_RTCEN;
    (void)RCC_BDCR;
    delay_ms(2u);

    /* Disable RTC write protection */
    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;

    /* Enter initialization mode */
    RTC_ISR = RTC_ISR_INIT;
    timeout = 1000000u;
    while ((RTC_ISR & RTC_ISR_INITF) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE002u, RTC_ISR);
            while (1) {}
        }
    }

    /*
     * Set prescalers for ~1 Hz from LSI (~32 kHz):
     * PREDIV_A = 127 (bits [22:16]), PREDIV_S = 249 (bits [14:0])
     * fCK_APRE = 32000 / (127+1) = 250 Hz
     * fCK_SPRE = 250 / (249+1) = 1 Hz
     */
    RTC_PRER = (127u << 16u) | 249u;

    /* Set time to 00:00:00 (BCD, all zeros = 00:00:00) */
    RTC_TR = 0u;
    RTC_DR = 0x00002101u;   /* arbitrary valid date: 2021-01-01 (Monday) */

    /* Exit initialization mode */
    RTC_ISR &= ~RTC_ISR_INIT;

    /* Re-enable write protection */
    RTC_WPR = 0xFFu;

    /* Wait 3 seconds via SysTick for RTC to tick */
    delay_ms(3000u);

    /* Wait for RSF (registers shadow synchronized) */
    timeout = 100000u;
    while ((RTC_ISR & RTC_ISR_RSF) == 0u) {
        if (--timeout == 0u) { break; }
    }

    /* Read TR: seconds in BCD at bits[6:4]=ST, [3:0]=SU */
    uint32_t tr = RTC_TR;
    uint32_t seconds_tens = (tr >> 4u) & 0x7u;
    uint32_t seconds_units = tr & 0xFu;
    uint32_t seconds = seconds_tens * 10u + seconds_units;

    /* Accept 2–5 seconds (generous for LSI ±20% tolerance) */
    if (seconds < 2u || seconds > 5u) {
        ael_mailbox_fail(0xE003u, tr);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = tr;   /* raw TR value (seconds field at bits[6:0]) */
    while (1) {}
}
