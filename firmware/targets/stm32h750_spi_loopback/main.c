/*
 * stm32h750_spi_loopback — SPI1 MISO↔MOSI loopback self-test
 *
 * SPI1 master sends 4 bytes; MISO↔MOSI bench wire returns them.
 * Verifies: SPI1 peripheral, GPIOB AF5, bench wire PB4↔PB5.
 *
 * Pin assignment (RM0433 Table 10, AF5):
 *   PB3 = SPI1_SCK  (AF5) — clock output, no bench wire needed
 *   PB4 = SPI1_MISO (AF5)  }
 *   PB5 = SPI1_MOSI (AF5)  } wired together: PB4↔PB5
 *
 * SPI1_BASE = 0x40013000 (APB2, D2 domain, RM0433 §57)
 * RCC_APB2ENR bit 12 = SPI1EN (RM0433 §8.7.40)
 *
 * SPI IP: SPIv2 (same as STM32G4). CFG1/CFG2/CR1/SR layout identical.
 * Configuration: 8-bit, Master, Full-Duplex, SSM=1, CKMODE=00, MBR=/4.
 * APB2 = HCLK = 64 MHz → SPI clock = 64/4 = 16 MHz.
 * Uses TSIZE=4 (auto-stop after 4 frames) + polling TXP/RXP.
 *
 * Error codes:
 *   bit 0: byte 0 (0x12) mismatch
 *   bit 1: byte 1 (0x34) mismatch
 *   bit 2: byte 2 (0xAB) mismatch
 *   bit 3: byte 3 (0xCD) mismatch
 *   0x10: TX FIFO timeout
 *   0x20: RX FIFO timeout
 *
 * All register addresses from RM0433.
 */

#define AEL_MAILBOX_ADDR  0x38000000u
#include "../ael_mailbox.h"

/* ── RCC ───────────────────────────────────────────────────────── */
#define RCC_BASE            0x58024400u
#define RCC_AHB4ENR         (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x0F0u))
#define RCC_AHB4ENR_GPIOBEN (1u << 1)
#define RCC_APB2ENR_SPI1EN  (1u << 12)

/* ── GPIOB (base 0x58020400) ──────────────────────────────────── */
#define GPIOB_BASE   0x58020400u
#define GPIOB_MODER  (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OSPEEDR (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_AFRL   (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))

/* ── SPI1 (RM0433 §57, APB2, base 0x40013000) ─────────────────── */
#define SPI1_BASE   0x40013000u
#define SPI1_CR1    (*(volatile uint32_t *)(SPI1_BASE + 0x00u))
#define SPI1_CR2    (*(volatile uint32_t *)(SPI1_BASE + 0x04u))
#define SPI1_CFG1   (*(volatile uint32_t *)(SPI1_BASE + 0x08u))
#define SPI1_CFG2   (*(volatile uint32_t *)(SPI1_BASE + 0x0Cu))
#define SPI1_SR     (*(volatile uint32_t *)(SPI1_BASE + 0x14u))
#define SPI1_IFCR   (*(volatile uint32_t *)(SPI1_BASE + 0x18u))
/* Byte-width access to TXDR / RXDR for 8-bit transfers */
#define SPI1_TXDR8  (*(volatile uint8_t  *)(SPI1_BASE + 0x20u))
#define SPI1_RXDR8  (*(volatile uint8_t  *)(SPI1_BASE + 0x30u))

/* SPI CR1 bits */
#define SPI_CR1_SPE    (1u << 0)
#define SPI_CR1_CSTART (1u << 9)
#define SPI_CR1_CSUSP  (1u << 10)
#define SPI_CR1_SSI    (1u << 12)

/* SPI SR bits */
#define SPI_SR_RXP   (1u << 0)
#define SPI_SR_TXP   (1u << 1)
#define SPI_SR_EOT   (1u << 3)
#define SPI_SR_SUSP  (1u << 11)

/* SPI CFG1: DSIZE[4:0]=7 (8-bit), MBR[30:28]=001 (/4) */
#define SPI_CFG1_VAL  ((7u << 0u) | (1u << 28u))

/* SPI CFG2: MASTER=1, SSM=1 */
#define SPI_CFG2_VAL  ((1u << 22u) | (1u << 26u))

/* ── SysTick ─────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static void delay_ticks(uint32_t ticks)
{
    for (volatile uint32_t i = 0u; i < ticks; i++) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Enable GPIOB and SPI1 clocks */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC_AHB4ENR;
    RCC_APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC_APB2ENR;

    ael_mailbox_init();

    /*
     * GPIO config for PB3(SCK), PB4(MISO), PB5(MOSI) — all AF5.
     * MODER: 10 = Alternate Function. AFRL covers pins 0-7.
     * AFRL bits [15:12]=PB3, [19:16]=PB4, [23:20]=PB5.
     */
    /* Set AF5 for PB3, PB4, PB5 */
    GPIOB_AFRL &= ~((0xFu << 12u) | (0xFu << 16u) | (0xFu << 20u));
    GPIOB_AFRL |=  ((5u << 12u) | (5u << 16u) | (5u << 20u));
    /* Set mode = AF (10b) for PB3[7:6], PB4[9:8], PB5[11:10] */
    GPIOB_MODER &= ~((0x3u << 6u) | (0x3u << 8u) | (0x3u << 10u));
    GPIOB_MODER |=  ((0x2u << 6u) | (0x2u << 8u) | (0x2u << 10u));
    /* Very high speed for SPI pins */
    GPIOB_OSPEEDR |= ((0x3u << 6u) | (0x3u << 8u) | (0x3u << 10u));

    /*
     * SPI1 configuration (SPIv2, same as STM32G4):
     *   CFG1: DSIZE=7 (8-bit), MBR=001 (/4 → 16 MHz from 64 MHz APB2)
     *   CFG2: MASTER=1, SSM=1 (software NSS management)
     *   CR2 : TSIZE=4 (auto-stop after 4 frames)
     *   CR1 : SSI=1 (drive NSS high internally), then SPE=1
     */
    SPI1_CFG1 = SPI_CFG1_VAL;
    SPI1_CFG2 = SPI_CFG2_VAL;
    SPI1_CR2  = 4u;                          /* TSIZE = 4 frames */
    SPI1_CR1  = SPI_CR1_SSI;                 /* SSI=1, SPE=0 yet */
    SPI1_CR1 |= SPI_CR1_SPE;                 /* enable SPI */
    SPI1_CR1 |= SPI_CR1_CSTART;              /* start master transfer */

    static const uint8_t tx[4] = { 0x12u, 0x34u, 0xABu, 0xCDu };
    uint8_t rx[4] = { 0u, 0u, 0u, 0u };
    uint32_t err = 0u;

    for (uint32_t i = 0u; i < 4u; i++) {
        /* Wait TXP: TX FIFO has space for at least one frame */
        uint32_t timeout = 1000000u;
        while (((SPI1_SR & SPI_SR_TXP) == 0u) && (--timeout > 0u)) {}
        if ((SPI1_SR & SPI_SR_TXP) == 0u) { err = 0x10u; goto done; }

        SPI1_TXDR8 = tx[i];

        /* Wait RXP: RX FIFO has at least one received frame */
        timeout = 1000000u;
        while (((SPI1_SR & SPI_SR_RXP) == 0u) && (--timeout > 0u)) {}
        if ((SPI1_SR & SPI_SR_RXP) == 0u) { err = 0x20u; goto done; }

        rx[i] = SPI1_RXDR8;
    }

    /* Wait for EOT (all frames transmitted and received) */
    {
        uint32_t timeout = 1000000u;
        while (((SPI1_SR & SPI_SR_EOT) == 0u) && (--timeout > 0u)) {}
    }

    /* Verify received bytes */
    for (uint32_t i = 0u; i < 4u; i++) {
        if (rx[i] != tx[i]) { err |= (1u << i); }
    }

done:
    /* Suspend and disable SPI */
    SPI1_CR1 |= SPI_CR1_CSUSP;
    delay_ticks(1u);
    SPI1_CR1 &= ~SPI_CR1_SPE;

    if (err == 0u) {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = ((uint32_t)rx[0] << 24u) |
                               ((uint32_t)rx[1] << 16u) |
                               ((uint32_t)rx[2] << 8u)  |
                               ((uint32_t)rx[3]);
        while (1) {}
    } else {
        ael_mailbox_fail(err,
            ((uint32_t)rx[0] << 24u) | ((uint32_t)rx[1] << 16u) |
            ((uint32_t)rx[2] << 8u)  | ((uint32_t)rx[3]));
        while (1) {}
    }
}
