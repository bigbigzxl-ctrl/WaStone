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
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_USART1EN    (1u << 14)
#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_ISR_TXE   (1u << 7)
#define USART_ISR_TC    (1u << 6)

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
    }
}

static void usart1_write_byte(uint8_t b)
{
    while ((USART1_ISR & USART_ISR_TXE) == 0u) {
    }
    USART1_TDR = b;
}

static void usart1_write_cstr(const char *s)
{
    while (*s != '\0') {
        usart1_write_byte((uint8_t)*s++);
    }
}

int main(void)
{
    uint32_t frames = 0u;

    RCC_AHBENR |= RCC_GPIOAEN;
    RCC_APB2ENR |= RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 69u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();

    while (1) {
        usart1_write_cstr("AEL_UART_MB 55 AA 12 34\r\n");
        while ((USART1_ISR & USART_ISR_TC) == 0u) {
        }

        frames++;
        AEL_MAILBOX->detail0 = frames;
        if (frames == 1u) {
            ael_mailbox_pass();
        }

        delay(240000u);
    }
}
