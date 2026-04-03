#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE           0x40021000u
#define RCC_AHBENR         (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_GPIOAEN        (1u << 17)
#define RCC_DMA1EN         (1u << 0)
#define RCC_USART1EN       (1u << 14)

#define GPIOA_BASE         0x48000000u
#define GPIOA_MODER        (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH         (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))
#define GPIOA_PUPDR        (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))

#define USART1_BASE        0x40013800u
#define USART1_CR1         (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_CR3         (*(volatile uint32_t *)(USART1_BASE + 0x14u))
#define USART1_BRR         (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR         (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_RDR         (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR         (*(volatile uint32_t *)(USART1_BASE + 0x28u))
#define USART_CR1_UE       (1u << 0)
#define USART_CR1_RE       (1u << 2)
#define USART_CR1_TE       (1u << 3)
#define USART_CR3_DMAT     (1u << 7)
#define USART_ISR_RXNE     (1u << 5)
#define USART_ISR_TC       (1u << 6)
#define USART_ISR_TEACK    (1u << 21)
#define USART_ISR_REACK    (1u << 22)

#define DMA1_BASE          0x40020000u
#define DMA1_ISR           (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR          (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR2          (*(volatile uint32_t *)(DMA1_BASE + 0x1Cu))
#define DMA1_CNDTR2        (*(volatile uint32_t *)(DMA1_BASE + 0x20u))
#define DMA1_CPAR2         (*(volatile uint32_t *)(DMA1_BASE + 0x24u))
#define DMA1_CMAR2         (*(volatile uint32_t *)(DMA1_BASE + 0x28u))
#define DMA_CCR_EN         (1u << 0)
#define DMA_CCR_DIR        (1u << 4)
#define DMA_CCR_MINC       (1u << 7)
#define DMA_ISR_TCIF2      (1u << 5)
#define DMA_ISR_TEIF2      (1u << 7)
#define DMA_IFCR_ALL2      (0x0Fu << 4u)

#define UART_LEN           8u
#define UART_TIMEOUT       2000000u

static uint8_t tx_bytes[UART_LEN] = {
    0xA1u, 0xB2u, 0xC3u, 0xD4u, 0x55u, 0x66u, 0x77u, 0x88u
};
static uint8_t rx_bytes[UART_LEN];

static void fail_detail(uint32_t code, uint32_t detail)
{
    ael_mailbox_fail(code, detail);
    while (1) {
    }
}

int main(void)
{
    RCC_AHBENR |= RCC_GPIOAEN | RCC_DMA1EN;
    RCC_APB2ENR |= RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);
    GPIOA_PUPDR &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_PUPDR |= (0x1u << 20u);

    USART1_CR1 = 0u;
    USART1_CR3 = 0u;
    USART1_BRR = 69u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    {
        uint32_t timeout = UART_TIMEOUT;
        while (((USART1_ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
                (USART_ISR_TEACK | USART_ISR_REACK)) &&
               timeout-- > 0u) {
        }
        if ((USART1_ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
            (USART_ISR_TEACK | USART_ISR_REACK)) {
            ael_mailbox_init();
            fail_detail(0xE170u, USART1_ISR);
        }
    }

    while ((USART1_ISR & USART_ISR_RXNE) != 0u) {
        (void)USART1_RDR;
    }

    DMA1_CCR2 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL2;
    DMA1_CPAR2 = (uint32_t)&USART1_TDR;
    DMA1_CMAR2 = (uint32_t)tx_bytes;
    DMA1_CNDTR2 = UART_LEN;
    DMA1_CCR2 = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_EN;
    USART1_CR3 = USART_CR3_DMAT;

    ael_mailbox_init();

    for (uint32_t i = 0u; i < UART_LEN; ++i) {
        uint32_t timeout = UART_TIMEOUT;
        while (((USART1_ISR & USART_ISR_RXNE) == 0u) && timeout-- > 0u) {
        }
        if ((USART1_ISR & USART_ISR_RXNE) == 0u) {
            fail_detail(0xE172u,
                        ((DMA1_CNDTR2 & 0xFFu) << 24u) | (DMA1_ISR & 0xFFFFu));
        }
        rx_bytes[i] = (uint8_t)USART1_RDR;
        AEL_MAILBOX->detail0 = ((i & 0xFFu) << 24u) |
                               ((DMA1_CNDTR2 & 0xFFu) << 16u) |
                               rx_bytes[i];
    }

    {
        uint32_t timeout = UART_TIMEOUT;
        while (((DMA1_ISR & DMA_ISR_TCIF2) == 0u) &&
               ((DMA1_ISR & DMA_ISR_TEIF2) == 0u) &&
               timeout-- > 0u) {
        }
        if ((DMA1_ISR & DMA_ISR_TEIF2) != 0u) {
            fail_detail(0xE171u, DMA1_ISR);
        }
        if ((DMA1_ISR & DMA_ISR_TCIF2) == 0u) {
            fail_detail(0xE174u,
                        ((DMA1_CNDTR2 & 0xFFu) << 24u) | (DMA1_ISR & 0xFFFFu));
        }
    }

    {
        uint32_t timeout = UART_TIMEOUT;
        while (((USART1_ISR & USART_ISR_TC) == 0u) && timeout-- > 0u) {
        }
        if ((USART1_ISR & USART_ISR_TC) == 0u) {
            fail_detail(0xE175u, USART1_ISR);
        }
    }

    for (uint32_t i = 0u; i < UART_LEN; ++i) {
        if (rx_bytes[i] != tx_bytes[i]) {
            fail_detail(0xE173u,
                        (i << 24u) | ((uint32_t)rx_bytes[i] << 16u) |
                        ((uint32_t)tx_bytes[i] << 8u));
        }
    }

    ael_mailbox_pass();
    while (1) {
        AEL_MAILBOX->detail0++;
    }
}
