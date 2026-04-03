/*
 * stm32h563rgt6_gpdma2_m2m_mailbox — GPDMA2 memory-to-memory self-test
 *
 * Mirrors dma_m2m_mailbox but uses the second DMA controller (GPDMA2).
 * GPDMA2 Channel 0 (software request, polling) copies 4 words.
 *
 * Address map (RM0481):
 *   GPDMA2_BASE    = 0x40021000 (AHB1 base + 0x1000)
 *   GPDMA2_CH0     = GPDMA2_BASE + 0x0050 = 0x40021050
 *   RCC_AHB1ENR    = RCC_BASE + 0x088, bit1 = GPDMA2EN
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

/* ── GPDMA2 Channel 0 ────────────────────────────────────────────── */
#define GPDMA2_CH0_BASE  0x40021050u
#define DMA_CFCR   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x0Cu))
#define DMA_CSR    (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x10u))
#define DMA_CCR    (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x14u))
#define DMA_CTR1   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x40u))
#define DMA_CTR2   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x44u))
#define DMA_CBR1   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x48u))
#define DMA_CSAR   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x4Cu))
#define DMA_CDAR   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x50u))
#define DMA_CLLR   (*(volatile uint32_t *)(GPDMA2_CH0_BASE + 0x7Cu))

#define DMA_CSR_IDLEF  (1u << 0)
#define DMA_CSR_TCF    (1u << 8)
#define DMA_CCR_EN     (1u << 0)
#define DMA_CCR_RESET  (1u << 1)

/* CTR1: 32-bit word, increment both src and dst */
#define DMA_CTR1_M2M_WORD  ((2u << 0) | (1u << 3) | (2u << 16) | (1u << 19))
/* CTR2: software request (M2M) */
#define DMA_CTR2_SWREQ  (1u << 9)

#define N_WORDS  4u
#define TIMEOUT  1000000u

static const uint32_t src[N_WORDS] = {
    0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u
};
static uint32_t dst[N_WORDS];

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPDMA2 (AHB1 bit1) */
    RCC_AHB1ENR |= (1u << 1);
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
    DMA_CBR1 = N_WORDS * 4u;
    DMA_CSAR = (uint32_t)src;
    DMA_CDAR = (uint32_t)dst;
    DMA_CLLR = 0u;

    /* 5. Enable channel — starts transfer immediately (SWREQ) */
    DMA_CCR = DMA_CCR_EN;

    /* 6. Poll TCF */
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

    AEL_MAILBOX->detail0 = dst[0];   /* 0x11223344 */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
