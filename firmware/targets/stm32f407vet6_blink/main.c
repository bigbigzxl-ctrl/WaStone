/*
 * STM32F407VET6 — LED blink via PC0/PC1/PC2 resistor network
 *
 * LED circuit:
 *   3.3V → LED → node A
 *   node A → 2kΩ → GND  (always-on pull-down)
 *   node A → 2kΩ → PC0
 *   node A → 2kΩ → PC1
 *   node A → 2kΩ → PC2
 *
 * Logic: PC0/PC1/PC2 LOW  → A pulled to GND → LED ON  (~2.6 mA)
 *        PC0/PC1/PC2 HIGH → A pulled to 3.3V → LED OFF (V_LED < Vf)
 */

#include <stdint.h>

/* RCC */
#define RCC_BASE       0x40023800U
#define RCC_AHB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x30U))

/* GPIOC */
#define GPIOC_BASE     0x40020800U
#define GPIOC_MODER    (*(volatile uint32_t *)(GPIOC_BASE + 0x00U))
#define GPIOC_ODR      (*(volatile uint32_t *)(GPIOC_BASE + 0x14U))
#define GPIOC_BSRR     (*(volatile uint32_t *)(GPIOC_BASE + 0x18U))

#define LED_PINS       ((1U << 0) | (1U << 1) | (1U << 2))  /* PC0, PC1, PC2 */

/* ~500 ms delay at 16 MHz HSI (≈4 cycles/nop iteration) */
#define DELAY_250MS    1000000U

static void delay(uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
    }
}

static inline void led_on(void)
{
    /* BSRR high word = reset (set to 0) → LOW output → LED ON */
    GPIOC_BSRR = (LED_PINS << 16);
}

static inline void led_off(void)
{
    /* BSRR low word = set (set to 1) → HIGH output → LED OFF */
    GPIOC_BSRR = LED_PINS;
}

int main(void)
{
    /* Enable GPIOC clock (bit 2 of AHB1ENR) */
    RCC_AHB1ENR |= (1U << 2);
    __asm__ volatile ("nop"); /* 2-cycle AHB clock enable delay */
    __asm__ volatile ("nop");

    /* PC0/PC1/PC2: output push-pull (MODER bits = 01) */
    GPIOC_MODER &= ~(0x3FU);          /* clear bits [5:0] */
    GPIOC_MODER |=  (0x15U);          /* 01 01 01 for PC2/PC1/PC0 */

    /* Start with LED OFF */
    led_off();

    while (1) {
        led_on();
        delay(DELAY_250MS);
        led_off();
        delay(DELAY_250MS);
    }

    return 0;
}
