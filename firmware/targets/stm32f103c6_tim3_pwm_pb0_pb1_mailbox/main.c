#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))

#define GPIOB_BASE      0x40010C00u
#define GPIOB_CRL       (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))

#define TIM2_BASE       0x40000000u
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00u))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28u))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2Cu))
#define TIM2_CNT        (*(volatile uint32_t *)(TIM2_BASE + 0x24u))
#define TIM2_EGR        (*(volatile uint32_t *)(TIM2_BASE + 0x14u))

#define TIM3_BASE       0x40000400u
#define TIM3_CR1        (*(volatile uint32_t *)(TIM3_BASE + 0x00u))
#define TIM3_EGR        (*(volatile uint32_t *)(TIM3_BASE + 0x14u))
#define TIM3_CCMR2      (*(volatile uint32_t *)(TIM3_BASE + 0x1Cu))
#define TIM3_CCER       (*(volatile uint32_t *)(TIM3_BASE + 0x20u))
#define TIM3_PSC        (*(volatile uint32_t *)(TIM3_BASE + 0x28u))
#define TIM3_ARR        (*(volatile uint32_t *)(TIM3_BASE + 0x2Cu))
#define TIM3_CCR3       (*(volatile uint32_t *)(TIM3_BASE + 0x3Cu))

#define TIM_CR1_CEN     (1u << 0)
#define TIM_CR1_ARPE    (1u << 7)

#define ERR_SYNC_TIMEOUT  0x71u
#define ERR_EDGE_TIMEOUT  0x72u
#define ERR_PERIOD_RANGE  0x73u
#define ERR_DUTY_RANGE    0x74u

static uint32_t read_pb1(void)
{
    return (GPIOB_IDR >> 1) & 1u;
}

static void delay_us(uint32_t us)
{
    uint32_t start = TIM2_CNT;
    while ((uint32_t)(TIM2_CNT - start) < us) {}
}

static uint32_t wait_for_level(uint32_t level, uint32_t timeout_us)
{
    while (timeout_us--) {
        if (read_pb1() == level) {
            return 1u;
        }
        delay_us(1u);
    }
    return 0u;
}

int main(void)
{
    RCC_APB2ENR |= (1u << 3);
    RCC_APB1ENR |= (1u << 0) | (1u << 1);

    /*
     * PB0: TIM3_CH3 output AF push-pull 50MHz -> CRL[3:0] = 0xB
     * PB1: input pull-down                    -> CRL[7:4] = 0x8, ODR1 = 0
     */
    GPIOB_CRL &= ~0xFFu;
    GPIOB_CRL |=  0x8Bu;
    GPIOB_ODR &= ~((1u << 1) | (1u << 0));

    TIM2_PSC = 7u;
    TIM2_ARR = 0xFFFFFFFFu;
    TIM2_EGR = 1u;
    TIM2_CNT = 0u;
    TIM2_CR1 = TIM_CR1_CEN;

    /* TIM3 at 1MHz: 1ms period, 25% duty on CH3/PB0 */
    TIM3_PSC = 7u;
    TIM3_ARR = 999u;
    TIM3_CCR3 = 250u;
    TIM3_CCMR2 = (6u << 4u) | (1u << 3u);
    TIM3_CCER = (1u << 8u);
    TIM3_EGR = 1u;
    TIM3_CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    ael_mailbox_init();

    if (!wait_for_level(0u, 5000u)) {
        ael_mailbox_fail(ERR_SYNC_TIMEOUT, 0u);
        while (1) {}
    }

    uint32_t rise1 = 0u;
    uint32_t fall1 = 0u;
    uint32_t rise2 = 0u;
    uint32_t last_state = 0u;
    uint32_t window_start = TIM2_CNT;

    while ((uint32_t)(TIM2_CNT - window_start) < 10000u) {
        uint32_t state = read_pb1();
        if (state == 1u && last_state == 0u) {
            if (rise1 == 0u) {
                rise1 = TIM2_CNT;
            } else if (fall1 != 0u) {
                rise2 = TIM2_CNT;
                break;
            }
        }
        if (state == 0u && last_state == 1u && rise1 != 0u && fall1 == 0u) {
            fall1 = TIM2_CNT;
        }
        last_state = state;
        delay_us(1u);
    }

    if (rise1 == 0u || fall1 == 0u || rise2 == 0u) {
        ael_mailbox_fail(ERR_EDGE_TIMEOUT, 0u);
        while (1) {}
    }

    {
        uint32_t high_us = fall1 - rise1;
        uint32_t period_us = rise2 - rise1;
        AEL_MAILBOX->detail0 = (period_us << 16u) | (high_us & 0xFFFFu);

        if (period_us < 900u || period_us > 1100u) {
            ael_mailbox_fail(ERR_PERIOD_RANGE, period_us);
            while (1) {}
        }
        if (high_us < 150u || high_us > 350u) {
            ael_mailbox_fail(ERR_DUTY_RANGE, high_us);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {
        delay_us(1000u);
        AEL_MAILBOX->detail0++;
    }
}
