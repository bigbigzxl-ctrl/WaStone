#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))

#define GPIOB_BASE      0x48000400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14u))
#define GPIOB_AFRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))

#define TIM3_BASE       0x40000400u
#define TIM3_CR1        (*(volatile uint32_t *)(TIM3_BASE + 0x00u))
#define TIM3_EGR        (*(volatile uint32_t *)(TIM3_BASE + 0x14u))
#define TIM3_CCMR2      (*(volatile uint32_t *)(TIM3_BASE + 0x1Cu))
#define TIM3_CCER       (*(volatile uint32_t *)(TIM3_BASE + 0x20u))
#define TIM3_CNT        (*(volatile uint32_t *)(TIM3_BASE + 0x24u))
#define TIM3_PSC        (*(volatile uint32_t *)(TIM3_BASE + 0x28u))
#define TIM3_ARR        (*(volatile uint32_t *)(TIM3_BASE + 0x2Cu))
#define TIM3_CCR3       (*(volatile uint32_t *)(TIM3_BASE + 0x3Cu))

#define RCC_GPIOBEN     (1u << 18)
#define RCC_TIM3EN      (1u << 1)
#define TIM_CR1_CEN     (1u << 0)
#define TIM_CR1_ARPE    (1u << 7)

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static uint32_t read_pb1(void)
{
    return (GPIOB_IDR >> 1u) & 1u;
}

int main(void)
{
    RCC_AHBENR |= RCC_GPIOBEN;
    RCC_APB1ENR |= RCC_TIM3EN;

    GPIOB_MODER &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOB_MODER |= (0x2u << 0u);
    GPIOB_PUPDR &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOB_PUPDR |= (0x2u << 2u);
    GPIOB_AFRL &= ~(0xFu << 0u);
    GPIOB_AFRL |= (0x1u << 0u);
    GPIOB_ODR &= ~((1u << 0u) | (1u << 1u));

    TIM3_PSC = 7u;
    TIM3_ARR = 999u;
    TIM3_CCR3 = 250u;
    TIM3_CCMR2 = (6u << 4u) | (1u << 3u);
    TIM3_CCER = (1u << 8u);
    TIM3_EGR = 1u;
    TIM3_CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    ael_mailbox_init();

    {
        uint32_t saw_high = 0u;
        uint32_t saw_low = 0u;

        for (uint32_t i = 0u; i < 4000u; ++i) {
            uint32_t cnt = TIM3_CNT;
            uint32_t level = read_pb1();

            if (cnt < 200u && level != 0u) {
                saw_high = 1u;
            }
            if (cnt > 400u && level == 0u) {
                saw_low = 1u;
            }
            if (saw_high != 0u && saw_low != 0u) {
                break;
            }
        }

        if (saw_high == 0u) {
            ael_mailbox_fail(0xE1A1u, TIM3_CNT);
            while (1) {}
        }
        if (saw_low == 0u) {
            ael_mailbox_fail(0xE1A2u, TIM3_CNT);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {
        delay_cycles(20000u);
        AEL_MAILBOX->detail0++;
    }
}
