#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"

void SystemInit(void) {}

/* Tune this value to change blink speed */
#define DELAY_COUNT  2000000u

int main(void)
{
    ael_mailbox_init();

    RCC->APB2PCENR |= RCC_IOPAEN;

    /* PA1 and PA2: push-pull output 50 MHz */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFFu << 4)) | (0x33u << 4);

    /* Start both LEDs off */
    GPIOA->OUTDR &= ~((1u << 1) | (1u << 2));

    ael_mailbox_pass();

    while (1)
    {
        for (volatile uint32_t i = 0; i < DELAY_COUNT; i++);
        GPIOA->OUTDR ^= (1u << 1);   /* toggle PA1 */

        for (volatile uint32_t i = 0; i < DELAY_COUNT; i++);
        GPIOA->OUTDR ^= (1u << 2);   /* toggle PA2 */
    }

    return 0;
}
