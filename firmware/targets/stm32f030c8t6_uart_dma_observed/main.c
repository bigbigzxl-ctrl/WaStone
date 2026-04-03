#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define SYSCFG_BASE     0x40010000u
#define SYSCFG_CFGR1    (*(volatile uint32_t *)(SYSCFG_BASE + 0x00u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_CR3      (*(volatile uint32_t *)(USART1_BASE + 0x14u))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_ICR      (*(volatile uint32_t *)(USART1_BASE + 0x20u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define DMA1_BASE       0x40020000u
#define DMA1_ISR        (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR2       (*(volatile uint32_t *)(DMA1_BASE + 0x1Cu))
#define DMA1_CNDTR2     (*(volatile uint32_t *)(DMA1_BASE + 0x20u))
#define DMA1_CPAR2      (*(volatile uint32_t *)(DMA1_BASE + 0x24u))
#define DMA1_CMAR2      (*(volatile uint32_t *)(DMA1_BASE + 0x28u))
#define DMA1_CCR4       (*(volatile uint32_t *)(DMA1_BASE + 0x44u))
#define DMA1_CNDTR4     (*(volatile uint32_t *)(DMA1_BASE + 0x48u))
#define DMA1_CPAR4      (*(volatile uint32_t *)(DMA1_BASE + 0x4Cu))
#define DMA1_CMAR4      (*(volatile uint32_t *)(DMA1_BASE + 0x50u))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_DMA1EN      (1u << 0)
#define RCC_SYSCFGEN    (1u << 0)
#define RCC_USART1EN    (1u << 14)

#define SYSCFG_CFGR1_USART1TX_DMA_RMP  (1u << 9)

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR3_DMAT  (1u << 7)
#define USART_ICR_TCCF  (1u << 6)
#define USART_ISR_TXE   (1u << 7)
#define USART_ISR_TC    (1u << 6)

#define DMA_CCR_EN      (1u << 0)
#define DMA_CCR_DIR     (1u << 4)
#define DMA_CCR_MINC    (1u << 7)

#define DMA_ISR_TCIF2   (1u << 5)
#define DMA_ISR_TEIF2   (1u << 7)
#define DMA_IFCR_ALL2   (0x0Fu << 4u)
#define DMA_ISR_TCIF4   (1u << 13)
#define DMA_ISR_TEIF4   (1u << 15)
#define DMA_IFCR_ALL4   (0x0Fu << 12u)

#define DMA_TIMEOUT     1000000u

static uint8_t TX_FRAME[39];

struct dma_variant_result {
    uint32_t code;
    uint32_t dma_isr;
    uint32_t cndtr;
    uint32_t ccr;
    uint32_t usart_isr;
    uint32_t syscfg_cfgr1;
};

static struct dma_variant_result g_r_ch2;
static struct dma_variant_result g_r_ch2_kick;
static struct dma_variant_result g_r_ch4;
static struct dma_variant_result g_r_ch4_kick;

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
    }
}

static void init_tx_frame(void)
{
    static const char pattern[] = "AEL_UART_DMA A1 B2 C3 D4 55 66 77 88\r\n";
    for (uint32_t i = 0u; i < sizeof(pattern); ++i) {
        TX_FRAME[i] = (uint8_t)pattern[i];
    }
}

static void uart_write_byte(uint8_t b)
{
    while ((USART1_ISR & USART_ISR_TXE) == 0u) {
    }
    USART1_TDR = b;
}

static void uart_write_cstr(const char *s)
{
    while (*s != '\0') {
        uart_write_byte((uint8_t)*s++);
    }
}

static void uart_write_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_write_byte((uint8_t)hex[(value >> shift) & 0xFu]);
    }
}

static void uart_write_kv_hex(const char *key, uint32_t value)
{
    uart_write_cstr(key);
    uart_write_byte('=');
    uart_write_hex32(value);
    uart_write_byte(' ');
}

static void uart_write_line(const char *s)
{
    uart_write_cstr(s);
    uart_write_cstr("\r\n");
    while ((USART1_ISR & USART_ISR_TC) == 0u) {
    }
}

static void dma_disable_all(void)
{
    DMA1_CCR2 = 0u;
    DMA1_CCR4 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL2 | DMA_IFCR_ALL4;
}

static struct dma_variant_result run_variant(uint32_t use_ch4, uint32_t kick_first)
{
    struct dma_variant_result out;
    volatile uint32_t *ccr;
    volatile uint32_t *cndtr;
    volatile uint32_t *cpar;
    volatile uint32_t *cmar;
    uint32_t tcif_mask;
    uint32_t teif_mask;
    uint32_t timeout = DMA_TIMEOUT;
    uint32_t payload_len = (uint32_t)(sizeof(TX_FRAME) - 1u);

    if (use_ch4 != 0u) {
        ccr = &DMA1_CCR4;
        cndtr = &DMA1_CNDTR4;
        cpar = &DMA1_CPAR4;
        cmar = &DMA1_CMAR4;
        tcif_mask = DMA_ISR_TCIF4;
        teif_mask = DMA_ISR_TEIF4;
        SYSCFG_CFGR1 |= SYSCFG_CFGR1_USART1TX_DMA_RMP;
    } else {
        ccr = &DMA1_CCR2;
        cndtr = &DMA1_CNDTR2;
        cpar = &DMA1_CPAR2;
        cmar = &DMA1_CMAR2;
        tcif_mask = DMA_ISR_TCIF2;
        teif_mask = DMA_ISR_TEIF2;
        SYSCFG_CFGR1 &= ~SYSCFG_CFGR1_USART1TX_DMA_RMP;
    }

    dma_disable_all();
    USART1_ICR = USART_ICR_TCCF;
    *cpar = (uint32_t)&USART1_TDR;
    *cmar = (uint32_t)TX_FRAME;
    *cndtr = payload_len;
    *ccr = DMA_CCR_DIR | DMA_CCR_MINC;
    if (kick_first != 0u) {
        while ((USART1_ISR & USART_ISR_TXE) == 0u) {
        }
        USART1_TDR = TX_FRAME[0];
        *cmar = (uint32_t)(TX_FRAME + 1u);
        *cndtr = payload_len - 1u;
    }
    *ccr |= DMA_CCR_EN;

    while (((DMA1_ISR & (tcif_mask | teif_mask)) == 0u) && timeout--) {
    }

    out.dma_isr = DMA1_ISR;
    out.cndtr = *cndtr;
    out.ccr = *ccr;
    out.usart_isr = USART1_ISR;
    out.syscfg_cfgr1 = SYSCFG_CFGR1;

    if ((DMA1_ISR & teif_mask) != 0u) {
        out.code = 0xD401u;
    } else if ((DMA1_ISR & tcif_mask) == 0u) {
        out.code = 0xD402u;
    } else {
        while ((USART1_ISR & USART_ISR_TC) == 0u) {
        }
        out.code = 0u;
    }

    dma_disable_all();
    return out;
}

static void report_variant(const char *tag, struct dma_variant_result r)
{
    uart_write_cstr(tag);
    uart_write_byte(' ');
    uart_write_kv_hex("code", r.code);
    uart_write_kv_hex("dma_isr", r.dma_isr);
    uart_write_kv_hex("cndtr", r.cndtr);
    uart_write_kv_hex("ccr", r.ccr);
    uart_write_kv_hex("usart_isr", r.usart_isr);
    uart_write_kv_hex("cfgr1", r.syscfg_cfgr1);
    uart_write_cstr("\r\n");
    while ((USART1_ISR & USART_ISR_TC) == 0u) {
    }
}

int main(void)
{
    RCC_AHBENR |= RCC_GPIOAEN | RCC_DMA1EN;
    RCC_APB2ENR |= RCC_SYSCFGEN | RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 69u;
    USART1_CR3 = USART_CR3_DMAT;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();
    init_tx_frame();

    g_r_ch2 = run_variant(0u, 0u);
    if (g_r_ch2.code == 0u) {
        ael_mailbox_pass();
        while (1) {
            uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
            report_variant("DMA_CH2", g_r_ch2);
            delay(240000u);
        }
    }

    delay(120000u);
    g_r_ch2_kick = run_variant(0u, 1u);
    if (g_r_ch2_kick.code == 0u) {
        ael_mailbox_pass();
        while (1) {
            uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
            report_variant("DMA_CH2_KICK", g_r_ch2_kick);
            delay(240000u);
        }
    }

    delay(120000u);
    g_r_ch4 = run_variant(1u, 0u);
    if (g_r_ch4.code == 0u) {
        ael_mailbox_pass();
        while (1) {
            uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
            report_variant("DMA_CH4", g_r_ch4);
            delay(240000u);
        }
    }

    delay(120000u);
    g_r_ch4_kick = run_variant(1u, 1u);
    if (g_r_ch4_kick.code == 0u) {
        ael_mailbox_pass();
        while (1) {
            uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
            report_variant("DMA_CH4_KICK", g_r_ch4_kick);
            delay(240000u);
        }
    }

    ael_mailbox_fail(0xD4FFu, 0u);
    while (1) {
        uart_write_line("AEL_UART_DMA_DIAG_BEGIN");
        report_variant("DMA_CH2", g_r_ch2);
        report_variant("DMA_CH2_KICK", g_r_ch2_kick);
        report_variant("DMA_CH4", g_r_ch4);
        report_variant("DMA_CH4_KICK", g_r_ch4_kick);
        delay(240000u);
    }
}
