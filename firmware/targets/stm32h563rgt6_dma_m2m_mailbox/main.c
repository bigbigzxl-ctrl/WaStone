/*
 * stm32h563rgt6_dma_m2m_mailbox — GPDMA1 memory-to-memory self-test
 *
 * Uses GPDMA1 Channel 0 (software request, polling) to copy 4 words
 * from src[] to dst[], then verifies the copy.
 *
 * GPDMA1 new IP (RM0481 §46):
 *   GPDMA1_BASE = 0x40020000 (AHB1 bit0)
 *   CH0 registers at GPDMA1_BASE + 0x0050:
 *     +0x0C CFCR  flag clear
 *     +0x10 CSR   status (bit0=IDLEF, bit8=TCF)
 *     +0x14 CCR   control (bit0=EN, bit1=RESET)
 *     +0x40 CTR1  data width + increment
 *     +0x44 CTR2  request (bit9=SWREQ for M2M)
 *     +0x48 CBR1  byte count BNDT[15:0]
 *     +0x4C CSAR  source address
 *     +0x50 CDAR  destination address
 *     +0x7C CLLR  linked list (0=none)
 *
 * CTR1 for 32-bit M2M:
 *   SDW_LOG2[1:0]=10 (word src), SINC=bit3, DDW_LOG2[17:16]=10 (word dst), DINC=bit19
 *   = (2<<0)|(1<<3)|(2<<16)|(1<<19) = 0x000A000A
 *
 * RCC_AHB1ENR = RCC_BASE(0x44020C00) + 0x088, bit0 = GPDMA1EN
 *
 * FAIL codes:
 *   0xE001 — TCF timeout (DMA never completed)
 *   0xE002 — data mismatch, detail0 = (word_idx<<16)|received
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))

/* ── GPDMA1 Channel 0 ────────────────────────────────────────────── */
#define GPDMA1_CH0_BASE  0x40020050u
#define DMA_CFCR   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x0Cu))
#define DMA_CSR    (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x10u))
#define DMA_CCR    (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x14u))
#define DMA_CTR1   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x40u))
#define DMA_CTR2   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x44u))
#define DMA_CBR1   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x48u))
#define DMA_CSAR   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x4Cu))
#define DMA_CDAR   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x50u))
#define DMA_CLLR   (*(volatile uint32_t *)(GPDMA1_CH0_BASE + 0x7Cu))

/* CSR */
#define DMA_CSR_IDLEF  (1u << 0)
#define DMA_CSR_TCF    (1u << 8)

/* CCR */
#define DMA_CCR_EN     (1u << 0)
#define DMA_CCR_RESET  (1u << 1)

/*
 * CTR1: word-width M2M with address increment on both sides.
 *   SDW_LOG2[1:0] = 2 → 32-bit source word
 *   SINC          = bit3
 *   DDW_LOG2[17:16] = 2 → 32-bit destination word
 *   DINC          = bit19
 */
#define DMA_CTR1_M2M_WORD  ((2u << 0) | (1u << 3) | (2u << 16) | (1u << 19))

/* CTR2: software request (no peripheral, M2M) */
#define DMA_CTR2_SWREQ  (1u << 9)

#define N_WORDS  4u
#define TIMEOUT  1000000u

static const uint32_t src[N_WORDS] = {
    0xDEADBEEFu, 0xCAFEBABEu, 0xA5A5A5A5u, 0x12345678u
};
static uint32_t dst[N_WORDS];

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPDMA1 (AHB1 bit0) */
    RCC_AHB1ENR |= (1u << 0);
    (void)RCC_AHB1ENR;

    /* 2. Reset channel 0 and wait for idle */
    DMA_CCR = DMA_CCR_RESET;
    uint32_t t;
    for (t = 0; t < TIMEOUT; t++) {
        if (DMA_CSR & DMA_CSR_IDLEF) break;
    }

    /* 3. Clear all pending flags */
    DMA_CFCR = 0xFFFFFFFFu;

    /* 4. Configure transfer */
    DMA_CTR1 = DMA_CTR1_M2M_WORD;
    DMA_CTR2 = DMA_CTR2_SWREQ;
    DMA_CBR1 = N_WORDS * 4u;           /* 16 bytes */
    DMA_CSAR = (uint32_t)src;
    DMA_CDAR = (uint32_t)dst;
    DMA_CLLR = 0u;                      /* no linked list */

    /* 5. Enable channel — starts transfer immediately (SWREQ) */
    DMA_CCR = DMA_CCR_EN;

    /* 6. Poll TCF (transfer complete flag) */
    for (t = 0; t < TIMEOUT; t++) {
        if (DMA_CSR & DMA_CSR_TCF) break;
    }
    if (!(DMA_CSR & DMA_CSR_TCF)) {
        ael_mailbox_fail(0xE001u, DMA_CSR);
        while (1) {}
    }

    /* 7. Verify */
    for (uint32_t i = 0u; i < N_WORDS; i++) {
        if (dst[i] != src[i]) {
            ael_mailbox_fail(0xE002u, (i << 16) | (dst[i] & 0xFFFFu));
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = dst[0];   /* 0xDEADBEEF */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
