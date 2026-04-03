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
#define USART1_RDR      (*(volatile uint32_t *)(USART1_BASE + 0x24u))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

#define DMA1_BASE       0x40020000u
#define DMA1_ISR        (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR3       (*(volatile uint32_t *)(DMA1_BASE + 0x30u))
#define DMA1_CNDTR3     (*(volatile uint32_t *)(DMA1_BASE + 0x34u))
#define DMA1_CPAR3      (*(volatile uint32_t *)(DMA1_BASE + 0x38u))
#define DMA1_CMAR3      (*(volatile uint32_t *)(DMA1_BASE + 0x3Cu))
#define DMA1_CCR5       (*(volatile uint32_t *)(DMA1_BASE + 0x58u))
#define DMA1_CNDTR5     (*(volatile uint32_t *)(DMA1_BASE + 0x5Cu))
#define DMA1_CPAR5      (*(volatile uint32_t *)(DMA1_BASE + 0x60u))
#define DMA1_CMAR5      (*(volatile uint32_t *)(DMA1_BASE + 0x64u))

#define RCC_GPIOAEN     (1u << 17)
#define RCC_DMA1EN      (1u << 0)
#define RCC_SYSCFGEN    (1u << 0)
#define RCC_USART1EN    (1u << 14)

#define SYSCFG_CFGR1_USART1RX_DMA_RMP  (1u << 10)

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR3_DMAR  (1u << 6)
#define USART_ISR_RXNE  (1u << 5)
#define USART_ISR_TXE   (1u << 7)
#define USART_ISR_TC    (1u << 6)
#define USART_ISR_TEACK (1u << 21)
#define USART_ISR_REACK (1u << 22)
#define USART_ICR_ORECF (1u << 3)
#define USART_ICR_FECF  (1u << 1)
#define USART_ICR_NCF   (1u << 2)
#define USART_ICR_PECF  (1u << 0)

#define DMA_CCR_EN      (1u << 0)
#define DMA_CCR_TCIE    (1u << 1)
#define DMA_CCR_TEIE    (1u << 3)
#define DMA_CCR_MINC    (1u << 7)
#define DMA_ISR_TCIF3   (1u << 9)
#define DMA_ISR_TEIF3   (1u << 11)
#define DMA_IFCR_ALL3   (0x0Fu << 8u)
#define DMA_ISR_TCIF5   (1u << 17)
#define DMA_ISR_TEIF5   (1u << 19)
#define DMA_IFCR_ALL5   (0x0Fu << 16u)

#define DMA_TIMEOUT     1400000u

#define NVIC_ISER       (*(volatile uint32_t *)0xE000E100u)
#define NVIC_ICER       (*(volatile uint32_t *)0xE000E180u)
#define NVIC_ICPR       (*(volatile uint32_t *)0xE000E280u)

#define IRQ_DMA1_CH2_3  (1u << 10)
#define IRQ_DMA1_CH4_5  (1u << 11)

static uint8_t RX_BUF[12];
struct rx_variant_result {
    uint32_t code;
    uint32_t dma_isr;
    uint32_t cndtr;
    uint32_t ccr;
    uint32_t usart_isr;
    uint32_t cfgr1;
};
static struct rx_variant_result g_r_ch3;
static struct rx_variant_result g_r_ch5;
static struct rx_variant_result g_r_ch3_late;
static struct rx_variant_result g_r_ch5_late;
static struct rx_variant_result g_r_ch3_irq;
static struct rx_variant_result g_r_ch5_irq;

static volatile uint32_t g_irq_tcif_mask;
static volatile uint32_t g_irq_teif_mask;
static volatile uint32_t g_irq_ifcr_mask;
static volatile uint32_t g_irq_dma_seen;
static volatile uint32_t g_irq_dma_tc_seen;
static volatile uint32_t g_irq_dma_te_seen;
static volatile uint32_t g_irq_dma_isr_snapshot;

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__ volatile ("nop");
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

static int rx_matches_expected(void)
{
    static const uint8_t EXPECTED[] = "PING_DMA_RX";
    for (uint32_t i = 0u; i < sizeof(EXPECTED) - 1u; ++i) {
        if (RX_BUF[i] != EXPECTED[i]) {
            return 0;
        }
    }
    return 1;
}

void DMA1_Channel2_3_IRQHandler(void)
{
    uint32_t isr = DMA1_ISR;

    if ((g_irq_ifcr_mask & DMA_IFCR_ALL3) == 0u) {
        return;
    }
    if ((isr & (g_irq_tcif_mask | g_irq_teif_mask)) == 0u) {
        return;
    }
    g_irq_dma_seen = 1u;
    g_irq_dma_isr_snapshot = isr;
    if ((isr & g_irq_tcif_mask) != 0u) {
        g_irq_dma_tc_seen = 1u;
    }
    if ((isr & g_irq_teif_mask) != 0u) {
        g_irq_dma_te_seen = 1u;
    }
    DMA1_IFCR = g_irq_ifcr_mask;
}

void DMA1_Channel4_5_IRQHandler(void)
{
    uint32_t isr = DMA1_ISR;

    if ((g_irq_ifcr_mask & DMA_IFCR_ALL5) == 0u) {
        return;
    }
    if ((isr & (g_irq_tcif_mask | g_irq_teif_mask)) == 0u) {
        return;
    }
    g_irq_dma_seen = 1u;
    g_irq_dma_isr_snapshot = isr;
    if ((isr & g_irq_tcif_mask) != 0u) {
        g_irq_dma_tc_seen = 1u;
    }
    if ((isr & g_irq_teif_mask) != 0u) {
        g_irq_dma_te_seen = 1u;
    }
    DMA1_IFCR = g_irq_ifcr_mask;
}

static uint32_t uart_enable_ready(void)
{
    uint32_t timeout = DMA_TIMEOUT;

    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    while (((USART1_ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
            (USART_ISR_TEACK | USART_ISR_REACK)) &&
           timeout--) {
    }
    if ((USART1_ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
        (USART_ISR_TEACK | USART_ISR_REACK)) {
        return 0xD514u;
    }
    return 0u;
}

static struct rx_variant_result run_rx_variant(uint32_t use_ch5, uint32_t late_dmar)
{
    struct rx_variant_result out;
    volatile uint32_t *ccr;
    volatile uint32_t *cndtr;
    volatile uint32_t *cpar;
    volatile uint32_t *cmar;
    uint32_t tcif_mask;
    uint32_t teif_mask;
    uint32_t timeout = DMA_TIMEOUT;

    for (uint32_t i = 0u; i < sizeof(RX_BUF); ++i) {
        RX_BUF[i] = 0u;
    }

    DMA1_CCR3 = 0u;
    DMA1_CCR5 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL3 | DMA_IFCR_ALL5;
    if (late_dmar != 0u) {
        USART1_CR3 = 0u;
    } else {
        USART1_CR3 = USART_CR3_DMAR;
    }

    if (use_ch5 != 0u) {
        SYSCFG_CFGR1 |= SYSCFG_CFGR1_USART1RX_DMA_RMP;
        ccr = &DMA1_CCR5;
        cndtr = &DMA1_CNDTR5;
        cpar = &DMA1_CPAR5;
        cmar = &DMA1_CMAR5;
        tcif_mask = DMA_ISR_TCIF5;
        teif_mask = DMA_ISR_TEIF5;
    } else {
        SYSCFG_CFGR1 &= ~SYSCFG_CFGR1_USART1RX_DMA_RMP;
        ccr = &DMA1_CCR3;
        cndtr = &DMA1_CNDTR3;
        cpar = &DMA1_CPAR3;
        cmar = &DMA1_CMAR3;
        tcif_mask = DMA_ISR_TCIF3;
        teif_mask = DMA_ISR_TEIF3;
    }

    USART1_ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
    *cpar = (uint32_t)&USART1_RDR;
    *cmar = (uint32_t)RX_BUF;
    *cndtr = sizeof(RX_BUF) - 1u;
    *ccr = DMA_CCR_MINC | DMA_CCR_EN;
    if (late_dmar != 0u) {
        USART1_CR3 = USART_CR3_DMAR;
    }

    while ((DMA1_ISR & (tcif_mask | teif_mask)) == 0u) {
        if (--timeout == 0u) {
            break;
        }
    }

    out.dma_isr = DMA1_ISR;
    out.cndtr = *cndtr;
    out.ccr = *ccr;
    out.usart_isr = USART1_ISR;
    out.cfgr1 = SYSCFG_CFGR1;

    if ((DMA1_ISR & teif_mask) != 0u) {
        out.code = 0xD511u;
    } else if ((DMA1_ISR & tcif_mask) == 0u) {
        out.code = 0xD513u;
    } else if (!rx_matches_expected()) {
        out.code = 0xD512u;
    } else {
        out.code = 0u;
    }

    DMA1_CCR3 = 0u;
    DMA1_CCR5 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL3 | DMA_IFCR_ALL5;
    return out;
}

static struct rx_variant_result run_rx_irq_variant(uint32_t use_ch5)
{
    struct rx_variant_result out;
    volatile uint32_t *ccr;
    volatile uint32_t *cndtr;
    volatile uint32_t *cpar;
    volatile uint32_t *cmar;
    uint32_t irq_mask;
    uint32_t timeout = DMA_TIMEOUT;

    for (uint32_t i = 0u; i < sizeof(RX_BUF); ++i) {
        RX_BUF[i] = 0u;
    }

    DMA1_CCR3 = 0u;
    DMA1_CCR5 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL3 | DMA_IFCR_ALL5;
    USART1_CR3 = 0u;

    if (use_ch5 != 0u) {
        SYSCFG_CFGR1 |= SYSCFG_CFGR1_USART1RX_DMA_RMP;
        ccr = &DMA1_CCR5;
        cndtr = &DMA1_CNDTR5;
        cpar = &DMA1_CPAR5;
        cmar = &DMA1_CMAR5;
        g_irq_tcif_mask = DMA_ISR_TCIF5;
        g_irq_teif_mask = DMA_ISR_TEIF5;
        g_irq_ifcr_mask = DMA_IFCR_ALL5;
        irq_mask = IRQ_DMA1_CH4_5;
    } else {
        SYSCFG_CFGR1 &= ~SYSCFG_CFGR1_USART1RX_DMA_RMP;
        ccr = &DMA1_CCR3;
        cndtr = &DMA1_CNDTR3;
        cpar = &DMA1_CPAR3;
        cmar = &DMA1_CMAR3;
        g_irq_tcif_mask = DMA_ISR_TCIF3;
        g_irq_teif_mask = DMA_ISR_TEIF3;
        g_irq_ifcr_mask = DMA_IFCR_ALL3;
        irq_mask = IRQ_DMA1_CH2_3;
    }

    g_irq_dma_seen = 0u;
    g_irq_dma_tc_seen = 0u;
    g_irq_dma_te_seen = 0u;
    g_irq_dma_isr_snapshot = 0u;

    USART1_ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
    *cpar = (uint32_t)&USART1_RDR;
    *cmar = (uint32_t)RX_BUF;
    *cndtr = sizeof(RX_BUF) - 1u;
    *ccr = DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE;

    NVIC_ICER = irq_mask;
    NVIC_ICPR = irq_mask;
    NVIC_ISER = irq_mask;

    *ccr |= DMA_CCR_EN;
    USART1_CR3 = USART_CR3_DMAR;

    while ((g_irq_dma_te_seen == 0u) && (g_irq_dma_tc_seen == 0u) && timeout--) {
    }

    out.dma_isr = (g_irq_dma_isr_snapshot != 0u) ? g_irq_dma_isr_snapshot : DMA1_ISR;
    out.cndtr = *cndtr;
    out.ccr = *ccr;
    out.usart_isr = USART1_ISR;
    out.cfgr1 = SYSCFG_CFGR1;

    if (g_irq_dma_te_seen != 0u) {
        out.code = 0xD516u;
    } else if (g_irq_dma_seen == 0u) {
        out.code = 0xD517u;
    } else if (!rx_matches_expected()) {
        out.code = 0xD518u;
    } else {
        out.code = 0u;
    }

    NVIC_ICER = irq_mask;
    NVIC_ICPR = irq_mask;
    DMA1_CCR3 = 0u;
    DMA1_CCR5 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL3 | DMA_IFCR_ALL5;
    USART1_CR3 = 0u;
    return out;
}

static void report_rx_variant(const char *tag, struct rx_variant_result r)
{
    uart_write_cstr(tag);
    uart_write_byte(' ');
    uart_write_kv_hex("code", r.code);
    uart_write_kv_hex("dma_isr", r.dma_isr);
    uart_write_kv_hex("cndtr", r.cndtr);
    uart_write_kv_hex("ccr", r.ccr);
    uart_write_kv_hex("usart_isr", r.usart_isr);
    uart_write_kv_hex("cfgr1", r.cfgr1);
    uart_write_cstr("\r\n");
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
    ael_mailbox_init();
    if (uart_enable_ready() != 0u) {
        ael_mailbox_fail(0xD514u, USART1_ISR);
        while (1) {
            uart_write_line("AEL_UART_DMA_RX_READY");
            uart_write_kv_hex("uart_enable_fail", USART1_ISR);
            uart_write_cstr("\r\n");
            delay(240000u);
        }
    }
    USART1_CR3 = USART_CR3_DMAR;

    while (1) {
        uart_write_line("AEL_UART_DMA_RX_READY");
        g_r_ch3 = run_rx_variant(0u, 0u);
        if (g_r_ch3.code == 0u) {
            uart_write_line("AEL_UART_DMA_RX_OK");
            ael_mailbox_pass();
            while (1) {
                delay(240000u);
            }
        }
        report_rx_variant("DMA_RX_CH3", g_r_ch3);
        delay(120000u);
        g_r_ch5 = run_rx_variant(1u, 0u);
        report_rx_variant("DMA_RX_CH5", g_r_ch5);
        delay(120000u);
        g_r_ch3_late = run_rx_variant(0u, 1u);
        report_rx_variant("DMA_RX_CH3_LATE", g_r_ch3_late);
        delay(120000u);
        g_r_ch5_late = run_rx_variant(1u, 1u);
        if (g_r_ch5.code == 0u || g_r_ch3_late.code == 0u || g_r_ch5_late.code == 0u) {
            uart_write_line("AEL_UART_DMA_RX_OK");
            ael_mailbox_pass();
            while (1) {
                delay(240000u);
            }
        }
        report_rx_variant("DMA_RX_CH5_LATE", g_r_ch5_late);
        delay(120000u);
        g_r_ch3_irq = run_rx_irq_variant(0u);
        report_rx_variant("DMA_RX_CH3_IRQ", g_r_ch3_irq);
        delay(120000u);
        g_r_ch5_irq = run_rx_irq_variant(1u);
        if (g_r_ch3_irq.code == 0u || g_r_ch5_irq.code == 0u) {
            uart_write_line("AEL_UART_DMA_RX_OK");
            ael_mailbox_pass();
            while (1) {
                delay(240000u);
            }
        }
        report_rx_variant("DMA_RX_CH5_IRQ", g_r_ch5_irq);
        report_rx_variant("DMA_RX_CH3_LATE", g_r_ch3_late);
        report_rx_variant("DMA_RX_CH5_LATE", g_r_ch5_late);
        ael_mailbox_fail(0xD515u, 0u);
        while (1) {
            uart_write_line("AEL_UART_DMA_RX_READY");
            report_rx_variant("DMA_RX_CH3", g_r_ch3);
            report_rx_variant("DMA_RX_CH5", g_r_ch5);
            report_rx_variant("DMA_RX_CH3_LATE", g_r_ch3_late);
            report_rx_variant("DMA_RX_CH5_LATE", g_r_ch5_late);
            report_rx_variant("DMA_RX_CH3_IRQ", g_r_ch3_irq);
            report_rx_variant("DMA_RX_CH5_IRQ", g_r_ch5_irq);
            delay(240000u);
        }
    }
}
