#include <stdint.h>
#include "../ael_mailbox.h"

/* STM32G431 — RCC */
#define RCC_BASE       0x40021000u
#define RCC_AHB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x4Cu))

/* GPIOA */
#define GPIOA_BASE     0x48000000u
#define GPIOA_MODER    (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_PUPDR    (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define GPIOA_OSPEEDR  (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_OTYPER   (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_ODR      (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

/* DAC1 (AHB2) — base 0x50000800 */
#define DAC1_BASE      0x50000800u
#define DAC1_CR        (*(volatile uint32_t *)(DAC1_BASE + 0x00u))
#define DAC1_DHR12R1   (*(volatile uint32_t *)(DAC1_BASE + 0x08u))
#define DAC1_DOR1      (*(volatile uint32_t *)(DAC1_BASE + 0x2Cu))

/* ADC2 (AHB2) — base 0x50000100 */
#define ADC2_BASE      0x50000100u
#define ADC2_ISR       (*(volatile uint32_t *)(ADC2_BASE + 0x00u))
#define ADC2_CR        (*(volatile uint32_t *)(ADC2_BASE + 0x08u))
#define ADC2_CFGR      (*(volatile uint32_t *)(ADC2_BASE + 0x0Cu))
#define ADC2_SMPR2     (*(volatile uint32_t *)(ADC2_BASE + 0x18u))
#define ADC2_SQR1      (*(volatile uint32_t *)(ADC2_BASE + 0x30u))
#define ADC2_DR        (*(volatile uint32_t *)(ADC2_BASE + 0x40u))

/* ADC12 common registers (shared CCR at 0x50000308) */
#define ADC12_CCR      (*(volatile uint32_t *)0x50000308u)

/* SysTick */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)

/* DAC output levels (12-bit: 0-4095, Vref=3.3 V)
 * HIGH: 3723 ≈ 3.0 V → ADC should read > 2500 counts
 * LOW:  372  ≈ 0.3 V → ADC should read < 1000 counts
 */
#define DAC_HIGH_CODE  3723u
#define DAC_LOW_CODE    372u
#define ADC_HIGH_THRESHOLD  2500u
#define ADC_LOW_THRESHOLD   1000u

static void delay_us(uint32_t us)
{
    /* Rough ~16-cycle loop at 16 MHz ≈ 1 us per iteration */
    volatile uint32_t n = us * 16u;
    while (n-- > 0u) { (void)n; }
}

static void adc2_init(void)
{
    /* ADC12 clock: CKMODE=01 (HCLK/1), set before ADVREGEN */
    ADC12_CCR |= (1u << 16);

    /* ADC2: exit deep power down, enable voltage regulator */
    ADC2_CR &= ~(1u << 29);   /* clear DEEPPWD */
    ADC2_CR |=  (1u << 28);   /* ADVREGEN */
    delay_us(30u);             /* regulator startup ≥20 us */

    /* Calibration (single-ended, ADCALDIF=0) */
    ADC2_CR |= (1u << 31);    /* ADCAL */
    while ((ADC2_CR & (1u << 31)) != 0u) {}

    /* Enable ADC2 */
    ADC2_CR |= (1u << 0);     /* ADEN */
    while ((ADC2_ISR & (1u << 0)) == 0u) {}  /* wait ADRDY */

    /* CH17: SMPR2 bits[23:21] = 111 (640.5 cycles) */
    ADC2_SMPR2 |= (7u << 21);

    /* Single conversion, SQ1=17 */
    ADC2_SQR1  = (17u << 6);
    ADC2_CFGR  = 0u;          /* single mode, right-aligned */
}

static uint8_t adc2_read(uint16_t *value_out)
{
    uint32_t timeout = 200000u;

    ADC2_ISR |= (1u << 2);          /* clear EOC */
    ADC2_CR  |= (1u << 2);          /* ADSTART */
    while (((ADC2_ISR & (1u << 2)) == 0u) && timeout-- > 0u) {}
    if ((ADC2_ISR & (1u << 2)) == 0u) { return 0u; }

    *value_out = (uint16_t)(ADC2_DR & 0xFFFFu);
    return 1u;
}

int main(void)
{
    uint16_t adc_high = 0u;
    uint16_t adc_low  = 0u;
    uint8_t ok_high;
    uint8_t ok_low;

    /* Enable GPIOA, DAC1 (bit16), ADC12 (bit13) on AHB2 */
    RCC_AHB2ENR |= (1u << 0) | (1u << 13) | (1u << 16);
    (void)RCC_AHB2ENR;

    /* PA4 = DAC1_OUT1 / ADC2_IN17: configure as analog (MODER=11, PUPDR=00) */
    GPIOA_MODER |=  (3u << 8);   /* PA4 bits[9:8] = 11 */
    GPIOA_PUPDR &= ~(3u << 8);   /* PA4 no pull */

    /* PA2 = signal output (LOW initially) */
    GPIOA_MODER  &= ~(3u << 4);
    GPIOA_MODER  |=  (1u << 4);
    GPIOA_OTYPER &= ~(1u << 2);
    GPIOA_OSPEEDR|=  (3u << 4);
    GPIOA_ODR    &= ~(1u << 2);

    /* DAC1 CH1: enable, no trigger (TEN1=0), output buffer enabled by default */
    DAC1_CR = (1u << 0);          /* EN1 */
    delay_us(10u);                 /* DAC startup */

    /* Initialise ADC2 */
    adc2_init();

    ael_mailbox_init();

    /* --- Test HIGH: write 3723 to DAC, read ADC --- */
    DAC1_DHR12R1 = DAC_HIGH_CODE;
    delay_us(200u);               /* settling time */
    ok_high = adc2_read(&adc_high);

    /* --- Test LOW: write 372 to DAC, read ADC --- */
    DAC1_DHR12R1 = DAC_LOW_CODE;
    delay_us(200u);
    ok_low = adc2_read(&adc_low);

    /* Evaluate */
    if (ok_high == 0u || ok_low == 0u) {
        ael_mailbox_fail(0xE001u, (uint32_t)(ok_high ? 1u : 0u));
    } else if (adc_high < ADC_HIGH_THRESHOLD) {
        ael_mailbox_fail(0xE002u, (uint32_t)adc_high);
    } else if (adc_low >= ADC_LOW_THRESHOLD) {
        ael_mailbox_fail(0xE003u, (uint32_t)adc_low);
    } else {
        ael_mailbox_pass();
        GPIOA_ODR |= (1u << 2);  /* PA2 HIGH on pass */
    }

    while (1) {}
}
