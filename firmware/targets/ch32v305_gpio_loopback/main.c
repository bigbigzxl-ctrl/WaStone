/* ch32v305_gpio_loopback — PA1→PA2 output drive, PA2 read back
 * Board: CH32V305RBT6
 * Wiring: PA1 ↔ PA2 jumper
 * Clock: 96 MHz
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

static void delay_us(uint32_t us) { for (uint32_t i = 0; i < us * 96; i++) __asm__("nop"); }

int main(void)
{
    ael_mailbox_init();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;

    /* PA1: output PP 2MHz (CFGLR bits[7:4] = 0x2)
     * PA2: input floating (CFGLR bits[11:8] = 0x4) */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFF << 4))
                 | (0x2u << 4)    /* PA1: OUT_PP 2MHz */
                 | (0x4u << 8);   /* PA2: IN_FLOAT */

    uint32_t err = 0;

    /* Drive HIGH, read PA2 */
    GPIOA->BSHR = (1u << 1); delay_us(10);
    if (!(GPIOA->INDR & (1u << 2))) err |= 1;

    /* Drive LOW, read PA2 */
    GPIOA->BCR = (1u << 1); delay_us(10);
    if (GPIOA->INDR & (1u << 2)) err |= 2;

    if (err) { ael_mailbox_fail(err, 0); }
    else     { ael_mailbox_pass(); }

    uint32_t tick = 0;
    while (1) {
        GPIOA->BSHR = (1u << 1); delay_us(500000 / 96);
        GPIOA->BCR  = (1u << 1); delay_us(500000 / 96);
        AEL_MAILBOX->detail0 = ++tick;
    }
}
