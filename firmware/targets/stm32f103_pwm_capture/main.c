#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))

#define GPIOA_BASE      0x40010800u
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define GPIOB_BASE      0x40010C00u
#define GPIOB_CRH       (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))

#define TIM1_BASE       0x40012C00u
#define TIM1_CR1        (*(volatile uint32_t *)(TIM1_BASE + 0x00u))
#define TIM1_EGR        (*(volatile uint32_t *)(TIM1_BASE + 0x14u))
#define TIM1_CCMR1      (*(volatile uint32_t *)(TIM1_BASE + 0x18u))
#define TIM1_CCER       (*(volatile uint32_t *)(TIM1_BASE + 0x20u))
#define TIM1_PSC        (*(volatile uint32_t *)(TIM1_BASE + 0x28u))
#define TIM1_ARR        (*(volatile uint32_t *)(TIM1_BASE + 0x2Cu))
#define TIM1_CCR1       (*(volatile uint32_t *)(TIM1_BASE + 0x34u))
#define TIM1_BDTR       (*(volatile uint32_t *)(TIM1_BASE + 0x44u))

#define TIM4_BASE       0x40000800u
#define TIM4_CR1        (*(volatile uint32_t *)(TIM4_BASE + 0x00u))
#define TIM4_SR         (*(volatile uint32_t *)(TIM4_BASE + 0x10u))
#define TIM4_EGR        (*(volatile uint32_t *)(TIM4_BASE + 0x14u))
#define TIM4_CCMR2      (*(volatile uint32_t *)(TIM4_BASE + 0x1Cu))
#define TIM4_CCER       (*(volatile uint32_t *)(TIM4_BASE + 0x20u))
#define TIM4_CNT        (*(volatile uint32_t *)(TIM4_BASE + 0x24u))
#define TIM4_PSC        (*(volatile uint32_t *)(TIM4_BASE + 0x28u))
#define TIM4_ARR        (*(volatile uint32_t *)(TIM4_BASE + 0x2Cu))
#define TIM4_CCR3       (*(volatile uint32_t *)(TIM4_BASE + 0x3Cu))

#define RCC_AFIOEN      (1u << 0)
#define RCC_IOPAEN      (1u << 2)
#define RCC_IOPBEN      (1u << 3)
#define RCC_TIM1EN      (1u << 11)
#define RCC_TIM4EN      (1u << 2)

#define TIM_CR1_CEN     (1u << 0)
#define TIM_CR1_ARPE    (1u << 7)
#define TIM_CCER_CC1E   (1u << 0)
#define TIM_CCER_CC3E   (1u << 8)
#define TIM_BDTR_MOE    (1u << 15)
#define TIM_SR_CC3IF    (1u << 3)

#define ERR_CAP_TIMEOUT 0x01u
#define ERR_PERIOD      0x02u
#define CAP_MIN_TICKS   150u
#define CAP_MAX_TICKS   250u

static void delay_cycles(volatile uint32_t n) {
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

int main(void) {
    RCC_APB2ENR |= (RCC_AFIOEN | RCC_IOPAEN | RCC_IOPBEN | RCC_TIM1EN);
    RCC_APB1ENR |= RCC_TIM4EN;

    GPIOA_CRH &= ~(0xFu << 0u);
    GPIOA_CRH |=  (0xBu << 0u);

    GPIOB_CRH &= ~(0xFu << 0u);
    GPIOB_CRH |=  (0x4u << 0u);

    TIM1_PSC = 7999u;
    TIM1_ARR = 19u;
    TIM1_CCR1 = 10u;
    TIM1_CCMR1 = (6u << 4u) | (1u << 3u);
    TIM1_CCER = TIM_CCER_CC1E;
    TIM1_EGR = 1u;
    TIM1_BDTR = TIM_BDTR_MOE;
    TIM1_CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    TIM4_PSC = 799u;
    TIM4_ARR = 0xFFFFu;
    TIM4_CNT = 0u;
    TIM4_CCMR2 = 0x1u;
    TIM4_CCER = TIM_CCER_CC3E;
    TIM4_EGR = 1u;
    TIM4_SR = 0u;
    TIM4_CR1 = TIM_CR1_CEN;

    ael_mailbox_init();

    uint32_t timeout = 2000000u;
    while ((TIM4_SR & TIM_SR_CC3IF) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(ERR_CAP_TIMEOUT, 0u);
            while (1) {}
        }
    }
    uint32_t t1 = TIM4_CCR3;

    timeout = 2000000u;
    while ((TIM4_SR & TIM_SR_CC3IF) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(ERR_CAP_TIMEOUT, 0u);
            while (1) {}
        }
    }
    uint32_t t2 = TIM4_CCR3;
    uint32_t period = (t2 - t1) & 0xFFFFu;

    if (period < CAP_MIN_TICKS || period > CAP_MAX_TICKS) {
        ael_mailbox_fail(ERR_PERIOD, period);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = period;
    while (1) {
        delay_cycles(20000u);
        AEL_MAILBOX->detail0 += 1u;
    }
}
