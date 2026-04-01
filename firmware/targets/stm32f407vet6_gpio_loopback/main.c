/*
 * STM32F407VET6 — GPIO loopback: PC2 (output) → PC3 (input)
 *
 * PC2 drives HIGH then LOW, PC3 reads back via wire.
 * 10 successful HIGH+LOW cycles required for PASS.
 * detail0 = cycle count, increments after PASS for liveness.
 *
 * Wiring: PC2 → PC3
 * Mailbox: 0x2001FC00 (SRAM1 top 1 KB)
 * Clock: 16 MHz HSI
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}


#define RCC_BASE    0x40023800U
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))

#define GPIOC_BASE  0x40020800U
#define GPIOC_MODER (*(volatile uint32_t *)(GPIOC_BASE + 0x00U))
#define GPIOC_PUPDR (*(volatile uint32_t *)(GPIOC_BASE + 0x0CU))
#define GPIOC_IDR   (*(volatile uint32_t *)(GPIOC_BASE + 0x10U))
#define GPIOC_BSRR  (*(volatile uint32_t *)(GPIOC_BASE + 0x18U))

/* ~1 ms delay at 16 MHz HSI (4 cycles/iter × 4000 = 16000 cycles) */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0U; i < ms; i++) {
        for (volatile uint32_t d = 0U; d < 4000U; d++) {
            __asm__ volatile ("nop");
        }
    }
}

int main(void)
{
    RCC_AHB1ENR |= (1U << 2U);   /* GPIOCEN */
    (void)RCC_AHB1ENR;

    /* PC2: output push-pull (MODER[5:4] = 01) */
    GPIOC_MODER &= ~(3U << 4U);
    GPIOC_MODER |=  (1U << 4U);

    /* PC3: input pull-down (MODER[7:6] = 00, PUPDR[7:6] = 10) */
    GPIOC_MODER &= ~(3U << 6U);
    GPIOC_PUPDR &= ~(3U << 6U);
    GPIOC_PUPDR |=  (2U << 6U);

    ael_mailbox_init();

    uint32_t pass_count = 0U;

    for (uint32_t cycle = 0U; cycle < 10U; cycle++) {
        /* Drive HIGH, settle, read */
        GPIOC_BSRR = (1U << 2U);
        delay_ms(2U);
        if ((GPIOC_IDR & (1U << 3U)) == 0U) {
            ael_mailbox_fail(0x10U | cycle, 0U);
            while (1) {}
        }

        /* Drive LOW, settle, read */
        GPIOC_BSRR = (1U << (2U + 16U));
        delay_ms(2U);
        if ((GPIOC_IDR & (1U << 3U)) != 0U) {
            ael_mailbox_fail(0x20U | cycle, 0U);
            while (1) {}
        }

        pass_count++;
        AEL_MAILBOX->detail0 = pass_count;
    }

    ael_mailbox_pass();

    while (1) {
        delay_ms(1U);
        AEL_MAILBOX->detail0 = ++pass_count;
    }
}
