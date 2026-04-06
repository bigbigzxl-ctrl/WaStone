/*
 * CH32V003 DMA memory-to-memory transfer test
 *
 * Uses DMA1 Channel3 (MEM2MEM mode) to copy SRC_BUF[32] → DST_BUF[32].
 * Verifies all 32 words match after TC3 flag.
 * No pins required.
 *
 * DMA1 Channel3 CFGR:
 *   MEM2MEM = 1 (bit14)
 *   PL      = 3 (very high priority, bits[13:12])
 *   MSIZE/PSIZE = 2 (32-bit, bits[11:10, 9:8])
 *   MINC/PINC = 1 (both increment, bits[7,6])
 *   DIR     = 1 (read from peripheral/src, bit4)
 *   EN      = 1 (bit0)
 *
 * TC3 flag = DMA1->INTFR bit9
 *
 * PASS if all 32 words match.
 * detail0 = mismatch_count << 1 (0 = all match → PASS)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

#define BUF_SIZE  32u

/* ch32v003fun.c startup has a known bug: .data is NOT copied from flash to SRAM.
 * Use uninitialized .bss buffers and fill SRC_BUF programmatically in main(). */
static uint32_t SRC_BUF[BUF_SIZE];
static uint32_t DST_BUF[BUF_SIZE];

int main(void)
{
    ael_mailbox_init();

    /* Fill SRC_BUF with known pattern (cannot use .data initializers — startup bug) */
    for (uint32_t i = 0; i < BUF_SIZE; i++) {
        SRC_BUF[i] = (i + 1u) * 0x01010101u;
    }

    /* Enable DMA1 clock (AHB) */
    RCC->AHBPCENR |= RCC_DMA1EN;

    /* Configure DMA1 Channel3 for MEM2MEM */
    DMA1_Channel3->CFGR = 0;  /* Disable first */
    DMA1_Channel3->PADDR = (uint32_t)SRC_BUF;
    DMA1_Channel3->MADDR = (uint32_t)DST_BUF;
    DMA1_Channel3->CNTR  = BUF_SIZE;  /* 32 transfers of 4 bytes each */

    /* CFGR: MEM2MEM(14)|PL_VERY_HIGH(13:12)|MSIZE_32(11:10)|PSIZE_32(9:8)|MINC(7)|PINC(6)|DIR_FROM_PERIPH(4) */
    DMA1_Channel3->CFGR =
        (1u << 14) |   /* MEM2MEM */
        (3u << 12) |   /* PL = very high */
        (2u << 10) |   /* MSIZE = 32-bit */
        (2u <<  8) |   /* PSIZE = 32-bit */
        (1u <<  7) |   /* MINC */
        (1u <<  6) |   /* PINC */
        (1u <<  4);    /* DIR = read from peripheral (src) */

    /* Clear TC3 flag (bit9 of INTFCR) */
    DMA1->INTFCR = (1u << 9);

    /* Enable channel */
    DMA1_Channel3->CFGR |= (1u << 0);  /* EN */

    /* Wait for transfer complete (TC3 = INTFR bit9) */
    while (!(DMA1->INTFR & (1u << 9)));

    /* Verify */
    uint32_t mismatches = 0;
    for (uint32_t i = 0; i < BUF_SIZE; i++) {
        if (DST_BUF[i] != SRC_BUF[i]) mismatches++;
    }

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = mismatches << 1;

    if (mismatches == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(mismatches, 0);

    /* Liveness: stay here, detail0 stays constant */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 = (*detail0 ^ 2u);   /* toggle bit1 for liveness */
    }

    return 0;
}
