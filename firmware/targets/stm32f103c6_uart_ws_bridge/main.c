/* STM32F103C6T6 Blue Pill — UART WebSocket bridge firmware
 * USART1: PA9=TX, PA10=RX, 115200 8N1, 8 MHz HSI
 * On boot   : sends "AEL_READY STM32F103C6T6 UART_BRIDGE\r\n"
 * Main loop : receives a line (\r or \n terminated, max 64 bytes)
 *             echoes "AEL_ECHO:<line>\r\n"
 *             also prints "AEL_IDLE count=N\r\n" every 500 ms when quiet
 *             toggles PC13 LED on each received line
 */
#include <stdint.h>

/* ── RCC ────────────────────────────────────────────────────────────────── */
#define RCC_BASE             0x40021000u
#define RCC_APB2ENR          (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB2ENR_IOPAEN   (1u << 2)
#define RCC_APB2ENR_IOPCEN   (1u << 4)
#define RCC_APB2ENR_USART1EN (1u << 14)

/* ── GPIO ───────────────────────────────────────────────────────────────── */
#define GPIOA_BASE  0x40010800u
#define GPIOA_CRH   (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define GPIOC_BASE  0x40011000u
#define GPIOC_CRH   (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR   (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

/* ── USART1 ─────────────────────────────────────────────────────────────── */
#define USART1_BASE  0x40013800u
#define USART1_SR    (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR    (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR   (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1   (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))

#define USART_SR_RXNE  (1u << 5)
#define USART_SR_TXE   (1u << 7)
#define USART_SR_TC    (1u << 6)
#define USART_CR1_UE   (1u << 13)
#define USART_CR1_TE   (1u << 3)
#define USART_CR1_RE   (1u << 2)

/* ── SysTick ────────────────────────────────────────────────────────────── */
#define SYST_CSR        (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR        (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR        (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

#define RX_BUF_LEN   64u
#define IDLE_TICKS   500u   /* ms between idle banners */

/* ── helpers ────────────────────────────────────────────────────────────── */
static volatile uint32_t g_tick_ms = 0u;

static void systick_poll(void)
{
    if (SYST_CSR & SYST_CSR_COUNTFLAG) {
        g_tick_ms++;
    }
}

static void uart_putc(uint8_t c)
{
    while ((USART1_SR & USART_SR_TXE) == 0u) { systick_poll(); }
    USART1_DR = c;
}

static void uart_puts(const char *s)
{
    while (*s) { uart_putc((uint8_t)*s++); }
}

static void uart_putu32(uint32_t v)
{
    char buf[10];
    uint32_t i = 0u;
    if (v == 0u) { uart_putc('0'); return; }
    while (v && i < sizeof(buf)) { buf[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i) { uart_putc((uint8_t)buf[--i]); }
}

static void uart_flush(void)
{
    while ((USART1_SR & USART_SR_TC) == 0u) { systick_poll(); }
}

static int uart_rx_ready(void)
{
    systick_poll();
    return (USART1_SR & USART_SR_RXNE) != 0u;
}

static uint8_t uart_getc(void)
{
    while (!uart_rx_ready()) {}
    return (uint8_t)(USART1_DR & 0xFFu);
}

void HardFault_Handler(void)
{
    volatile uint32_t *aircr = (volatile uint32_t *)0xE000ED0Cu;
    *aircr = 0x05FA0004u;
    while (1) {}
}

/* ── init ───────────────────────────────────────────────────────────────── */
static void hw_init(void)
{
    /* SysTick: 1 ms at 8 MHz HSI */
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Clocks */
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PA9 = USART1_TX AF push-pull 2 MHz
     * PA10 = USART1_RX floating input        CRH bits [11:8] */
    GPIOA_CRH &= ~(0xFFu << 4u);
    GPIOA_CRH |=  (0x0Bu << 4u)   /* PA9  TX */
               |  (0x04u << 8u);  /* PA10 RX */

    /* PC13 = output push-pull 2 MHz, LED off (active-low) */
    GPIOC_CRH &= ~(0xFu << 20u);
    GPIOC_CRH |=  (0x2u << 20u);
    GPIOC_ODR |=  (1u << 13);

    /* USART1: 115200 8N1 @ 8 MHz → BRR = 0x45 */
    USART1_BRR = 0x45u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/* ── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    hw_init();

    uart_puts("AEL_READY STM32F103C6T6 UART_BRIDGE\r\n");
    uart_flush();

    char     rxbuf[RX_BUF_LEN];
    uint32_t rxlen       = 0u;
    uint32_t idle_count  = 0u;
    uint32_t last_idle_t = g_tick_ms;

    while (1) {
        /* ── idle banner when no input ── */
        systick_poll();
        if (!uart_rx_ready()) {
            if ((g_tick_ms - last_idle_t) >= IDLE_TICKS) {
                uart_puts("AEL_IDLE count=");
                uart_putu32(idle_count++);
                uart_puts("\r\n");
                uart_flush();
                last_idle_t = g_tick_ms;
            }
            continue;
        }

        /* ── receive one character ── */
        uint8_t c = uart_getc();

        if (c == '\r' || c == '\n') {
            if (rxlen == 0u) { continue; }        /* skip blank lines */

            rxbuf[rxlen] = '\0';

            /* echo */
            uart_puts("AEL_ECHO:");
            uart_puts(rxbuf);
            uart_puts("\r\n");
            uart_flush();

            /* toggle LED */
            GPIOC_ODR ^= (1u << 13);

            rxlen       = 0u;
            last_idle_t = g_tick_ms;   /* reset idle timer */
        } else {
            if (rxlen < RX_BUF_LEN - 1u) {
                rxbuf[rxlen++] = (char)c;
            }
            last_idle_t = g_tick_ms;
        }
    }
}
