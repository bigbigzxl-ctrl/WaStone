/*
 * stm32h563rgt6_spi_loopback
 *
 * SPI1 MOSI→MISO loopback: PA7(MOSI AF5) → wire → PA6(MISO AF5), PA5=SCK.
 * Master, full-duplex, 8-bit, Mode 0, SSM=1.
 * Sends {0x55, 0xAA, 0x12, 0x34}, verifies each received byte matches sent.
 *
 * *** KEY: SPI1 kernel clock ***
 * H563 default SPI1SEL = 0 (pll1_q_ck) — PLL1 not running after reset.
 * Must switch to per_ck (bit-field = 4) via RCC_CCIPR3[2:0].
 * per_ck default = CKPERSEL=0 → HSI (64 MHz) per RCC_CCIPR5 reset=0.
 * RCC_CCIPR3 = RCC_BASE(0x44020C00) + 0xE0 = 0x44020CE0.
 *
 * H5 SPI init sequence (RM0481 §57.4.9):
 *   1. SSI=1 in CR1 FIRST (while SPE=0)
 *   2. Write CFG1, CFG2 (SPE must be 0)
 *   3. Write CR2 (TSIZE)
 *   4. Write CR1 with SSI|SPE atomically
 *   5. Check/clear MODF (spurious on first SPE=1)
 *   6. Write CR1 with SSI|SPE|CSTART
 *   7. For each byte: wait TXP → write TXDR8 → wait RXP → read RXDR8
 *
 * GPIOA clock: AHB2 bit0. SPI1 clock: APB2 bit12.
 *
 * FAIL codes:
 *   0x10 — TX FIFO timeout
 *   0x20|i — RXP timeout at byte i, detail0 = SPI_SR
 *   0x30 — MODF persists after 5 retries
 *   0x40|i — byte mismatch at byte i, detail0 = (expected<<8)|received
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))
#define RCC_APB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x0A0u))
/* RCC_CCIPR3: SPI1SEL at bits[2:0]; 4 = per_ck (HSI 64 MHz) */
#define RCC_CCIPR3      (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))

/* GPIOA (AHB2) */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20u))

/* SPI1 (APB2) */
#define SPI1_BASE       0x40013000u
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00u))
#define SPI1_CR2        (*(volatile uint32_t *)(SPI1_BASE + 0x04u))
#define SPI1_CFG1       (*(volatile uint32_t *)(SPI1_BASE + 0x08u))
#define SPI1_CFG2       (*(volatile uint32_t *)(SPI1_BASE + 0x0Cu))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x14u))
#define SPI1_IFCR       (*(volatile uint32_t *)(SPI1_BASE + 0x18u))
#define SPI1_TXDR8      (*((volatile uint8_t  *)(SPI1_BASE + 0x20u)))
#define SPI1_RXDR8      (*((volatile uint8_t  *)(SPI1_BASE + 0x30u)))

#define SPI_CR1_SPE     (1u <<  0)
#define SPI_CR1_CSTART  (1u <<  9)
#define SPI_CR1_CSUSP   (1u << 10)
#define SPI_CR1_SSI     (1u << 12)
#define SPI_SR_RXP      (1u <<  0)
#define SPI_SR_TXP      (1u <<  1)
#define SPI_SR_EOT      (1u <<  3)
#define SPI_SR_MODF     (1u <<  9)
#define SPI_IFCR_MODFC  (1u <<  9)

/* CFG1: DSIZE=7(8-bit), MBR=5(÷64→1MHz) */
#define SPI_CFG1_VAL    ((7u << 0) | (5u << 28))
/* CFG2: MASTER=1(bit22), SSM=1(bit26), COMM=0(full-duplex) */
#define SPI_CFG2_VAL    ((1u << 22) | (1u << 26))

#define TIMEOUT  500000u

static const uint8_t tx_data[4] = {0x55u, 0xAAu, 0x12u, 0x34u};

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPIOA (AHB2 bit0), SPI1 (APB2 bit12) */
    RCC_AHB2ENR |= (1u << 0);
    RCC_APB2ENR  |= (1u << 12);
    (void)RCC_AHB2ENR;
    (void)RCC_APB2ENR;

    /* 2. Switch SPI1 kernel clock to per_ck (HSI 64 MHz).
     *    Default = pll1_q_ck which is NOT running after reset. */
    RCC_CCIPR3 = (RCC_CCIPR3 & ~(0x7u << 0)) | (4u << 0);
    (void)RCC_CCIPR3;

    /* 3. Reset SPI1 for clean state */
    RCC_APB2RSTR |= (1u << 12);
    for (volatile uint32_t d = 0; d < 200; d++) {}
    RCC_APB2RSTR &= ~(1u << 12);
    for (volatile uint32_t d = 0; d < 200; d++) {}

    /* 4. PA5=SCK(AF5), PA6=MISO(AF5), PA7=MOSI(AF5) */
    GPIOA_MODER   = (GPIOA_MODER   & ~(0x3Fu << 10)) | (0x2Au << 10);
    GPIOA_OSPEEDR = (GPIOA_OSPEEDR & ~(0x3Fu << 10)) | (0x2Au << 10);
    GPIOA_AFRL    = (GPIOA_AFRL    & ~(0xFFFu << 20)) | (0x555u << 20);

    /* 5. SPI init (RM0481 §57.4.9):
     *    a) SSI=1 FIRST (SPE=0) to prevent MODF on SPE=1 */
    SPI1_CR1  = SPI_CR1_SSI;
    SPI1_CFG1 = SPI_CFG1_VAL;
    SPI1_CFG2 = SPI_CFG2_VAL;
    SPI1_CR2  = 4u;   /* TSIZE=4 */

    /* b) Enable SPE; handle potential spurious MODF (H5 SPI first-enable quirk) */
    {
        uint32_t tries = 0u;
        for (;;) {
            SPI1_CR1 = SPI_CR1_SSI | SPI_CR1_SPE;
            /* Small delay for clock edge stabilization */
            for (volatile uint32_t d = 0; d < 100; d++) {}
            if (!(SPI1_SR & SPI_SR_MODF)) break;
            SPI1_IFCR = SPI_IFCR_MODFC;
            (void)SPI1_SR;
            if (++tries >= 5u) {
                ael_mailbox_fail(0x30u, SPI1_SR);
                while (1) {}
            }
        }
    }

    /* c) Start transfer (SSI|SPE|CSTART atomically — avoids MODF race) */
    SPI1_CR1 = SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART;

    /* 6. Transfer 4 bytes */
    for (uint32_t i = 0; i < 4u; i++) {
        uint32_t t;
        /* Wait TXP */
        for (t = 0; t < TIMEOUT; t++) {
            if (SPI1_SR & SPI_SR_TXP) break;
        }
        if (!(SPI1_SR & SPI_SR_TXP)) {
            ael_mailbox_fail(0x10u, i);
            while (1) {}
        }
        SPI1_TXDR8 = tx_data[i];

        /* Wait RXP */
        for (t = 0; t < TIMEOUT; t++) {
            if (SPI1_SR & SPI_SR_RXP) break;
        }
        if (!(SPI1_SR & SPI_SR_RXP)) {
            AEL_MAILBOX->detail0 = SPI1_SR;
            ael_mailbox_fail(0x20u | i, SPI1_SR);
            while (1) {}
        }
        uint8_t rx = SPI1_RXDR8;
        if (rx != tx_data[i]) {
            ael_mailbox_fail(0x40u | i, ((uint32_t)tx_data[i] << 8) | rx);
            while (1) {}
        }
    }

    /* 7. Disable SPI cleanly */
    SPI1_CR1 |= SPI_CR1_CSUSP;
    for (volatile uint32_t d = 0; d < 1000; d++) {}
    SPI1_CR1 &= ~SPI_CR1_SPE;

    AEL_MAILBOX->detail0 = 4u;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
