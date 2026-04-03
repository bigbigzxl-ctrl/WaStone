#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))

#define TIM3_BASE       0x40000400u
#define TIM3_CR1        (*(volatile uint32_t *)(TIM3_BASE + 0x00u))
#define TIM3_SR         (*(volatile uint32_t *)(TIM3_BASE + 0x10u))
#define TIM3_EGR        (*(volatile uint32_t *)(TIM3_BASE + 0x14u))
#define TIM3_PSC        (*(volatile uint32_t *)(TIM3_BASE + 0x28u))
#define TIM3_ARR        (*(volatile uint32_t *)(TIM3_BASE + 0x2Cu))

#define RCC_TIM3EN      (1u << 1)
#define TIM_CR1_CEN     (1u << 0)
#define TIM_SR_UIF      (1u << 0)

int main(void)
{
    uint32_t count = 0u;

    RCC_APB1ENR |= RCC_TIM3EN;
    ael_mailbox_init();

    TIM3_PSC = 7999u;
    TIM3_ARR = 99u;
    TIM3_EGR = 1u;
    TIM3_SR = 0u;
    TIM3_CR1 = TIM_CR1_CEN;

    while (1) {
        if ((TIM3_SR & TIM_SR_UIF) == 0u) {
            continue;
        }
        TIM3_SR &= ~TIM_SR_UIF;
        count++;
        AEL_MAILBOX->detail0 = count;
        if (count >= 10u) {
            ael_mailbox_pass();
            while (1) {
                AEL_MAILBOX->detail0++;
            }
        }
    }
}
