/*
 * stm32h563rgt6_uart_loopback
 *
 * USART1 loopback: PA9 (TX, AF7) → wire → PA10 (RX, AF7).
 * Sends {0x55, 0xAA, 0x12, 0x34}, verifies echo byte-by-byte.
 * Baud: 115200 @ 64 MHz HSI → BRR = 556.
 *
 * Clocks: USART1 = APB2 bit14, GPIOA = AHB2 bit0.
 *
 * FAIL codes:
 *   0xE001 — RXNE timeout
 *   0xE002 — byte mismatch, detail0 = (expected<<8)|received
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))  /* PA8-PA15 */

#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_RDR      (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_ISR_RXNE  (1u << 5)
#define USART_ISR_TXE   (1u << 7)

/* BRR = 64000000 / 115200 = 556 */
#define BRR_115200_64M  556u

#define TIMEOUT  1000000u

static const uint8_t tx_data[4] = {0x55u, 0xAAu, 0x12u, 0x34u};

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPIOA (AHB2 bit0), USART1 (APB2 bit14) */
    RCC_AHB2ENR |= (1u << 0);
    RCC_APB2ENR  |= (1u << 14);
    (void)RCC_AHB2ENR;
    (void)RCC_APB2ENR;

    /* 2. PA9=AF7 (TX), PA10=AF7 (RX)
     *    MODER bits[19:18]=10, [21:20]=10
     *    AFRH bits[7:4]=0111 (PA9), [11:8]=0111 (PA10) */
    GPIOA_MODER   = (GPIOA_MODER  & ~(0xFu << 18)) | (0xAu << 18);
    GPIOA_OSPEEDR = (GPIOA_OSPEEDR & ~(0xFu << 18)) | (0xAu << 18);
    GPIOA_AFRH    = (GPIOA_AFRH   & ~(0xFFu << 4))  | (0x77u << 4);

    /* 3. Configure USART1: 115200, 8N1, TE+RE+UE */
    USART1_CR1 = 0u;
    USART1_BRR = BRR_115200_64M;
    USART1_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    /* 4. Send and receive each byte */
    for (uint32_t i = 0; i < 4u; i++) {
        /* Wait TXE then send */
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (USART1_ISR & USART_ISR_TXE) break;
        }
        USART1_TDR = tx_data[i];

        /* Wait RXNE */
        for (t = 0; t < TIMEOUT; t++) {
            if (USART1_ISR & USART_ISR_RXNE) break;
        }
        if (!(USART1_ISR & USART_ISR_RXNE)) {
            ael_mailbox_fail(0xE001u, i);
            while (1) {}
        }
        uint32_t received = USART1_RDR & 0xFFu;
        if (received != tx_data[i]) {
            ael_mailbox_fail(0xE002u, ((uint32_t)tx_data[i] << 8) | received);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = 4u;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
