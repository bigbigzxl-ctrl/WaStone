/*
 * stm32u585_spi_loopback_mailbox — SPI1 MOSI→MISO loopback test
 * STM32U585CIU6, MSI 4MHz
 *
 * Wire: PA7 (MOSI) → PA6 (MISO)   SCK=PA5
 *
 * STM32U5 "next-gen SPI" (H7-style IP) key findings:
 *   1. SSI must be set in CR1 BEFORE writing CFG2 — otherwise MASTER bit silently dropped
 *   2. Use TSIZE=0 (infinite mode) + CSUSP to stop — TSIZE=N has last-byte RXP edge case
 *   3. SPI1 must use secure alias 0x50013000 on this board
 *   4. Uses 8-bit TXDR/RXDR byte accesses to match DSIZE=7 (8-bit data)
 *
 * FAIL codes:
 *   0xE001 — TXP or RXP timeout
 *   0xE002 — data mismatch (detail0 = rx[0:3] packed)
 *   0xE011 — CFG2 MASTER bit not set after write (SSI must be high first)
 *   0xE012 — SPE not set after enable
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))
#define RCC_APB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x07Cu))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20u))

/* SPI1 — secure alias (0x50013000) required on STM32U585 */
#define SPI1_BASE       0x50013000u
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00u))
#define SPI1_CR2        (*(volatile uint32_t *)(SPI1_BASE + 0x04u))
#define SPI1_CFG1       (*(volatile uint32_t *)(SPI1_BASE + 0x08u))
#define SPI1_CFG2       (*(volatile uint32_t *)(SPI1_BASE + 0x0Cu))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x14u))
#define SPI1_IFCR       (*(volatile uint32_t *)(SPI1_BASE + 0x18u))
#define SPI1_TXDR8      (*((volatile uint8_t *)(SPI1_BASE + 0x20u)))
#define SPI1_RXDR8      (*((volatile uint8_t *)(SPI1_BASE + 0x30u)))

/* CR1 bits */
#define SPI_CR1_SPE     (1u << 0u)
#define SPI_CR1_CSTART  (1u << 9u)
#define SPI_CR1_CSUSP   (1u << 10u)
#define SPI_CR1_SSI     (1u << 12u)

/* CFG1: DSIZE[4:0]=7 (8-bit), MBR[30:28]=5 (÷64 ≈ 62.5kHz @ 4MHz MSI) */
#define SPI_CFG1_DSIZE7_MBR64  ((7u << 0u) | (5u << 28u))

/* CFG2 bits */
#define SPI_CFG2_MASTER   (1u << 22u)
#define SPI_CFG2_SSM      (1u << 26u)
#define SPI_CFG2_AFCNTR   (1u << 31u)

/* SR bits */
#define SPI_SR_RXP      (1u << 0u)
#define SPI_SR_TXP      (1u << 1u)
#define SPI_SR_SUSP     (1u << 11u)
#define SPI_SR_TXC      (1u << 12u)

#define TX_LEN    4u
#define TIMEOUT   1000000u

static const uint8_t tx_buf[TX_LEN] = {0xA5u, 0x5Au, 0xC3u, 0x3Cu};

int main(void)
{
    ael_mailbox_init();

    /* Enable clocks; hard-reset SPI1 to clear any residual state */
    RCC_AHB2ENR1 |= (1u << 0u);     /* GPIOAEN */
    RCC_APB2ENR  |= (1u << 12u);    /* SPI1EN */
    volatile uint32_t dummy = RCC_APB2ENR; (void)dummy;
    RCC_APB2RSTR |=  (1u << 12u);
    RCC_APB2RSTR &= ~(1u << 12u);
    dummy = RCC_APB2RSTR; (void)dummy;

    /* PA5=SCK(AF5), PA6=MISO(AF5), PA7=MOSI(AF5) */
    GPIOA_MODER  &= ~((3u << 10u) | (3u << 12u) | (3u << 14u));
    GPIOA_MODER  |=  ((2u << 10u) | (2u << 12u) | (2u << 14u));
    GPIOA_OTYPER &= ~((1u << 5u)  | (1u << 6u)  | (1u << 7u));
    GPIOA_OSPEEDR|=  ((3u << 10u) | (3u << 12u) | (3u << 14u));
    GPIOA_PUPDR  &= ~(3u << 12u);
    GPIOA_PUPDR  |=  (1u << 12u);   /* pull-up on MISO (PA6) */
    GPIOA_AFRL   &= ~((0xFu << 20u) | (0xFu << 24u) | (0xFu << 28u));
    GPIOA_AFRL   |=  ((5u   << 20u) | (5u   << 24u) | (5u   << 28u));

    /* Set SSI=1 in CR1 BEFORE writing CFG2 — required for MASTER bit to latch */
    SPI1_CR1  = SPI_CR1_SSI;
    SPI1_IFCR = 0xFFFFFFFFu;

    /* Configure: master, 8-bit, SSM, AFCNTR, MBR÷64 */
    SPI1_CFG2 = SPI_CFG2_MASTER | SPI_CFG2_SSM | SPI_CFG2_AFCNTR;
    SPI1_CFG1 = SPI_CFG1_DSIZE7_MBR64;
    {
        uint32_t cfg2_rb = SPI1_CFG2;
        if (!(cfg2_rb & SPI_CFG2_MASTER)) {
            AEL_MAILBOX->detail0 = cfg2_rb;
            ael_mailbox_fail(0xE011u, cfg2_rb);
            while (1) {}
        }
    }

    uint8_t rx_buf[TX_LEN];

    /* TSIZE=0 (infinite mode) avoids last-byte RXP edge case of TSIZE=N */
    SPI1_CR2 = 0u;
    SPI1_CR1 = SPI_CR1_SSI | SPI_CR1_SPE;
    {
        uint32_t cr1_rb = SPI1_CR1;
        if (!(cr1_rb & SPI_CR1_SPE)) {
            AEL_MAILBOX->detail0 = cr1_rb;
            ael_mailbox_fail(0xE012u, cr1_rb);
            while (1) {}
        }
    }
    SPI1_CR1 = SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART;

    for (uint32_t i = 0u; i < TX_LEN; i++) {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) { if (SPI1_SR & SPI_SR_TXP) break; }
        if (t >= TIMEOUT) {
            AEL_MAILBOX->detail0 = (SPI1_CR1 << 16u) | (uint32_t)(SPI1_SR & 0xFFFFu);
            ael_mailbox_fail(0xE001u, i); while (1) {}
        }
        SPI1_TXDR8 = tx_buf[i];

        for (t = 0u; t < TIMEOUT; t++) { if (SPI1_SR & SPI_SR_RXP) break; }
        if (t >= TIMEOUT) {
            AEL_MAILBOX->detail0 = (0x10u << 16u) | (i << 8u) | (uint32_t)(SPI1_SR & 0xFFu);
            ael_mailbox_fail(0xE001u, i); while (1) {}
        }
        rx_buf[i] = SPI1_RXDR8;
    }

    /* Suspend transfer and wait for idle */
    SPI1_CR1 = SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSUSP;
    { uint32_t t; for (t = 0u; t < TIMEOUT; t++) { if (SPI1_SR & SPI_SR_SUSP) break; } }
    { uint32_t t; for (t = 0u; t < TIMEOUT; t++) { if (SPI1_SR & SPI_SR_TXC)  break; } }

    /* Verify */
    AEL_MAILBOX->detail0 = ((uint32_t)rx_buf[0] << 24u) | ((uint32_t)rx_buf[1] << 16u) |
                           ((uint32_t)rx_buf[2] <<  8u) |  (uint32_t)rx_buf[3];
    for (uint32_t i = 0u; i < TX_LEN; i++) {
        if (rx_buf[i] != tx_buf[i]) {
            AEL_MAILBOX->error_code = 0xE002u;
            AEL_MAILBOX->status    = AEL_STATUS_FAIL;
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
