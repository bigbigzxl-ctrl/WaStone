/*
 * CH32V003 UART bidirectional test via WCH-Link CDC bridge
 *
 * USART1 TX = PD5 → WCH-Link RX → /dev/ttyACM0 (TX test)
 * USART1 RX = PD6 ← WCH-Link TX ← /dev/ttyACM0 (RX test)
 *
 * Clock: 24 MHz HSI, APB2 = 8 MHz (HPRE/PPRE2 default = /3)
 * Baud:  115200  (BRR = 0x0045, error < 0.7%)
 *
 * TX: sends "AEL_UART_TX OK <n>\r\n" every ~500 ms (verified by observe_uart)
 * RX: counts received bytes; detail0 = rx_count << 1
 *     (detail0_increment check fires when host sends bytes via /dev/ttyACM0)
 *
 * Mailbox layout:
 *   [0] magic    = 0xAE100001
 *   [1] status   = PASS (2)
 *   [2] error    = 0
 *   [3] detail0  = rx_count << 1  (bits[15:1] = rx_count for toggle_count check)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

/* ── USART helpers ────────────────────────────────────────────────────── */
static void usart_init(void)
{
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPDEN | RCC_USART1EN;
    RCC->APB2PRSTR |=  RCC_USART1RST;
    RCC->APB2PRSTR &= ~RCC_USART1RST;

    /* PD5 = USART1_TX: AF push-pull 50 MHz (nibble = 0xB) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 20)) | (0xBu << 20);
    /* PD6 = USART1_RX: floating input (nibble = 0x4) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 24)) | (0x4u << 24);

    /* 115200 @ 8 MHz: BRR = 0x0045 (~0.7% error) */
    USART1->BRR   = 0x0045;
    /* Enable TX + RX + USART */
    USART1->CTLR1 = USART_CTLR1_TE | USART_CTLR1_RE | USART_CTLR1_UE;
}

static void usart_putc(char c)
{
    while (!(USART1->STATR & USART_STATR_TXE));
    USART1->DATAR = (uint8_t)c;
}

static void usart_puts(const char *s)
{
    while (*s) usart_putc(*s++);
}

/* Simple itoa — repeated subtraction (no division, rv32ec nostdlib) */
static void usart_putu32(uint32_t v)
{
    static const uint32_t pow10[] = {
        1000000000u, 100000000u, 10000000u, 1000000u,
        100000u, 10000u, 1000u, 100u, 10u, 1u
    };
    uint8_t started = 0;
    for (uint8_t i = 0; i < 10; i++) {
        uint8_t digit = 0;
        while (v >= pow10[i]) { v -= pow10[i]; digit++; }
        if (digit || started || i == 9) {
            usart_putc('0' + digit);
            started = 1;
        }
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    ael_mailbox_init();
    usart_init();
    ael_mailbox_pass();

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint32_t tx_count = 0;
    uint32_t rx_count = 0;

    while (1)
    {
        /* TX: send a line so observe_uart can verify TX path */
        usart_puts("AEL_UART_TX OK ");
        usart_putu32(tx_count);
        usart_puts("\r\n");
        tx_count++;

        /* ~500 ms delay — poll RX throughout to capture incoming bytes */
        for (volatile uint32_t i = 0; i < 2000000u; i++) {
            if (USART1->STATR & USART_STATR_RXNE) {
                (void)USART1->DATAR;   /* consume byte, clear RXNE */
                rx_count++;
                /* detail0 bits[15:1] = rx_count, bit[0] = 0
                 * toggle_count = (detail0 >> 1) & 0x7FFF = rx_count
                 * Any received byte changes toggle_count → detail0_increment PASS */
                *detail0 = rx_count << 1;
            }
        }
    }

    return 0;
}
