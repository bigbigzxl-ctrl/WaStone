/*
 * STM32F407 — DMA2 Memory-to-Memory Transfer Test
 *
 * Uses DMA2 Stream0 Ch0 in memory-to-memory mode.
 * Only DMA2 supports M2M on STM32F4 (DMA1 cannot).
 *
 * Transfers N_WORDS words from src[] → dst[] in SRAM.
 * Verifies all words match after transfer completes.
 *
 * No external wiring required. No PLL — 16 MHz HSI only.
 *
 * Clock: 16 MHz HSI. AHB = 16 MHz.
 *
 * Mailbox: 0x2001FC00
 *   PASS: all N_WORDS verified. detail0 = N_WORDS.
 *   FAIL: error_code = ERR_TEIF(1) or ERR_TIMEOUT(2) or ERR_MISMATCH(3)
 *         detail0 = mismatch index (on ERR_MISMATCH)
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* HardFault: SYSRESETREQ instead of LOCKUP (pattern ef195d1e) */
void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---------------------------------------------------------------- */
#define RCC_AHB1ENR (*(volatile uint32_t *)(0x40023800u + 0x30u))
#define RCC_AHB1ENR_DMA2EN (1u << 22)

/* ---- DMA2 (base 0x40026400) --------------------------------------------- */
#define DMA2_BASE   0x40026400u
/* Low interrupt status / clear registers (streams 0-3) */
#define DMA2_LISR   (*(volatile uint32_t *)(DMA2_BASE + 0x000u))
#define DMA2_LIFCR  (*(volatile uint32_t *)(DMA2_BASE + 0x008u))
/* Stream 0 registers */
#define DMA2_S0CR   (*(volatile uint32_t *)(DMA2_BASE + 0x010u))
#define DMA2_S0NDTR (*(volatile uint32_t *)(DMA2_BASE + 0x014u))
#define DMA2_S0PAR  (*(volatile uint32_t *)(DMA2_BASE + 0x018u))
#define DMA2_S0M0AR (*(volatile uint32_t *)(DMA2_BASE + 0x01Cu))
#define DMA2_S0FCR  (*(volatile uint32_t *)(DMA2_BASE + 0x024u))

/* DMA2 LISR bits for Stream 0 */
#define DMA_LISR_TCIF0  (1u << 5)   /* transfer complete */
#define DMA_LISR_TEIF0  (1u << 3)   /* transfer error */

/* DMA SxCR bits */
#define DMA_CR_EN    (1u << 0)
#define DMA_CR_DIR_M2M (2u << 6)    /* DIR[1:0]=10 */
#define DMA_CR_MINC  (1u << 10)     /* memory increment */
#define DMA_CR_PINC  (1u << 9)      /* peripheral (source) increment */
#define DMA_CR_MSIZE_WORD (2u << 13) /* 32-bit */
#define DMA_CR_PSIZE_WORD (2u << 11)

/* Error codes */
#define ERR_TEIF     1u
#define ERR_TIMEOUT  2u
#define ERR_MISMATCH 3u

#define N_WORDS  16u
#define TIMEOUT  1000000u

static const uint32_t src[N_WORDS] = {
    0xDEADBEEFu, 0xCAFEBABEu, 0x12345678u, 0xABCDEF01u,
    0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u,
    0x0F0E0D0Cu, 0x0B0A0908u, 0x07060504u, 0x03020100u,
    0xFEDCBA98u, 0x76543210u, 0xA5A5A5A5u, 0x5A5A5A5Au,
};
static uint32_t dst[N_WORDS];

int main(void)
{
    /* Enable DMA2 clock */
    RCC_AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC_AHB1ENR;

    ael_mailbox_init();

    /* Clear dst */
    for (uint32_t i = 0u; i < N_WORDS; i++) { dst[i] = 0u; }

    /* Clear any prior Stream 0 interrupt flags */
    DMA2_LIFCR = 0x3Fu;  /* clear all Stream0 flags */

    /* Disable stream before config */
    DMA2_S0CR &= ~DMA_CR_EN;
    { uint32_t t = TIMEOUT; while ((DMA2_S0CR & DMA_CR_EN) && --t) {} }

    /* Configure DMA2 Stream 0 for M2M */
    DMA2_S0PAR  = (uint32_t)src;        /* source */
    DMA2_S0M0AR = (uint32_t)dst;        /* destination */
    DMA2_S0NDTR = N_WORDS;
    /* Direct mode (FIFO disabled): FCR DMDIS=0 (default) */
    DMA2_S0FCR  = 0u;
    DMA2_S0CR   = DMA_CR_DIR_M2M
                | DMA_CR_PINC
                | DMA_CR_MINC
                | DMA_CR_PSIZE_WORD
                | DMA_CR_MSIZE_WORD;

    /* Start transfer */
    DMA2_S0CR |= DMA_CR_EN;

    /* Wait for transfer complete or error */
    {
        uint32_t t = TIMEOUT;
        while (!(DMA2_LISR & (DMA_LISR_TCIF0 | DMA_LISR_TEIF0))) {
            if (--t == 0u) {
                ael_mailbox_fail(ERR_TIMEOUT, 0u);
                while (1) {}
            }
        }
    }

    if (DMA2_LISR & DMA_LISR_TEIF0) {
        ael_mailbox_fail(ERR_TEIF, 0u);
        while (1) {}
    }

    /* Verify */
    for (uint32_t i = 0u; i < N_WORDS; i++) {
        if (dst[i] != src[i]) {
            ael_mailbox_fail(ERR_MISMATCH, i);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = N_WORDS;

    while (1) {}
}
