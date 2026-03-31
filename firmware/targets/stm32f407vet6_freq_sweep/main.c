/*
 * STM32F407VET6 — DMA-driven GPIO frequency sweep on PC0
 *
 * Same TIM2+DMA setup as dma_toggle.  Steps through an ARR table
 * every HOLD_CYCLES CPU cycles, cycling forever.
 *
 * Toggle freq = 42 MHz / (ARR+1)
 *
 * ARR table and expected toggle frequencies:
 *   ARR= 83  →  0.50 MHz
 *   ARR= 41  →  1.00 MHz
 *   ARR= 20  →  2.00 MHz
 *   ARR=  9  →  4.20 MHz
 *   ARR=  4  →  8.40 MHz
 *   ARR=  1  → 21.00 MHz
 *
 * Each frequency held for ~10 s (via DWT cycle counter at 168 MHz).
 */

#include <stdint.h>

/* ── Register definitions (same as dma_toggle) ──────────────────────────── */
#define RCC_BASE    0x40023800U
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_PLLCFGR (*(volatile uint32_t *)(RCC_BASE + 0x04U))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40U))

#define FLASH_ACR   (*(volatile uint32_t *)(0x40023C00U))
#define PWR_CR      (*(volatile uint32_t *)(0x40007000U))

#define GPIOC_MODER   (*(volatile uint32_t *)(0x40020800U))
#define GPIOC_OSPEEDR (*(volatile uint32_t *)(0x40020808U))
#define GPIOC_BSRR    (*(volatile uint32_t *)(0x40020818U))

#define TIM2_CR1  (*(volatile uint32_t *)(0x40000000U + 0x00U))
#define TIM2_DIER (*(volatile uint32_t *)(0x40000000U + 0x0CU))
#define TIM2_EGR  (*(volatile uint32_t *)(0x40000000U + 0x14U))
#define TIM2_PSC  (*(volatile uint32_t *)(0x40000000U + 0x28U))
#define TIM2_ARR  (*(volatile uint32_t *)(0x40000000U + 0x2CU))

#define DMA1_LIFCR  (*(volatile uint32_t *)(0x40026000U + 0x008U))
#define DMA1_S1CR   (*(volatile uint32_t *)(0x40026000U + 0x028U))
#define DMA1_S1NDTR (*(volatile uint32_t *)(0x40026000U + 0x02CU))
#define DMA1_S1PAR  (*(volatile uint32_t *)(0x40026000U + 0x030U))
#define DMA1_S1M0AR (*(volatile uint32_t *)(0x40026000U + 0x034U))

#define DMA_CR_CHSEL3  (3U << 25)  /* bits[27:25] */
#define DMA_CR_PL_VH   (3U << 16)  /* bits[17:16] */
#define DMA_CR_MSIZE32 (2U << 13)  /* bits[14:13] */
#define DMA_CR_PSIZE32 (2U << 11)  /* bits[12:11] */
#define DMA_CR_MINC    (1U << 10)  /* bit[10] */
#define DMA_CR_CIRC    (1U <<  8)  /* bit[8] */
#define DMA_CR_M2P     (1U <<  6)  /* bits[7:6] DIR=01 */
#define DMA_CR_EN      (1U <<  0)
#define DMA_LIFCR_S1ALL 0x00000F40U

/* ── DWT cycle counter for ~10 s holds ───────────────────────────────────── */
#define CoreDebug_DEMCR (*(volatile uint32_t *)(0xE000EDFCU))
#define DWT_CTRL        (*(volatile uint32_t *)(0xE0001000U))
#define DWT_CYCCNT      (*(volatile uint32_t *)(0xE0001004U))
#define HOLD_CYCLES     (168000000U * 10U)   /* 10 s at 168 MHz */

static void dwt_init(void)
{
    CoreDebug_DEMCR |= (1U << 24);   /* TRCENA */
    DWT_CYCCNT       = 0U;
    DWT_CTRL        |= (1U << 0);    /* CYCCNTENA */
}

static void hold_10s(void)
{
    uint32_t start = DWT_CYCCNT;
    while ((DWT_CYCCNT - start) < HOLD_CYCLES);
}

/* ── Frequency sweep table ───────────────────────────────────────────────── */
static const uint32_t arr_table[] = { 83, 41, 20, 9, 4, 1 };
#define N_STEPS (sizeof(arr_table) / sizeof(arr_table[0]))

/* ── DMA buffer (values set in main) ────────────────────────────────────── */
static volatile uint32_t bsrr_buf[2];

static void clock_168mhz(void)
{
    RCC_APB1ENR |= (1U << 28);
    PWR_CR      |= (3U << 14);
    FLASH_ACR    = (5U) | (1U<<8) | (1U<<9) | (1U<<10);
    RCC_CFGR     = (0x5U << 10) | (0x4U << 13);
    RCC_PLLCFGR  = (16U<<0)|(336U<<6)|(0U<<16)|(7U<<24);
    RCC_CR |= (1U << 24);
    while (!(RCC_CR & (1U << 25)));
    RCC_CFGR |= (2U << 0);
    while ((RCC_CFGR & (3U << 2)) != (2U << 2));
}

int main(void)
{
    clock_168mhz();
    dwt_init();

    RCC_AHB1ENR |= (1U << 21) | (1U << 2);
    RCC_APB1ENR |= (1U << 0);
    __asm__ volatile ("nop"); __asm__ volatile ("nop");

    GPIOC_MODER   = (GPIOC_MODER   & ~0x3U) | 0x1U;
    GPIOC_OSPEEDR = (GPIOC_OSPEEDR & ~0x3U) | 0x3U;

    bsrr_buf[0] = (1U <<  0);
    bsrr_buf[1] = (1U << 16);

    /* DMA1 Stream 1 */
    DMA1_S1CR   = 0;
    while (DMA1_S1CR & DMA_CR_EN);
    DMA1_LIFCR  = DMA_LIFCR_S1ALL;
    DMA1_S1NDTR = 2U;
    DMA1_S1PAR  = (uint32_t)&GPIOC_BSRR;
    DMA1_S1M0AR = (uint32_t)bsrr_buf;
    DMA1_S1CR   = DMA_CR_CHSEL3 | DMA_CR_PL_VH | DMA_CR_MSIZE32 |
                  DMA_CR_PSIZE32 | DMA_CR_MINC  | DMA_CR_CIRC    |
                  DMA_CR_M2P;
    DMA1_S1CR  |= DMA_CR_EN;

    /* TIM2: start at first ARR value */
    TIM2_PSC  = 0U;
    TIM2_ARR  = arr_table[0];
    TIM2_EGR  = 1U;
    TIM2_DIER = (1U << 8);
    TIM2_CR1  = (1U << 0);

    /* Sweep loop */
    while (1) {
        for (uint32_t i = 0; i < N_STEPS; i++) {
            /* Change ARR — takes effect on next update event (ARPE=0) */
            TIM2_ARR = arr_table[i];
            hold_10s();
        }
    }
}
