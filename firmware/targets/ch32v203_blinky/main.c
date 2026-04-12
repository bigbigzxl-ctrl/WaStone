/*
 * ch32v203_blinky — PA15 LED blink (nanoCH32V203 v1.1)
 * LED D1 (Blue): 3V3 → R3(10K) → anode → cathode → PA15
 * PA15 LOW = LED ON
 */
#include "ch32v20x.h"

static void delay_ms(uint32_t ms)
{
    /* ~72MHz: rough busy-wait */
    for (uint32_t i = 0; i < ms * 7200; i++) {
        __asm__("nop");
    }
}

int main(void)
{
    /* Enable GPIOA clock */
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;

    /* PA15: output push-pull 2MHz (CRH bits [31:28] = 0x2) */
    GPIOA->CFGHR &= ~(0xF << 28);
    GPIOA->CFGHR |=  (0x2 << 28);

    while (1) {
        GPIOA->BSHR = (1 << 31);  /* PA15 LOW  → LED ON  */
        delay_ms(500);
        GPIOA->BSHR = (1 << 15);  /* PA15 HIGH → LED OFF */
        delay_ms(500);
    }
}
