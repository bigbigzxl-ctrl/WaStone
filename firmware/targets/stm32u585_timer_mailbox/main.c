/*
 * stm32u585_timer_mailbox — AEL TIM2 update interrupt test (polling mode)
 * STM32U585CIU6, MSI 4MHz default clock
 *
 * Configures TIM2 for 100ms period (PSC=3999, ARR=99) and polls the
 * update interrupt flag. PASS after 10 overflows (~1 second).
 *
 * FAIL codes: none (pure polling, no failure path)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_APB1ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))

/* TIM2: APB1, RCC_APB1ENR1 bit 0 */
#define TIM2_BASE       0x40000000u
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00u))
#define TIM2_SR         (*(volatile uint32_t *)(TIM2_BASE + 0x10u))
#define TIM2_EGR        (*(volatile uint32_t *)(TIM2_BASE + 0x14u))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28u))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2Cu))

int main(void)
{
    /* 1. Enable TIM2 clock */
    RCC_APB1ENR1 |= (1u << 0u);
    /* Small clock-settle read-back */
    volatile uint32_t dummy = RCC_APB1ENR1;
    (void)dummy;

    /* 2. Configure TIM2
     *    MSI = 4 MHz
     *    PSC = 3999  →  tick = 4 000 000 / 4000 = 1 kHz (1 ms)
     *    ARR = 99    →  period = 100 ms
     */
    TIM2_PSC = 3999u;
    TIM2_ARR = 99u;
    TIM2_EGR = 1u;   /* UG: force load PSC/ARR shadow registers */
    TIM2_SR  = 0u;   /* clear any pending UIF from UG */

    /* 3. Start counter */
    TIM2_CR1 = 1u;   /* CEN = 1 */

    ael_mailbox_init();

    /* 4. Poll UIF (bit 0 of SR) until 10 overflows */
    volatile uint32_t count = 0u;
    while (count < 10u) {
        if (TIM2_SR & 1u) {
            TIM2_SR = 0u;   /* clear UIF */
            count++;
        }
    }

    /* 5. PASS — store overflow count in detail0 */
    AEL_MAILBOX->detail0 = count;
    ael_mailbox_pass();

    while (1) {}
    return 0;
}
