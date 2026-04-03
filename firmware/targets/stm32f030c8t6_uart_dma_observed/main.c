#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define SYSCFG_BASE     0x40010000u
#define SYSCFG_CFGR1    (*(volatile uint32_t *)(SYSCFG_BASE + 0x00u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_CR3      (*(volatile uint32_t *)(USART1_BASE + 0x14u))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_ICR      (*(volatile uint32_t *)(USART1_BASE + 0x20u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define DMA1_BASE       0x40020000u
#define DMA1_ISR        (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR4       (*(volatile uint32_t *)(DMA1_BASE + 0x44u))
#define DMA1_CNDTR4     (*(volatile uint32_t *)(DMA1_BASE + 0x48u))
#define DMA1_CPAR4      (*(volatile uint32_t *)(DMA1_BASE + 0x4Cu))
#define DMA1_CMAR4      (*(volatile uint32_t *)(DMA1_BASE + 0x50u))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_DMA1EN      (1u << 0)
#define RCC_SYSCFGEN    (1u << 0)
#define RCC_USART1EN    (1u << 14)

#define SYSCFG_CFGR1_USART1TX_DMA_RMP  (1u << 9)

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR3_DMAT  (1u << 7)
#define USART_ICR_TCCF  (1u << 6)
#define USART_ISR_TXE   (1u << 7)
#define USART_ISR_TC    (1u << 6)

#define DMA_CCR_EN      (1u << 0)
#define DMA_CCR_DIR     (1u << 4)
#define DMA_CCR_MINC    (1u << 7)
#define DMA_ISR_TCIF4   (1u << 13)
#define DMA_ISR_TEIF4   (1u << 15)
#define DMA_IFCR_ALL4   (0x0Fu << 12u)

#define DMA_TIMEOUT     2000000u

static uint8_t TX_FRAME[] =
    "AEL_UART_DMA A1 B2 C3 D4 55 66 77 88\r\n";

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
    }
}

static uint32_t run_dma_frame(void)
{
    uint32_t timeout = DMA_TIMEOUT;
    uint32_t dma_len = (uint32_t)(sizeof(TX_FRAME) - 2u);

    DMA1_CCR4 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL4;
    USART1_ICR = USART_ICR_TCCF;
    DMA1_CPAR4 = (uint32_t)&USART1_TDR;
    DMA1_CMAR4 = (uint32_t)(TX_FRAME + 1u);
    DMA1_CNDTR4 = dma_len;
    while ((USART1_ISR & USART_ISR_TXE) == 0u) {
    }
    USART1_TDR = TX_FRAME[0];
    DMA1_CCR4 = DMA_CCR_DIR | DMA_CCR_MINC;
    DMA1_CCR4 |= DMA_CCR_EN;

    while (((DMA1_ISR & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) == 0u) && timeout--) {
    }
    if ((DMA1_ISR & DMA_ISR_TEIF4) != 0u) {
        return 0xD401u;
    }
    if ((DMA1_ISR & DMA_ISR_TCIF4) == 0u) {
        return 0xD402u;
    }

    while ((USART1_ISR & USART_ISR_TC) == 0u) {
    }
    DMA1_CCR4 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL4;
    return 0u;
}

int main(void)
{
    uint32_t frames = 0u;

    RCC_AHBENR |= RCC_GPIOAEN | RCC_DMA1EN;
    RCC_APB2ENR |= RCC_SYSCFGEN | RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);
    SYSCFG_CFGR1 |= SYSCFG_CFGR1_USART1TX_DMA_RMP;

    USART1_BRR = 69u;
    USART1_CR3 = USART_CR3_DMAT;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();

    while (1) {
        uint32_t err = run_dma_frame();
        if (err != 0u) {
            ael_mailbox_fail(err, frames);
            while (1) {
            }
        }

        frames++;
        AEL_MAILBOX->detail0 = frames;
        if (frames == 1u) {
            ael_mailbox_pass();
        }

        delay(240000u);
    }
}
