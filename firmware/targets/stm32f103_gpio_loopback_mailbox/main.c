#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

#define RCC_BASE 0x40021000u
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE 0x40010800u
#define GPIOA_CRH (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_ODR (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))

#define GPIOB_BASE 0x40010C00u
#define GPIOB_CRH (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_IDR (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))

#define RCC_IOPAEN (1u << 2)
#define RCC_IOPBEN (1u << 3)

static void delay_cycles(volatile uint32_t n) {
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    RCC_APB2ENR |= RCC_IOPAEN | RCC_IOPBEN;

    GPIOA_CRH &= ~(0xFu << 0u);
    GPIOA_CRH |=  (0x3u << 0u);

    GPIOB_CRH &= ~(0xFu << 0u);
    GPIOB_CRH |=  (0x4u << 0u);

    ael_mailbox_init();

    for (uint32_t i = 0u; i < 16u; ++i) {
        const uint32_t expected = (i & 1u) ? (1u << 8u) : 0u;
        if (expected != 0u) {
            GPIOA_ODR |= (1u << 8u);
        } else {
            GPIOA_ODR &= ~(1u << 8u);
        }
        delay_cycles(12000u);

        if (((GPIOB_IDR & (1u << 8u)) != 0u) != (expected != 0u)) {
            ael_mailbox_fail((expected != 0u) ? 0x6001u : 0x6002u, i);
            while (1) {}
        }
        AEL_MAILBOX->detail0 = i + 1u;
    }

    ael_mailbox_pass();
    while (1) {
        AEL_MAILBOX->detail0 += 1u;
        delay_cycles(20000u);
    }
}
