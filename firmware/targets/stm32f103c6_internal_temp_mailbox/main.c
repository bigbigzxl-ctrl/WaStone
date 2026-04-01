#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define ADC1_BASE       0x40012400u
#define ADC1_SR         (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR1        (*(volatile uint32_t *)(ADC1_BASE + 0x04u))
#define ADC1_CR2        (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_SMPR1      (*(volatile uint32_t *)(ADC1_BASE + 0x0Cu))
#define ADC1_SQR1       (*(volatile uint32_t *)(ADC1_BASE + 0x2Cu))
#define ADC1_SQR3       (*(volatile uint32_t *)(ADC1_BASE + 0x34u))
#define ADC1_DR         (*(volatile uint32_t *)(ADC1_BASE + 0x4Cu))

#define RCC_ADC1EN             (1u << 9)
#define ADC_CR2_ADON           (1u << 0)
#define ADC_CR2_CAL            (1u << 2)
#define ADC_CR2_RSTCAL         (1u << 3)
#define ADC_CR2_EXTTRIG        (1u << 20)
#define ADC_CR2_SWSTART        (1u << 22)
#define ADC_CR2_TSVREFE        (1u << 23)
#define ADC_CR2_EXTSEL_SWSTART (0x7u << 17)
#define ADC_SR_EOC             (1u << 1)

#define ERR_ADC_TIMEOUT 0x10u
#define ERR_SAMPLE_ZERO 0x20u
#define ERR_SAMPLE_SAT  0x21u

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static void adc1_init_internal_temp(void)
{
    RCC_APB2ENR |= RCC_ADC1EN;
    (void)RCC_APB2ENR;

    ADC1_CR1 = 0u;
    ADC1_SQR1 = 0u;
    ADC1_SQR3 = 16u;
    ADC1_SMPR1 &= ~(0x7u << 18u);
    ADC1_SMPR1 |=  (0x7u << 18u);

    ADC1_CR2 = ADC_CR2_ADON | ADC_CR2_TSVREFE;
    delay_cycles(10000u);
    ADC1_CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1_CR2 & ADC_CR2_RSTCAL) != 0u) {}
    ADC1_CR2 |= ADC_CR2_CAL;
    while ((ADC1_CR2 & ADC_CR2_CAL) != 0u) {}
    delay_cycles(200000u);
}

static uint8_t adc1_read(uint16_t *value_out)
{
    uint32_t timeout = 500000u;

    ADC1_SR = 0u;
    ADC1_CR2 &= ~(0x7u << 17u);
    ADC1_CR2 |= ADC_CR2_EXTSEL_SWSTART;
    ADC1_CR2 |= ADC_CR2_ADON;
    ADC1_CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_SWSTART;
    while (((ADC1_SR & ADC_SR_EOC) == 0u) && timeout-- > 0u) {
    }
    if ((ADC1_SR & ADC_SR_EOC) == 0u) {
        return 0u;
    }

    *value_out = (uint16_t)(ADC1_DR & 0xFFFFu);
    return 1u;
}

int main(void)
{
    uint32_t sum = 0u;

    ael_mailbox_init();
    adc1_init_internal_temp();

    for (uint32_t i = 0u; i < 8u; ++i) {
        uint16_t sample = 0u;
        if (adc1_read(&sample) == 0u) {
            ael_mailbox_fail(ERR_ADC_TIMEOUT, i);
            while (1) {}
        }
        sum += sample;
        AEL_MAILBOX->detail0 = sample;
        delay_cycles(50000u);
    }

    {
        const uint16_t avg = (uint16_t)(sum / 8u);
        AEL_MAILBOX->detail0 = avg;
        if (avg == 0u) {
            ael_mailbox_fail(ERR_SAMPLE_ZERO, avg);
            while (1) {}
        }
        if (avg >= 4095u) {
            ael_mailbox_fail(ERR_SAMPLE_SAT, avg);
            while (1) {}
        }
    }

    ael_mailbox_pass();

    while (1) {
        AEL_MAILBOX->detail0 ^= 0x00010000u;
        delay_cycles(200000u);
    }
}
