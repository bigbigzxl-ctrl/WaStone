/*
 * STM32F407VET6 — SPI1 loopback: PA7 (MOSI) → PA6 (MISO), PA5 = SCK
 *
 * SPI1 master, 8-bit, mode 0, ~62 kHz (APB2/256 at 16 MHz HSI).
 * Sends {0xA5, 0x5A, 0xF0, 0x0F, 0x12, 0x34, 0x56, 0x78} and verifies echo.
 * PASS after all 8 bytes matched.
 * detail0 = matched byte count (0→8), then heartbeat after PASS.
 *
 * Wiring: PA7 (MOSI) → PA6 (MISO)   [PA5 = SCK, no loopback needed]
 * SPI1 on APB2 (AF5 for PA5/PA6/PA7).
 * Mailbox: 0x2001FC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* RCC */
#define RCC_BASE    0x40023800U
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x44U))

/* GPIOA */
#define GPIOA_BASE  0x40020000U
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_AFRL  (*(volatile uint32_t *)(GPIOA_BASE + 0x20U))

/* SPI1 on APB2 */
#define SPI1_BASE   0x40013000U
#define SPI1_CR1    (*(volatile uint32_t *)(SPI1_BASE + 0x00U))
#define SPI1_SR     (*(volatile uint32_t *)(SPI1_BASE + 0x08U))
#define SPI1_DR     (*(volatile uint32_t *)(SPI1_BASE + 0x0CU))

#define SPI_SR_RXNE (1U << 0)
#define SPI_SR_TXE  (1U << 1)
#define SPI_SR_BSY  (1U << 7)

/*
 * SPI1 CR1: MSTR=1, BR=111 (/256 → 62.5 kHz at 16 MHz APB2),
 *           SSM=1, SSI=1 (software NSS), SPE=1
 */
#define SPI1_CR1_VAL ((1U<<2)|(7U<<3)|(1U<<9)|(1U<<8))  /* MSTR,BR=111,SSM,SSI */
#define SPI_CR1_SPE  (1U << 6)

#define TIMEOUT 200000U

static uint32_t spi_xfer(uint8_t tx, uint8_t *rx_out)
{
    /* Drain stale RX */
    if (SPI1_SR & SPI_SR_RXNE) { (void)SPI1_DR; }

    uint32_t t = TIMEOUT;
    while (!(SPI1_SR & SPI_SR_TXE)) { if (!--t) return 0U; }
    SPI1_DR = tx;

    t = TIMEOUT;
    while (!(SPI1_SR & SPI_SR_RXNE)) { if (!--t) return 0U; }
    *rx_out = (uint8_t)(SPI1_DR & 0xFFU);

    t = TIMEOUT;
    while (SPI1_SR & SPI_SR_BSY) { if (!--t) return 0U; }
    return 1U;
}

int main(void)
{
    /* Enable GPIOA (AHB1 bit 0) + SPI1 (APB2 bit 12) */
    RCC_AHB1ENR |= (1U << 0U);
    RCC_APB2ENR |= (1U << 12U);
    (void)RCC_APB2ENR;

    /*
     * PA5 (SCK,  AF5): MODER[11:10]=10, AFRL[23:20]=5
     * PA6 (MISO, AF5): MODER[13:12]=10, AFRL[27:24]=5
     * PA7 (MOSI, AF5): MODER[15:14]=10, AFRL[31:28]=5
     */
    GPIOA_MODER &= ~(0x3FU << 10U);
    GPIOA_MODER |=  (0x2AU << 10U);  /* 101010b = AF for pins 5,6,7 */
    GPIOA_AFRL  &= ~(0xFFFU << 20U);
    GPIOA_AFRL  |=  (0x555U << 20U); /* AF5 for PA5, PA6, PA7 */

    SPI1_CR1 = SPI1_CR1_VAL;
    SPI1_CR1 |= SPI_CR1_SPE;

    ael_mailbox_init();

    static const uint8_t TX[8] = {0xA5U, 0x5AU, 0xF0U, 0x0FU,
                                   0x12U, 0x34U, 0x56U, 0x78U};
    uint32_t matched = 0U;

    for (uint32_t i = 0U; i < 8U; i++) {
        uint8_t rx = 0U;
        if (!spi_xfer(TX[i], &rx)) {
            ael_mailbox_fail(0x10U | i, matched);
            while (1) {}
        }
        if (rx != TX[i]) {
            ael_mailbox_fail(0x20U | i, rx);
            while (1) {}
        }
        matched++;
        AEL_MAILBOX->detail0 = matched;
    }

    ael_mailbox_pass();
    while (1) {
        for (volatile uint32_t d = 0U; d < 4000U; d++) { __asm__ volatile("nop"); }
        AEL_MAILBOX->detail0 = ++matched;
    }
}
