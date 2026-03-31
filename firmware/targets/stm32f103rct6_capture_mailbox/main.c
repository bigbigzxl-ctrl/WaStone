/*
 * STM32F103RCT6 — PB8/PB9 capture mailbox test
 *
 * Observable behaviour:
 *   - PB8 drives a software square wave
 *   - PB9 samples the looped-back waveform
 *   - TIM2 free-runs at 1 MHz to measure the period between rising edges
 *   - PASS when the measured period is inside the expected window
 *   - detail0 = measured period in microseconds
 *
 * Wiring required: PB8 -> PB9
 * Mailbox address: 0x2000BC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1CU))

#define GPIOB_BASE      0x40010C00U
#define GPIOB_CRH       (*(volatile uint32_t *)(GPIOB_BASE + 0x04U))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x08U))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0CU))

#define TIM2_BASE       0x40000000U
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00U))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28U))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2CU))
#define TIM2_CNT        (*(volatile uint32_t *)(TIM2_BASE + 0x24U))
#define TIM2_EGR        (*(volatile uint32_t *)(TIM2_BASE + 0x14U))

#define TIM_CR1_CEN     (1U << 0)

#define ERR_SYNC_TIMEOUT  0xC101U
#define ERR_RISE_TIMEOUT  0xC102U
#define ERR_PERIOD_RANGE  0xC103U

static void delay_us(volatile uint32_t us)
{
    while (us--) {
        for (volatile uint32_t i = 0U; i < 8U; i++) {
            __asm__ volatile ("nop");
        }
    }
}

static uint32_t wait_for_level(uint32_t level, uint32_t timeout_us)
{
    while (timeout_us--) {
        uint32_t state = (GPIOB_IDR >> 9) & 1U;
        if (state == level) {
            return 1U;
        }
        delay_us(1U);
    }
    return 0U;
}

int main(void)
{
    RCC_APB2ENR |= (1U << 3);
    RCC_APB1ENR |= (1U << 0);

    /*
     * PB8: output push-pull 50 MHz -> CRH[3:0] = 0x3
     * PB9: input pull-down         -> CRH[7:4] = 0x8, ODR9 = 0
     */
    GPIOB_CRH &= ~0xFFU;
    GPIOB_CRH |=  0x83U;
    GPIOB_ODR &= ~((1U << 9) | (1U << 8));

    /* TIM2 @ 1 MHz from 8 MHz HSI */
    TIM2_PSC = 7U;
    TIM2_ARR = 0xFFFFFFFFU;
    TIM2_EGR = 1U;
    TIM2_CNT = 0U;
    TIM2_CR1 = TIM_CR1_CEN;

    ael_mailbox_init();

    /* Force a known idle low level on the wire before measuring. */
    GPIOB_ODR &= ~(1U << 8);
    if (!wait_for_level(0U, 5000U)) {
        ael_mailbox_fail(ERR_SYNC_TIMEOUT, 0U);
        while (1) {}
    }

    uint32_t first = 0U;
    uint32_t second = 0U;
    uint32_t seen_first = 0U;
    uint32_t last_state = 0U;

    for (uint32_t cycles = 0U; cycles < 32U; cycles++) {
        GPIOB_ODR |= (1U << 8);
        delay_us(500U);
        uint32_t state = (GPIOB_IDR >> 9) & 1U;
        if (state == 1U && last_state == 0U) {
            if (!seen_first) {
                first = TIM2_CNT;
                seen_first = 1U;
            } else {
                second = TIM2_CNT;
                break;
            }
        }
        last_state = state;

        GPIOB_ODR &= ~(1U << 8);
        delay_us(500U);
        last_state = (GPIOB_IDR >> 9) & 1U;
    }

    if (second == 0U) {
        ael_mailbox_fail(ERR_RISE_TIMEOUT, seen_first);
        while (1) {}
    }

    uint32_t period = second - first;
    AEL_MAILBOX->detail0 = period;

    if (period < 800U || period > 1300U) {
        ael_mailbox_fail(ERR_PERIOD_RANGE, period);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {
        delay_us(1000U);
        AEL_MAILBOX->detail0++;
    }
}
