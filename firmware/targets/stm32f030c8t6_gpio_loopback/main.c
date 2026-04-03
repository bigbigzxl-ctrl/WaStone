#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define GPIOB_BASE      0x48000400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14u))

#define RCC_GPIOBEN     (1u << 18)

static void delay(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    uint32_t pass_count = 0u;

    RCC_AHBENR |= RCC_GPIOBEN;

    GPIOB_MODER &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOB_MODER |= (0x1u << 0u);
    GPIOB_PUPDR &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOB_PUPDR |= (0x2u << 2u);
    GPIOB_ODR &= ~((1u << 0u) | (1u << 1u));

    ael_mailbox_init();

    while (1) {
        GPIOB_ODR |= (1u << 0u);
        delay(4000u);
        if ((GPIOB_IDR & (1u << 1u)) == 0u) {
            ael_mailbox_fail(0xE111u, pass_count);
            while (1) {}
        }

        GPIOB_ODR &= ~(1u << 0u);
        delay(4000u);
        if ((GPIOB_IDR & (1u << 1u)) != 0u) {
            ael_mailbox_fail(0xE112u, pass_count);
            while (1) {}
        }

        pass_count++;
        AEL_MAILBOX->detail0 = pass_count;
        if (pass_count >= 10u) {
            ael_mailbox_pass();
            while (1) {
                delay(4000u);
                AEL_MAILBOX->detail0++;
            }
        }
    }
}
