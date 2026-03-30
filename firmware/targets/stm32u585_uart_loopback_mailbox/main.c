/*
 * stm32u585_uart_loopback_mailbox — USART1 TX→RX loopback test
 * STM32U585CIU6, MSI 4MHz
 *
 * Wire: PA9 (TX) → PA10 (RX)
 *
 * Sends 4 bytes, verifies echo. Uses polling.
 * BRR = 4MHz / 9600 = 417.
 *
 * FAIL codes:
 *   0xE001 — TXE timeout
 *   0xE002 — RXNE timeout
 *   0xE003 — data mismatch (detail0 = [expected<<8 | received])
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

/* USART1 (APB2) */
#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_RDR      (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define USART1_ISR_TXE    (1u << 7u)
#define USART1_ISR_RXNE   (1u << 5u)

#define TIMEOUT         500000u

static const uint8_t tx_data[4] = {0xA5u, 0x5Au, 0xC3u, 0x3Cu};

int main(void)
{
    ael_mailbox_init();

    /* Clocks */
    RCC_AHB2ENR1 |= (1u << 0u);     /* GPIOAEN */
    RCC_APB2ENR  |= (1u << 14u);    /* USART1EN */
    volatile uint32_t dummy = RCC_APB2ENR;
    (void)dummy;

    /* PA9 = TX (AF7), PA10 = RX (AF7) — both in AFRH */
    /* PA9: MODER[19:18]=10, OTYPER=0, OSPEEDR[19:18]=11, AFRH[7:4]=0111 */
    GPIOA_MODER   &= ~((3u << 18u) | (3u << 20u));
    GPIOA_MODER   |=  ((2u << 18u) | (2u << 20u));
    GPIOA_OTYPER  &= ~((1u <<  9u) | (1u << 10u));
    GPIOA_OSPEEDR |=  ((3u << 18u) | (3u << 20u));
    GPIOA_AFRH    &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH    |=  ((7u   << 4u) | (7u   << 8u));

    /* USART1: 9600 baud @ 4MHz */
    USART1_BRR = 417u;
    USART1_CR1 = (1u << 3u) | (1u << 2u) | (1u << 0u); /* TE | RE | UE */

    for (uint32_t i = 0u; i < 4u; i++) {
        uint8_t tx = tx_data[i];

        /* Wait TXE */
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (USART1_ISR & USART1_ISR_TXE) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE001u, i);
            while (1) {}
        }

        USART1_TDR = tx;

        /* Wait RXNE */
        for (t = 0u; t < TIMEOUT; t++) {
            if (USART1_ISR & USART1_ISR_RXNE) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE002u, i);
            while (1) {}
        }

        uint8_t rx = (uint8_t)(USART1_RDR & 0xFFu);
        if (rx != tx) {
            AEL_MAILBOX->detail0 = ((uint32_t)tx << 8u) | rx;
            ael_mailbox_fail(0xE003u, ((uint32_t)tx << 8u) | rx);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
