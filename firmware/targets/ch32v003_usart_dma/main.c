/*
 * CH32V003 USART1 DMA TX test
 *
 * Uses DMA1 Channel4 to transmit a 32-byte string via USART1 TX (PD5).
 * WCH-LinkE RX receives the data (bench connection: PD5 → WCH-Link RX).
 *
 * DMA1_Channel4 is hardware-mapped to USART1 TX on CH32V003.
 * Direction: memory → USART1->DATAR (PeripheralDST).
 *
 * PASS if DMA TC4 flag set (transfer complete) and USART TC also set.
 * detail0 = transfer_byte_count << 1 (liveness indicator).
 *
 * Note: ch32v003fun.c .data startup bug → use .bss buffer, fill at runtime.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

#define TX_SIZE  32u

static uint8_t tx_buf[TX_SIZE];

int main(void)
{
    ael_mailbox_init();

    /* Fill TX buffer at runtime (avoid .data initializer bug) */
    const char *msg = "AEL_USART_DMA_TEST_CH32V003_OK\r\n";
    for (uint32_t i = 0; i < TX_SIZE; i++) {
        tx_buf[i] = (uint8_t)msg[i % 32];
    }

    /* Enable DMA1 + GPIOD + USART1 clocks */
    RCC->AHBPCENR  |= RCC_DMA1EN;
    RCC->APB2PCENR |= RCC_IOPDEN | RCC_USART1EN;

    /* PD5 = USART1_TX: AF_PP 50 MHz (CFGLR bits[23:20] = 0xB) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 20)) | (0xBu << 20);

    /* PD6 = USART1_RX: floating input (CFGLR bits[27:24] = 0x4) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 24)) | (0x4u << 24);

    /* Configure USART1: 115200 baud, 8N1
     * BRR = fPCLK2 / baud = 8000000 / 115200 ≈ 69 */
    USART1->BRR  = 69u;
    USART1->CTLR1 = (1u << 3) | (1u << 2);  /* TE | RE */
    USART1->CTLR1 |= (1u << 13);  /* UE */

    /* Configure DMA1 Channel4: tx_buf → USART1->DATAR
     * Direction: MemoryDST (DIR=1), Normal mode, byte-width */
    DMA1_Channel4->CFGR  = 0u;
    DMA1_Channel4->PADDR = (uint32_t)&USART1->DATAR;
    DMA1_Channel4->MADDR = (uint32_t)tx_buf;
    DMA1_Channel4->CNTR  = TX_SIZE;
    DMA1_Channel4->CFGR  =
        (3u << 12) |  /* PL = very high */
        (0u << 10) |  /* MSIZE = 8-bit */
        (0u <<  8) |  /* PSIZE = 8-bit */
        (1u <<  7) |  /* MINC */
        (1u <<  4);   /* DIR = memory → peripheral */

    /* Clear TC4 flag */
    DMA1->INTFCR = (1u << 13);  /* CTCIF4 */

    /* Enable DMA channel */
    DMA1_Channel4->CFGR |= (1u << 0);

    /* Enable USART DMA TX request */
    USART1->CTLR3 |= (1u << 7);  /* DMAT */

    /* Wait for DMA transfer complete (TC4 = DMA1->INTFR bit13) */
    uint32_t timeout = 0u;
    while (!(DMA1->INTFR & (1u << 13))) {
        if (++timeout > 2000000u) break;
    }

    /* Also wait for USART transmit complete (TC bit6 in STATR) */
    timeout = 0u;
    while (!(USART1->STATR & (1u << 6))) {
        if (++timeout > 2000000u) break;
    }

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = TX_SIZE << 1;

    uint32_t dma_ok  = (DMA1->INTFR  & (1u << 13)) ? 1u : 0u;
    uint32_t uart_ok = (USART1->STATR & (1u <<  6)) ? 1u : 0u;

    if (dma_ok && uart_ok)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(dma_ok, uart_ok);

    /* Liveness */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 ^= 2u;
    }

    return 0;
}
