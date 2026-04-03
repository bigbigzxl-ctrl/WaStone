/*
 * stm32h563rgt6_uart_banner
 *
 * Tries USART1 (PA9 TX / PA10 RX, AF7) first, then USART3 (PB10 TX / PB11 RX, AF7).
 * Both are initialised; both transmit the banner string repeatedly.
 * Mailbox is set to PASS immediately so the host can confirm firmware ran.
 *
 * Banner: "AEL_READY STM32H563RGT6\r\n"
 * Baud: 115200, HSI 64 MHz → BRR = 556
 *
 * Observe /dev/ttyACM1 at 115200 to confirm which UART the DAPLink bridges.
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

/* GPIOA (AHB2) */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))  /* PA8-PA15 */

/* GPIOB (AHB2) */
#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OSPEEDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_AFRH      (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))  /* PB8-PB15 */

/* USART1 (APB2) — PA9(TX)/PA10(RX), AF7 */
#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

/* USART3 (APB1) — PB10(TX)/PB11(RX), AF7 */
#define USART3_BASE     0x40004800u
#define USART3_CR1      (*(volatile uint32_t *)(USART3_BASE + 0x00u))
#define USART3_BRR      (*(volatile uint32_t *)(USART3_BASE + 0x0Cu))
#define USART3_ISR      (*(volatile uint32_t *)(USART3_BASE + 0x1Cu))
#define USART3_TDR      (*(volatile uint32_t *)(USART3_BASE + 0x28u))

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_TE    (1u << 3)
#define USART_ISR_TXE   (1u << 7)   /* TXE: TX data register empty */
#define USART_ISR_TC    (1u << 6)   /* TC: transmission complete */

/* BRR for 115200 at 64 MHz HSI: 64000000/115200 = 555.5 → 556 */
#define BRR_115200_64M  556u
/* BRR for 9600 at 64 MHz HSI: 64000000/9600 = 6666.7 → 6667 */
#define BRR_9600_64M    6667u

static void usart_putc(volatile uint32_t *isr, volatile uint32_t *tdr, char c)
{
    while (!(*isr & USART_ISR_TXE)) {}
    *tdr = (uint8_t)c;
}

static void usart_puts(volatile uint32_t *isr, volatile uint32_t *tdr, const char *s)
{
    while (*s) {
        usart_putc(isr, tdr, *s++);
    }
}

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPIOA, GPIOB clocks (AHB2 bits 0, 1) */
    RCC_AHB2ENR |= (1u << 0) | (1u << 1);
    (void)RCC_AHB2ENR;

    /* 2. Enable USART1 (APB2 bit14), USART3 (APB1L bit18) */
    RCC_APB2ENR  |= (1u << 14);
    RCC_APB1LENR |= (1u << 18);
    (void)RCC_APB2ENR;
    (void)RCC_APB1LENR;

    /* 3. PA9 = AF7 (USART1_TX), PA10 = AF7 (USART1_RX)
     *    MODER: bits[19:18]=10 (AF), bits[21:20]=10 (AF)
     *    AFRH: [7:4]=0111 (AF7 for PA9), [11:8]=0111 (AF7 for PA10) */
    GPIOA_MODER  = (GPIOA_MODER  & ~(0xFu << 18)) | (0xAu << 18);
    GPIOA_OSPEEDR = (GPIOA_OSPEEDR & ~(0xFu << 18)) | (0xAu << 18);  /* high speed */
    GPIOA_AFRH   = (GPIOA_AFRH   & ~(0xFFu << 4))  | (0x77u << 4);

    /* 4. PB10 = AF7 (USART3_TX), PB11 = AF7 (USART3_RX)
     *    MODER: bits[21:20]=10 (AF), bits[23:22]=10 (AF)
     *    AFRH: [11:8]=0111 (AF7 for PB10), [15:12]=0111 (AF7 for PB11) */
    GPIOB_MODER  = (GPIOB_MODER  & ~(0xFu << 20)) | (0xAu << 20);
    GPIOB_OSPEEDR = (GPIOB_OSPEEDR & ~(0xFu << 20)) | (0xAu << 20);
    GPIOB_AFRH   = (GPIOB_AFRH   & ~(0xFFu << 8))  | (0x77u << 8);

    /* 5. Configure USART1 at both 115200 and 9600 (try 9600 first to find DAPLink baud) */
    USART1_CR1 = 0u;
    USART1_BRR = BRR_9600_64M;
    USART1_CR1 = USART_CR1_TE | USART_CR1_UE;

    /* 6. Configure USART3 at 9600 */
    USART3_CR1 = 0u;
    USART3_BRR = BRR_9600_64M;
    USART3_CR1 = USART_CR1_TE | USART_CR1_UE;

    /* 7. Write mailbox PASS — firmware is running */
    ael_mailbox_pass();

    /* 8. Transmit banner on both UARTs repeatedly */
    const char *banner = "AEL_READY STM32H563RGT6\r\n";
    while (1) {
        usart_puts(&USART1_ISR, &USART1_TDR, banner);
        usart_puts(&USART3_ISR, &USART3_TDR, banner);
        /* ~500ms delay */
        for (volatile uint32_t d = 0; d < 3000000u; d++) {}
    }
    return 0;
}
