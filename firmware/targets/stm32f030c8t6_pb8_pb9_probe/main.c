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
    while (n--) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    uint32_t high_ok = 0u;
    uint32_t low_ok = 0u;

    RCC_AHBENR |= RCC_GPIOBEN;

    GPIOB_MODER &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_MODER |= (0x1u << 16u);
    GPIOB_PUPDR &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_PUPDR |= (0x2u << 18u);
    GPIOB_ODR &= ~(1u << 9u);

    ael_mailbox_init();

    for (uint32_t i = 0u; i < 10u; i++) {
        GPIOB_ODR |= (1u << 8u);
        delay(12000u);
        AEL_MAILBOX->detail0 = (high_ok << 16u) | low_ok;
        if ((GPIOB_IDR & (1u << 9u)) == 0u) {
            ael_mailbox_fail(0xE201u, (high_ok << 16u) | low_ok);
            while (1) {}
        }
        high_ok++;

        GPIOB_ODR &= ~(1u << 8u);
        delay(12000u);
        AEL_MAILBOX->detail0 = (high_ok << 16u) | low_ok;
        if ((GPIOB_IDR & (1u << 9u)) != 0u) {
            ael_mailbox_fail(0xE202u, (high_ok << 16u) | low_ok);
            while (1) {}
        }
        low_ok++;
    }

    AEL_MAILBOX->detail0 = (high_ok << 16u) | low_ok;
    ael_mailbox_pass();
    while (1) {
        delay(12000u);
        AEL_MAILBOX->detail0++;
    }
}
