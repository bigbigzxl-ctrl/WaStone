/*
 * CH32V003 SPI1 DMA loopback test
 *
 * Wire: SPI1_MOSI (PC6) ↔ SPI1_MISO (PC7).
 *
 * SPI1 master, full-duplex.
 * DMA1_Channel3: tx_buf → SPI1->DATAR (TX, PeripheralDST).
 * DMA1_Channel2: SPI1->DATAR → rx_buf (RX, PeripheralSRC).
 * 8 bytes transferred, looped back via MOSI↔MISO wire.
 *
 * DMA channel mapping on CH32V003:
 *   DMA1_Ch2: SPI1_RX
 *   DMA1_Ch3: SPI1_TX
 *
 * PASS if all 8 received bytes match transmitted bytes.
 * detail0 = mismatch_count << 1.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

#define BUF_SIZE  8u

static uint8_t tx_buf[BUF_SIZE];
static uint8_t rx_buf[BUF_SIZE];

int main(void)
{
    ael_mailbox_init();

    /* Fill TX buffer at runtime */
    for (uint32_t i = 0; i < BUF_SIZE; i++) {
        tx_buf[i] = (uint8_t)(0x11u * (i + 1u));  /* 0x11 0x22 ... 0x88 */
    }

    /* Enable DMA1 + GPIOC + SPI1 clocks */
    RCC->AHBPCENR  |= RCC_DMA1EN;
    RCC->APB2PCENR |= RCC_IOPCEN | RCC_SPI1EN;

    /* PC5 = SCK: AF_PP 50 MHz (CFGLR bits[23:20] = 0xB) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 20)) | (0xBu << 20);
    /* PC6 = MOSI: AF_PP 50 MHz (CFGLR bits[27:24] = 0xB) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 24)) | (0xBu << 24);
    /* PC7 = MISO: floating input (CFGLR bits[31:28] = 0x4) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 28)) | (0x4u << 28);

    /* SPI1: Master, full-duplex, 8-bit, CPOL=0 CPHA=0, SW NSS, BRdiv=8 */
    /* CTLR1: MSTR(2)|BR[2:0]=010(3)(div8)|SPE(6)|SSM(9)|SSI(8) */
    SPI1->CTLR1 = (1u << 2)   /* MSTR */
                | (2u << 3)   /* BR = 010 → fPCLK/8 */
                | (1u << 9)   /* SSM */
                | (1u << 8);  /* SSI */

    /* Enable SPI DMA requests */
    SPI1->CTLR2 = (1u << 0) | (1u << 1);  /* RXDMAEN | TXDMAEN */

    /* Configure DMA1 Channel2: SPI1_RX → rx_buf (PeripheralSRC) */
    DMA1_Channel2->CFGR  = 0u;
    DMA1_Channel2->PADDR = (uint32_t)&SPI1->DATAR;
    DMA1_Channel2->MADDR = (uint32_t)rx_buf;
    DMA1_Channel2->CNTR  = BUF_SIZE;
    DMA1_Channel2->CFGR  =
        (3u << 12) |  /* PL = very high */
        (0u << 10) |  /* MSIZE = 8-bit */
        (0u <<  8) |  /* PSIZE = 8-bit */
        (1u <<  7);   /* MINC; DIR=0 periph→mem */

    /* Configure DMA1 Channel3: tx_buf → SPI1_TX (PeripheralDST) */
    DMA1_Channel3->CFGR  = 0u;
    DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = (uint32_t)tx_buf;
    DMA1_Channel3->CNTR  = BUF_SIZE;
    DMA1_Channel3->CFGR  =
        (3u << 12) |  /* PL = very high */
        (0u << 10) |  /* MSIZE = 8-bit */
        (0u <<  8) |  /* PSIZE = 8-bit */
        (1u <<  7) |  /* MINC */
        (1u <<  4);   /* DIR = mem→periph */

    /* Clear TC2/TC3 flags */
    DMA1->INTFCR = (1u << 5) | (1u << 9);  /* CTCIF2 | CTCIF3 */

    /* Enable RX DMA first (avoid losing first byte) */
    DMA1_Channel2->CFGR |= (1u << 0);

    /* Enable SPI */
    SPI1->CTLR1 |= (1u << 6);  /* SPE */

    /* Enable TX DMA */
    DMA1_Channel3->CFGR |= (1u << 0);

    /* Wait for both TC2 and TC3 */
    uint32_t timeout = 0u;
    while ((DMA1->INTFR & ((1u << 5) | (1u << 9))) != ((1u << 5) | (1u << 9))) {
        if (++timeout > 2000000u) break;
    }

    /* Verify rx_buf matches tx_buf */
    uint32_t mismatches = 0u;
    for (uint32_t i = 0; i < BUF_SIZE; i++) {
        if (rx_buf[i] != tx_buf[i]) mismatches++;
    }

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = mismatches << 1;

    if (mismatches == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(mismatches, (uint32_t)rx_buf[0]);

    /* Liveness */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 ^= 2u;
    }

    return 0;
}
