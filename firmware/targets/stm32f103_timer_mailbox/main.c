#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1CU))

#define TIM2_BASE       0x40000000U
#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00U))
#define TIM2_DIER       (*(volatile uint32_t *)(TIM2_BASE + 0x0CU))
#define TIM2_SR         (*(volatile uint32_t *)(TIM2_BASE + 0x10U))
#define TIM2_EGR        (*(volatile uint32_t *)(TIM2_BASE + 0x14U))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28U))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2CU))

#define NVIC_ISER0      (*(volatile uint32_t *)0xE000E100U)

static volatile uint32_t tim2_irq_count = 0U;
static volatile uint32_t test_passed = 0U;

void TIM2_IRQHandler(void)
{
    if (TIM2_SR & 1U) {
        TIM2_SR &= ~1U;
        tim2_irq_count++;
        AEL_MAILBOX->detail0 = tim2_irq_count;
        if (tim2_irq_count >= 10U && !test_passed) {
            ael_mailbox_pass();
            test_passed = 1U;
        }
    }
}

int main(void)
{
    RCC_APB1ENR |= (1U << 0);

    ael_mailbox_init();

    TIM2_PSC  = 7999U;
    TIM2_ARR  = 99U;
    TIM2_EGR  = 1U;
    TIM2_SR   = 0U;
    TIM2_DIER = 1U;
    TIM2_CR1  = 1U;

    NVIC_ISER0 = (1U << 28);

    while (1) {}
}
