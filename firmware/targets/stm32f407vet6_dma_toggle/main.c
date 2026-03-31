/*
 * STM32F407VET6 — DMA-driven GPIO toggle on PC0
 *
 * TIM2 (84 MHz) update event → DMA1 Stream1 Ch3 → GPIOC_BSRR
 * Buffer: [SET_PC0, CLR_PC0] in circular mode → 21 MHz square wave
 * CPU is completely idle (while(1) nop loop).
 *
 * Expected toggle frequency:
 *   TIM2_CLK / (ARR+1) / 2 = 84 MHz / 2 / 2 = 21 MHz
 *
 * DMA1 Stream 1 Ch 3 = TIM2_UP  (RM0090 Table 43)
 */

#include <stdint.h>

/* ── RCC ─────────────────────────────────────────────────────────────────── */
#define RCC_BASE    0x40023800U
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_PLLCFGR (*(volatile uint32_t *)(RCC_BASE + 0x04U))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40U))

/* ── Flash / PWR ─────────────────────────────────────────────────────────── */
#define FLASH_ACR   (*(volatile uint32_t *)(0x40023C00U))
#define PWR_CR      (*(volatile uint32_t *)(0x40007000U))

/* ── GPIOC ───────────────────────────────────────────────────────────────── */
#define GPIOC_MODER   (*(volatile uint32_t *)(0x40020800U))
#define GPIOC_OSPEEDR (*(volatile uint32_t *)(0x40020808U))
#define GPIOC_BSRR    (*(volatile uint32_t *)(0x40020818U))

/* ── TIM2  (APB1, TIM_CLK = 84 MHz) ─────────────────────────────────────── */
#define TIM2_CR1  (*(volatile uint32_t *)(0x40000000U + 0x00U))
#define TIM2_DIER (*(volatile uint32_t *)(0x40000000U + 0x0CU))
#define TIM2_EGR  (*(volatile uint32_t *)(0x40000000U + 0x14U))
#define TIM2_PSC  (*(volatile uint32_t *)(0x40000000U + 0x28U))
#define TIM2_ARR  (*(volatile uint32_t *)(0x40000000U + 0x2CU))

/* ── DMA1 Stream 1 ───────────────────────────────────────────────────────── */
#define DMA1_LISR   (*(volatile uint32_t *)(0x40026000U + 0x000U))
#define DMA1_LIFCR  (*(volatile uint32_t *)(0x40026000U + 0x008U))
#define DMA1_S1CR   (*(volatile uint32_t *)(0x40026000U + 0x028U))
#define DMA1_S1NDTR (*(volatile uint32_t *)(0x40026000U + 0x02CU))
#define DMA1_S1PAR  (*(volatile uint32_t *)(0x40026000U + 0x030U))
#define DMA1_S1M0AR (*(volatile uint32_t *)(0x40026000U + 0x034U))
#define DMA1_S1FCR  (*(volatile uint32_t *)(0x40026000U + 0x03CU))
/* DMA_SxFCR: bit2=DMDIS(1=FIFO mode), bits[1:0]=FTH(11=full threshold) */
#define DMA_FCR_FIFO_FULL  ((1U << 2) | (3U << 0))  /* DMDIS | FTH=full */

/* DMA_SxCR bit fields (RM0090 §10.5.5):
 *   [27:25] CHSEL  [17:16] PL  [15:14] MSIZE  [13:12] PSIZE
 *   [11] MINC  [9] CIRC  [7:6] DIR(01=M2P)  [0] EN
 */
#define DMA_CR_CHSEL3   (3U << 25)  /* bits[27:25] channel 3 = TIM2_UP */
#define DMA_CR_PL_VH    (3U << 16)  /* bits[17:16] very high priority */
#define DMA_CR_MSIZE32  (2U << 13)  /* bits[14:13] memory  32-bit words */
#define DMA_CR_PSIZE32  (2U << 11)  /* bits[12:11] periph  32-bit words */
#define DMA_CR_MINC     (1U << 10)  /* bit[10]     memory address increment */
#define DMA_CR_CIRC     (1U <<  8)  /* bit[8]      circular mode */
#define DMA_CR_M2P      (1U <<  6)  /* bits[7:6]   direction M2P (DIR=01) */
#define DMA_CR_EN       (1U <<  0)

/* LIFCR stream-1 clear bits: TC(11) HT(10) TE(9) DME(8) FE(6) */
#define DMA_LIFCR_S1ALL  0x00000F40U

/* DMA source buffer — in .bss (zero-init), values written in main() */
static volatile uint32_t bsrr_buf[2];

/* Debug: live register snapshots readable via GDB */
static volatile uint32_t dbg_dma_cr;
static volatile uint32_t dbg_dma_ndtr;
static volatile uint32_t dbg_dma_lifsr;
static volatile uint32_t dbg_tim2_cr1;
static volatile uint32_t dbg_tim2_sr;
static volatile uint32_t dbg_bsrr0;
static volatile uint32_t dbg_bsrr1;

static void clock_168mhz(void)
{
    RCC_APB1ENR |= (1U << 28);                           /* PWREN */
    PWR_CR      |= (3U << 14);                           /* VOS Scale 1 */
    FLASH_ACR    = (5U) | (1U<<8) | (1U<<9) | (1U<<10); /* 5WS + caches */
    RCC_CFGR     = (0x5U << 10) | (0x4U << 13);         /* APB1/4 APB2/2 */
    RCC_PLLCFGR  = (16U<<0)|(336U<<6)|(0U<<16)|(7U<<24);/* M16 N336 P2 Q7 */
    RCC_CR |= (1U << 24);                                /* PLLON */
    while (!(RCC_CR & (1U << 25)));                      /* wait PLLRDY */
    RCC_CFGR |= (2U << 0);                               /* SW = PLL */
    while ((RCC_CFGR & (3U << 2)) != (2U << 2));        /* wait SWS=PLL */
}

int main(void)
{
    clock_168mhz();

    /* Enable DMA1, GPIOC, TIM2 clocks */
    RCC_AHB1ENR |= (1U << 21) | (1U << 2);   /* DMA1EN | GPIOCEN */
    RCC_APB1ENR |= (1U << 0);                 /* TIM2EN */
    __asm__ volatile ("nop"); __asm__ volatile ("nop");

    /* PC0: output push-pull, very-high-speed (100 MHz slew) */
    GPIOC_MODER   = (GPIOC_MODER   & ~0x3U) | 0x1U;
    GPIOC_OSPEEDR = (GPIOC_OSPEEDR & ~0x3U) | 0x3U;

    /* Init DMA buffer here (not as global initializer — avoids .data copy issues) */
    bsrr_buf[0] = (1U <<  0);    /* set   PC0 */
    bsrr_buf[1] = (1U << 16);    /* clear PC0 */

    /* ── DMA1 Stream 1 setup ──────────────────────────────────────────── */
    DMA1_S1CR   = 0;                      /* disable stream first */
    while (DMA1_S1CR & DMA_CR_EN);        /* wait until disabled  */
    DMA1_LIFCR  = DMA_LIFCR_S1ALL;       /* clear any stale flags */
    DMA1_S1FCR  = DMA_FCR_FIFO_FULL;     /* FIFO mode, full threshold */
    DMA1_S1NDTR = 2U;
    DMA1_S1PAR  = (uint32_t)&GPIOC_BSRR;
    DMA1_S1M0AR = (uint32_t)bsrr_buf;
    DMA1_S1CR   = DMA_CR_CHSEL3 | DMA_CR_PL_VH | DMA_CR_MSIZE32 |
                  DMA_CR_PSIZE32 | DMA_CR_MINC  | DMA_CR_CIRC    |
                  DMA_CR_M2P;
    DMA1_S1CR  |= DMA_CR_EN;

    /* ── TIM2 setup ───────────────────────────────────────────────────── */
    TIM2_PSC  = 0U;    /* no prescaler: TIM2 runs at 84 MHz */
    TIM2_ARR  = 41U;   /* period = ARR+1 = 42 ticks → update at 2 MHz     */
                       /* toggle = update/2 = 1 MHz (2 DMA xfers/cycle)   */
                       /* ARR=1 (21 MHz) exceeds DMA AHB transfer budget   */
    TIM2_EGR  = 1U;    /* UG: load PSC/ARR immediately */
    TIM2_DIER = (1U << 8); /* UDE: enable Update DMA request */
    TIM2_CR1  = (1U << 0); /* CEN: start counter */

    /* CPU is now idle — DMA handles all GPIO activity */
    while (1) {
        /* Continuously snapshot peripheral state for GDB inspection */
        dbg_dma_cr    = *(volatile uint32_t *)0x40026028U; /* DMA1_S1CR   */
        dbg_dma_ndtr  = *(volatile uint32_t *)0x4002602CU; /* DMA1_S1NDTR */
        dbg_dma_lifsr = *(volatile uint32_t *)0x40026000U; /* DMA1_LISR   */
        dbg_tim2_cr1  = *(volatile uint32_t *)0x40000000U; /* TIM2_CR1    */
        dbg_tim2_sr   = *(volatile uint32_t *)0x40000010U; /* TIM2_SR     */
        dbg_bsrr0     = bsrr_buf[0];
        dbg_bsrr1     = bsrr_buf[1];
    }
}
