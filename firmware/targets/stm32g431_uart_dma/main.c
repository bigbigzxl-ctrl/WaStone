#include <stdint.h>
#include "../ael_mailbox.h"

/* STM32G431 — RCC */
#define RCC_BASE       0x40021000u
#define RCC_AHB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x48u))
#define RCC_AHB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x4Cu))
#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x60u))

/* GPIOA */
#define GPIOA_BASE     0x48000000u
#define GPIOA_MODER    (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER   (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR  (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_ODR      (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))
#define GPIOA_AFRH     (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

/* USART1 (APB2) — G431 new-style register map */
#define USART1_BASE    0x40013800u
#define USART1_CR1     (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_CR3     (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_BRR     (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR     (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_RDR_ADDR (USART1_BASE + 0x24u)
#define USART1_TDR_ADDR (USART1_BASE + 0x28u)

/* DMA1 — AHB1 peripheral */
#define DMA1_BASE      0x40020000u
#define DMA1_ISR       (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR      (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
/* CH4 (TX: mem→periph) at base + 0x44 */
#define DMA1_CCR4      (*(volatile uint32_t *)(DMA1_BASE + 0x44u))
#define DMA1_CNDTR4    (*(volatile uint32_t *)(DMA1_BASE + 0x48u))
#define DMA1_CPAR4     (*(volatile uint32_t *)(DMA1_BASE + 0x4Cu))
#define DMA1_CMAR4     (*(volatile uint32_t *)(DMA1_BASE + 0x50u))
/* CH5 (RX: periph→mem) at base + 0x58 */
#define DMA1_CCR5      (*(volatile uint32_t *)(DMA1_BASE + 0x58u))
#define DMA1_CNDTR5    (*(volatile uint32_t *)(DMA1_BASE + 0x5Cu))
#define DMA1_CPAR5     (*(volatile uint32_t *)(DMA1_BASE + 0x60u))
#define DMA1_CMAR5     (*(volatile uint32_t *)(DMA1_BASE + 0x64u))

/* DMAMUX1 — channel request registers (each 4 bytes, 0-indexed maps to DMA1_CH1...) */
/* DMA1_CH4 → DMAMUX1 channel 3 → C3CR at +0x0C */
/* DMA1_CH5 → DMAMUX1 channel 4 → C4CR at +0x10 */
#define DMAMUX1_BASE   0x40020800u
#define DMAMUX1_C3CR   (*(volatile uint32_t *)(DMAMUX1_BASE + 0x0Cu))
#define DMAMUX1_C4CR   (*(volatile uint32_t *)(DMAMUX1_BASE + 0x10u))

/* DMAMUX1 request IDs — RM0440 Table 91 */
#define DMAMUX_REQ_USART1_RX  24u
#define DMAMUX_REQ_USART1_TX  25u

/* DMA1 ISR flags: TCIF4=bit13, TCIF5=bit17, TEIF4=bit15, TEIF5=bit19 */
#define DMA1_ISR_TCIF4  (1u << 13)
#define DMA1_ISR_TCIF5  (1u << 17)
#define DMA1_ISR_TEIF4  (1u << 15)
#define DMA1_ISR_TEIF5  (1u << 19)

#define TX_LEN  8u

static const uint8_t tx_buf[TX_LEN] = {0xA1, 0xB2, 0xC3, 0xD4, 0x55, 0x66, 0x77, 0x88};
static volatile uint8_t rx_buf[TX_LEN];

static void gpioa_set_af_high(uint32_t pin, uint32_t af)
{
    /* pin must be 8-15 (uses AFRH) */
    const uint32_t sh   = pin * 2u;
    const uint32_t afsh = (pin - 8u) * 4u;
    GPIOA_MODER  &= ~(3u << sh);
    GPIOA_MODER  |=  (2u << sh);   /* AF mode */
    GPIOA_OTYPER &= ~(1u << pin);  /* push-pull */
    GPIOA_OSPEEDR|=  (3u << sh);   /* high speed */
    GPIOA_AFRH   &= ~(0xFu << afsh);
    GPIOA_AFRH   |=  (af   << afsh);
}

int main(void)
{
    uint32_t timeout;

    /* Clocks: GPIOA on AHB2, USART1 on APB2, DMA1 on AHB1 */
    RCC_AHB2ENR |= (1u << 0);    /* GPIOAEN */
    RCC_APB2ENR |= (1u << 14);   /* USART1EN */
    RCC_AHB1ENR |= (1u << 0) | (1u << 2);  /* DMA1EN | DMAMUX1EN (G431: separate RCC bit) */
    (void)RCC_AHB1ENR;           /* pipeline flush */

    /* PA9 = USART1_TX AF7, PA10 = USART1_RX AF7 */
    gpioa_set_af_high(9u, 7u);
    gpioa_set_af_high(10u, 7u);

    /* PA2 = signal output (set LOW initially) */
    GPIOA_MODER  &= ~(3u << 4);
    GPIOA_MODER  |=  (1u << 4);
    GPIOA_OTYPER &= ~(1u << 2);
    GPIOA_OSPEEDR|=  (3u << 4);
    GPIOA_ODR    &= ~(1u << 2);

    /* DMAMUX: DMA1_CH4 = USART1_TX (req 25), DMA1_CH5 = USART1_RX (req 24) */
    DMAMUX1_C3CR = DMAMUX_REQ_USART1_TX;
    DMAMUX1_C4CR = DMAMUX_REQ_USART1_RX;

    /* DMA1 CH5 (RX): periph→mem, MINC, byte, PL=medium */
    DMA1_CCR5  = 0u;
    DMA1_IFCR  = 0xFFFFFFFFu;               /* clear all flags */
    DMA1_CPAR5 = USART1_RDR_ADDR;
    DMA1_CMAR5 = (uint32_t)((void *)rx_buf);
    DMA1_CNDTR5= TX_LEN;
    /* CCR5: MINC=bit7, PL=01 at bits13:12, EN=bit0 */
    DMA1_CCR5  = (1u << 7) | (1u << 12) | (1u << 0);

    /* USART1: BRR=139 (115200 @ 16 MHz), enable DMA TX+RX in CR3 before UE */
    USART1_BRR = 139u;
    USART1_CR3 = (1u << 7) | (1u << 6);    /* DMAT | DMAR */
    USART1_CR1 = (1u << 0) | (1u << 3) | (1u << 2); /* UE | TE | RE */

    /* DMA1 CH4 (TX): mem→periph, DIR, MINC, byte, PL=medium */
    DMA1_CCR4  = 0u;
    DMA1_CPAR4 = USART1_TDR_ADDR;
    DMA1_CMAR4 = (uint32_t)((const void *)tx_buf);
    DMA1_CNDTR4= TX_LEN;
    /* CCR4: DIR=bit4, MINC=bit7, PL=01 at bits13:12, EN=bit0 */
    DMA1_CCR4  = (1u << 4) | (1u << 7) | (1u << 12) | (1u << 0);

    /* Wait for RX transfer complete (TCIF5 = DMA1_ISR bit17) */
    timeout = 2000000u;
    while (((DMA1_ISR & DMA1_ISR_TCIF5) == 0u) &&
           ((DMA1_ISR & DMA1_ISR_TEIF5) == 0u) &&
           timeout-- > 0u) {}

    ael_mailbox_init();

    if ((DMA1_ISR & DMA1_ISR_TEIF5) != 0u) {
        /* DMA transfer error */
        ael_mailbox_fail(0xE003u, 0u);
    } else if ((DMA1_ISR & DMA1_ISR_TCIF5) == 0u) {
        /* Timeout — no data received */
        ael_mailbox_fail(0xE001u, 0u);
    } else {
        /* Compare TX and RX buffers */
        uint8_t ok = 1u;
        uint32_t first_mismatch = 0u;
        for (uint32_t i = 0u; i < TX_LEN; i++) {
            if (rx_buf[i] != tx_buf[i]) {
                ok = 0u;
                first_mismatch = (uint32_t)i;
                break;
            }
        }
        if (ok != 0u) {
            ael_mailbox_pass();
            GPIOA_ODR |= (1u << 2);  /* PA2 HIGH on pass */
        } else {
            ael_mailbox_fail(0xE002u,
                ((uint32_t)first_mismatch << 16) |
                ((uint32_t)rx_buf[first_mismatch] << 8) |
                tx_buf[first_mismatch]);
        }
    }

    while (1) {}
}
