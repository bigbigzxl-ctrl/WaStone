#include <stdint.h>
#include <stdbool.h>

#define RCC_BASE             0x40021000u
#define RCC_APB2ENR          (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_APB2ENR_IOPAEN   (1u << 2)
#define RCC_APB2ENR_IOPCEN   (1u << 4)
#define RCC_APB2ENR_USART1EN (1u << 14)

#define GPIOA_BASE           0x40010800u
#define GPIOA_CRH            (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define GPIOC_BASE           0x40011000u
#define GPIOC_CRH            (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR            (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define USART1_BASE          0x40013800u
#define USART1_SR            (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR            (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR           (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1           (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))

#define USART_SR_RXNE        (1u << 5)
#define USART_SR_TXE         (1u << 7)
#define USART_SR_TC          (1u << 6)
#define USART_CR1_UE         (1u << 13)
#define USART_CR1_TE         (1u << 3)
#define USART_CR1_RE         (1u << 2)

#define SYST_CSR             (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR             (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR             (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE      (1u << 0)
#define SYST_CSR_CLKSOURCE   (1u << 2)
#define SYST_CSR_COUNTFLAG   (1u << 16)

#define RX_BUF_LEN           64u
#define IDLE_PERIOD_MS       500u
#define FLUSH_GAP_MS         30u

static void delay_ms(uint32_t ms)
{
    while (ms-- > 0u) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
        }
    }
}

static void usart1_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PA9 = USART1_TX AF push-pull 2 MHz, PA10 = USART1_RX floating input. */
    GPIOA_CRH &= ~(0xFFu << 4u);
    GPIOA_CRH |= (0x0Bu << 4u) | (0x04u << 8u);

    /* PC13 = status LED output push-pull 2 MHz. */
    GPIOC_CRH &= ~(0xFu << 20u);
    GPIOC_CRH |= (0x2u << 20u);

    /* 8 MHz HSI / 115200 8N1. */
    USART1_BRR = 0x45u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void usart1_write_u8(uint8_t value)
{
    while ((USART1_SR & USART_SR_TXE) == 0u) {
    }
    USART1_DR = value;
}

static void usart1_write_str(const char *s)
{
    while (*s != '\0') {
        usart1_write_u8((uint8_t)*s++);
    }
}

static void usart1_write_u32_dec(uint32_t value)
{
    char buf[10];
    uint32_t i = 0u;

    if (value == 0u) {
        usart1_write_u8((uint8_t)'0');
        return;
    }

    while (value > 0u && i < (uint32_t)sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0u) {
        usart1_write_u8((uint8_t)buf[--i]);
    }
}

static bool usart1_try_read_u8(uint8_t *value)
{
    if ((USART1_SR & USART_SR_RXNE) == 0u) {
        return false;
    }
    *value = (uint8_t)USART1_DR;
    return true;
}

static void wait_tx_complete(void)
{
    while ((USART1_SR & USART_SR_TC) == 0u) {
    }
}

static void emit_idle_line(uint32_t counter)
{
    usart1_write_str("AEL_IDLE count=");
    usart1_write_u32_dec(counter);
    usart1_write_str(" baud=115200 8N1\r\n");
}

static void emit_echo_line(const char *line, uint32_t len)
{
    usart1_write_str("AEL_ECHO:");
    for (uint32_t i = 0u; i < len; ++i) {
        usart1_write_u8((uint8_t)line[i]);
    }
    usart1_write_str("\r\n");
}

static bool is_line_break(uint8_t value)
{
    return value == (uint8_t)'\r' || value == (uint8_t)'\n';
}

static bool systick_elapsed_1ms(void)
{
    return (SYST_CSR & SYST_CSR_COUNTFLAG) != 0u;
}

int main(void)
{
    char rx_buf[RX_BUF_LEN];
    uint32_t rx_len = 0u;
    uint32_t idle_counter = 0u;
    uint32_t idle_ms = 0u;
    uint32_t rx_gap_ms = 0u;

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    usart1_init();

    /* LED off initially on active-low PC13. */
    GPIOC_ODR |= (1u << 13);

    delay_ms(50u);
    usart1_write_str("AEL_READY STM32F103C6T6 UART_BRIDGE\r\n");
    wait_tx_complete();

    while (1) {
        bool saw_rx = false;
        uint8_t value = 0u;

        while (usart1_try_read_u8(&value)) {
            saw_rx = true;
            idle_ms = 0u;
            rx_gap_ms = 0u;

            if (is_line_break(value)) {
                if (rx_len > 0u) {
                    emit_echo_line(rx_buf, rx_len);
                    wait_tx_complete();
                    rx_len = 0u;
                    GPIOC_ODR ^= (1u << 13);
                }
                continue;
            }

            if (rx_len < (RX_BUF_LEN - 1u)) {
                rx_buf[rx_len++] = (char)value;
                rx_buf[rx_len] = '\0';
            } else {
                emit_echo_line(rx_buf, rx_len);
                wait_tx_complete();
                rx_len = 0u;
                GPIOC_ODR ^= (1u << 13);
            }
        }

        if (!saw_rx && systick_elapsed_1ms()) {
            if (rx_len > 0u) {
                rx_gap_ms++;
                if (rx_gap_ms >= FLUSH_GAP_MS) {
                    emit_echo_line(rx_buf, rx_len);
                    wait_tx_complete();
                    rx_len = 0u;
                    rx_gap_ms = 0u;
                    GPIOC_ODR ^= (1u << 13);
                }
            }

            idle_ms++;
            if (idle_ms >= IDLE_PERIOD_MS) {
                emit_idle_line(idle_counter++);
                wait_tx_complete();
                idle_ms = 0u;
                GPIOC_ODR ^= (1u << 13);
            }
        }
    }
}
