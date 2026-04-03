#include <stdint.h>

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

#define GPIOC_BASE      0x48000800u
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00u))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x14u))

#define USART1_BASE     0x40013800u
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))

#define SYSTICK_BASE    0xE000E010u
#define SYST_CSR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))

#define RCC_GPIOAEN         (1u << 17)
#define RCC_GPIOCEN         (1u << 19)
#define RCC_USART1EN        (1u << 14)
#define USART_ISR_TXE       (1u << 7)
#define USART_CR1_UE        (1u << 0)
#define USART_CR1_TE        (1u << 3)
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

static void uart1_init(void)
{
    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);
    USART1_BRR = 69u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void pc13_output(void)
{
    GPIOC_MODER &= ~(0x3u << 26u);
    GPIOC_MODER |=  (0x1u << 26u);
}

static void uart1_write_str(const char *s)
{
    while (*s != '\0') {
        while ((USART1_ISR & USART_ISR_TXE) == 0u) {
        }
        USART1_TDR = (uint8_t)(*s++);
    }
}

int main(void)
{
    uint32_t led_ms = 0u;
    uint32_t banner_ms = 0u;

    RCC_AHBENR |= RCC_GPIOAEN | RCC_GPIOCEN;
    RCC_APB2ENR |= RCC_USART1EN;

    pc13_output();
    uart1_init();

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    uart1_write_str("AEL_READY STM32F030C8T6 UART\r\n");

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }
        if (++led_ms >= 250u) {
            led_ms = 0u;
            GPIOC_ODR ^= (1u << 13);
        }
        if (++banner_ms >= 1000u) {
            banner_ms = 0u;
            uart1_write_str("AEL_READY STM32F030C8T6 UART\r\n");
        }
    }
}
