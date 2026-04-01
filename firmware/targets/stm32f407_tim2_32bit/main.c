/*
 * STM32F407 — TIM2 32-bit Counter Verification Test
 *
 * TIM2 and TIM5 are the only 32-bit timers on STM32F407.
 * This test verifies the full 32-bit counter range by:
 *   1. Setting ARR = 0xFFFFFFFF (max 32-bit value)
 *   2. Configuring PSC for ~1 MHz tick (PSC=15 at 16MHz)
 *   3. Running free-running and comparing two snapshots
 *   4. Verifying counter increments are in expected range
 *   5. Verifying counter exceeds 16-bit limit (0xFFFF)
 *
 * TIM2 clock: APB1 = 16MHz (no PLL). Since APB1 prescaler=1,
 * TIM2_CLK = PCLK1 = 16MHz.
 * PSC=15: tick = 16MHz/16 = 1MHz → 1 µs per tick.
 *
 * No wiring required. No PLL.
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = counter value at snapshot2 (>0xFFFF proves 32-bit)
 *   FAIL: error_code=1 counter didn't increment
 *         error_code=2 counter didn't exceed 16-bit range in time
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---- */
#define RCC_BASE     0x40023800u
#define RCC_APB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x40u))

/* ---- TIM2 ---- */
#define TIM2_BASE    0x40000000u
#define TIM2_CR1     (*(volatile uint32_t *)(TIM2_BASE + 0x00u))
#define TIM2_EGR     (*(volatile uint32_t *)(TIM2_BASE + 0x14u))
#define TIM2_CNT     (*(volatile uint32_t *)(TIM2_BASE + 0x24u))
#define TIM2_PSC     (*(volatile uint32_t *)(TIM2_BASE + 0x28u))
#define TIM2_ARR     (*(volatile uint32_t *)(TIM2_BASE + 0x2Cu))

#define TIM_CR1_CEN  (1u << 0)
#define TIM_EGR_UG   (1u << 0)

static void delay_loop(uint32_t n)
{
    volatile uint32_t i = n;
    while (i--) {}
}

int main(void)
{
    ael_mailbox_init();

    /* Enable TIM2 clock (APB1 bit 0) */
    RCC_APB1ENR |= (1u << 0);
    (void)RCC_APB1ENR;

    /* Configure TIM2: PSC=15 → 16MHz/16=1MHz tick, ARR=0xFFFFFFFF (full 32-bit) */
    TIM2_CR1 = 0u;
    TIM2_PSC = 15u;
    TIM2_ARR = 0xFFFFFFFFu;
    TIM2_CNT = 0u;
    TIM2_EGR = TIM_EGR_UG;  /* apply PSC (double-buffered) */
    TIM2_CR1 = TIM_CR1_CEN; /* start */

    /* --- Test 1: verify counter increments --- */
    uint32_t t0 = TIM2_CNT;
    delay_loop(1000u);
    uint32_t t1 = TIM2_CNT;
    if (t1 <= t0) {
        ael_mailbox_fail(1u, t1);
        while (1) {}
    }

    /* --- Test 2: wait for counter to exceed 0xFFFF (proves 32-bit range) ---
     * At 1MHz, crossing 65535 takes ~65 ms.
     * We wait with a loop that won't block too long but gives enough time.
     * Maximum wait: ~100M cycles at 16MHz = ~6 seconds — too long.
     * Instead: reset counter, wait from 0, poll until >0xFFFF.
     * Timeout: 200000 loop iterations ≈ some milliseconds.
     */
    TIM2_CR1 = 0u;
    TIM2_CNT = 0u;
    TIM2_EGR = TIM_EGR_UG;
    TIM2_CR1 = TIM_CR1_CEN;

    /* poll until CNT > 0xFFFF or timeout */
    uint32_t timeout = 10000000u; /* ~several seconds of polling */
    uint32_t snap = 0u;
    while (1) {
        snap = TIM2_CNT;
        if (snap > 0xFFFFu) break;
        if (--timeout == 0u) {
            ael_mailbox_fail(2u, snap);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = snap; /* > 0xFFFF proves 32-bit counter */
    while (1) {}
}
