#include <stdint.h>

#define RCC_BASE 0x40021000u
#define RCC_AHBENR (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define GPIOC_BASE 0x48000800u
#define GPIOC_MODER (*(volatile uint32_t *)(GPIOC_BASE + 0x00u))
#define GPIOC_ODR (*(volatile uint32_t *)(GPIOC_BASE + 0x14u))

#define SYSTICK_BASE 0xE000E010u
#define SYST_CSR (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))

#define RCC_GPIOCEN (1u << 19)
#define SYST_CSR_ENABLE (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static inline void gpio_set_output(volatile uint32_t *moder, uint32_t pin) {
    const uint32_t shift = pin * 2u;
    *moder &= ~(0x3u << shift);
    *moder |= (0x1u << shift);
}

int main(void) {
    RCC_AHBENR |= RCC_GPIOCEN;
    gpio_set_output(&GPIOC_MODER, 13u);

    /* Common blue-pill style boards wire the LED active-low on PC13. */
    GPIOC_ODR |= (1u << 13);

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    uint32_t led_ms = 0;
    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) != 0u) {
            led_ms += 1u;
        }
        if (led_ms >= 250u) {
            led_ms = 0u;
            GPIOC_ODR ^= (1u << 13);
        }
    }
}
