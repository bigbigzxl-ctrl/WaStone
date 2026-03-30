/*
 * stm32u585_iwdg_mailbox — AEL IWDG watchdog test
 * STM32U585CIU6, MSI 4MHz default clock
 *
 * Configures IWDG with ~2s timeout (LSI ≈ 32 kHz, PR=4 ÷64, RLR=999).
 * Reloads 20 times with ~50 ms delay each (~1 s total) then writes PASS.
 * Continues reloading forever to keep the watchdog fed while the mailbox
 * is read by the host.
 *
 * FAIL codes: none (if IWDG fires the MCU resets — mailbox stays RUNNING)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* IWDG: always-on (no RCC enable needed) */
#define IWDG_BASE   0x40003000u
#define IWDG_KR     (*(volatile uint32_t *)(IWDG_BASE + 0x00u))
#define IWDG_PR     (*(volatile uint32_t *)(IWDG_BASE + 0x04u))
#define IWDG_RLR    (*(volatile uint32_t *)(IWDG_BASE + 0x08u))
#define IWDG_SR     (*(volatile uint32_t *)(IWDG_BASE + 0x0Cu))

/* IWDG keys */
#define IWDG_KEY_UNLOCK  0x5555u
#define IWDG_KEY_RELOAD  0xAAAAu
#define IWDG_KEY_START   0xCCCCu

/* SR bits */
#define IWDG_SR_PVU  (1u << 0u)
#define IWDG_SR_RVU  (1u << 1u)

/*
 * Busy-loop delay: ~50 ms at 4 MHz
 * Each iteration ≈ 5 cycles (LDR/CMP/SUB/BNE + overhead) → ~250 000 iterations
 * Use 200 000 to be conservative.
 */
#define DELAY_50MS_ITERS  200000u

static void delay_50ms(void)
{
    for (volatile uint32_t i = 0u; i < DELAY_50MS_ITERS; i++) {}
}

int main(void)
{
    /* 1. Unlock IWDG registers */
    IWDG_KR = IWDG_KEY_UNLOCK;

    /* 2. Set prescaler: PR=4 → ÷64
     *    timeout = (64 × (999+1)) / 32000 ≈ 2.0 s  */
    IWDG_PR  = 4u;

    /* 3. Set reload value */
    IWDG_RLR = 999u;

    /* 4. Wait for PVU and RVU to clear (registers latched into IWDG domain) */
    for (volatile uint32_t t = 0u; t < 1000000u; t++) {
        if ((IWDG_SR & (IWDG_SR_PVU | IWDG_SR_RVU)) == 0u) break;
    }

    /* 5. Reload counter */
    IWDG_KR = IWDG_KEY_RELOAD;

    /* 6. Start IWDG */
    IWDG_KR = IWDG_KEY_START;

    ael_mailbox_init();

    /* 7. Reload 20 times with ~50 ms gap (~1 s total, well within 2 s timeout) */
    for (uint32_t i = 0u; i < 20u; i++) {
        IWDG_KR = IWDG_KEY_RELOAD;
        delay_50ms();
    }

    /* 8. PASS — store reload count in detail0 */
    AEL_MAILBOX->detail0 = 20u;
    ael_mailbox_pass();

    /* 9. Keep reloading forever so IWDG does not fire while host reads mailbox */
    while (1) {
        IWDG_KR = IWDG_KEY_RELOAD;
        delay_50ms();
    }

    return 0;
}
