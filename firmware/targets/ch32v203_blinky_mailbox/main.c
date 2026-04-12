/* ch32v203_blinky_mailbox — PA15 LED blink + AEL mailbox (nanoCH32V203 v1.1)
 *
 * LED D1 (Blue): 3V3 → R3(10K) → PA15, LOW = ON
 * Blinks at 1 Hz. Writes mailbox PASS immediately after GPIO init,
 * then increments detail0 on each toggle.
 *
 * Mailbox addr: 0x20000600, detail0 increments every 500ms.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

static void delay_ms(uint32_t ms)
{
    /* ~72 MHz: rough busy-wait (~7200 cycles/ms) */
    for (uint32_t i = 0; i < ms * 7200; i++)
        __asm__("nop");
}

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOA clock */
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;

    /* PA15: output push-pull 2 MHz (CRH bits [31:28] = 0x2) */
    GPIOA->CFGHR &= ~(0xF << 28);
    GPIOA->CFGHR |=  (0x2 << 28);

    ael_mailbox_pass();

    uint32_t tick = 0;
    while (1) {
        GPIOA->BSHR = (1u << 31);   /* PA15 LOW  → LED ON  */
        delay_ms(500);
        GPIOA->BSHR = (1u << 15);   /* PA15 HIGH → LED OFF */
        delay_ms(500);
        AEL_MAILBOX->detail0 = ++tick;
    }
}
