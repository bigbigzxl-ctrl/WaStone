#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"
#include "stm32f030x8.h"

static uint8_t rx_buf[16];
static const uint8_t expected[] = "PING_DMA_RX";

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

static int rx_matches_expected(void)
{
    for (uint32_t i = 0u; i < (sizeof(expected) - 1u); ++i) {
        if (rx_buf[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
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

int main(void)
{
    uint32_t timeout = 4000000u;
    uint32_t last_ready = 0u;

    ael_mailbox_init();
    uart_init();

    for (uint32_t i = 0u; i < sizeof(rx_buf); ++i) {
        rx_buf[i] = 0u;
    }

    DMA1_Channel3->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF3;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;
    USART1->RQR = USART_RQR_RXFRQ;

    DMA1_Channel3->CPAR = (uint32_t)&USART1->RDR;
    DMA1_Channel3->CMAR = (uint32_t)rx_buf;
    DMA1_Channel3->CNDTR = sizeof(expected) - 1u;
    DMA1_Channel3->CCR = DMA_CCR_MINC;

    USART1->CR3 |= USART_CR3_DMAR;
    DMA1_Channel3->CCR |= DMA_CCR_EN;

    while (timeout-- != 0u) {
        if ((timeout - last_ready) > 400000u || last_ready == 0u) {
            uart_write_line("AEL_UART_DMA_RX_READY");
            last_ready = timeout;
        }

        if ((DMA1->ISR & DMA_ISR_TEIF3) != 0u) {
            ael_mailbox_fail(0xD611u, DMA1->ISR);
            while (1) {
                uart_write_line("AEL_UART_DMA_RX_READY");
                uart_write_cstr("AEL_UART_DMA_RX_ERR dma_isr=");
                uart_write_hex32(DMA1->ISR);
                uart_write_cstr(" cndtr=");
                uart_write_hex32(DMA1_Channel3->CNDTR);
                uart_write_cstr("\r\n");
                delay(320000u);
            }
        }

        if ((DMA1->ISR & DMA_ISR_TCIF3) != 0u) {
            DMA1->IFCR = DMA_IFCR_CGIF3;
            DMA1_Channel3->CCR &= ~DMA_CCR_EN;
            USART1->CR3 &= ~USART_CR3_DMAR;
            if (rx_matches_expected()) {
                ael_mailbox_pass();
                while (1) {
                    uart_write_line("AEL_UART_DMA_RX_READY");
                    uart_write_line("AEL_UART_DMA_RX_OK");
                    delay(320000u);
                }
            }
            ael_mailbox_fail(0xD612u, rx_buf[0]);
            while (1) {
                uart_write_line("AEL_UART_DMA_RX_READY");
                uart_write_cstr("AEL_UART_DMA_RX_BAD rx0=");
                uart_write_hex32(rx_buf[0]);
                uart_write_cstr(" rx1=");
                uart_write_hex32(rx_buf[1]);
                uart_write_cstr(" rx2=");
                uart_write_hex32(rx_buf[2]);
                uart_write_cstr(" rx3=");
                uart_write_hex32(rx_buf[3]);
                uart_write_cstr("\r\n");
                delay(320000u);
            }
        }
    }

    ael_mailbox_fail(0xD613u, DMA1_Channel3->CNDTR);
    while (1) {
        uart_write_line("AEL_UART_DMA_RX_READY");
        uart_write_cstr("AEL_UART_DMA_RX_TIMEOUT dma_isr=");
        uart_write_hex32(DMA1->ISR);
        uart_write_cstr(" cndtr=");
        uart_write_hex32(DMA1_Channel3->CNDTR);
        uart_write_cstr(" ccr=");
        uart_write_hex32(DMA1_Channel3->CCR);
        uart_write_cstr(" usart_isr=");
        uart_write_hex32(USART1->ISR);
        uart_write_cstr("\r\n");
        delay(320000u);
    }
}
