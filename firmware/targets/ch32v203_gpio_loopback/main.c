/* ch32v203_gpio_loopback — PA2→PA3 output drive, PA3 read back */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"
static void delay_us(uint32_t us) { for (uint32_t i=0;i<us*72;i++) __asm__("nop"); }
int main(void)
{
    ael_mailbox_init();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;
    /* PA2: output PP 50MHz, PA3: input floating */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFF << 8))
                 | (0x3 << 8)   /* PA2: OUT_PP 50MHz */
                 | (0x4 << 12); /* PA3: IN_FLOAT */
    uint32_t err = 0;
    /* Drive HIGH, read PA3 */
    GPIOA->BSHR = (1 << 2); delay_us(10);
    if (!(GPIOA->INDR & (1 << 3))) err |= 1;
    /* Drive LOW, read PA3 */
    GPIOA->BCR = (1 << 2); delay_us(10);
    if (GPIOA->INDR & (1 << 3)) err |= 2;
    if (err) { ael_mailbox_fail(err, 0); }
    else { ael_mailbox_pass(); }
    uint32_t tick = 0;
    while (1) {
        GPIOA->BSHR = (1<<2); delay_us(500000/72);
        GPIOA->BCR  = (1<<2); delay_us(500000/72);
        AEL_MAILBOX->detail0 = ++tick;
    }
}
