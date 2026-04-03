#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_CR2         (*(volatile uint32_t *)(RCC_BASE + 0x34u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define ADC1_BASE       0x40012400u
#define ADC_ISR         (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC_CR          (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC_CFGR1       (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC_SMPR        (*(volatile uint32_t *)(ADC1_BASE + 0x14u))
#define ADC_CHSELR      (*(volatile uint32_t *)(ADC1_BASE + 0x28u))
#define ADC_DR          (*(volatile uint32_t *)(ADC1_BASE + 0x40u))
#define ADC_CCR         (*(volatile uint32_t *)(0x40012708u))

#define RCC_ADCEN       (1u << 9)
#define RCC_HSI14ON     (1u << 0)
#define RCC_HSI14RDY    (1u << 1)
#define ADC_ISR_ADRDY   (1u << 0)
#define ADC_ISR_EOC     (1u << 2)
#define ADC_CR_ADEN     (1u << 0)
#define ADC_CR_ADSTART  (1u << 2)
#define ADC_CR_ADCAL    (1u << 31)
#define ADC_CCR_TSEN    (1u << 23)

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
    uint32_t sum = 0u;

    RCC_APB2ENR |= RCC_ADCEN;
    RCC_CR2 |= RCC_HSI14ON;
    while ((RCC_CR2 & RCC_HSI14RDY) == 0u) {}

    ADC_CCR |= ADC_CCR_TSEN;
    ADC_CFGR1 = 0u;
    ADC_SMPR = 0x7u;
    ADC_CHSELR = (1u << 16u);

    ADC_CR |= ADC_CR_ADCAL;
    while ((ADC_CR & ADC_CR_ADCAL) != 0u) {}
    ADC_ISR |= ADC_ISR_ADRDY;
    ADC_CR |= ADC_CR_ADEN;
    while ((ADC_ISR & ADC_ISR_ADRDY) == 0u) {}

    ael_mailbox_init();

    for (uint32_t i = 0u; i < 8u; ++i) {
        uint32_t sample = adc_read_once();
        sum += sample;
        AEL_MAILBOX->detail0 = sample;
        delay(40000u);
    }

    {
        uint32_t avg = sum / 8u;
        AEL_MAILBOX->detail0 = avg;
        if (avg == 0u) {
            ael_mailbox_fail(0xE241u, avg);
            while (1) {}
        }
        if (avg >= 4095u) {
            ael_mailbox_fail(0xE242u, avg);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {
        delay(200000u);
        AEL_MAILBOX->detail0++;
    }
}
