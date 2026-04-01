#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x04U))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

#define GPIOA_BASE      0x40010800U
#define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0CU))

#define ADC1_BASE       0x40012400U
#define ADC1_SR         (*(volatile uint32_t *)(ADC1_BASE + 0x00U))
#define ADC1_CR1        (*(volatile uint32_t *)(ADC1_BASE + 0x04U))
#define ADC1_CR2        (*(volatile uint32_t *)(ADC1_BASE + 0x08U))
#define ADC1_SMPR2      (*(volatile uint32_t *)(ADC1_BASE + 0x10U))
#define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x2CU))
#define ADC1_SQR3       (*(volatile uint32_t *)(ADC1_BASE + 0x34U))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x4CU))

#define ADC_SR_EOC      (1U << 1)
#define ADC_CR2_ADON    (1U << 0)
#define ADC_CR2_RSTCAL  (1U << 3)
#define ADC_CR2_CAL     (1U << 2)
#define ADC_CR2_EXTTRIG (1U << 20)
#define ADC_CR2_SWSTART (1U << 22)

#define HIGH_MIN 2500u
#define LOW_MAX  900u

static void delay(uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

static uint32_t adc_read(void)
{
    ADC1_SR &= ~ADC_SR_EOC;
    ADC1_CR2 |= ADC_CR2_SWSTART;
    for (volatile uint32_t t = 400000u; (ADC1_SR & ADC_SR_EOC) == 0u && t > 0u; t--) {
    }
    return ADC1_DR & 0xFFFu;
}

int main(void)
{
    RCC_CFGR &= ~(3u << 14);
    RCC_APB2ENR |= (1u << 2) | (1u << 9);

    GPIOA_CRL &= ~0xFFu;
    GPIOA_CRL |= 0x03u;
    GPIOA_ODR &= ~(1u << 0u);

    ADC1_CR1 = 0u;
    ADC1_CR2 = ADC_CR2_ADON | (7u << 17) | ADC_CR2_EXTTRIG;
    ADC1_SMPR2 = (7u << 3);
    ADC1_SQR1 = 0u;
    ADC1_SQR3 = 1u;

    for (volatile uint32_t i = 0; i < 200u; i++) {
        __asm__ volatile ("nop");
    }
    ADC1_CR2 |= ADC_CR2_RSTCAL;
    while (ADC1_CR2 & ADC_CR2_RSTCAL) {}
    ADC1_CR2 |= ADC_CR2_CAL;
    while (ADC1_CR2 & ADC_CR2_CAL) {}

    ael_mailbox_init();

    GPIOA_ODR |= (1u << 0u);
    delay(4000u);
    {
        uint32_t high = adc_read();
        if (high < HIGH_MIN) {
            ael_mailbox_fail(0xE401u, high);
            while (1) {}
        }
        AEL_MAILBOX->detail0 = (high << 16u);
    }

    GPIOA_ODR &= ~(1u << 0u);
    delay(4000u);
    {
        uint32_t low = adc_read();
        AEL_MAILBOX->detail0 |= low;
        if (low > LOW_MAX) {
            ael_mailbox_fail(0xE402u, AEL_MAILBOX->detail0);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {
        delay(12000u);
        AEL_MAILBOX->detail0++;
    }
}
