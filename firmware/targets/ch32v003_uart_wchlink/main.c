/*
 * CH32V003 UART → WCH-Link CDC test
 *
 * USART1 TX = PD5  (connected to WCH-Link RX → /dev/ttyACM0)
 * USART1 RX = PD6  (not used in this TX-only test)
 *
 * Clock: 24 MHz HSI
 * Baud:  115200  (BRR = 0x00D0, error < 0.2%)
 *
 * After mailbox PASS the firmware sends:
 *   "AEL_UART_TEST OK <counter>\r\n"
 * every ~500 ms, incrementing counter.
 *
 * detail0 = counter (for AEL liveness check)
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

/* ── USART helpers ────────────────────────────────────────────────────── */
static void usart_init(void)
{
    /* Clocks: AFIO + GPIOD + USART1 on APB2 */
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPDEN | RCC_USART1EN;
    /* Reset USART1 */
    RCC->APB2PRSTR |=  RCC_USART1RST;
    RCC->APB2PRSTR &= ~RCC_USART1RST;

    /* PD5 = USART1_TX: AF push-pull 50 MHz (CFGLR bits[23:20] = 0xB) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 20)) | (0xBu << 20);
    /* PD6 = USART1_RX: floating input (CFGLR bits[27:24] = 0x4) */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 24)) | (0x4u << 24);

    /* 115200 baud @ 8 MHz: USARTDIV = 8000000/(16*115200) = 4.34
       mantissa=4, fraction=5 (5/16=0.3125) → BRR = 0x0045
       actual = 8000000/(16*4.3125) = 115942 (~0.6% error) */
    USART1->BRR   = 0x0045;
    USART1->CTLR1 = USART_CTLR1_TE | USART_CTLR1_UE;
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

/* Simple itoa for unsigned decimal — no division (not available in rv32ec nostdlib) */
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
    uint32_t counter = 0;

    while (1)
    {
        usart_puts("AEL_UART_TEST OK ");
        usart_putu32(counter);
        usart_puts("\r\n");

        *detail0 = counter;
        counter++;

        for (volatile uint32_t i = 0; i < 2000000u; i++);
    }

    return 0;
}
