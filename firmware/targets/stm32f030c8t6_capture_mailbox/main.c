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

#define TIM3_BASE       0x40000400u
#define TIM3_CR1        (*(volatile uint32_t *)(TIM3_BASE + 0x00u))
#define TIM3_EGR        (*(volatile uint32_t *)(TIM3_BASE + 0x14u))
#define TIM3_CNT        (*(volatile uint32_t *)(TIM3_BASE + 0x24u))
#define TIM3_PSC        (*(volatile uint32_t *)(TIM3_BASE + 0x28u))
#define TIM3_ARR        (*(volatile uint32_t *)(TIM3_BASE + 0x2Cu))

#define RCC_GPIOBEN     (1u << 18)
#define RCC_TIM3EN      (1u << 1)
#define TIM_CR1_CEN     (1u << 0)

static void delay_us(uint32_t us)
{
    uint32_t start = TIM3_CNT;
    while ((uint32_t)(TIM3_CNT - start) < us) {}
}

static uint32_t read_pb9(void)
{
    return (GPIOB_IDR >> 9u) & 1u;
}

int main(void)
{
    RCC_AHBENR |= RCC_GPIOBEN;
    RCC_APB1ENR |= RCC_TIM3EN;

    GPIOB_MODER &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_MODER |= (0x1u << 16u);
    GPIOB_PUPDR &= ~((0x3u << 16u) | (0x3u << 18u));
    GPIOB_PUPDR |= (0x2u << 18u);
    GPIOB_ODR &= ~((1u << 8u) | (1u << 9u));

    TIM3_PSC = 7u;
    TIM3_ARR = 0xFFFFu;
    TIM3_EGR = 1u;
    TIM3_CNT = 0u;
    TIM3_CR1 = TIM_CR1_CEN;

    ael_mailbox_init();

    {
        uint32_t rise1 = 0u;
        uint32_t rise2 = 0u;
        uint32_t seen_first = 0u;
        uint32_t last_state = 0u;

        for (uint32_t cycles = 0u; cycles < 32u; ++cycles) {
            GPIOB_ODR |= (1u << 8u);
            delay_us(500u);
            if (read_pb9() == 1u && last_state == 0u) {
                if (seen_first == 0u) {
                    rise1 = TIM3_CNT;
                    seen_first = 1u;
                } else {
                    rise2 = TIM3_CNT;
                    break;
                }
            }
            last_state = read_pb9();

            GPIOB_ODR &= ~(1u << 8u);
            delay_us(500u);
            last_state = read_pb9();
        }

        if (rise2 == 0u) {
            ael_mailbox_fail(0xE181u, seen_first);
            while (1) {}
        }

        {
            uint32_t period = rise2 - rise1;
            AEL_MAILBOX->detail0 = period;
            if (period < 800u || period > 1300u) {
                ael_mailbox_fail(0xE182u, period);
                while (1) {}
            }
        }
    }

    ael_mailbox_pass();
    while (1) {
        delay_us(1000u);
        AEL_MAILBOX->detail0++;
    }
}
