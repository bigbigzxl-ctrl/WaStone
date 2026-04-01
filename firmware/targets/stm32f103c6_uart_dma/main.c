#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../ael_mailbox.h"

#define RCC_BASE           0x40021000u
#define RCC_AHBENR         (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_IOPAEN         (1u << 2)
#define RCC_DMA1EN         (1u << 0)
#define RCC_USART1EN       (1u << 14)

#define GPIOA_BASE         0x40010800u
#define GPIOA_CRH          (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define USART1_BASE        0x40013800u
#define USART1_SR          (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR          (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR         (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1         (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_CR3         (*(volatile uint32_t *)(USART1_BASE + 0x14u))
#define USART_CR1_RE       (1u << 2)
#define USART_CR1_TE       (1u << 3)
#define USART_CR1_UE       (1u << 13)
#define USART_CR3_DMAR     (1u << 6)
#define USART_CR3_DMAT     (1u << 7)

#define DMA1_BASE          0x40020000u
#define DMA1_ISR           (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR          (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR4          (*(volatile uint32_t *)(DMA1_BASE + 0x44u))
#define DMA1_CNDTR4        (*(volatile uint32_t *)(DMA1_BASE + 0x48u))
#define DMA1_CPAR4         (*(volatile uint32_t *)(DMA1_BASE + 0x4Cu))
#define DMA1_CMAR4         (*(volatile uint32_t *)(DMA1_BASE + 0x50u))
#define DMA1_CCR5          (*(volatile uint32_t *)(DMA1_BASE + 0x58u))
#define DMA1_CNDTR5        (*(volatile uint32_t *)(DMA1_BASE + 0x5Cu))
#define DMA1_CPAR5         (*(volatile uint32_t *)(DMA1_BASE + 0x60u))
#define DMA1_CMAR5         (*(volatile uint32_t *)(DMA1_BASE + 0x64u))
#define DMA_CCR_EN         (1u << 0)
#define DMA_CCR_DIR        (1u << 4)
#define DMA_CCR_MINC       (1u << 7)
#define DMA_ISR_TCIF4      (1u << 13)
#define DMA_ISR_TEIF4      (1u << 15)
#define DMA_ISR_TCIF5      (1u << 17)
#define DMA_ISR_TEIF5      (1u << 19)
#define DMA_IFCR_ALL4      (0x0Fu << 12u)
#define DMA_IFCR_ALL5      (0x0Fu << 16u)

#define SYST_CSR           (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR           (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR           (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

#define ERR_RX_TIMEOUT     0xD001u
#define ERR_DMA_TRANSFER   0xD002u
#define ERR_DATA_MISMATCH  0xD003u

#define UART_LEN           8u
#define DMA_TIMEOUT        2000000u

static const uint8_t TX_BYTES[UART_LEN] = {
    0xA1u, 0xB2u, 0xC3u, 0xD4u, 0x55u, 0x66u, 0x77u, 0x88u
};
static uint8_t rx_bytes[UART_LEN];

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0u; i < ms; ++i) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
        }
    }
}

int main(void)
{
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    RCC_AHBENR |= RCC_DMA1EN;
    RCC_APB2ENR |= RCC_IOPAEN | RCC_USART1EN;

    /* PA9 = USART1_TX AF push-pull, PA10 = USART1_RX floating input. */
    GPIOA_CRH &= ~(0xFFu << 4u);
    GPIOA_CRH |= (0x0Bu << 4u) | (0x04u << 8u);

    DMA1_CCR4 = 0u;
    DMA1_CCR5 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL4 | DMA_IFCR_ALL5;

    DMA1_CPAR5 = (uint32_t)&USART1_DR;
    DMA1_CMAR5 = (uint32_t)rx_bytes;
    DMA1_CNDTR5 = UART_LEN;
    DMA1_CCR5 = DMA_CCR_MINC;

    USART1_CR1 = 0u;
    USART1_CR3 = 0u;
    USART1_BRR = 0x1D4Cu;
    USART1_CR3 = USART_CR3_DMAT | USART_CR3_DMAR;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    while ((USART1_SR & (1u << 5)) != 0u) {
        (void)USART1_DR;
    }

    DMA1_CCR5 |= DMA_CCR_EN;

    DMA1_CPAR4 = (uint32_t)&USART1_DR;
    DMA1_CMAR4 = (uint32_t)TX_BYTES;
    DMA1_CNDTR4 = UART_LEN;
    DMA1_CCR4 = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_EN;

    ael_mailbox_init();

    uint32_t timeout = DMA_TIMEOUT;
    while (((DMA1_ISR & DMA_ISR_TCIF5) == 0u) &&
           ((DMA1_ISR & (DMA_ISR_TEIF4 | DMA_ISR_TEIF5)) == 0u) &&
           timeout-- > 0u) {
    }

    if ((DMA1_ISR & (DMA_ISR_TEIF4 | DMA_ISR_TEIF5)) != 0u) {
        ael_mailbox_fail(ERR_DMA_TRANSFER, DMA1_ISR);
        while (1) {
        }
    }

    if ((DMA1_ISR & DMA_ISR_TCIF5) == 0u) {
        ael_mailbox_fail(ERR_RX_TIMEOUT, 0u);
        while (1) {
        }
    }

    uint32_t matched = 0u;
    uint32_t detail = 0u;
    for (uint32_t i = 0u; i < UART_LEN; ++i) {
        if (rx_bytes[i] != TX_BYTES[i]) {
            detail = (i << 16u) | ((uint32_t)rx_bytes[i] << 8u) | TX_BYTES[i];
            ael_mailbox_fail(ERR_DATA_MISMATCH, detail);
            while (1) {
            }
        }
        matched++;
    }

    ael_mailbox_pass();
    while (1) {
        delay_ms(1u);
        AEL_MAILBOX->detail0 = ++matched;
    }
}
