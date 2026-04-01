/*
 * STM32F407 — UART DMA Loopback Test
 *
 * USART2 (PD5=TX, PD6=RX, AF7) at 115200 8N1 on 16 MHz HSI.
 * TX path: DMA1 Stream6 Channel4 (M2P)
 * RX path: DMA1 Stream5 Channel4 (P2M)
 *
 * Sends 8 known bytes via DMA, receives via DMA, verifies match.
 * Tests DMA1, USART2 DMA request lines, and memory ↔ peripheral transfer.
 *
 * Wiring: PD5 → PD6 (loopback wire, same as uart_loopback test)
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = 8 (bytes verified)
 *   FAIL: error_code=1 RX DMA timeout
 *         error_code=2 TX DMA error (TEIF6)
 *         error_code=3 RX DMA error (TEIF5)
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

/* ---- GPIOD ---- */
#define GPIOD_BASE   0x40020C00u
#define GPIOD_MODER  (*(volatile uint32_t *)(GPIOD_BASE + 0x00u))
#define GPIOD_AFRL   (*(volatile uint32_t *)(GPIOD_BASE + 0x20u))

/* ---- USART2 ---- */
#define USART2_BASE  0x40004400u
#define USART2_SR    (*(volatile uint32_t *)(USART2_BASE + 0x00u))
#define USART2_DR    (*(volatile uint32_t *)(USART2_BASE + 0x04u))
#define USART2_BRR   (*(volatile uint32_t *)(USART2_BASE + 0x08u))
#define USART2_CR1   (*(volatile uint32_t *)(USART2_BASE + 0x0Cu))
#define USART2_CR3   (*(volatile uint32_t *)(USART2_BASE + 0x14u))

/* ---- DMA1 ---- */
#define DMA1_BASE    0x40026000u
#define DMA1_LISR    (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_HISR    (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_LIFCR   (*(volatile uint32_t *)(DMA1_BASE + 0x08u))
#define DMA1_HIFCR   (*(volatile uint32_t *)(DMA1_BASE + 0x0Cu))

/* Stream 5 (RX): base + 0x88 */
#define DMA1_S5CR    (*(volatile uint32_t *)(DMA1_BASE + 0x88u + 0x00u))
#define DMA1_S5NDTR  (*(volatile uint32_t *)(DMA1_BASE + 0x88u + 0x04u))
#define DMA1_S5PAR   (*(volatile uint32_t *)(DMA1_BASE + 0x88u + 0x08u))
#define DMA1_S5M0AR  (*(volatile uint32_t *)(DMA1_BASE + 0x88u + 0x0Cu))

/* Stream 6 (TX): base + 0xA0 */
#define DMA1_S6CR    (*(volatile uint32_t *)(DMA1_BASE + 0xA0u + 0x00u))
#define DMA1_S6NDTR  (*(volatile uint32_t *)(DMA1_BASE + 0xA0u + 0x04u))
#define DMA1_S6PAR   (*(volatile uint32_t *)(DMA1_BASE + 0xA0u + 0x08u))
#define DMA1_S6M0AR  (*(volatile uint32_t *)(DMA1_BASE + 0xA0u + 0x0Cu))

/* HISR bits — stream 5: bits [11:6], stream 6: bits [21:16] */
#define TCIF5   (1u << 11)
#define TEIF5   (1u << 9)
#define TCIF6   (1u << 21)
#define TEIF6   (1u << 19)

/* DMA CR bits */
#define DMA_CR_CHSEL(n)  ((n) << 25)   /* channel select [27:25] */
#define DMA_CR_DIR_P2M   (0u << 6)
#define DMA_CR_DIR_M2P   (1u << 6)
#define DMA_CR_MINC      (1u << 10)
#define DMA_CR_EN        (1u << 0)

#define LEN  8u

static volatile uint8_t tx_buf[LEN] = {0xA1u, 0xB2u, 0xC3u, 0xD4u,
                                        0xE5u, 0xF6u, 0x07u, 0x18u};
static volatile uint8_t rx_buf[LEN];

int main(void)
{
    ael_mailbox_init();

    /* ---- Clocks: GPIOD, DMA1, USART2 ---- */
    RCC_AHB1ENR |= (1u << 3)  /* GPIODEN */
                |  (1u << 21); /* DMA1EN */
    RCC_APB1ENR |= (1u << 17); /* USART2EN */
    (void)RCC_APB1ENR;         /* ensure clock active */

    /* ---- PD5=TX AF7, PD6=RX AF7 ---- */
    GPIOD_MODER &= ~(0xFu << 10);
    GPIOD_MODER |=  (0xAu << 10);   /* AF mode for PD5,PD6 */
    GPIOD_AFRL  &= ~(0xFFu << 20);
    GPIOD_AFRL  |=  (0x77u << 20);  /* AF7 for PD5[23:20], PD6[27:24] */

    /* ---- USART2: 115200 8N1, DMA enable on TX+RX ---- */
    /* BRR = 16MHz / (16 * 115200) => USARTDIV=8.68 => BRR=0x8B */
    USART2_BRR = 0x8Bu;
    USART2_CR3 = (1u << 7) | (1u << 6); /* DMAT=1, DMAR=1 */
    USART2_CR1 = (1u << 13) | (1u << 3) | (1u << 2); /* UE|TE|RE */

    /* ---- DMA1 Stream5 (RX, P2M, CH4) ---- */
    DMA1_S5CR   = 0u; /* disable, reset */
    /* Clear any leftover flags */
    DMA1_HIFCR |= TCIF5 | TEIF5 | (1u << 10) | (1u << 8) | (1u << 6);
    DMA1_S5PAR  = (uint32_t)&USART2_DR;
    DMA1_S5M0AR = (uint32_t)rx_buf;
    DMA1_S5NDTR = LEN;
    DMA1_S5CR   = DMA_CR_CHSEL(4u) | DMA_CR_DIR_P2M | DMA_CR_MINC | DMA_CR_EN;

    /* ---- DMA1 Stream6 (TX, M2P, CH4) ---- */
    DMA1_S6CR   = 0u;
    DMA1_HIFCR |= TCIF6 | TEIF6 | (1u << 20) | (1u << 18) | (1u << 16);
    DMA1_S6PAR  = (uint32_t)&USART2_DR;
    DMA1_S6M0AR = (uint32_t)tx_buf;
    DMA1_S6NDTR = LEN;
    DMA1_S6CR   = DMA_CR_CHSEL(4u) | DMA_CR_DIR_M2P | DMA_CR_MINC | DMA_CR_EN;

    /* ---- Wait for RX DMA complete ---- */
    uint32_t timeout = 2000000u;   /* ~1 s at 16 MHz */
    while (!(DMA1_HISR & TCIF5)) {
        if (DMA1_HISR & TEIF6) { ael_mailbox_fail(2u, 0u); while (1) {} }
        if (DMA1_HISR & TEIF5) { ael_mailbox_fail(3u, 0u); while (1) {} }
        if (--timeout == 0u)   { ael_mailbox_fail(1u, 0u); while (1) {} }
    }

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
