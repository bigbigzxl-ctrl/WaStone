#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_CR2         (*(volatile uint32_t *)(RCC_BASE + 0x34u))
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

#define ADC1_BASE       0x40012400u
#define ADC_ISR         (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC_CR          (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC_CFGR1       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC_SMPR        (*(volatile uint32_t *)(ADC1_BASE + 0x14u))
#define ADC_CHSELR      (*(volatile uint32_t *)(ADC1_BASE + 0x28u))
#define ADC_DR          (*(volatile uint32_t *)(ADC1_BASE + 0x40u))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_ADCEN       (1u << 9)
#define RCC_HSI14ON     (1u << 0)
#define RCC_HSI14RDY    (1u << 1)

#define ADC_ISR_ADRDY   (1u << 0)
#define ADC_ISR_EOC     (1u << 2)
#define ADC_CR_ADEN     (1u << 0)
#define ADC_CR_ADSTART  (1u << 2)
#define ADC_CR_ADCAL    (1u << 31)

#define HIGH_MIN        3000u
#define LOW_MAX         900u

static void delay(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static uint32_t adc_read_once(void)
{
    ADC_ISR |= ADC_ISR_EOC;
    ADC_CR |= ADC_CR_ADSTART;
    for (volatile uint32_t t = 0; t < 400000u; ++t) {
        if ((ADC_ISR & ADC_ISR_EOC) != 0u) {
            break;
        }
    }
    return ADC_DR & 0x0FFFu;
}

int main(void)
{
    uint32_t pass_count = 0u;

    RCC_AHBENR |= RCC_GPIOAEN;
    RCC_APB2ENR |= RCC_ADCEN;
    RCC_CR2 |= RCC_HSI14ON;
    while ((RCC_CR2 & RCC_HSI14RDY) == 0u) {}

    GPIOA_MODER &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOA_MODER |= (0x1u << 0u) | (0x3u << 2u);
    GPIOA_PUPDR &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOA_ODR &= ~(1u << 0u);

    ADC_CFGR1 = 0u;
    ADC_SMPR = 0x7u;
    ADC_CHSELR = (1u << 1u);

    ADC_CR |= ADC_CR_ADCAL;
    while ((ADC_CR & ADC_CR_ADCAL) != 0u) {}

    ADC_ISR |= ADC_ISR_ADRDY;
    ADC_CR |= ADC_CR_ADEN;
    while ((ADC_ISR & ADC_ISR_ADRDY) == 0u) {}

    ael_mailbox_init();

    while (1) {
        uint32_t high;
        uint32_t low;

        GPIOA_ODR |= (1u << 0u);
        delay(4000u);
        high = adc_read_once();
        if (high < HIGH_MIN) {
            ael_mailbox_fail(0xE131u, high);
            while (1) {}
        }

        GPIOA_ODR &= ~(1u << 0u);
        delay(4000u);
        low = adc_read_once();
        if (low > LOW_MAX) {
            ael_mailbox_fail(0xE132u, low);
            while (1) {}
        }

        pass_count++;
        AEL_MAILBOX->detail0 = (high << 16u) | (low & 0xFFFFu);
        if (pass_count >= 5u) {
            ael_mailbox_pass();
            while (1) {
                delay(12000u);
                AEL_MAILBOX->detail0++;
            }
        }
    }
}
