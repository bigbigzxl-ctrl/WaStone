/*
 * stm32h563rgt6_lpuart_mailbox — LPUART1 loopback self-test
 *
 * LPUART1 transmits 4 bytes and receives them back via an external loopback
 * wire on the DUT pins.
 *
 * Pins (alternate function AF8):
 *   PA2 → LPUART1_TX   (AF8)
 *   PA3 → LPUART1_RX   (AF8)
 *   Bench wire: PA2 → PA3
 *
 * Clock: HSI = 64 MHz (default after reset)
 * LPUART BRR: 256 × f_clk / baud = 256 × 64_000_000 / 115200 = 142_222
 *
 * LPUART1_BASE = APB3 + 0x2400 = 0x44002400
 * RCC_APB3ENR  = RCC_BASE + 0x0A8, bit6 = LPUART1EN
 * GPIOA_BASE   = AHB2 + 0 (offset 0 from AHB2PERIPH_NS = 0x42020000)
 *
 * LPUART registers (same layout as USART):
 *   CR1  +0x00: bit0=UE, bit2=RE, bit3=TE
 *   CR2  +0x04
 *   CR3  +0x08
 *   BRR  +0x0C: 20-bit value (256× oversampling)
 *   ISR  +0x1C: bit5=RXNE (read not empty), bit6=TC (TX complete), bit7=TXE
 *   RDR  +0x24: receive data (8-bit)
 *   TDR  +0x28: transmit data (8-bit)
 *
 * FAIL codes:
 *   0xE001 — TX timeout (TXE never set)
 *   0xE002 — RX timeout (RXNE never set), detail0 = byte index
 *   0xE003 — data mismatch, detail0 = (expected<<8)|received
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

/* GPIOA (AHB2, first peripheral at offset 0) */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20u))

/* LPUART1 */
#define LPUART1_BASE    0x44002400u
#define LPUART_CR1      (*(volatile uint32_t *)(LPUART1_BASE + 0x00u))
#define LPUART_BRR      (*(volatile uint32_t *)(LPUART1_BASE + 0x0Cu))
#define LPUART_ISR      (*(volatile uint32_t *)(LPUART1_BASE + 0x1Cu))
#define LPUART_ICR      (*(volatile uint32_t *)(LPUART1_BASE + 0x20u))
#define LPUART_RDR      (*(volatile uint32_t *)(LPUART1_BASE + 0x24u))
#define LPUART_TDR      (*(volatile uint32_t *)(LPUART1_BASE + 0x28u))

#define LPUART_CR1_UE   (1u << 0)
#define LPUART_CR1_RE   (1u << 2)
#define LPUART_CR1_TE   (1u << 3)
#define LPUART_ISR_RXNE (1u << 5)
#define LPUART_ISR_TC   (1u << 6)
#define LPUART_ISR_TXE  (1u << 7)

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

    /* 2. Configure PA2 and PA3 as AF8 (LPUART1_TX / LPUART1_RX)
     *    MODER[5:4] = 10 (AF), MODER[7:6] = 10 (AF)
     *    AFRL bits[11:8]=AF8 for PA2, bits[15:12]=AF8 for PA3
     */
    GPIOA_MODER = (GPIOA_MODER & ~(0xFu << 4)) | (0xAu << 4);  /* PA2,PA3 = AF */
    GPIOA_AFRL  = (GPIOA_AFRL  & ~(0xFFu << 8)) | (0x88u << 8); /* AF8 for PA2,PA3 */

    /* 3. Enable LPUART1 clock (APB3ENR bit6) */
    RCC_APB3ENR |= (1u << 6);
    (void)RCC_APB3ENR;

    /* 4. Configure LPUART1: 115200 baud, 8N1 */
    LPUART_CR1 = 0u;               /* disable while configuring */
    LPUART_BRR = LPUART_BRR_VAL;
    LPUART_CR1 = LPUART_CR1_UE | LPUART_CR1_TE | LPUART_CR1_RE;

    /* 5. Send 4 bytes and receive them back */
    for (uint32_t i = 0u; i < 4u; i++) {
        /* Wait for TXE (transmit register empty) */
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (LPUART_ISR & LPUART_ISR_TXE) break;
        }
        if (t == TIMEOUT) {
            ael_mailbox_fail(0xE001u, i);
            while (1) {}
        }
        LPUART_TDR = tx_data[i];

        /* Wait for RXNE */
        for (t = 0u; t < TIMEOUT; t++) {
            if (LPUART_ISR & LPUART_ISR_RXNE) break;
        }
        if (t == TIMEOUT) {
            ael_mailbox_fail(0xE002u, i);
            while (1) {}
        }
        uint8_t rx = (uint8_t)(LPUART_RDR & 0xFFu);
        if (rx != tx_data[i]) {
            ael_mailbox_fail(0xE003u, ((uint32_t)tx_data[i] << 8) | rx);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = 4u;   /* 4 bytes verified */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
