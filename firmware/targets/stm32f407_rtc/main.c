/*
 * STM32F407 — RTC with LSI Verification Test
 *
 * Uses the internal LSI (~32 kHz) oscillator as RTC clock source.
 * Configures RTC with async/sync prescalers for 1 Hz tick:
 *   PREDIV_A = 127, PREDIV_S = 249  →  32000 / 128 / 250 = 1.000 Hz
 *
 * Test flow:
 *   1. Enable PWR clock, unlock backup domain (DBP=1)
 *   2. Enable LSI; wait LSIRDY (timeout → FAIL 1)
 *   3. Select LSI as RTC source; enable RTC clock
 *   4. Enter RTC init mode; configure prescalers; set 00:00:00
 *   5. Exit init mode; wait RSF
 *   6. Poll TR seconds field until it increments (≤3 s window)
 *      Timeout → FAIL 2; incremented → PASS
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = measured loop count when second incremented
 *   FAIL: error_code = failing phase (1=LSI, 2=second never incremented)
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---------- Register addresses ---------- */
#define RCC_BASE    0x40023800u
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40u))
#define RCC_CSR     (*(volatile uint32_t *)(RCC_BASE + 0x74u))
#define RCC_BDCR    (*(volatile uint32_t *)(RCC_BASE + 0x70u))

#define PWR_BASE    0x40007000u
#define PWR_CR      (*(volatile uint32_t *)(PWR_BASE + 0x00u))

#define RTC_BASE    0x40002800u
#define RTC_TR      (*(volatile uint32_t *)(RTC_BASE + 0x00u))
#define RTC_ISR     (*(volatile uint32_t *)(RTC_BASE + 0x0Cu))
#define RTC_PRER    (*(volatile uint32_t *)(RTC_BASE + 0x10u))
#define RTC_WPR     (*(volatile uint32_t *)(RTC_BASE + 0x24u))

/* ---------- Bit definitions ---------- */
#define RCC_APB1ENR_PWREN   (1u << 28)
#define PWR_CR_DBP          (1u << 8)
#define RCC_CSR_LSION       (1u << 0)
#define RCC_CSR_LSIRDY      (1u << 1)
#define RCC_BDCR_RTCSEL_LSI (2u << 8)   /* RTCSEL[9:8] = 10 */
#define RCC_BDCR_RTCEN      (1u << 15)
#define RTC_ISR_INITF       (1u << 6)
#define RTC_ISR_INIT        (1u << 7)
#define RTC_ISR_RSF         (1u << 5)

static void delay_cycles(uint32_t n)
{
    volatile uint32_t i = n;
    while (i--) {}
}

int main(void)
{
    ael_mailbox_init();

    /* ---- Step 1: Enable PWR clock, unlock backup domain ---- */
    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC_APB1ENR; /* read-back to ensure write completes */
    PWR_CR |= PWR_CR_DBP;

    /* ---- Step 2: Enable LSI, wait LSIRDY ---- */
    RCC_CSR |= RCC_CSR_LSION;
    uint32_t timeout = 500000u;
    while (!(RCC_CSR & RCC_CSR_LSIRDY)) {
        if (--timeout == 0u) {
            ael_mailbox_fail(1u, 0u);
            while (1) {}
        }
    }

    /* ---- Step 3: Select LSI as RTC clock source, enable RTC ---- */
    /* Clear RTCSEL first (must be done when RTCEN=0) */
    RCC_BDCR &= ~(3u << 8);
    RCC_BDCR |= RCC_BDCR_RTCSEL_LSI;
    RCC_BDCR |= RCC_BDCR_RTCEN;
    delay_cycles(2000u);

    /* ---- Step 4: Unlock RTC write protection ---- */
    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;

    /* Enter init mode */
    RTC_ISR |= RTC_ISR_INIT;
    timeout = 200000u;
    while (!(RTC_ISR & RTC_ISR_INITF)) {
        if (--timeout == 0u) {
            ael_mailbox_fail(1u, 1u);
            while (1) {}
        }
    }

    /* Set prescalers: PREDIV_A=127, PREDIV_S=249
     * Must write async first (high bits), then sync (low bits) in same write
     * PRER[22:16] = PREDIV_A, PRER[14:0] = PREDIV_S */
    RTC_PRER = (127u << 16) | 249u;

    /* Set time to 00:00:00 BCD */
    RTC_TR = 0x00000000u;

    /* Exit init mode */
    RTC_ISR &= ~RTC_ISR_INIT;

    /* Re-lock write protection */
    RTC_WPR = 0xFFu;

    /* Wait for RSF (shadow registers synchronized) */
    RTC_ISR &= ~RTC_ISR_RSF;
    timeout = 200000u;
    while (!(RTC_ISR & RTC_ISR_RSF)) {
        if (--timeout == 0u) {
            ael_mailbox_fail(1u, 2u);
            while (1) {}
        }
    }

    /* ---- Step 5: Poll TR seconds field for increment ---- */
    /* At 16MHz HSI, ~4.8M iterations ≈ 3 seconds */
    uint32_t initial_su = (RTC_TR >> 0) & 0xFu;   /* BCD seconds units */
    uint32_t initial_st = (RTC_TR >> 4) & 0x7u;   /* BCD seconds tens */
    uint32_t loop_count = 0u;
    uint32_t max_loops = 6000000u;   /* ~3.5 s at 16 MHz */

    while (loop_count < max_loops) {
        uint32_t tr = RTC_TR;
        uint32_t su = (tr >> 0) & 0xFu;
        uint32_t st = (tr >> 4) & 0x7u;
        if (su != initial_su || st != initial_st) {
            /* Seconds field changed — RTC is ticking */
            ael_mailbox_pass();
            AEL_MAILBOX->detail0 = loop_count;
            while (1) {}
        }
        loop_count++;
    }

    /* Timeout: second never incremented */
    ael_mailbox_fail(2u, 0u);
    while (1) {}
}
