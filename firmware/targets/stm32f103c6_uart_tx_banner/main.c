#include <stdint.h>

#define RCC_BASE             0x40021000u
#define RCC_APB2ENR          (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB2ENR_IOPAEN   (1u << 2)
#define RCC_APB2ENR_IOPCEN   (1u << 4)
#define RCC_APB2ENR_USART1EN (1u << 14)

#define GPIOA_BASE           0x40010800u
#define GPIOA_CRH            (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define GPIOC_BASE           0x40011000u
#define GPIOC_CRH            (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR            (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define USART1_BASE          0x40013800u
#define USART1_SR            (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR            (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR           (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1           (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))

#define USART_SR_TXE         (1u << 7)
#define USART_SR_TC          (1u << 6)
#define USART_CR1_UE         (1u << 13)
#define USART_CR1_TE         (1u << 3)

#define SYST_CSR             (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR             (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR             (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE      (1u << 0)
#define SYST_CSR_CLKSOURCE   (1u << 2)
#define SYST_CSR_COUNTFLAG   (1u << 16)

static void delay_ms(uint32_t ms)
{
    while (ms-- > 0u) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
        }
    }
}

static void usart1_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PA9 = USART1_TX AF push-pull 2 MHz. Leave PA10 untouched for this one-way TX test. */
    GPIOA_CRH &= ~(0xFu << 4u);
    GPIOA_CRH |= (0x0Bu << 4u);

    /* PC13 = status LED output push-pull 2 MHz. */
    GPIOC_CRH &= ~(0xFu << 20u);
    GPIOC_CRH |= (0x2u << 20u);

    /* Reset clock is 8 MHz HSI; BRR 0x45 gives ~115200 8N1. */
    USART1_BRR = 0x45u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void usart1_write_u8(uint8_t value)
{
    while ((USART1_SR & USART_SR_TXE) == 0u) {
    }
    USART1_DR = value;
}

static void usart1_write_str(const char *s)
{
    while (*s != '\0') {
        usart1_write_u8((uint8_t)*s++);
    }
}

static void usart1_write_u32_dec(uint32_t value)
{
    char buf[10];
    uint32_t i = 0u;

    if (value == 0u) {
        usart1_write_u8((uint8_t)'0');
        return;
    }

    while (value > 0u && i < (uint32_t)sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0u) {
        usart1_write_u8((uint8_t)buf[--i]);
    }
}

static void usart1_write_line_prefix(uint32_t counter)
{
    usart1_write_str("AEL_UART STM32F103C6T6 count=");
    usart1_write_u32_dec(counter);
    usart1_write_str(" baud=115200 8N1\r\n");
}

int main(void)
{
    uint32_t counter = 0u;

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    usart1_init();

    /* LED off initially on active-low PC13. */
    GPIOC_ODR |= (1u << 13);

    delay_ms(50u);
    usart1_write_str("AEL_READY STM32F103C6T6 UART_TX_ONLY\r\n");

    while (1) {
        GPIOC_ODR ^= (1u << 13);
        usart1_write_line_prefix(counter++);
        while ((USART1_SR & USART_SR_TC) == 0u) {
        }
        delay_ms(500u);
    }
}
