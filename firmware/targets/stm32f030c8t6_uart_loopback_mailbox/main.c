#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_RDR      (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_USART1EN    (1u << 14)
#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_ISR_RXNE  (1u << 5)
#define USART_ISR_TXE   (1u << 7)

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static uint8_t usart1_transfer(uint8_t value, uint8_t *out)
{
    uint32_t timeout = 200000u;

    while ((USART1_ISR & USART_ISR_RXNE) != 0u) {
        (void)USART1_RDR;
    }

    while (((USART1_ISR & USART_ISR_TXE) == 0u) && timeout-- > 0u) {}
    if ((USART1_ISR & USART_ISR_TXE) == 0u) {
        return 0u;
    }

    USART1_TDR = value;

    timeout = 200000u;
    while (((USART1_ISR & USART_ISR_RXNE) == 0u) && timeout-- > 0u) {}
    if ((USART1_ISR & USART_ISR_RXNE) == 0u) {
        return 0u;
    }

    *out = (uint8_t)USART1_RDR;
    return 1u;
}

int main(void)
{
    static const uint8_t patterns[] = {0x55u, 0xA5u, 0x12u, 0x34u, 0xC3u, 0x3Cu};

    RCC_AHBENR |= RCC_GPIOAEN;
    RCC_APB2ENR |= RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 69u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();

    for (uint32_t i = 0u; i < (sizeof(patterns) / sizeof(patterns[0])); ++i) {
        uint8_t rx = 0u;
        uint8_t tx = patterns[i];

        if (usart1_transfer(tx, &rx) == 0u) {
            ael_mailbox_fail(0xE151u, i);
            while (1) {}
        }
        if (rx != tx) {
            ael_mailbox_fail(0xE152u, ((uint32_t)tx << 8u) | rx);
            while (1) {}
        }
        AEL_MAILBOX->detail0 = ((uint32_t)i << 16u) | rx;
        delay_cycles(30000u);
    }

    ael_mailbox_pass();
    while (1) {
        delay_cycles(40000u);
        AEL_MAILBOX->detail0++;
    }
}
