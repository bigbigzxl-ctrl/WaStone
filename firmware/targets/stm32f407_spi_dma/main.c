/*
 * STM32F407 — SPI DMA Loopback Test
 *
 * SPI2 master loopback (PB15=MOSI ↔ PB14=MISO, PB13=SCK).
 * TX path: DMA1 Stream4 Channel0 (M2P → SPI2_TX)
 * RX path: DMA1 Stream3 Channel0 (P2M ← SPI2_RX)
 *
 * Sends 8 bytes simultaneously via DMA TX+RX, verifies match.
 * Tests DMA1 with SPI request lines and bi-directional memory transfers.
 *
 * Wiring: PB15 → PB14 (loopback wire, same as spi_loopback test)
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = 8 (bytes verified)
 *   FAIL: error_code=1 RX DMA timeout
 *         error_code=2 TX DMA error (TEIF4)
 *         error_code=3 RX DMA error (TEIF3)
 *         error_code=4 data mismatch; detail0=first bad byte index
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---- */
#define RCC_BASE     0x40023800u
#define RCC_AHB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x30u))
#define RCC_APB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x40u))

/* ---- GPIOB ---- */
#define GPIOB_BASE    0x40020400u
#define GPIOB_MODER   (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OSPEEDR (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_AFRH    (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

/* ---- SPI2 ---- */
#define SPI2_BASE    0x40003800u
#define SPI2_CR1     (*(volatile uint32_t *)(SPI2_BASE + 0x00u))
#define SPI2_CR2     (*(volatile uint32_t *)(SPI2_BASE + 0x04u))
#define SPI2_SR      (*(volatile uint32_t *)(SPI2_BASE + 0x08u))
#define SPI2_DR      (*(volatile uint32_t *)(SPI2_BASE + 0x0Cu))

/* ---- DMA1 ---- */
#define DMA1_BASE    0x40026000u
#define DMA1_LISR    (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_HISR    (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_LIFCR   (*(volatile uint32_t *)(DMA1_BASE + 0x08u))
#define DMA1_HIFCR   (*(volatile uint32_t *)(DMA1_BASE + 0x0Cu))

/* Stream 3 (SPI2_RX): base + 0x58 */
#define DMA1_S3CR    (*(volatile uint32_t *)(DMA1_BASE + 0x58u + 0x00u))
#define DMA1_S3NDTR  (*(volatile uint32_t *)(DMA1_BASE + 0x58u + 0x04u))
#define DMA1_S3PAR   (*(volatile uint32_t *)(DMA1_BASE + 0x58u + 0x08u))
#define DMA1_S3M0AR  (*(volatile uint32_t *)(DMA1_BASE + 0x58u + 0x0Cu))

/* Stream 4 (SPI2_TX): base + 0x70 */
#define DMA1_S4CR    (*(volatile uint32_t *)(DMA1_BASE + 0x70u + 0x00u))
#define DMA1_S4NDTR  (*(volatile uint32_t *)(DMA1_BASE + 0x70u + 0x04u))
#define DMA1_S4PAR   (*(volatile uint32_t *)(DMA1_BASE + 0x70u + 0x08u))
#define DMA1_S4M0AR  (*(volatile uint32_t *)(DMA1_BASE + 0x70u + 0x0Cu))

/*
 * LISR bits — stream 3: [27:22]
 *   TCIF3=27, HTIF3=26, TEIF3=25, DMEIF3=24, FEIF3=22
 * HISR bits — stream 4: [5:0]
 *   TCIF4=5, HTIF4=4, TEIF4=3, DMEIF4=2, FEIF4=0
 */
#define TCIF3  (1u << 27)
#define TEIF3  (1u << 25)
#define TCIF4  (1u << 5)
#define TEIF4  (1u << 3)

#define DMA_CR_CHSEL(n)  ((n) << 25)
#define DMA_CR_DIR_P2M   (0u << 6)
#define DMA_CR_DIR_M2P   (1u << 6)
#define DMA_CR_MINC      (1u << 10)
#define DMA_CR_EN        (1u << 0)

#define LEN  8u

static volatile uint8_t tx_buf[LEN] = {0x11u, 0x22u, 0x33u, 0x44u,
                                        0x55u, 0x66u, 0x77u, 0x88u};
static volatile uint8_t rx_buf[LEN];

int main(void)
{
    ael_mailbox_init();

    /* ---- Clocks: GPIOB, DMA1, SPI2 ---- */
    RCC_AHB1ENR |= (1u << 1)  /* GPIOBEN */
                |  (1u << 21); /* DMA1EN */
    RCC_APB1ENR |= (1u << 14); /* SPI2EN */
    (void)RCC_APB1ENR;

    /* ---- PB13=SCK AF5, PB14=MISO AF5, PB15=MOSI AF5 ---- */
    GPIOB_MODER   &= ~0xFC000000u;
    GPIOB_MODER   |=  0xA8000000u;  /* AF mode */
    GPIOB_OSPEEDR |=  0xFC000000u;  /* high speed */
    GPIOB_AFRH    &= ~0xFFF00000u;
    GPIOB_AFRH    |=  0x55500000u;  /* AF5 for PB13,PB14,PB15 */

    /* ---- SPI2: master, /256, mode 0, SSM+SSI, DMA enable ---- */
    SPI2_CR1 = (1u << 2)   /* MSTR */
             | (7u << 3)   /* BR = /256 (~62 kHz at 16 MHz) */
             | (1u << 8)   /* SSI */
             | (1u << 9);  /* SSM */
    SPI2_CR2 = (1u << 1)   /* TXDMAEN */
             | (1u << 0);  /* RXDMAEN */

    /* ---- DMA1 Stream3 (RX, P2M, CH0) ---- */
    DMA1_S3CR   = 0u;
    /* Clear all Stream3 flags in LIFCR */
    DMA1_LIFCR |= TCIF3 | TEIF3 | (1u << 26) | (1u << 24) | (1u << 22);
    DMA1_S3PAR  = (uint32_t)&SPI2_DR;
    DMA1_S3M0AR = (uint32_t)rx_buf;
    DMA1_S3NDTR = LEN;
    DMA1_S3CR   = DMA_CR_CHSEL(0u) | DMA_CR_DIR_P2M | DMA_CR_MINC | DMA_CR_EN;

    /* ---- DMA1 Stream4 (TX, M2P, CH0) ---- */
    DMA1_S4CR   = 0u;
    /* Clear all Stream4 flags in HIFCR */
    DMA1_HIFCR |= TCIF4 | TEIF4 | (1u << 4) | (1u << 2) | (1u << 0);
    DMA1_S4PAR  = (uint32_t)&SPI2_DR;
    DMA1_S4M0AR = (uint32_t)tx_buf;
    DMA1_S4NDTR = LEN;
    DMA1_S4CR   = DMA_CR_CHSEL(0u) | DMA_CR_DIR_M2P | DMA_CR_MINC | DMA_CR_EN;

    /* Enable SPI after DMA is armed */
    SPI2_CR1 |= (1u << 6); /* SPE */

    /* ---- Wait for RX DMA complete ---- */
    uint32_t timeout = 2000000u;
    while (!(DMA1_LISR & TCIF3)) {
        if (DMA1_HISR & TEIF4) { ael_mailbox_fail(2u, 0u); while (1) {} }
        if (DMA1_LISR & TEIF3) { ael_mailbox_fail(3u, 0u); while (1) {} }
        if (--timeout == 0u)   { ael_mailbox_fail(1u, 0u); while (1) {} }
    }

    /* Wait for SPI not busy */
    timeout = 200000u;
    while (SPI2_SR & (1u << 7)) { if (--timeout == 0u) break; }

    /* ---- Verify ---- */
    for (uint32_t i = 0u; i < LEN; i++) {
        if (rx_buf[i] != tx_buf[i]) {
            ael_mailbox_fail(4u, i);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = LEN;
    while (1) {}
}
