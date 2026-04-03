/*
 * stm32h563rgt6_gpio_loopback
 *
 * PB0 (push-pull output) → wire → PB1 (input with pull-down).
 * 10 HIGH/LOW cycles; PASS if all read back correctly.
 *
 * GPIOB clock: AHB2 bit1.
 *
 * FAIL codes:
 *   0x10|cycle — HIGH mismatch at cycle n
 *   0x20|cycle — LOW  mismatch at cycle n
 *   detail0    = cycle count reached
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))
#define GPIOB_BSRR      (*(volatile uint32_t *)(GPIOB_BASE + 0x18u))

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOB (AHB2 bit1) */
    RCC_AHB2ENR |= (1u << 1);
    (void)RCC_AHB2ENR;

    /* PB0: output (MODER bits[1:0]=01) */
    GPIOB_MODER = (GPIOB_MODER & ~(3u << 0)) | (1u << 0);
    /* PB1: input (MODER bits[3:2]=00), pull-down (PUPDR bits[3:2]=10) */
    GPIOB_MODER &= ~(3u << 2);
    GPIOB_PUPDR  = (GPIOB_PUPDR & ~(3u << 2)) | (2u << 2);

    for (uint32_t i = 0; i < 10u; i++) {
        /* Drive HIGH */
        GPIOB_BSRR = (1u << 0);
        for (volatile uint32_t d = 0; d < 1000u; d++) {}
        if (!(GPIOB_IDR & (1u << 1))) {
            ael_mailbox_fail(0x10u | i, i);
            while (1) {}
        }

        /* Drive LOW */
        GPIOB_BSRR = (1u << 16);
        for (volatile uint32_t d = 0; d < 1000u; d++) {}
        if (GPIOB_IDR & (1u << 1)) {
            ael_mailbox_fail(0x20u | i, i);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = 10u;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
