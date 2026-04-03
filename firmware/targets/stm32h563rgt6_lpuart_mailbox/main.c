/*
 * stm32h563rgt6_lpuart_mailbox — LPUART1 TX + register accessibility self-test
 *
 * Verifies LPUART1 peripheral: clock enable, GPIO AF config, BRR, and TX path.
 * Sends 4 bytes and verifies Transmission Complete (TC) fires for each.
 * No external loopback wire required.
 *
 * Pin (alternate function AF8):
 *   PA2 → LPUART1_TX (AF8) — driven low during TX, idle high
 *
 * Clock: HSI = 64 MHz (default after reset)
 * LPUART BRR: 256 × f_clk / baud = 256 × 64_000_000 / 115200 = 142_222
 *
 * LPUART1_BASE = APB3 + 0x2400 = 0x44002400
 * RCC_APB3ENR  = RCC_BASE + 0x0A8, bit6 = LPUART1EN
 * GPIOA_BASE   = AHB2PERIPH_NS = 0x42020000
 *
 * LPUART registers:
 *   CR1  +0x00: bit0=UE, bit3=TE
 *   CR3  +0x08
 *   BRR  +0x0C: 20-bit value (256× oversampling)
 *   ISR  +0x1C: bit6=TC (TX complete), bit7=TXE (transmit data reg empty)
 *   ICR  +0x20: write-1-to-clear
 *   TDR  +0x28: transmit data (8-bit)
 *
 * FAIL codes:
 *   0xE001 — TXE timeout (LPUART TX not draining), detail0 = byte index
 *   0xE002 — TC timeout (transmission never completed), detail0 = byte index
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20u))

/* LPUART1 */
#define LPUART1_BASE    0x44002400u
#define LPUART_CR1      (*(volatile uint32_t *)(LPUART1_BASE + 0x00u))
#define LPUART_BRR      (*(volatile uint32_t *)(LPUART1_BASE + 0x0Cu))
#define LPUART_ISR      (*(volatile uint32_t *)(LPUART1_BASE + 0x1Cu))
#define LPUART_ICR      (*(volatile uint32_t *)(LPUART1_BASE + 0x20u))
#define LPUART_TDR      (*(volatile uint32_t *)(LPUART1_BASE + 0x28u))

#define LPUART_CR1_UE   (1u << 0)
#define LPUART_CR1_TE   (1u << 3)
#define LPUART_ISR_TC   (1u << 6)
#define LPUART_ISR_TXE  (1u << 7)
#define LPUART_ICR_TCCF (1u << 6)  /* clear TC flag */

/* BRR = 256 * 64_000_000 / 115200 = 142_222 */
#define LPUART_BRR_VAL  142222u

#define TIMEOUT  1000000u

static const uint8_t tx_data[4] = {0x4Cu, 0x50u, 0x55u, 0xAAu};

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPIOA clock (AHB2ENR bit0) */
    RCC_AHB2ENR |= (1u << 0);
    (void)RCC_AHB2ENR;

    /* 2. Configure PA2 as AF8 (LPUART1_TX)
     *    MODER[5:4] = 10 (AF mode), AFRL[11:8] = 0x8 (AF8)
     */
    GPIOA_MODER = (GPIOA_MODER & ~(0x3u << 4)) | (0x2u << 4);
    GPIOA_AFRL  = (GPIOA_AFRL  & ~(0xFu << 8)) | (0x8u << 8);

    /* 3. Enable LPUART1 clock (APB3ENR bit6) */
    RCC_APB3ENR |= (1u << 6);
    (void)RCC_APB3ENR;

    /* 4. Configure LPUART1: 115200 baud, 8N1, TX only */
    LPUART_CR1 = 0u;               /* disable while configuring */
    LPUART_BRR = LPUART_BRR_VAL;
    LPUART_ICR = LPUART_ICR_TCCF;  /* clear any stale TC flag */
    LPUART_CR1 = LPUART_CR1_UE | LPUART_CR1_TE;

    /* 5. Send 4 bytes and verify TC fires for each */
    for (uint32_t i = 0u; i < 4u; i++) {
        /* Wait for TXE (transmit data register empty) */
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (LPUART_ISR & LPUART_ISR_TXE) break;
        }
        if (t == TIMEOUT) {
            ael_mailbox_fail(0xE001u, i);
            while (1) {}
        }

        LPUART_ICR = LPUART_ICR_TCCF;  /* clear TC before writing new byte */
        LPUART_TDR = tx_data[i];

        /* Wait for TC (transmission complete) */
        for (t = 0u; t < TIMEOUT; t++) {
            if (LPUART_ISR & LPUART_ISR_TC) break;
        }
        if (t == TIMEOUT) {
            ael_mailbox_fail(0xE002u, i);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = 4u;   /* 4 bytes transmitted */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
