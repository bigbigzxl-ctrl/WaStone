/*
 * stm32h750_bdma — BDMA memory-to-memory transfer self-test
 *
 * D3 domain Basic DMA (BDMA) performs a 32-word memory-to-memory
 * transfer within SRAM4 (D3 domain, AHB-accessible).
 *
 * BDMA base: 0x58025400 (D3_AHB1PERIPH_BASE + 0x5400)
 * RCC_AHB4ENR bit 21 = BDMAEN
 * SRAM4: 0x38000000 (64 KB, accessible by BDMA and CPU)
 *
 * Test:
 *   1. Write pattern 0xA0000000..0xA000001F to SRAM4 src block
 *   2. BDMA Channel0 M2M: 32×32-bit words src→dst (within SRAM4)
 *   3. Poll TCIF0 (ISR bit 1) for transfer complete
 *   4. CPU reads dst block, verifies pattern
 *   5. Second transfer with inverted pattern, verify again
 *
 * Error codes:
 *   0xE001 = TEIF (transfer error interrupt flag set)
 *   0xE002 = TCIF timeout (transfer complete never set)
 *   0xE003 = data mismatch on first pattern (detail0 = first bad word)
 *   0xE004 = data mismatch on second pattern
 *
 * On PASS: detail0 = 32 (words transferred successfully, ×2 passes)
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE          0x58024400u
#define RCC_AHB4ENR       (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_AHB4ENR_BDMAEN (1u << 21u)

/* ── BDMA (D3_AHB1PERIPH_BASE=0x58020000, +0x5400) ──────────────── */
#define BDMA_BASE         0x58025400u
#define BDMA_ISR          (*(volatile uint32_t *)(BDMA_BASE + 0x000u))
#define BDMA_IFCR         (*(volatile uint32_t *)(BDMA_BASE + 0x004u))
/* Channel 0 registers (offset 0x008) */
#define BDMA_CCR0         (*(volatile uint32_t *)(BDMA_BASE + 0x008u))
#define BDMA_CNDTR0       (*(volatile uint32_t *)(BDMA_BASE + 0x00Cu))
#define BDMA_CPAR0        (*(volatile uint32_t *)(BDMA_BASE + 0x010u))
#define BDMA_CMAR0        (*(volatile uint32_t *)(BDMA_BASE + 0x014u))

/* ISR flags for channel 0 */
#define BDMA_ISR_GIF0     (1u << 0u)
#define BDMA_ISR_TCIF0    (1u << 1u)
#define BDMA_ISR_HTIF0    (1u << 2u)
#define BDMA_ISR_TEIF0    (1u << 3u)
#define BDMA_IFCR_CGIF0   (1u << 0u)

/* CCR bits */
#define BDMA_CCR_EN       (1u << 0u)
#define BDMA_CCR_TCIE     (1u << 1u)
#define BDMA_CCR_HTIE     (1u << 2u)
#define BDMA_CCR_TEIE     (1u << 3u)
#define BDMA_CCR_DIR      (1u << 4u)    /* 0=periph-to-mem, 1=mem-to-periph */
#define BDMA_CCR_PINC     (1u << 6u)
#define BDMA_CCR_MINC     (1u << 7u)
#define BDMA_CCR_MEM2MEM  (1u << 14u)
/* Size: 00=8bit, 01=16bit, 10=32bit */
#define BDMA_CCR_PSIZE32  (2u << 8u)
#define BDMA_CCR_MSIZE32  (2u << 10u)

/* CCR for M2M 32-bit with source (CPAR) and dest (CMAR) increment */
#define BDMA_CCR_M2M_32   (BDMA_CCR_MEM2MEM | BDMA_CCR_PSIZE32 | BDMA_CCR_MSIZE32 \
                           | BDMA_CCR_PINC | BDMA_CCR_MINC)

/* ── SRAM4 ───────────────────────────────────────────────────────── */
#define SRAM4_BASE        0x38000000u
/* Source block at start of SRAM4, dest block offset by 256 bytes */
#define DMA_SRC           ((volatile uint32_t *)SRAM4_BASE)
#define DMA_DST           ((volatile uint32_t *)(SRAM4_BASE + 0x100u))
#define N_WORDS           32u

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0u)
#define SYST_CSR_CLKSOURCE (1u << 2u)
#define SYST_CSR_COUNTFLAG (1u << 16u)

/* Run one BDMA transfer: src→dst, N_WORDS words, verify with mask */
static uint32_t run_bdma_transfer(uint32_t pattern_base, uint32_t err_pass, uint32_t err_mismatch)
{
    /* Write source pattern */
    for (uint32_t i = 0u; i < N_WORDS; i++) {
        DMA_SRC[i] = pattern_base + i;
    }
    /* Clear dest */
    for (uint32_t i = 0u; i < N_WORDS; i++) {
        DMA_DST[i] = 0u;
    }

    /* Clear BDMA flags */
    BDMA_IFCR = BDMA_IFCR_CGIF0 | (BDMA_IFCR_CGIF0 << 4u) | (BDMA_IFCR_CGIF0 << 8u)
              | (BDMA_IFCR_CGIF0 << 12u) | (BDMA_IFCR_CGIF0 << 16u)
              | (BDMA_IFCR_CGIF0 << 20u) | (BDMA_IFCR_CGIF0 << 24u)
              | (BDMA_IFCR_CGIF0 << 28u);

    /* Configure Channel 0 */
    BDMA_CCR0  = 0u;                                      /* disable first */
    BDMA_CNDTR0 = N_WORDS;
    BDMA_CPAR0 = (uint32_t)DMA_SRC;                      /* source */
    BDMA_CMAR0 = (uint32_t)DMA_DST;                      /* destination */
    BDMA_CCR0  = BDMA_CCR_M2M_32;                        /* configure (not enabled) */
    BDMA_CCR0 |= BDMA_CCR_EN;                            /* start */

    /* Wait for TCIF0 */
    uint32_t timeout = 1000000u;
    while (((BDMA_ISR & BDMA_ISR_TCIF0) == 0u) && ((BDMA_ISR & BDMA_ISR_TEIF0) == 0u)) {
        if (--timeout == 0u) {
            return err_pass;   /* timeout error */
        }
    }
    if ((BDMA_ISR & BDMA_ISR_TEIF0) != 0u) {
        return 0xE001u;   /* transfer error */
    }

    BDMA_CCR0 = 0u;   /* disable channel */

    /* Verify destination */
    for (uint32_t i = 0u; i < N_WORDS; i++) {
        if (DMA_DST[i] != (pattern_base + i)) {
            return err_mismatch;
        }
    }
    return 0u;   /* success */
}

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Enable BDMA clock (AHB4ENR bit 21) */
    RCC_AHB4ENR |= RCC_AHB4ENR_BDMAEN;
    (void)RCC_AHB4ENR;

    /* Pass 1: pattern 0xA0000000..0xA000001F */
    uint32_t err = run_bdma_transfer(0xA0000000u, 0xE002u, 0xE003u);
    if (err != 0u) {
        ael_mailbox_fail(err, BDMA_ISR);
        while (1) {}
    }

    /* Pass 2: inverted pattern 0x5FFFFFFF..0x5FFFFFE0 */
    err = run_bdma_transfer(0x5FFFFFFFu - (N_WORDS - 1u), 0xE002u, 0xE004u);
    if (err != 0u) {
        ael_mailbox_fail(err, BDMA_ISR);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = N_WORDS * 2u;   /* 64 words transferred in total */
    while (1) {}
}
