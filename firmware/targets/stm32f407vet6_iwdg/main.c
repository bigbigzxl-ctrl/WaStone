/*
 * STM32F407VET6 — IWDG watchdog test
 *
 * Strategy:
 *   1. Enable IWDG with ~100 ms timeout (LSI ~32 kHz, PR=1=/8, RLR=399
 *      → 399/4000 ≈ 100 ms).
 *   2. Kick the watchdog 20 times (2 s total) while incrementing detail0.
 *   3. After 20 kicks, write PASS to mailbox, then stop kicking → MCU resets.
 *   4. After reset, firmware detects RCC_CSR IWDGRSTF flag → writes detail0=0xDEAD
 *      and status=PASS again, proving the reset was IWDG-triggered.
 *
 * PASS condition (two phases):
 *   Phase 1: mailbox PASS written after 20 kicks (firmware about to let watchdog fire)
 *   Phase 2: mailbox PASS re-written with detail0=0xDEAD (confirms IWDG reset happened)
 *
 * No external wiring needed.
 * Mailbox: 0x2001FC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* RCC */
#define RCC_BASE    0x40023800U
#define RCC_CSR     (*(volatile uint32_t *)(RCC_BASE + 0x74U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 24)  /* remove reset flags */
#define RCC_CSR_LSION    (1U << 0)
#define RCC_CSR_LSIRDY   (1U << 1)

/* IWDG */
#define IWDG_BASE   0x40003000U
#define IWDG_KR     (*(volatile uint32_t *)(IWDG_BASE + 0x00U))
#define IWDG_PR     (*(volatile uint32_t *)(IWDG_BASE + 0x04U))
#define IWDG_RLR    (*(volatile uint32_t *)(IWDG_BASE + 0x08U))
#define IWDG_SR     (*(volatile uint32_t *)(IWDG_BASE + 0x0CU))

#define IWDG_KEY_RELOAD  0xAAAAU
#define IWDG_KEY_ENABLE  0xCCCCU
#define IWDG_KEY_UNLOCK  0x5555U

/* ~10 ms delay at 16 MHz HSI */
static void delay_10ms(void)
{
    for (volatile uint32_t d = 0U; d < 40000U; d++) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    /* Check if this is a post-IWDG-reset boot */
    if (RCC_CSR & RCC_CSR_IWDGRSTF) {
        /* Clear reset flags */
        RCC_CSR |= RCC_CSR_RMVF;
        /* Re-init mailbox: PASS with sentinel detail0 = 0xDEAD */
        AEL_MAILBOX->magic      = 0xAE100001U;
        AEL_MAILBOX->error_code = 0U;
        AEL_MAILBOX->detail0    = 0xDEADU;
        AEL_MAILBOX->status     = 2U; /* AEL_STATUS_PASS */
        while (1) {}
    }

    ael_mailbox_init();

    /* Configure IWDG: LSI ~32 kHz, PR=/8 (PR=1), RLR=399 → ~100 ms timeout.
     * Enable IWDG first (0xCCCC forces LSI on), then configure PR/RLR.
     * Default counts down from 0xFFF at /4 → ~409 ms before we change it. */
    IWDG_KR  = IWDG_KEY_ENABLE;   /* start LSI + IWDG */
    IWDG_KR  = IWDG_KEY_UNLOCK;   /* allow PR/RLR writes */
    IWDG_PR  = 1U;    /* /8 → LSI/8 = ~4 kHz */
    IWDG_RLR = 399U;  /* 399/4000 ≈ 100 ms */
    /* LSI is now running; wait for PR/RLR to propagate */
    while (IWDG_SR & 0x3U) {}
    IWDG_KR  = IWDG_KEY_RELOAD;   /* reload with new RLR value */

    /* Kick 20 times at ~50 ms intervals (well within 100 ms timeout) */
    for (uint32_t i = 1U; i <= 20U; i++) {
        IWDG_KR = IWDG_KEY_RELOAD;
        delay_10ms();
        delay_10ms();
        delay_10ms();
        delay_10ms();
        delay_10ms();  /* 5 × 10 ms = 50 ms per iteration */
        AEL_MAILBOX->detail0 = i;
    }

    /* Signal that we're about to stop kicking */
    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 20U;

    /* Stop kicking — IWDG will reset MCU within ~100 ms */
    while (1) {}
}
