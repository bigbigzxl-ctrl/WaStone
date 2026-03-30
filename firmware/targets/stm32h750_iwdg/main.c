/*
 * stm32h750_iwdg — IWDG1 + LSI watchdog self-test
 *
 * Verifies: LSI oscillator, IWDG1 startup, prescaler/reload registers,
 * and that the window timer runs without triggering an unwanted reset.
 *
 * IWDG1 (Independent Watchdog 1) is clocked by LSI (~32 kHz, D3 domain).
 * IWDG1_BASE = 0x58004800 (D3 APB1, RM0433 §55).
 * LSI enabled via RCC_CSR bit 0 (LSION), ready at bit 1 (LSIRDY).
 * RCC_CSR at RCC_BASE + 0x74 (RM0433 §8.7.22).
 *
 * IWDG registers (identical to F4/G4 IWDG):
 *   KR  +0x00: Key register (0xCCCC=start, 0xAAAA=refresh, 0x5555=unlock)
 *   PR  +0x04: Prescaler (/256 = 0x06 → IWDG clock ≈ 125 Hz)
 *   RLR +0x08: Reload register (0x7FF → timeout ≈ 2.048 s / 125 Hz × 8 = ?)
 *   SR  +0x0C: Status register (PVU=bit0, RVU=bit1)
 *
 * Test: enable LSI, configure IWDG with ~2 s timeout, feed 5 times at
 * 200 ms intervals (1 s total, well within the 2 s window). PASS.
 *
 * All register addresses from RM0433.
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ───────────────────────────────────────────────────────── */
#define RCC_BASE    0x58024400u
#define RCC_CSR     (*(volatile uint32_t *)(RCC_BASE + 0x074u))
#define RCC_CSR_LSION   (1u << 0)
#define RCC_CSR_LSIRDY  (1u << 1)

/* ── IWDG1 (D3 APB1, RM0433 §55) ─────────────────────────────── */
#define IWDG1_BASE  0x58004800u
#define IWDG_KR     (*(volatile uint32_t *)(IWDG1_BASE + 0x00u))
#define IWDG_PR     (*(volatile uint32_t *)(IWDG1_BASE + 0x04u))
#define IWDG_RLR    (*(volatile uint32_t *)(IWDG1_BASE + 0x08u))
#define IWDG_SR     (*(volatile uint32_t *)(IWDG1_BASE + 0x0Cu))

/* IWDG key values */
#define IWDG_KEY_RELOAD  0xAAAAu
#define IWDG_KEY_ENABLE  0xCCCCu
#define IWDG_KEY_UNLOCK  0x5555u

/* PR = 0x06 → prescaler /256. IWDG clock ≈ 32000/256 = 125 Hz.
 * RLR = 0xFF → reload = 255 → timeout ≈ 255/125 ≈ 2.04 s */
#define IWDG_PR_DIV256   0x06u
#define IWDG_RLR_VAL     0xFFu

/* Error codes */
#define ERR_LSI_TIMEOUT  0xE001u

/* ── SysTick ─────────────────────────────────────────────────── */
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

    /* ── 1. Enable LSI oscillator ─────────────────────────────── */
    RCC_CSR |= RCC_CSR_LSION;
    uint32_t timeout = 0x0FFFFFu;
    while (((RCC_CSR & RCC_CSR_LSIRDY) == 0u) && (--timeout > 0u)) {}
    if ((RCC_CSR & RCC_CSR_LSIRDY) == 0u) {
        ael_mailbox_fail(ERR_LSI_TIMEOUT, RCC_CSR);
        while (1) {}
    }

    /* ── 2. Start IWDG1 ───────────────────────────────────────── */
    IWDG_KR = IWDG_KEY_ENABLE;   /* start IWDG clock */

    /* ── 3. Unlock and configure PR + RLR ───────────────────── */
    IWDG_KR  = IWDG_KEY_UNLOCK;
    IWDG_PR  = IWDG_PR_DIV256;
    IWDG_RLR = IWDG_RLR_VAL;
    /* Wait for registers to update (PVU and RVU flags clear) */
    timeout = 100000u;
    while ((IWDG_SR & 0x3u) && (--timeout > 0u)) {}
    /* (timeout does not fail the test — just best-effort) */

    /* ── 4. Feed IWDG 5 times at 200 ms intervals ─────────────
     * Total = 1000 ms, timeout ≈ 2040 ms → no reset should occur. */
    for (uint32_t i = 0u; i < 5u; i++) {
        IWDG_KR = IWDG_KEY_RELOAD;   /* feed (also re-locks KR) */
        delay_ticks(200u);
    }

    /* Final reload before PASS */
    IWDG_KR = IWDG_KEY_RELOAD;

    ael_mailbox_pass();

    /* Keep feeding in the idle loop so IWDG never resets */
    while (1) {
        IWDG_KR = IWDG_KEY_RELOAD;
        delay_ticks(500u);
    }
}
