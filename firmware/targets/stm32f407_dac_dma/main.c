/*
 * STM32F407 — DAC DMA Waveform + ADC Verification Test
 *
 * DMA2 Stream5 Channel7 drives DAC1 (PA4) with a stepped waveform from SRAM.
 * ADC1 CH4 (PA4) samples the output. Circular DMA loads DAC with 4 levels:
 *   0x000, 0x555, 0xAAA, 0xFFF (0%, 33%, 66%, 100% of 3.3V)
 *
 * DAC: 12-bit right-aligned (DHR12R1 at 0x40007408), BOFF1=1 (buffer off)
 * DMA2 Stream5 CH7: MEM→DAC, circular, 16-bit (halfword), MINC=1
 * TIM6 triggers DAC (TRGO), 1kHz update rate from 16MHz APB1
 * ADC1: single conversion CH4, SMP=7 (480 cycles)
 *
 * After each DAC level settles, read ADC, verify within ±10% of expected.
 *
 * Wiring: PA4 = DAC1_OUT = ADC1_IN4 (same pin, self-loopback)
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = last ADC reading (~4095 at 100%)
 *   FAIL: error_code=1 ADC timeout
 *         error_code=2 level mismatch; detail0=ADC reading at bad level
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---- */
#define RCC_BASE     0x40023800u
#define RCC_AHB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x30u))
#define RCC_AHB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x34u))
#define RCC_APB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x40u))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x44u))

/* ---- GPIOA ---- */
#define GPIOA_BASE   0x40020000u
#define GPIOA_MODER  (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))

/* ---- DAC ---- */
#define DAC_BASE     0x40007400u
#define DAC_CR       (*(volatile uint32_t *)(DAC_BASE + 0x00u))
#define DAC_DHR12R1  (*(volatile uint32_t *)(DAC_BASE + 0x08u))

/* ---- TIM6 ---- */
#define TIM6_BASE    0x40001000u
#define TIM6_CR1     (*(volatile uint32_t *)(TIM6_BASE + 0x00u))
#define TIM6_CR2     (*(volatile uint32_t *)(TIM6_BASE + 0x04u))
#define TIM6_DIER    (*(volatile uint32_t *)(TIM6_BASE + 0x0Cu))
#define TIM6_EGR     (*(volatile uint32_t *)(TIM6_BASE + 0x14u))
#define TIM6_PSC     (*(volatile uint32_t *)(TIM6_BASE + 0x28u))
#define TIM6_ARR     (*(volatile uint32_t *)(TIM6_BASE + 0x2Cu))

/* ---- DMA2 ---- */
#define DMA2_BASE    0x40026400u
#define DMA2_HISR    (*(volatile uint32_t *)(DMA2_BASE + 0x04u))
#define DMA2_HIFCR   (*(volatile uint32_t *)(DMA2_BASE + 0x0Cu))
/* Stream5: base + 0x88 */
#define DMA2_S5CR    (*(volatile uint32_t *)(DMA2_BASE + 0x88u))
#define DMA2_S5NDTR  (*(volatile uint32_t *)(DMA2_BASE + 0x8Cu))
#define DMA2_S5PAR   (*(volatile uint32_t *)(DMA2_BASE + 0x90u))
#define DMA2_S5M0AR  (*(volatile uint32_t *)(DMA2_BASE + 0x94u))

/* ---- ADC1 ---- */
#define ADC1_BASE    0x40012000u
#define ADC_CCR      (*(volatile uint32_t *)(0x40012300u))
#define ADC1_SR      (*(volatile uint32_t *)(ADC1_BASE + 0x00u))
#define ADC1_CR1     (*(volatile uint32_t *)(ADC1_BASE + 0x04u))
#define ADC1_CR2     (*(volatile uint32_t *)(ADC1_BASE + 0x08u))
#define ADC1_SMPR2   (*(volatile uint32_t *)(ADC1_BASE + 0x14u))
#define ADC1_SQR3    (*(volatile uint32_t *)(ADC1_BASE + 0x34u))
#define ADC1_DR      (*(volatile uint32_t *)(ADC1_BASE + 0x4Cu))

#define ADC_SR_EOC   (1u << 1)
#define ADC_CR2_ADON (1u << 0)
#define ADC_CR2_SWST (1u << 30)

static volatile uint16_t dac_wave[4] = {0x000u, 0x555u, 0xAAAu, 0xFFFu};
static const uint32_t    dac_expect[4] = {0u, 1365u, 2730u, 4095u};

static void delay_us(uint32_t us)
{
    /* ~16 cycles/iteration at 16MHz ≈ 1µs; crude but sufficient */
    volatile uint32_t n = us * 16u;
    while (n--) {}
}

static uint32_t adc_read(void)
{
    ADC1_CR2 |= ADC_CR2_SWST;
    uint32_t t = 200000u;
    while (!(ADC1_SR & ADC_SR_EOC)) {
        if (--t == 0u) return 0xFFFFFFFFu;
    }
    return ADC1_DR;
}

int main(void)
{
    ael_mailbox_init();

    /* ---- Clocks ---- */
    RCC_AHB1ENR |= (1u << 0)  /* GPIOAEN */
                |  (1u << 22); /* DMA2EN */
    RCC_AHB2ENR |= (1u << 0); /* ADCEN (ADC1..3) */
    RCC_APB1ENR |= (1u << 29) /* DACEN */
                |  (1u << 4); /* TIM6EN */
    (void)RCC_APB1ENR;

    /* ---- PA4: analog mode (MODER[9:8] = 11) ---- */
    GPIOA_MODER |= (3u << 8);

    /* ---- TIM6: 1kHz TRGO from 16MHz APB1
     * PSC=15 → 1MHz tick; ARR=999 → 1kHz
     * CR2.MMS=010 (Update event as TRGO)
     */
    TIM6_PSC = 15u;
    TIM6_ARR = 999u;
    TIM6_CR2 = (2u << 4); /* MMS=010: Update→TRGO */
    TIM6_EGR = (1u << 0); /* UG: apply PSC */

    /* ---- DAC1: TIM6 TRGO trigger, BOFF1=1, DMAEN1=1
     * CR[2:3]: TSEL1=000 (TIM6 TRGO)
     * CR[5]:   TEN1=1
     * CR[1]:   BOFF1=1 (output buffer off)
     * CR[12]:  DMAEN1=1
     * CR[0]:   EN1=1
     */
    DAC_CR = (1u << 12)  /* DMAEN1 */
           | (0u << 3)   /* TSEL1=000=TIM6 */
           | (1u << 2)   /* TEN1 */
           | (1u << 1)   /* BOFF1 */
           | (1u << 0);  /* EN1 */

    /* ---- DMA2 Stream5 CH7: MEM→DAC, circular, halfword ----
     * DAC_DHR12R1 = 0x40007408
     * CHSEL=7(<<25), DIR=M2P(1<<6), CIRC(1<<8), MINC(1<<10)
     * PSIZE=halfword(1<<11), MSIZE=halfword(1<<13)
     */
    DMA2_S5CR  = 0u;
    DMA2_HIFCR = 0xFC0u; /* clear stream5 flags (bits[11:6]) */
    DMA2_S5PAR  = (uint32_t)&DAC_DHR12R1;
    DMA2_S5M0AR = (uint32_t)dac_wave;
    DMA2_S5NDTR = 4u;
    DMA2_S5CR = (7u << 25)  /* CHSEL=7 */
              | (1u << 13)  /* MSIZE=halfword */
              | (1u << 11)  /* PSIZE=halfword */
              | (1u << 10)  /* MINC */
              | (1u << 8)   /* CIRC */
              | (1u << 6);  /* DIR=M2P */
    DMA2_S5CR |= (1u << 0); /* EN */

    /* Start TIM6 */
    TIM6_CR1 = (1u << 0); /* CEN */

    /* ---- ADC1: CH4, SMP=7 (480 cycles), independent, no prescaler ---- */
    ADC_CCR    = (1u << 16); /* ADCPRE=00 (PCLK2/2), but set TSVREFE */
    ADC1_CR1   = 0u;
    ADC1_CR2   = ADC_CR2_ADON;
    ADC1_SMPR2 = (7u << 12); /* SMP4=7 (480 cycles) */
    ADC1_SQR3  = 4u;         /* first conversion = CH4 */
    delay_us(10u);            /* ADC stabilise */

    /* ---- Sample each of the 4 DAC levels ---- */
    uint32_t last_adc = 0u;
    for (uint32_t lvl = 0u; lvl < 4u; lvl++) {
        /* Wait for DMA to load this level into DAC (~1ms per step) */
        delay_us(3000u); /* 3ms: enough for 3 TIM6 triggers at 1kHz */

        uint32_t adc_val = adc_read();
        if (adc_val == 0xFFFFFFFFu) {
            ael_mailbox_fail(1u, lvl);
            while (1) {}
        }

        uint32_t exp = dac_expect[lvl];
        uint32_t tol = 410u; /* ±10% of 4095 */
        uint32_t diff = (adc_val > exp) ? (adc_val - exp) : (exp - adc_val);
        if (diff > tol) {
            ael_mailbox_fail(2u, adc_val);
            while (1) {}
        }
        last_adc = adc_val;
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = last_adc;
    while (1) {}
}
