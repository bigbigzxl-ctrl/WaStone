#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20u))

#define SPI1_BASE       0x40013000u
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00u))
#define SPI1_CR2        (*(volatile uint32_t *)(SPI1_BASE + 0x04u))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08u))
#define SPI1_DR8        (*(volatile uint8_t *)(SPI1_BASE + 0x0Cu))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_SPI1EN      (1u << 12)

#define SPI_CR1_MSTR      (1u << 2)
#define SPI_CR1_BR_DIV16  (0x3u << 3)
#define SPI_CR1_SPE       (1u << 6)
#define SPI_CR1_SSI       (1u << 8)
#define SPI_CR1_SSM       (1u << 9)

#define SPI_CR2_DS_8BIT   (0x7u << 8)
#define SPI_CR2_FRXTH     (1u << 12)

#define SPI_SR_RXNE       (1u << 0)
#define SPI_SR_TXE        (1u << 1)
#define SPI_SR_BSY        (1u << 7)

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

static void spi1_init(void)
{
    GPIOA_MODER &= ~((0x3u << 10u) | (0x3u << 12u) | (0x3u << 14u));
    GPIOA_MODER |=  (0x2u << 10u) | (0x2u << 12u) | (0x2u << 14u);

    GPIOA_AFRL &= ~((0xFu << 20u) | (0xFu << 24u) | (0xFu << 28u));
    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV16 | SPI_CR1_SSI | SPI_CR1_SSM;
    SPI1_CR2 = SPI_CR2_DS_8BIT | SPI_CR2_FRXTH;
    SPI1_CR1 |= SPI_CR1_SPE;
}

static uint8_t spi1_transfer(uint8_t tx, uint8_t *rx)
{
    uint32_t timeout = 100000u;

    while ((SPI1_SR & SPI_SR_RXNE) != 0u) {
        (void)SPI1_DR8;
    }

    while (((SPI1_SR & SPI_SR_TXE) == 0u) && timeout-- > 0u) {}
    if ((SPI1_SR & SPI_SR_TXE) == 0u) {
        return 0u;
    }

    SPI1_DR8 = tx;

    timeout = 100000u;
    while (((SPI1_SR & SPI_SR_RXNE) == 0u) && timeout-- > 0u) {}
    if ((SPI1_SR & SPI_SR_RXNE) == 0u) {
        return 0u;
    }

    *rx = SPI1_DR8;
    while (((SPI1_SR & SPI_SR_BSY) != 0u) && timeout-- > 0u) {}
    return 1u;
}

int main(void)
{
    static const uint8_t patterns[] = {0x55u, 0xA5u, 0x3Cu, 0xC3u, 0x96u, 0x69u};

    RCC_AHBENR |= RCC_GPIOAEN;
    RCC_APB2ENR |= RCC_SPI1EN;

    spi1_init();
    ael_mailbox_init();

    for (uint32_t i = 0u; i < (sizeof(patterns) / sizeof(patterns[0])); ++i) {
        uint8_t tx = patterns[i];
        uint8_t rx = 0u;

        if (spi1_transfer(tx, &rx) == 0u) {
            ael_mailbox_fail(0xE141u, i);
            while (1) {}
        }
        if (rx != tx) {
            ael_mailbox_fail(0xE142u, ((uint32_t)tx << 8u) | rx);
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
