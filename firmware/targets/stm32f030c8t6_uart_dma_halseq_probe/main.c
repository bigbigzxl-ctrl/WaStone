#include <stdint.h>

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18u))

#define SYSCFG_BASE     0x40010000u
#define SYSCFG_CFGR1    (*(volatile uint32_t *)(SYSCFG_BASE + 0x00u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
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

#define RCC_GPIOAEN     (1u << 17)
#define RCC_DMA1EN      (1u << 0)
#define RCC_SYSCFGEN    (1u << 0)
#define RCC_USART1EN    (1u << 14)

#define USART_CR1_UE    (1u << 0)
#define USART_CR1_RE    (1u << 2)
#define USART_CR1_TE    (1u << 3)
#define USART_CR1_TCIE  (1u << 6)
#define USART_CR3_DMAT  (1u << 7)
#define USART_ICR_TCCF  (1u << 6)
#define USART_ISR_TXE   (1u << 7)
#define USART_ISR_TC    (1u << 6)
#define USART_ISR_TEACK (1u << 21)
#define USART_ISR_REACK (1u << 22)

#define DMA_CCR_EN      (1u << 0)
#define DMA_CCR_TCIE    (1u << 1)
#define DMA_CCR_TEIE    (1u << 3)
#define DMA_CCR_DIR     (1u << 4)
#define DMA_CCR_MINC    (1u << 7)

#define DMA_ISR_TCIF2   (1u << 5)
#define DMA_ISR_TEIF2   (1u << 7)
#define DMA_IFCR_ALL2   (0x0Fu << 4u)

#define NVIC_ISER       (*(volatile uint32_t *)0xE000E100u)
#define NVIC_ICER       (*(volatile uint32_t *)0xE000E180u)
#define NVIC_ICPR       (*(volatile uint32_t *)0xE000E280u)

#define IRQ_DMA1_CH2_3  (1u << 10)
#define IRQ_USART1      (1u << 27)

#define TIMEOUT         1000000u

static const uint8_t TX_FRAME[] = "AEL_HALSEQ_DMA_TX\r\n";

static volatile uint32_t g_dma_tc;
static volatile uint32_t g_dma_te;
static volatile uint32_t g_uart_tc;
static volatile uint32_t g_dma_isr_snapshot;
static volatile uint32_t g_usart_isr_snapshot;

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

static void uart_wait_tc(void)
{
    while ((USART1_ISR & USART_ISR_TC) == 0u) {
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

void DMA1_Channel2_3_IRQHandler(void)
{
    uint32_t isr = DMA1_ISR;
    g_dma_isr_snapshot = isr;
    if ((isr & DMA_ISR_TCIF2) != 0u) {
        g_dma_tc = 1u;
    }
    if ((isr & DMA_ISR_TEIF2) != 0u) {
        g_dma_te = 1u;
    }
    DMA1_IFCR = DMA_IFCR_ALL2;
}

void USART1_IRQHandler(void)
{
    uint32_t isr = USART1_ISR;
    g_usart_isr_snapshot = isr;
    if (((USART1_CR1 & USART_CR1_TCIE) != 0u) && ((isr & USART_ISR_TC) != 0u)) {
        USART1_ICR = USART_ICR_TCCF;
        USART1_CR1 &= ~USART_CR1_TCIE;
        g_uart_tc = 1u;
    }
}

static uint32_t uart_enable_ready(void)
{
    uint32_t timeout = TIMEOUT;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    while (((USART1_ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
            (USART_ISR_TEACK | USART_ISR_REACK)) &&
           timeout--) {
    }
    if ((USART1_ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
        (USART_ISR_TEACK | USART_ISR_REACK)) {
        return 0u;
    }
    return 1u;
}

int main(void)
{
    uint32_t timeout = TIMEOUT;

    RCC_AHBENR |= RCC_GPIOAEN | RCC_DMA1EN;
    RCC_APB2ENR |= RCC_SYSCFGEN | RCC_USART1EN;

    GPIOA_MODER &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_MODER |= (0x2u << 18u) | (0x2u << 20u);
    GPIOA_OSPEEDR &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_OSPEEDR |= (0x3u << 18u) | (0x3u << 20u);
    GPIOA_PUPDR &= ~((0x3u << 18u) | (0x3u << 20u));
    GPIOA_PUPDR |= (0x1u << 18u) | (0x1u << 20u);
    GPIOA_AFRH &= ~((0xFu << 4u) | (0xFu << 8u));
    GPIOA_AFRH |= (0x1u << 4u) | (0x1u << 8u);

    USART1_BRR = 833u;
    if (uart_enable_ready() == 0u) {
        while (1) {
        }
    }

    g_dma_tc = 0u;
    g_dma_te = 0u;
    g_uart_tc = 0u;
    g_dma_isr_snapshot = 0u;
    g_usart_isr_snapshot = 0u;

    SYSCFG_CFGR1 &= ~(1u << 9);
    DMA1_CCR2 &= ~DMA_CCR_EN;
    DMA1_IFCR = DMA_IFCR_ALL2;
    USART1_CR1 &= ~USART_CR1_TCIE;
    USART1_CR3 = 0u;
    DMA1_CPAR2 = (uint32_t)&USART1_TDR;
    DMA1_CMAR2 = (uint32_t)TX_FRAME;
    DMA1_CNDTR2 = (uint32_t)(sizeof(TX_FRAME) - 1u);
    DMA1_CCR2 = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_TEIE;

    NVIC_ICER = IRQ_DMA1_CH2_3 | IRQ_USART1;
    NVIC_ICPR = IRQ_DMA1_CH2_3 | IRQ_USART1;
    NVIC_ISER = IRQ_DMA1_CH2_3 | IRQ_USART1;

    uart_write_cstr("AEL_HALSEQ_BEGIN\r\n");
    uart_wait_tc();

    DMA1_CCR2 |= DMA_CCR_EN;
    USART1_ICR = USART_ICR_TCCF;
    USART1_CR3 = USART_CR3_DMAT;

    while ((g_dma_te == 0u) && (g_uart_tc == 0u) && timeout--) {
        if ((g_dma_tc != 0u) && ((USART1_CR1 & USART_CR1_TCIE) == 0u)) {
            DMA1_CCR2 = 0u;
            USART1_CR3 = 0u;
            USART1_ICR = USART_ICR_TCCF;
            USART1_CR1 |= USART_CR1_TCIE;
        }
    }

    if (g_uart_tc != 0u) {
        uart_write_cstr("AEL_HALSEQ_OK\r\n");
    } else {
        uart_write_cstr("AEL_HALSEQ_FAIL ");
        uart_write_kv_hex("dma_tc", g_dma_tc);
        uart_write_kv_hex("dma_te", g_dma_te);
        uart_write_kv_hex("uart_tc", g_uart_tc);
        uart_write_kv_hex("dma_isr", g_dma_isr_snapshot);
        uart_write_kv_hex("cndtr", DMA1_CNDTR2);
        uart_write_kv_hex("ccr", DMA1_CCR2);
        uart_write_kv_hex("usart_isr", g_usart_isr_snapshot != 0u ? g_usart_isr_snapshot : USART1_ISR);
        uart_write_cstr("\r\n");
    }

    while (1) {
        delay(240000u);
    }
}
