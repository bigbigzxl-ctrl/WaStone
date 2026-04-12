/* ch32v305_blinky — PA3 LED blink, program_only (no mailbox)
 *
 * Board: CH32V305RBT6
 * LED: PA3 push-pull, LOW=ON (active-low)
 * Clock: 96 MHz HSE (system_ch32v30x.c default)
 */
#include "ch32v30x.h"

static void delay_ms(uint32_t ms)
{
    /* ~96 MHz: rough busy-wait (~9600 cycles/ms) */
    for (uint32_t i = 0; i < ms * 9600; i++)
        __asm__("nop");
}

int main(void)
{
    /* Enable GPIOA clock */
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;

    /* PA3: output push-pull 2 MHz (CFGLR bits [15:12] = 0x2) */
    GPIOA->CFGLR &= ~(0xF << 12);
    GPIOA->CFGLR |=  (0x2 << 12);

    while (1) {
        GPIOA->BSHR = (1u << 19);   /* PA3 LOW  → LED ON  */
        delay_ms(500);
        GPIOA->BSHR = (1u << 3);    /* PA3 HIGH → LED OFF */
        delay_ms(500);
    }
}
