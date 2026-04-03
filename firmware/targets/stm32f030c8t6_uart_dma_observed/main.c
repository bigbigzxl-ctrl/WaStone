#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"
#include "stm32f030x8.h"

static volatile uint32_t g_dma_done;
static volatile uint32_t g_dma_error;
static volatile uint32_t g_uart_tc_done;
static volatile uint32_t g_dma_isr_snapshot;
static volatile uint32_t g_usart_isr_snapshot;

static uint8_t tx_frame[] = "AEL_UART_DMA_TX_BARE\r\n";

static void uart_write_byte(uint8_t b)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0u) {
    }
    USART1->TDR = b;
}

static void uart_write_cstr(const char *s)
{
    while (*s != '\0') {
        uart_write_byte((uint8_t)*s++);
    }
}

static void uart_write_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_write_byte((uint8_t)hex[(value >> shift) & 0xFu]);
    }
}

static void uart_write_line(const char *s)
{
    uart_write_cstr(s);
    uart_write_cstr("\r\n");
    while ((USART1->ISR & USART_ISR_TC) == 0u) {
    }
}

static void delay(volatile uint32_t count)
{
    while (count-- != 0u) {
        __NOP();
    }
}

static void uart_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_DMA1EN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOA->MODER &= ~((3u << (9u * 2u)) | (3u << (10u * 2u)));
    GPIOA->MODER |= (2u << (9u * 2u)) | (2u << (10u * 2u));
    GPIOA->OSPEEDR &= ~((3u << (9u * 2u)) | (3u << (10u * 2u)));
    GPIOA->OSPEEDR |= (3u << (9u * 2u)) | (3u << (10u * 2u));
    GPIOA->PUPDR &= ~((3u << (9u * 2u)) | (3u << (10u * 2u)));
    GPIOA->PUPDR |= (1u << (9u * 2u)) | (1u << (10u * 2u));
    GPIOA->AFR[1] &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA->AFR[1] |= (1u << 4u) | (1u << 8u);

    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = 0u;
    USART1->BRR = 69u;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF |
                  USART_ICR_PECF | USART_ICR_TCCF;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
    }
}

void DMA1_Channel2_3_IRQHandler(void)
{
    uint32_t isr = DMA1->ISR;

    g_dma_isr_snapshot = isr;
    if ((isr & DMA_ISR_TEIF2) != 0u) {
        DMA1->IFCR = DMA_IFCR_CGIF2;
        DMA1_Channel2->CCR &= ~DMA_CCR_EN;
        g_dma_error = 0xD401u;
        return;
    }
    if ((isr & DMA_ISR_TCIF2) != 0u) {
        DMA1->IFCR = DMA_IFCR_CGIF2;
        DMA1_Channel2->CCR &= ~(DMA_CCR_TCIE | DMA_CCR_TEIE);
        USART1->CR1 |= USART_CR1_TCIE;
        g_dma_done = 1u;
    }
}

void USART1_IRQHandler(void)
{
    uint32_t isr = USART1->ISR;

    g_usart_isr_snapshot = isr;
    if (((USART1->CR1 & USART_CR1_TCIE) != 0u) && ((isr & USART_ISR_TC) != 0u)) {
        USART1->ICR = USART_ICR_TCCF;
        USART1->CR1 &= ~USART_CR1_TCIE;
        g_uart_tc_done = 1u;
    }
}

int main(void)
{
    uint32_t timeout = 4000000u;

    ael_mailbox_init();
    uart_init();

    uart_write_line("AEL_UART_DMA_DIAG_BEGIN");

    g_dma_done = 0u;
    g_dma_error = 0u;
    g_uart_tc_done = 0u;
    g_dma_isr_snapshot = 0u;
    g_usart_isr_snapshot = 0u;

    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF2;
    DMA1_Channel2->CPAR = (uint32_t)&USART1->TDR;
    DMA1_Channel2->CMAR = (uint32_t)tx_frame;
    DMA1_Channel2->CNDTR = sizeof(tx_frame) - 1u;
    DMA1_Channel2->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE | DMA_CCR_TEIE;

    NVIC_ClearPendingIRQ(DMA1_Channel2_3_IRQn);
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);

    USART1->ICR = USART_ICR_TCCF;
    USART1->CR3 |= USART_CR3_DMAT;
    DMA1_Channel2->CCR |= DMA_CCR_EN;

    while ((g_uart_tc_done == 0u) && (g_dma_error == 0u) && (timeout-- != 0u)) {
    }

    if ((g_dma_done != 0u) && (g_uart_tc_done != 0u)) {
        ael_mailbox_pass();
        while (1) {
            uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
            uart_write_line("AEL_UART_DMA_TX_BARE");
            uart_write_line("AEL_UART_DMA_OK");
            delay(320000u);
        }
    }

    ael_mailbox_fail(0xD402u, DMA1_Channel2->CNDTR);
    while (1) {
        uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
        uart_write_cstr("AEL_UART_DMA_FAIL dma_isr=");
        uart_write_hex32(g_dma_isr_snapshot);
        uart_write_cstr(" usart_isr=");
        uart_write_hex32(g_usart_isr_snapshot);
        uart_write_cstr(" dma_ccr=");
        uart_write_hex32(DMA1_Channel2->CCR);
        uart_write_cstr(" dma_cndtr=");
        uart_write_hex32(DMA1_Channel2->CNDTR);
        uart_write_cstr(" usart_cr3=");
        uart_write_hex32(USART1->CR3);
        uart_write_cstr(" usart_isr_now=");
        uart_write_hex32(USART1->ISR);
        uart_write_cstr("\r\n");
        delay(320000u);
    }
}
