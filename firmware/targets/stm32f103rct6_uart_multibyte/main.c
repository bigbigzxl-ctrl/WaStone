/*
 * STM32F103RCT6 — USART1 multibyte TX proof
 *
 * Observable behaviour:
 *   - USART1 sends a repeated multi-byte frame over PA9 at 115200 8N1
 *   - Host UART observer should see: "AEL_UART_MB 55 AA 12 34"
 *   - Mailbox PASS after the first complete frame is transmitted
 *   - detail0 = transmitted frame count
 *
 * Wiring required:
 *   - PA9/USART1_TX -> DAPLink RX
 *   - DAPLink TX -> PA10/USART1_RX (present but unused by this proof)
 * Mailbox address: 0x2000BC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

#define GPIOA_BASE      0x40010800U
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04U))

#define USART1_BASE     0x40013800U
#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00U))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04U))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08U))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0CU))

#define USART_SR_TXE    (1U << 7)
#define USART_SR_TC     (1U << 6)

static void delay(volatile uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

static void usart1_write_byte(uint8_t b)
{
    while ((USART1_SR & USART_SR_TXE) == 0U) {}
    USART1_DR = b;
}

static void usart1_write_cstr(const char *s)
{
    while (*s) {
        usart1_write_byte((uint8_t)*s++);
    }
}

int main(void)
{
    RCC_APB2ENR |= (1U << 2) | (1U << 14);

    /*
     * PA9  (TX): AF push-pull 50 MHz -> CRH[7:4]  = 0xB
     * PA10 (RX): input floating      -> CRH[11:8] = 0x4
     */
    GPIOA_CRH &= ~0xFF0U;
    GPIOA_CRH |=  0x4B0U;

    /* 8 MHz HSI / 115200 -> BRR 0x45 */
    USART1_BRR = 0x45U;
    USART1_CR1 = (1U << 13) | (1U << 3) | (1U << 2);

    ael_mailbox_init();

    uint32_t frames = 0U;

    while (1) {
        usart1_write_cstr("AEL_UART_MB 55 AA 12 34\r\n");
        while ((USART1_SR & USART_SR_TC) == 0U) {}

        frames++;
        AEL_MAILBOX->detail0 = frames;
        if (frames == 1U) {
            ael_mailbox_pass();
        }

        delay(240000U);
    }
}
