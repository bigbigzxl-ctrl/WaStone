/*
 * stm32h750_uart_dma — USART1 DMA loopback test
 *
 * DMA1 Stream 0 (RX, P→M) and Stream 1 (TX, M→P) transfer 8 bytes
 * through USART1, looped back via bench wire PA9↔PA10. DMAMUX1
 * routes: C0→USART1_RX (req 41), C1→USART1_TX (req 42).
 *
 * Key H750 differences vs STM32G4 DMA:
 *   - DMA is stream-based (8 streams per controller), not channel-based.
 *   - DMAMUX1 requires NO separate RCC clock enable on H750 — it is
 *     always enabled alongside DMA1/DMA2 (no DMAMUX1EN bit in H750).
 *   - Stream register stride: 0x18 bytes per stream (CR+NDTR+PAR+M0AR+M1AR+FCR).
 *   - TCIF in LISR: Stream0=bit5, Stream1=bit11.
 *
 * DMA1_BASE     = 0x40020000 (D2 AHB1, RM0433 §15)
 * DMAMUX1_BASE  = 0x40020800 (D2 AHB1, no separate RCC enable on H750)
 * USART1_BASE   = 0x40011000 (D2 APB2, RM0433 §53)
 * USART1 RDR    = USART1_BASE + 0x24 (DMA peripheral source for RX)
 * USART1 TDR    = USART1_BASE + 0x28 (DMA peripheral target for TX)
 * BRR = 556 for 115200 @ 64 MHz APB2 (from existing uart_loopback).
 *
 * Error codes (in error_code):
 *   0xE001 = DMA RX timeout (TCIF0 never set after TX complete)
 *   0xE002 = data mismatch (detail0 = first mismatch idx << 16 | rx_byte)
 *   0xE003 = DMA transfer error (TEIF set)
 *
 * All register addresses from RM0433 + stm32h750xx.h (cmsis_device_h7).
 */

#define AEL_MAILBOX_ADDR  0x38000000u
#include "../ael_mailbox.h"

/* ── RCC ───────────────────────────────────────────────────────── */
#define RCC_BASE             0x58024400u
#define RCC_AHB1ENR          (*(volatile uint32_t *)(RCC_BASE + 0x0D8u))
#define RCC_AHB4ENR          (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_APB2ENR          (*(volatile uint32_t *)(RCC_BASE + 0x0F0u))
#define RCC_AHB1ENR_DMA1EN   (1u << 0)
#define RCC_AHB4ENR_GPIOAEN  (1u << 0)
#define RCC_APB2ENR_USART1EN (1u << 4)

/* ── GPIOA (base 0x58020000) ──────────────────────────────────── */
#define GPIOA_BASE  0x58020000u
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH  (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

/* ── USART1 (APB2, base 0x40011000) ──────────────────────────── */
#define USART1_BASE  0x40011000u
#define USART1_CR1   (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_CR3   (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_BRR   (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_RDR_ADDR  (USART1_BASE + 0x24u)  /* peripheral address for DMA RX */
#define USART1_TDR_ADDR  (USART1_BASE + 0x28u)  /* peripheral address for DMA TX */

#define USART_CR1_UE   (1u << 0)
#define USART_CR1_RE   (1u << 2)
#define USART_CR1_TE   (1u << 3)
#define USART_CR3_DMAR (1u << 6)
#define USART_CR3_DMAT (1u << 7)

/* ── DMA1 (D2 AHB1, base 0x40020000) ─────────────────────────── */
#define DMA1_BASE    0x40020000u
#define DMA1_LISR    (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_LIFCR   (*(volatile uint32_t *)(DMA1_BASE + 0x08u))

/* Stream 0 registers (offset 0x10 + 0×0x18 = 0x10) */
#define DMA1_S0CR    (*(volatile uint32_t *)(DMA1_BASE + 0x10u))
#define DMA1_S0NDTR  (*(volatile uint32_t *)(DMA1_BASE + 0x14u))
#define DMA1_S0PAR   (*(volatile uint32_t *)(DMA1_BASE + 0x18u))
#define DMA1_S0M0AR  (*(volatile uint32_t *)(DMA1_BASE + 0x1Cu))

/* Stream 1 registers (offset 0x10 + 1×0x18 = 0x28) */
#define DMA1_S1CR    (*(volatile uint32_t *)(DMA1_BASE + 0x28u))
#define DMA1_S1NDTR  (*(volatile uint32_t *)(DMA1_BASE + 0x2Cu))
#define DMA1_S1PAR   (*(volatile uint32_t *)(DMA1_BASE + 0x30u))
#define DMA1_S1M0AR  (*(volatile uint32_t *)(DMA1_BASE + 0x34u))

/* DMA1_LISR bits: Stream0 TCIF=bit5, TEIF=bit3; Stream1 TCIF=bit11, TEIF=bit9 */
#define DMA_TCIF0  (1u << 5)
#define DMA_TEIF0  (1u << 3)
#define DMA_TCIF1  (1u << 11)
#define DMA_TEIF1  (1u << 9)

/* DMA SxCR bits */
#define DMA_SCR_EN    (1u << 0)
#define DMA_SCR_MINC  (1u << 10)
#define DMA_SCR_DIR_M2P (1u << 6)   /* DIR=01: Memory→Peripheral */
/* Priority medium: PL[17:16]=01 */
#define DMA_SCR_PL_MED (1u << 16)

/* ── DMAMUX1 (D2 AHB1, base 0x40020800) ─────────────────────── */
/* Channel n register at DMAMUX1_BASE + 4*n. DMAMUX1 channels 0-7 → DMA1 streams 0-7. */
#define DMAMUX1_C0CR  (*(volatile uint32_t *)0x40020800u)  /* → DMA1 Stream 0 */
#define DMAMUX1_C1CR  (*(volatile uint32_t *)0x40020804u)  /* → DMA1 Stream 1 */
/* Request IDs (RM0433 Table 119 / stm32h7xx_ll_dmamux.h): */
#define DMAMUX_REQ_USART1_RX  41u
#define DMAMUX_REQ_USART1_TX  42u

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

#define TX_LEN 8u
static const uint8_t tx_buf[TX_LEN] = {
    0x55u, 0xAAu, 0x12u, 0x34u, 0x56u, 0x78u, 0xABu, 0xCDu
};
static uint8_t rx_buf[TX_LEN];

int main(void)
{
    uint32_t i;

    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Enable clocks: GPIOA, USART1, DMA1. DMAMUX1 needs NO separate enable on H750. */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    (void)RCC_AHB4ENR;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC_APB2ENR;
    RCC_AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC_AHB1ENR;

    ael_mailbox_init();

    /* ── GPIO: PA9=USART1_TX (AF7), PA10=USART1_RX (AF7) ──────── */
    /* AFRH: PA9 at bits[7:4]=AF7, PA10 at bits[11:8]=AF7 */
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |=  ((7u  << 4u) | (7u  << 8u));
    /* MODER: PA9[19:18]=10(AF), PA10[21:20]=10(AF) */
    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |=  ((0x2u << 18u) | (0x2u << 20u));

    /* ── DMAMUX1: route requests to DMA1 streams 0 and 1 ────────── */
    DMAMUX1_C0CR = DMAMUX_REQ_USART1_RX;  /* Stream 0 ← USART1_RX */
    DMAMUX1_C1CR = DMAMUX_REQ_USART1_TX;  /* Stream 1 ← USART1_TX */

    /* ── DMA1 Stream 0: USART1_RX → rx_buf (P→M, MINC, PL_MED) ── */
    DMA1_S0CR   = 0u;   /* disable before configuring */
    DMA1_S0NDTR = TX_LEN;
    DMA1_S0PAR  = USART1_RDR_ADDR;
    DMA1_S0M0AR = (uint32_t)rx_buf;
    DMA1_S0CR   = DMA_SCR_MINC | DMA_SCR_PL_MED;  /* P→M, MINC, no EN yet */

    /* ── DMA1 Stream 1: tx_buf → USART1_TDR (M→P, MINC, PL_MED) ─ */
    DMA1_S1CR   = 0u;
    DMA1_S1NDTR = TX_LEN;
    DMA1_S1PAR  = USART1_TDR_ADDR;
    DMA1_S1M0AR = (uint32_t)tx_buf;
    DMA1_S1CR   = DMA_SCR_DIR_M2P | DMA_SCR_MINC | DMA_SCR_PL_MED;

    /* ── USART1: configure BRR, CR3 (DMA enable), CR1 (UE+TE+RE) ─ */
    USART1_BRR = 556u;                         /* 115200 @ 64 MHz APB2 */
    USART1_CR3 = USART_CR3_DMAT | USART_CR3_DMAR;  /* enable DMA TX+RX */

    /* Enable DMA streams before enabling USART (avoids missing first byte) */
    DMA1_LIFCR = DMA_TCIF0 | DMA_TEIF0 | DMA_TCIF1 | DMA_TEIF1; /* clear flags */
    DMA1_S0CR |= DMA_SCR_EN;   /* start RX DMA */
    DMA1_S1CR |= DMA_SCR_EN;   /* start TX DMA */

    /* Enable USART (UE last, triggers DMA requests immediately) */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* ── Wait for DMA transfer error or TX complete ─────────────── */
    uint32_t timeout = 5000u;   /* 5 seconds maximum */
    while (timeout-- > 0u) {
        uint32_t lisr = DMA1_LISR;
        if (lisr & (DMA_TEIF0 | DMA_TEIF1)) {
            ael_mailbox_fail(0xE003u, lisr);
            while (1) {}
        }
        if ((lisr & (DMA_TCIF0 | DMA_TCIF1)) == (DMA_TCIF0 | DMA_TCIF1)) {
            break;   /* both streams complete */
        }
        delay_ticks(1u);
    }

    if (timeout == 0u) {
        /* Check which stream timed out */
        ael_mailbox_fail(0xE001u, DMA1_LISR);
        while (1) {}
    }

    /* ── Verify received data ───────────────────────────────────── */
    uint32_t err = 0u;
    for (i = 0u; i < TX_LEN; i++) {
        if (rx_buf[i] != tx_buf[i]) {
            err = 0xE002u;
            ael_mailbox_fail(err, ((uint32_t)i << 16u) | rx_buf[i]);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = TX_LEN;
    while (1) {}
}
