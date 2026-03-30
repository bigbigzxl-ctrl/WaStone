#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

#define RCC_BASE            0x40021000u
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_IOPAEN          (1u << 2)
#define RCC_IOPBEN          (1u << 3)
#define RCC_USART1EN        (1u << 14)
#define RCC_ADC1EN          (1u << 9)

#define GPIOA_BASE          0x40010800u
#define GPIOA_CRL           (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_CRH           (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_IDR           (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_ODR           (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))

#define GPIOB_BASE          0x40010C00u
#define GPIOB_CRH           (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_IDR           (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))

#define USART1_BASE         0x40013800u
#define USART1_SR           (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR           (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR          (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1          (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART_SR_RXNE       (1u << 5)
#define USART_SR_TXE        (1u << 7)
#define USART_CR1_RE        (1u << 2)
#define USART_CR1_TE        (1u << 3)
#define USART_CR1_UE        (1u << 13)

#define ADC1_BASE           0x40012400u
#define ADC1_SR             (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR1            (*(volatile uint32_t *)(ADC1_BASE + 0x04u))
#define ADC1_CR2            (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_SMPR2          (*(volatile uint32_t *)(ADC1_BASE + 0x10u))
#define ADC1_SQR3           (*(volatile uint32_t *)(ADC1_BASE + 0x34u))
#define ADC1_DR             (*(volatile uint32_t *)(ADC1_BASE + 0x4Cu))
#define ADC_CR2_ADON        (1u << 0)
#define ADC_CR2_CAL         (1u << 2)
#define ADC_CR2_RSTCAL      (1u << 3)
#define ADC_CR2_EXTTRIG     (1u << 20)
#define ADC_CR2_SWSTART     (1u << 22)
#define ADC_CR2_EXTSEL_SWSTART (0x7u << 17)
#define ADC_SR_EOC          (1u << 1)

#define ERR_GPIO_HIGH       (1u << 0)
#define ERR_GPIO_LOW        (1u << 1)
#define ERR_UART            (1u << 2)
#define ERR_ADC_HIGH        (1u << 3)
#define ERR_ADC_LOW         (1u << 4)

static void delay_cycles(volatile uint32_t n) {
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static uint32_t test_gpio_loopback(void)
{
    uint32_t err = 0u;

    GPIOA_CRH &= ~(0xFu << 0u);
    GPIOA_CRH |=  (0x3u << 0u);

    GPIOB_CRH &= ~(0xFu << 0u);
    GPIOB_CRH |=  (0x4u << 0u);

    GPIOA_ODR |= (1u << 8u);
    delay_cycles(12000u);
    if ((GPIOB_IDR & (1u << 8u)) == 0u) {
        err |= ERR_GPIO_HIGH;
    }

    GPIOA_ODR &= ~(1u << 8u);
    delay_cycles(12000u);
    if ((GPIOB_IDR & (1u << 8u)) != 0u) {
        err |= ERR_GPIO_LOW;
    }

    return err;
}

static uint32_t test_uart_loopback(void)
{
    GPIOA_CRH &= ~(0xFFu << 4u);
    GPIOA_CRH |=  (0x0Bu << 4u) | (0x04u << 8u);

    USART1_CR1 = 0u;
    USART1_BRR = 0x45u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    const uint8_t patterns[] = {0x55u, 0xA5u, 0x3Cu, 0xC3u};
    for (uint32_t i = 0u; i < 4u; ++i) {
        uint32_t timeout = 200000u;
        while (((USART1_SR & USART_SR_RXNE) != 0u) && timeout-- > 0u) {
            (void)USART1_DR;
        }

        timeout = 200000u;
        while (((USART1_SR & USART_SR_TXE) == 0u) && timeout-- > 0u) {}
        if ((USART1_SR & USART_SR_TXE) == 0u) {
            return ERR_UART;
        }
        USART1_DR = patterns[i];

        timeout = 200000u;
        while (((USART1_SR & USART_SR_RXNE) == 0u) && timeout-- > 0u) {}
        if ((USART1_SR & USART_SR_RXNE) == 0u) {
            return ERR_UART;
        }
        if ((uint8_t)USART1_DR != patterns[i]) {
            return ERR_UART;
        }
    }

    return 0u;
}

static void adc1_init(void)
{
    GPIOA_CRL &= ~((0xFu << 0u) | (0xFu << 4u));
    GPIOA_CRL |=  (0x3u << 4u);

    ADC1_SMPR2 |= (0x7u << 0u);
    ADC1_CR1 = 0u;
    ADC1_SQR3 = 0u;
    ADC1_CR2 = ADC_CR2_ADON;
    delay_cycles(10000u);
    ADC1_CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1_CR2 & ADC_CR2_RSTCAL) != 0u) {}
    ADC1_CR2 |= ADC_CR2_CAL;
    while ((ADC1_CR2 & ADC_CR2_CAL) != 0u) {}
}

static uint8_t adc1_read(uint16_t *out)
{
    ADC1_SR = 0u;
    ADC1_CR2 |= ADC_CR2_ADON;
    ADC1_CR2 &= ~(0x7u << 17u);
    ADC1_CR2 |= ADC_CR2_EXTSEL_SWSTART;
    ADC1_CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_SWSTART;
    for (uint32_t i = 0u; i < 100000u; ++i) {
        if ((ADC1_SR & ADC_SR_EOC) != 0u) {
            *out = (uint16_t)(ADC1_DR & 0xFFFFu);
            return 1u;
        }
    }
    return 0u;
}

static uint32_t test_adc_loopback(void)
{
    uint32_t err = 0u;
    uint16_t value = 0u;

    GPIOA_ODR |= (1u << 1u);
    delay_cycles(16000u);
    if (adc1_read(&value) == 0u || value < 3000u) {
        err |= ERR_ADC_HIGH;
    }

    GPIOA_ODR &= ~(1u << 1u);
    delay_cycles(16000u);
    if (adc1_read(&value) == 0u || value > 1000u) {
        err |= ERR_ADC_LOW;
    }

    return err;
}

int main(void)
{
    RCC_APB2ENR |= RCC_IOPAEN | RCC_IOPBEN | RCC_USART1EN | RCC_ADC1EN;
    (void)RCC_APB2ENR;

    ael_mailbox_init();
    adc1_init();

    uint32_t err = 0u;
    err |= test_gpio_loopback();
    err |= test_uart_loopback();
    err |= test_adc_loopback();

    if (err == 0u) {
        ael_mailbox_pass();
        while (1) {
            AEL_MAILBOX->detail0 += 1u;
            delay_cycles(20000u);
        }
    }

    ael_mailbox_fail(err, err);
    while (1) {}
}
