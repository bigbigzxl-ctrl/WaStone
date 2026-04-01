#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE      0x40010800u
#define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))

#define SPI1_BASE       0x40013000u
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00u))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08u))
#define SPI1_DR         (*(volatile uint32_t *)(SPI1_BASE + 0x0Cu))

#define RCC_IOPAEN      (1u << 2)
#define RCC_SPI1EN      (1u << 12)

#define SPI_CR1_CPHA      (1u << 0)
#define SPI_CR1_CPOL      (1u << 1)
#define SPI_CR1_MSTR      (1u << 2)
#define SPI_CR1_BR_DIV16  (0x3u << 3)
#define SPI_CR1_SPE       (1u << 6)
#define SPI_CR1_LSBFIRST  (1u << 7)
#define SPI_CR1_SSI       (1u << 8)
#define SPI_CR1_SSM       (1u << 9)

#define SPI_SR_RXNE       (1u << 0)
#define SPI_SR_TXE        (1u << 1)
#define SPI_SR_BSY        (1u << 7)

#define ERR_TIMEOUT       0x81u
#define ERR_MISMATCH      0x82u

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static void spi1_init(void)
{
    /*
     * PA5 SCK  = AF push-pull 50MHz -> 0xB
     * PA6 MISO = input floating     -> 0x4
     * PA7 MOSI = AF push-pull 50MHz -> 0xB
     */
    GPIOA_CRL &= ~(0xFFFu << 20u);
    GPIOA_CRL |=  (0xB4Bu << 20u);

    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV16 | SPI_CR1_SSI | SPI_CR1_SSM;
    SPI1_CR1 |= SPI_CR1_SPE;
}

static uint8_t spi1_transfer(uint8_t tx, uint8_t *rx)
{
    uint32_t timeout = 100000u;

    while ((SPI1_SR & SPI_SR_RXNE) != 0u) {
        (void)SPI1_DR;
    }

    while (((SPI1_SR & SPI_SR_TXE) == 0u) && timeout-- > 0u) {}
    if ((SPI1_SR & SPI_SR_TXE) == 0u) {
        return 0u;
    }

    SPI1_DR = tx;

    timeout = 100000u;
    while (((SPI1_SR & SPI_SR_RXNE) == 0u) && timeout-- > 0u) {}
    if ((SPI1_SR & SPI_SR_RXNE) == 0u) {
        return 0u;
    }

    *rx = (uint8_t)(SPI1_DR & 0xFFu);

    timeout = 100000u;
    while (((SPI1_SR & SPI_SR_BSY) != 0u) && timeout-- > 0u) {}
    return 1u;
}

int main(void)
{
    static const uint8_t patterns[] = {0x55u, 0xA5u, 0x3Cu, 0xC3u, 0x96u, 0x69u};

    RCC_APB2ENR |= (RCC_IOPAEN | RCC_SPI1EN);
    (void)RCC_APB2ENR;

    spi1_init();
    ael_mailbox_init();

    for (uint32_t i = 0u; i < (sizeof(patterns) / sizeof(patterns[0])); ++i) {
        uint8_t rx = 0u;
        uint8_t tx = patterns[i];

        if (!spi1_transfer(tx, &rx)) {
            ael_mailbox_fail(ERR_TIMEOUT, i);
            while (1) {}
        }
        if (rx != tx) {
            ael_mailbox_fail(ERR_MISMATCH, ((uint32_t)tx << 8u) | rx);
            while (1) {}
        }

        AEL_MAILBOX->detail0 = ((uint32_t)i << 16u) | rx;
        delay_cycles(12000u);
    }

    ael_mailbox_pass();
    while (1) {
        AEL_MAILBOX->detail0++;
        delay_cycles(40000u);
    }
}
