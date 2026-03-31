#include <stdint.h>

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOC_BASE      0x40011000u
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define SYSTICK_BASE    0xE000E010u
#define SYST_CSR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))

#define RCC_IOPCEN          (1u << 4)
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

static void gpio_pc7_output_push_pull(void)
{
    const uint32_t shift = (7u - 4u) * 4u;
    GPIOC_CRH &= ~(0xFu << shift);
    GPIOC_CRH |=  (0x2u << shift); /* 2 MHz push-pull output */
}

int main(void)
{
    RCC_APB2ENR |= RCC_IOPCEN;
    gpio_pc7_output_push_pull();

    /* 1 kHz SysTick from 8 MHz HSI */
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    uint32_t led_ms = 0u;

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }
        if (++led_ms >= 250u) {
            led_ms = 0u;
            GPIOC_ODR ^= (1u << 7);
        }
    }
}
