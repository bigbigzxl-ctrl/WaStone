/* ch32v203_spi_loopback — SPI1 MOSI(PA7)→MISO(PA6) loopback */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"
int main(void)
{
    ael_mailbox_init();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1;
    /* PA5: SCK AF_PP, PA6: MISO IN_FLOAT, PA7: MOSI AF_PP */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFFF<<20))
                 | (0xB<<20) | (0x4<<24) | (0xB<<28);
    /* SPI1: master, /8, CPOL=0 CPHA=0, 8-bit, SSI+SSM */
    SPI1->CTLR1 = SPI_CTLR1_MSTR | SPI_CTLR1_BR_1 | SPI_CTLR1_SSM | SPI_CTLR1_SSI | SPI_CTLR1_SPE;
    uint8_t pattern[] = {0xA5, 0x5A, 0x55, 0xAA};
    uint32_t err = 0;
    for (int i = 0; i < 4; i++) {
        while (!(SPI1->STATR & SPI_STATR_TXE));
        SPI1->DATAR = pattern[i];
        uint32_t t = 100000;
        while (!(SPI1->STATR & SPI_STATR_RXNE) && --t);
        if (!t || (SPI1->DATAR & 0xFF) != pattern[i]) err |= (1<<i);
    }
    if (err) { ael_mailbox_fail(err, 0); }
    else { ael_mailbox_pass(); }
    uint32_t tick = 0;
    while (1) { AEL_MAILBOX->detail0 = ++tick; for (volatile int i=0;i<720000;i++); }
}
