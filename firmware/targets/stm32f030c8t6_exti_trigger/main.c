#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

#define EXTI_BASE       0x40010400u
#define EXTI_IMR        (*(volatile uint32_t *)(EXTI_BASE + 0x00u))
#define EXTI_RTSR       (*(volatile uint32_t *)(EXTI_BASE + 0x08u))
#define EXTI_PR         (*(volatile uint32_t *)(EXTI_BASE + 0x14u))

#define RCC_GPIOAEN     (1u << 17)

static void delay(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    uint32_t count = 0u;

    RCC_AHBENR |= RCC_GPIOAEN;

    GPIOA_MODER &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOA_MODER |= (0x1u << 0u);
    GPIOA_PUPDR &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOA_PUPDR |= (0x2u << 2u);
    GPIOA_ODR &= ~((1u << 0u) | (1u << 1u));

    EXTI_PR = (1u << 1u);
    EXTI_IMR |= (1u << 1u);
    EXTI_RTSR |= (1u << 1u);

    ael_mailbox_init();

    for (uint32_t i = 0u; i < 64u && count < 10u; ++i) {
        GPIOA_ODR &= ~(1u << 0u);
        delay(10000u);
        GPIOA_ODR |= (1u << 0u);
        delay(10000u);

        if ((EXTI_PR & (1u << 1u)) != 0u) {
            EXTI_PR = (1u << 1u);
            count++;
            AEL_MAILBOX->detail0 = count;
        }
    }

    if (count < 10u) {
        ael_mailbox_fail(0xE121u, count);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {
        delay(12000u);
        AEL_MAILBOX->detail0++;
    }
}
