/*
 * CH32V003 SPI1 loopback: MOSI (PC6) → MISO (PC7)
 *
 * SPI1 master, software NSS, mode 0 (CPOL=0 CPHA=0)
 * PC5 = SCK  (AF push-pull)
 * PC6 = MOSI (AF push-pull) → wired to PC7
 * PC7 = MISO (floating input) ← wired from PC6
 *
 * Wire: PC6 ↔ PC7
 *
 * Test: send 5 known bytes, read back, verify match.
 * Liveness: keep transferring, detail0 = ok_count << 1
 *
 * Clock: 24 MHz HSI, APB2 = 8 MHz
 * SPI baud: BR=7 → fPCLK/256 = ~31 kHz
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

/* ── SPI1 helpers ─────────────────────────────────────────────────────── */
static void spi_init(void)
{
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPCEN | RCC_SPI1EN;
    RCC->APB2PRSTR |=  RCC_SPI1RST;
    RCC->APB2PRSTR &= ~RCC_SPI1RST;

    /* PC5 = SCK  AF push-pull 50 MHz (CFGLR bits[23:20] = 0xB) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 20)) | (0xBu << 20);
    /* PC6 = MOSI AF push-pull 50 MHz (CFGLR bits[27:24] = 0xB) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 24)) | (0xBu << 24);
    /* PC7 = MISO floating input (CFGLR bits[31:28] = 0x4) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 28)) | (0x4u << 28);

    /* SPI1: master, software NSS, mode 0, BR=7 (fPCLK/256) */
    SPI1->CTLR1 = SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI |
                  (0x7u << 3) |        /* BR[2:0] = 7 */
                  SPI_CTLR1_SPE;
}

/* Transfer one byte; return received byte. Timeout = busy loop. */
static uint8_t spi_transfer(uint8_t tx)
{
    while (!(SPI1->STATR & SPI_STATR_TXE));
    SPI1->DATAR = tx;
    while (!(SPI1->STATR & SPI_STATR_RXNE));
    return (uint8_t)SPI1->DATAR;
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    ael_mailbox_init();
    spi_init();

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    /* ── initial loopback check (5 bytes) ──────────────────────────── */
    static const uint8_t probe[] = { 0xA5u, 0x5Au, 0xF0u, 0x0Fu, 0x96u };
    uint32_t err = 0u;
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t rx = spi_transfer(probe[i]);
        if (rx != probe[i]) err |= (1u << i);
    }

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, 0);

    /* ── liveness: keep transferring ──────────────────────────────── */
    uint32_t ok_count = 0;
    uint8_t  tx_byte  = 0;
    while (1) {
        uint8_t rx = spi_transfer(tx_byte);
        if (rx == tx_byte) {
            ok_count++;
            *detail0 = ok_count << 1;
        }
        tx_byte++;
    }

    return 0;
}
