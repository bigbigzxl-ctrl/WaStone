/*
 * STM32F103RCT6 — PB8/PB9 wiring probe
 *
 * Observable behaviour:
 *   - PB8 (output push-pull) toggles high/low 10 times
 *   - PB9 (input pull-down) must track each state
 *   - PASS after 20 correct reads
 *   - detail0 = ((high_ok << 16) | low_ok)
 *
 * Wiring required: PB8 -> PB9
 * Mailbox address: 0x2000BC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

#define GPIOB_BASE      0x40010C00U
#define GPIOB_CRH       (*(volatile uint32_t *)(GPIOB_BASE + 0x04U))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x08U))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0CU))

static void delay(volatile uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

int main(void)
{
    uint32_t high_ok = 0U;
    uint32_t low_ok = 0U;

    RCC_APB2ENR |= (1U << 3);

    /*
     * PB8: output push-pull 50 MHz -> CRH[3:0] = 0x3
     * PB9: input pull-down        -> CRH[7:4] = 0x8, ODR9 = 0
     */
    GPIOB_CRH &= ~0xFFU;
    GPIOB_CRH |=  0x83U;
    GPIOB_ODR &= ~(1U << 9);

    ael_mailbox_init();

    for (uint32_t i = 0U; i < 10U; i++) {
        GPIOB_ODR |= (1U << 8);
        delay(12000U);
        AEL_MAILBOX->detail0 = (high_ok << 16) | low_ok;
        if (GPIOB_IDR & (1U << 9)) {
            high_ok++;
        } else {
            ael_mailbox_fail(0x01U, (high_ok << 16) | low_ok);
            while (1) {}
        }

        GPIOB_ODR &= ~(1U << 8);
        delay(12000U);
        AEL_MAILBOX->detail0 = (high_ok << 16) | low_ok;
        if ((GPIOB_IDR & (1U << 9)) == 0U) {
            low_ok++;
        } else {
            ael_mailbox_fail(0x02U, (high_ok << 16) | low_ok);
            while (1) {}
        }

        AEL_MAILBOX->detail0 = (high_ok << 16) | low_ok;
    }

    ael_mailbox_pass();
    while (1) {
        delay(12000U);
        AEL_MAILBOX->detail0++;
    }
}
