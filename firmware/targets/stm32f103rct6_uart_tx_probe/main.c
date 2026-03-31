#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "../stm32f103rct6/ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

#define GPIOA_BASE      0x40010800U
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04U))

#define USART1_BASE     0x40013800U
#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00U))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04U))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08U))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0CU))

#define USART_SR_TXE    (1U << 7)
#define USART_SR_TC     (1U << 6)

static void delay(volatile uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

static void uart_write_str(const char *s)
{
    while (*s) {
        while ((USART1_SR & USART_SR_TXE) == 0u) {}
        USART1_DR = (uint8_t)*s++;
    }
}

int main(void)
{
    uint32_t frames = 0u;

    RCC_APB2ENR |= (1u << 2) | (1u << 14);

    /* PA9 TX AF push-pull, PA10 RX input floating. */
    GPIOA_CRH &= ~0xFF0u;
    GPIOA_CRH |= 0x4B0u;

    USART1_BRR = 0x45u;
    USART1_CR1 = (1u << 13) | (1u << 3) | (1u << 2);

    ael_mailbox_init();

    while (1) {
        uart_write_str("AEL_UART_TX_PROBE\r\n");
        while ((USART1_SR & USART_SR_TC) == 0u) {}
        frames++;
        AEL_MAILBOX->detail0 = frames;
        if (frames == 1u) {
            ael_mailbox_pass();
        }
        delay(240000u);
    }
}
