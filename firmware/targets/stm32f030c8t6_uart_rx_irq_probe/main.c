#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE            0x40021000u
#define RCC_AHBENR          (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE          0x48000000u
#define GPIOA_MODER         (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH          (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

#define USART1_BASE         0x40013800u
#define USART1_CR1          (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR          (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR          (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_ICR          (*(volatile uint32_t *)(USART1_BASE + 0x20u))
#define USART1_RDR          (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR          (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define NVIC_ISER           (*(volatile uint32_t *)0xE000E100u)

#define RCC_GPIOAEN         (1u << 17)
#define RCC_USART1EN        (1u << 14)
#define USART_CR1_UE        (1u << 0)
#define USART_CR1_RE        (1u << 2)
#define USART_CR1_TE        (1u << 3)
#define USART_CR1_RXNEIE    (1u << 5)
#define USART_ISR_PE        (1u << 0)
#define USART_ISR_FE        (1u << 1)
#define USART_ISR_NE        (1u << 2)
#define USART_ISR_ORE       (1u << 3)
#define USART_ISR_RXNE      (1u << 5)
#define USART_ISR_TXE       (1u << 7)
#define USART_ISR_TC        (1u << 6)
#define USART_ICR_PECF      (1u << 0)
#define USART_ICR_ORECF     (1u << 3)
#define USART_ICR_FECF      (1u << 1)
#define USART_ICR_NCF       (1u << 2)

#define USART1_IRQN         27u

static volatile char rx_buf[32];
static volatile uint32_t rx_len = 0u;
static volatile uint32_t rx_ready = 0u;

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
    while ((USART1_ISR & USART_ISR_TC) == 0u) {
    }
}

static int streq_volatile(const volatile char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

void USART1_IRQHandler(void)
{
    uint32_t isr = USART1_ISR;

    if ((isr & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) != 0u) {
        USART1_ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF;
        if ((isr & USART_ISR_RXNE) != 0u) {
            (void)USART1_RDR;
        }
        return;
    }

    if ((isr & USART_ISR_RXNE) != 0u) {
        uint8_t b = (uint8_t)USART1_RDR;
        if (b == '\r' || b == '\n') {
            if (rx_len != 0u) {
                rx_buf[rx_len] = '\0';
                rx_ready = 1u;
            }
            return;
        }
        if ((rx_len + 1u) < sizeof(rx_buf)) {
            rx_buf[rx_len++] = (char)b;
        } else {
            rx_len = 0u;
            rx_ready = 0u;
        }
    }
}

int main(void)
{
    uint32_t heartbeat = 0u;
    uint32_t announce_div = 0u;

    RCC_AHBENR |= RCC_GPIOAEN;
    RCC_APB2ENR |= RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 69u;
    USART1_ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();
    uart_write_cstr("AEL_UART_RX_IRQ_READY\r\n");
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_ISER = (1u << USART1_IRQN);
    __asm__ volatile ("cpsie i");

    while (1) {
        for (uint32_t spin = 0u; spin < 200000u; ++spin) {
            if (rx_ready != 0u) {
                break;
            }
            delay(8u);
        }
        if (rx_ready == 0u) {
            if ((announce_div++ & 0x3u) == 0u) {
                uart_write_cstr("AEL_UART_RX_IRQ_READY\r\n");
            }
            delay(80000u);
            continue;
        }
        if (!streq_volatile(rx_buf, "PING_IRQ_RX")) {
            uart_write_cstr("AEL_UART_RX_IRQ_BAD\r\n");
            rx_len = 0u;
            rx_ready = 0u;
            rx_buf[0] = '\0';
            AEL_MAILBOX->detail0++;
            delay(80000u);
            continue;
        }
        uart_write_cstr("AEL_UART_RX_IRQ_OK\r\n");
        heartbeat++;
        AEL_MAILBOX->detail0 = heartbeat;
        ael_mailbox_pass();
        while (1) {
            heartbeat++;
            AEL_MAILBOX->detail0 = heartbeat;
            delay(240000u);
        }
    }
}
