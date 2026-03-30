/*
 * stm32u585_gpio_loopback_mailbox — GPIO loopback test
 * STM32U585CIU6, MSI 4MHz
 *
 * Wire: PA8 (output) → PB4 (input)
 *
 * Sets PA8 HIGH, verifies PB4 reads HIGH.
 * Sets PA8 LOW,  verifies PB4 reads LOW.
 *
 * FAIL codes:
 *   0xE001 — PB4 not HIGH when PA8 driven HIGH (IDR in detail0)
 *   0xE002 — PB4 not LOW  when PA8 driven LOW  (IDR in detail0)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

/* GPIOB */
#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOA and GPIOB clocks (AHB2ENR1 bits 0 and 1) */
    RCC_AHB2ENR1 |= (1u << 0u) | (1u << 1u);
    volatile uint32_t dummy = RCC_AHB2ENR1;
    (void)dummy;

    /* PA8: output push-pull, no pull (MODER[17:16]=01) */
    GPIOA_MODER   &= ~(3u << 16u);
    GPIOA_MODER   |=  (1u << 16u);
    GPIOA_OTYPER  &= ~(1u << 8u);   /* push-pull */
    GPIOA_OSPEEDR |=  (3u << 16u);  /* high speed */

    /* PB4: input, no pull (MODER[9:8]=00, PUPDR[9:8]=00) */
    GPIOB_MODER &= ~(3u << 8u);
    GPIOB_PUPDR &= ~(3u << 8u);

    /* Settle delay */
    for (volatile uint32_t t = 0u; t < 1000u; t++) {}

    /* Test 1: drive PA8 HIGH, read PB4 */
    GPIOA_ODR |= (1u << 8u);
    for (volatile uint32_t t = 0u; t < 1000u; t++) {}
    {
        uint32_t idr = GPIOB_IDR;
        if (!(idr & (1u << 4u))) {
            AEL_MAILBOX->detail0 = idr;
            ael_mailbox_fail(0xE001u, idr);
            while (1) {}
        }
    }

    /* Test 2: drive PA8 LOW, read PB4 */
    GPIOA_ODR &= ~(1u << 8u);
    for (volatile uint32_t t = 0u; t < 1000u; t++) {}
    {
        uint32_t idr = GPIOB_IDR;
        if (idr & (1u << 4u)) {
            AEL_MAILBOX->detail0 = idr;
            ael_mailbox_fail(0xE002u, idr);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
