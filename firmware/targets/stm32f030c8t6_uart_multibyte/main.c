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

static const uint8_t TX_BYTES[8] = {
    0x55u, 0xAAu, 0x12u, 0x34u, 0xC3u, 0x3Cu, 0x96u, 0x69u
};

int main(void)
{
    RCC_AHBENR |= RCC_GPIOAEN;
    RCC_APB2ENR |= RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 69u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();

    for (uint32_t i = 0u; i < 8u; ++i) {
        uint32_t timeout = 200000u;

        while ((USART1_ISR & USART_ISR_RXNE) != 0u && timeout-- > 0u) {
            (void)USART1_RDR;
        }

        timeout = 200000u;
        while ((USART1_ISR & USART_ISR_TXE) == 0u) {
            if (--timeout == 0u) {
                ael_mailbox_fail(0xE161u, i);
                while (1) {}
            }
        }
        USART1_TDR = TX_BYTES[i];

        timeout = 200000u;
        while ((USART1_ISR & USART_ISR_RXNE) == 0u) {
            if (--timeout == 0u) {
                ael_mailbox_fail(0xE162u, i);
                while (1) {}
            }
        }
        if ((uint8_t)USART1_RDR != TX_BYTES[i]) {
            ael_mailbox_fail(0xE163u, i);
            while (1) {}
        }
        AEL_MAILBOX->detail0 = i + 1u;
    }

    ael_mailbox_pass();
    while (1) {
        AEL_MAILBOX->detail0++;
    }
}
