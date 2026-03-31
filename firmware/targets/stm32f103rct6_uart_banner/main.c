#include <stdint.h>

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE      0x40010800u
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define GPIOC_BASE      0x40011000u
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define USART1_BASE     0x40013800u
#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))

#define SYSTICK_BASE    0xE000E010u
#define SYST_CSR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))

#define RCC_IOPAEN          (1u << 2)
#define RCC_IOPCEN          (1u << 4)
#define RCC_USART1EN        (1u << 14)
#define USART_SR_TXE        (1u << 7)
#define USART_CR1_UE        (1u << 13)
#define USART_CR1_TE        (1u << 3)
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

static void uart1_init(void)
{
    /* PA9 = USART1 TX, 50 MHz alternate-function push-pull. */
    GPIOA_CRH &= ~(0xFu << 4);
    GPIOA_CRH |=  (0xBu << 4);
    USART1_BRR = 0x45u; /* 8 MHz / 115200 */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void gpio_pc7_output_push_pull(void)
{
    const uint32_t shift = (7u - 4u) * 4u;
    GPIOC_CRH &= ~(0xFu << shift);
    GPIOC_CRH |=  (0x2u << shift);
}

static void uart1_write_str(const char *s)
{
    while (*s != '\0') {
        while ((USART1_SR & USART_SR_TXE) == 0u) {
        }
        USART1_DR = (uint32_t)(uint8_t)(*s++);
    }
}

int main(void)
{
    RCC_APB2ENR |= RCC_IOPAEN | RCC_IOPCEN | RCC_USART1EN;

    gpio_pc7_output_push_pull();
    uart1_init();

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    uart1_write_str("AEL_READY STM32F103RCT6 UART\r\n");

    uint32_t led_ms = 0u;
    uint32_t banner_ms = 0u;

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }
        if (++led_ms >= 250u) {
            led_ms = 0u;
            GPIOC_ODR ^= (1u << 7);
        }
        if (++banner_ms >= 1000u) {
            banner_ms = 0u;
            uart1_write_str("AEL_READY STM32F103RCT6 UART\r\n");
        }
    }
}
