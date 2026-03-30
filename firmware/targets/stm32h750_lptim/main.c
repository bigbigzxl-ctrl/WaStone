/*
 * stm32h750_lptim — LPTIM1 continuous counting self-test
 *
 * LPTIM1 (D2, APB1, 0x40002400) counts with internal clock (PCLK1).
 * Config: PRESC=÷128 → 64 MHz/128 = 500 kHz tick.
 * After 50 ms SysTick delay, CNT should be ~25 000.
 * Accept ±15% → [21250 … 28750].
 *
 * RCC_APB1LENR bit 9 = LPTIM1EN
 * PCLK1 (APB1) = 64 MHz at default HSI boot.
 *
 * Error codes:
 *   0xE001 = ARROK timeout (ARR register update never confirmed)
 *   0xE002 = CNTSTRT did not start counting (CNT still 0 after timeout)
 *   0xE003 = CNT out of range (detail0 = measured count)
 *
 * On PASS: detail0 = measured CNT value (expect ~25000)
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE          0x58024400u
#define RCC_APB1LENR      (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))
#define RCC_APB1LENR_LPTIM1EN  (1u << 9u)

/* ── LPTIM1 (D2_APB1PERIPH_BASE + 0x2400 = 0x40002400) ─────────── */
#define LPTIM1_BASE  0x40002400u
#define LPTIM1_ISR   (*(volatile uint32_t *)(LPTIM1_BASE + 0x00u))
#define LPTIM1_ICR   (*(volatile uint32_t *)(LPTIM1_BASE + 0x04u))
#define LPTIM1_IER   (*(volatile uint32_t *)(LPTIM1_BASE + 0x08u))
#define LPTIM1_CFGR  (*(volatile uint32_t *)(LPTIM1_BASE + 0x0Cu))
#define LPTIM1_CR    (*(volatile uint32_t *)(LPTIM1_BASE + 0x10u))
#define LPTIM1_CMP   (*(volatile uint32_t *)(LPTIM1_BASE + 0x14u))
#define LPTIM1_ARR   (*(volatile uint32_t *)(LPTIM1_BASE + 0x18u))
#define LPTIM1_CNT   (*(volatile uint32_t *)(LPTIM1_BASE + 0x1Cu))

/* ISR bits */
#define LPTIM_ISR_ARROK  (1u << 4u)   /* ARR write OK */
#define LPTIM_ISR_CMPOK  (1u << 3u)   /* CMP write OK */
/* CR bits */
#define LPTIM_CR_ENABLE    (1u << 0u)
#define LPTIM_CR_SNGSTRT   (1u << 1u)
#define LPTIM_CR_CNTSTRT   (1u << 2u)

/*
 * CFGR: PRESC[11:9] = 0b111 (÷128)
 * CKSEL[0] = 0 (internal APB clock)
 */
#define LPTIM_CFGR_PRESC128  (7u << 9u)

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

    /* Enable LPTIM1 clock */
    RCC_APB1LENR |= RCC_APB1LENR_LPTIM1EN;
    (void)RCC_APB1LENR;

    /* Configure: PRESC=÷128, internal clock (CKSEL=0) */
    LPTIM1_CFGR = LPTIM_CFGR_PRESC128;

    /* Enable LPTIM1 (must be done before writing ARR) */
    LPTIM1_CR = LPTIM_CR_ENABLE;

    /* Write ARR = 0xFFFF (free-running 16-bit) */
    LPTIM1_ARR = 0xFFFFu;

    /* Wait for ARROK (ARR register update handshake) */
    uint32_t timeout = 1000000u;
    while ((LPTIM1_ISR & LPTIM_ISR_ARROK) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE001u, LPTIM1_ISR);
            while (1) {}
        }
    }

    /* Start continuous counting */
    LPTIM1_CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;

    /* Wait 50 ms — expect ~25000 ticks at 500 kHz */
    delay_ms(50u);

    /* Read CNT (may need two reads for coherence per RM0433) */
    uint32_t cnt1 = LPTIM1_CNT;
    uint32_t cnt2 = LPTIM1_CNT;
    uint32_t cnt = (cnt1 == cnt2) ? cnt1 : LPTIM1_CNT;

    /* Sanity: counter should have started */
    if (cnt == 0u) {
        ael_mailbox_fail(0xE002u, 0u);
        while (1) {}
    }

    /* Accept 21250 – 28750 (±15% of 25000) */
    if (cnt < 21250u || cnt > 28750u) {
        ael_mailbox_fail(0xE003u, cnt);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = cnt;
    while (1) {}
}
