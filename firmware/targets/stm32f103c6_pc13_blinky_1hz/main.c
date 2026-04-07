/* STM32F103C6T6 Blue Pill — PC13 LED blink at 1 Hz (500 ms on / 500 ms off)
 * Clock: 8 MHz HSI (reset default)
 * SysTick: 1 ms period (8000 counts)
 * PC13 is active-low (LED ON when pin = 0)
 */
#include <stdint.h>

#define RCC_BASE           0x40021000u
#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB2ENR_IOPCEN (1u << 4)

#define GPIOC_BASE         0x40011000u
#define GPIOC_CRH          (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR          (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define SYST_CSR           (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR           (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR           (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static void delay_ms(uint32_t ms)
{
    while (ms-- > 0u) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

void HardFault_Handler(void)
{
    /* Assert SYSRESETREQ so SWD does not lock up */
    volatile uint32_t *aircr = (volatile uint32_t *)0xE000ED0Cu;
    *aircr = 0x05FA0004u;
    while (1) {}
}

int main(void)
{
    /* SysTick: 1 ms tick at 8 MHz HSI */
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Enable GPIOC clock */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* PC13: output push-pull 2 MHz (CRH bits [23:20] = 0b0010) */
    GPIOC_CRH &= ~(0xFu << 20u);
    GPIOC_CRH |=  (0x2u << 20u);

    /* LED off initially (PC13 active-low) */
    GPIOC_ODR |= (1u << 13);

    while (1) {
        GPIOC_ODR &= ~(1u << 13);   /* LED ON  */
        delay_ms(1000u);
        GPIOC_ODR |=  (1u << 13);   /* LED OFF */
        delay_ms(1000u);
    }
}
