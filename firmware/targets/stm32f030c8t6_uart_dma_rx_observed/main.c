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
#define USART1_RDR      (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define DMA1_BASE       0x40020000u
#define DMA1_ISR        (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR3       (*(volatile uint32_t *)(DMA1_BASE + 0x30u))
#define DMA1_CNDTR3     (*(volatile uint32_t *)(DMA1_BASE + 0x34u))
#define DMA1_CPAR3      (*(volatile uint32_t *)(DMA1_BASE + 0x38u))
#define DMA1_CMAR3      (*(volatile uint32_t *)(DMA1_BASE + 0x3Cu))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_DMA1EN      (1u << 0)
#define RCC_SYSCFGEN    (1u << 0)
#define RCC_USART1EN    (1u << 14)

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR3_DMAR  (1u << 6)
#define USART_ISR_RXNE  (1u << 5)
#define USART_ISR_TXE   (1u << 7)
#define USART_ISR_TC    (1u << 6)
#define USART_ICR_ORECF (1u << 3)
#define USART_ICR_FECF  (1u << 1)
#define USART_ICR_NCF   (1u << 2)
#define USART_ICR_PECF  (1u << 0)

#define DMA_CCR_EN      (1u << 0)
#define DMA_CCR_MINC    (1u << 7)
#define DMA_ISR_TCIF3   (1u << 9)
#define DMA_ISR_TEIF3   (1u << 11)
#define DMA_IFCR_ALL3   (0x0Fu << 8u)

#define DMA_TIMEOUT     1400000u

static uint8_t RX_BUF[12];

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
    }
}

static void uart_write_byte(uint8_t b)
{
    while ((USART1_ISR & USART_ISR_TXE) == 0u) {
    }
    USART1_TDR = b;
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

static void uart_write_kv_hex(const char *key, uint32_t value)
{
    uart_write_cstr(key);
    uart_write_byte('=');
    uart_write_hex32(value);
    uart_write_byte(' ');
}

static void uart_write_line(const char *s)
{
    uart_write_cstr(s);
    uart_write_cstr("\r\n");
    while ((USART1_ISR & USART_ISR_TC) == 0u) {
    }
}

static int rx_matches_expected(void)
{
    static const uint8_t EXPECTED[] = "PING_DMA_RX";
    for (uint32_t i = 0u; i < sizeof(EXPECTED) - 1u; ++i) {
        if (RX_BUF[i] != EXPECTED[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    uint32_t timeout = DMA_TIMEOUT;

    RCC_AHBENR |= RCC_GPIOAEN | RCC_DMA1EN;
    RCC_APB2ENR |= RCC_SYSCFGEN | RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 69u;
    USART1_ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
    USART1_CR3 = USART_CR3_DMAR;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    DMA1_CCR3 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL3;
    DMA1_CPAR3 = (uint32_t)&USART1_RDR;
    DMA1_CMAR3 = (uint32_t)RX_BUF;
    DMA1_CNDTR3 = sizeof(RX_BUF) - 1u;
    DMA1_CCR3 = DMA_CCR_MINC | DMA_CCR_EN;

    ael_mailbox_init();

    while (1) {
        uart_write_line("AEL_UART_DMA_RX_READY");
        for (uint32_t spin = 0u; spin < 8u; ++spin) {
            delay(80000u);
            if ((DMA1_ISR & (DMA_ISR_TCIF3 | DMA_ISR_TEIF3)) != 0u) {
                break;
            }
        }
        if ((DMA1_ISR & DMA_ISR_TEIF3) != 0u) {
            uart_write_cstr("AEL_UART_DMA_RX_FAIL ");
            uart_write_kv_hex("dma_isr", DMA1_ISR);
            uart_write_kv_hex("cndtr", DMA1_CNDTR3);
            uart_write_kv_hex("ccr", DMA1_CCR3);
            uart_write_kv_hex("usart_isr", USART1_ISR);
            uart_write_kv_hex("cfgr1", SYSCFG_CFGR1);
            uart_write_cstr("\r\n");
            ael_mailbox_fail(0xD511u, DMA1_ISR);
            while (1) {
                delay(240000u);
            }
        }
        if ((DMA1_ISR & DMA_ISR_TCIF3) != 0u) {
            if (rx_matches_expected()) {
                uart_write_line("AEL_UART_DMA_RX_OK");
                ael_mailbox_pass();
                while (1) {
                    delay(240000u);
                }
            }
            uart_write_cstr("AEL_UART_DMA_RX_BAD ");
            uart_write_kv_hex("dma_isr", DMA1_ISR);
            uart_write_kv_hex("cndtr", DMA1_CNDTR3);
            uart_write_kv_hex("rx0", RX_BUF[0]);
            uart_write_kv_hex("rx1", RX_BUF[1]);
            uart_write_kv_hex("rx2", RX_BUF[2]);
            uart_write_kv_hex("rx3", RX_BUF[3]);
            uart_write_cstr("\r\n");
            ael_mailbox_fail(0xD512u, RX_BUF[0]);
            while (1) {
                delay(240000u);
            }
        }
        if (timeout-- == 0u) {
            uart_write_cstr("AEL_UART_DMA_RX_TIMEOUT ");
            uart_write_kv_hex("dma_isr", DMA1_ISR);
            uart_write_kv_hex("cndtr", DMA1_CNDTR3);
            uart_write_kv_hex("ccr", DMA1_CCR3);
            uart_write_kv_hex("usart_isr", USART1_ISR);
            uart_write_kv_hex("cfgr1", SYSCFG_CFGR1);
            uart_write_cstr("\r\n");
            ael_mailbox_fail(0xD513u, DMA1_CNDTR3);
            while (1) {
                delay(240000u);
            }
        }
    }
}
