/*
 * stm32h750_pll1_clock — PLL1 full-speed clock self-test
 *
 * Configures PLL1 from HSI (64 MHz) to SYSCLK = 400 MHz.
 * Verifies by measuring TIM2 ticks during a SysTick-timed 1 ms interval.
 *
 * Clock tree after PLL switch:
 *   HSI = 64 MHz → PLLM=4 → 16 MHz ref
 *   PLL1: DIVN=50 → VCO=800 MHz; DIVP=2 → PLL1_P=400 MHz = SYSCLK
 *   HPRE=/2   → D1HCLK = 200 MHz
 *   D2PPRE1=/2 → APB1 = 100 MHz  → TIM2 clock = 200 MHz (APB1×2)
 *   SysTick uses FCLK = 400 MHz
 *
 * TIM2 counts in 1 ms at 200 MHz = 200 000.
 * Accept ±10%: 180 000 – 220 000.
 *
 * Error codes:
 *   0xE001 = PLL1 lock timeout
 *   0xE002 = SYSCLK switch timeout (SWS never read back 3)
 *   0xE003 = TIM2 delta out of range (detail0 = measured count)
 *
 * VOS1 + Flash: LATENCY=4, WRHIGHFREQ=2 (safe for 200 MHz HCLK).
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE          0x58024400u
#define RCC_CR            (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_CFGR          (*(volatile uint32_t *)(RCC_BASE + 0x010u))
#define RCC_D1CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x018u))
#define RCC_D2CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x01Cu))
#define RCC_PLLCKSELR     (*(volatile uint32_t *)(RCC_BASE + 0x028u))
#define RCC_PLLCFGR       (*(volatile uint32_t *)(RCC_BASE + 0x02Cu))
#define RCC_PLL1DIVR      (*(volatile uint32_t *)(RCC_BASE + 0x030u))
#define RCC_APB1LENR      (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))

#define RCC_CR_PLL1ON     (1u << 24u)
#define RCC_CR_PLL1RDY    (1u << 25u)
#define RCC_APB1LENR_TIM2EN (1u << 0u)

/* ── PWR ─────────────────────────────────────────────────────────── */
#define PWR_BASE          0x58024800u
#define PWR_D3CR          (*(volatile uint32_t *)(PWR_BASE + 0x018u))
#define PWR_D3CR_VOS_VOS1 (3u << 14u)   /* VOS1 = 0b11 */
#define PWR_D3CR_ACTVOSRDY (1u << 13u)

/* ── FLASH ───────────────────────────────────────────────────────── */
#define FLASH_BASE        0x52002000u
#define FLASH_ACR         (*(volatile uint32_t *)(FLASH_BASE + 0x000u))
/* LATENCY[3:0]=4, WRHIGHFREQ[5:4]=2 (for HCLK > 185 MHz) */
#define FLASH_ACR_VAL     ((2u << 4u) | 4u)   /* = 0x24 */

/* ── TIM2 ────────────────────────────────────────────────────────── */
#define TIM2_BASE         0x40000000u
#define TIM2_CR1          (*(volatile uint32_t *)(TIM2_BASE + 0x00u))
#define TIM2_PSC          (*(volatile uint32_t *)(TIM2_BASE + 0x28u))
#define TIM2_ARR          (*(volatile uint32_t *)(TIM2_BASE + 0x2Cu))
#define TIM2_CNT          (*(volatile uint32_t *)(TIM2_BASE + 0x24u))
#define TIM2_EGR          (*(volatile uint32_t *)(TIM2_BASE + 0x14u))

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0u)
#define SYST_CSR_CLKSOURCE (1u << 2u)   /* processor clock */
#define SYST_CSR_COUNTFLAG (1u << 16u)

static void delay_ms(uint32_t ms)
{
    for (uint32_t m = 0u; m < ms; m++) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    /* ── Step 1: SysTick at HSI 64 MHz (1 ms/tick) ─────────────── */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* ── Step 2: Enable TIM2 clock, start free-running at 64 MHz ── */
    RCC_APB1LENR |= RCC_APB1LENR_TIM2EN;
    (void)RCC_APB1LENR;
    TIM2_PSC  = 0u;       /* prescaler /1 → TIM2 at 64 MHz (APB1=64 MHz before PLL) */
    TIM2_ARR  = 0xFFFFFFFFu;  /* 32-bit free-running */
    TIM2_EGR  = 1u;       /* UG: reload PSC/ARR */
    TIM2_CR1  = 1u;       /* CEN=1 */

    /* Small settle */
    delay_ms(1u);

    /* ── Step 3: Set FLASH wait states BEFORE raising frequency ─── */
    FLASH_ACR = FLASH_ACR_VAL;
    (void)FLASH_ACR;

    /* ── Step 4: PWR VOS1 ─────────────────────────────────────── */
    PWR_D3CR = (PWR_D3CR & ~(3u << 14u)) | PWR_D3CR_VOS_VOS1;
    /* Wait for ACTVOSRDY */
    uint32_t timeout = 1000000u;
    while ((PWR_D3CR & PWR_D3CR_ACTVOSRDY) == 0u) {
        if (--timeout == 0u) { break; }  /* proceed anyway; VOS1 may already be set */
    }
    delay_ms(1u);

    /* ── Step 5: Configure PLL1 (still disabled) ─────────────── */
    /*
     * Source: HSI (PLLSRC=1), DIVM1=4 → 64/4 = 16 MHz ref
     * PLLCFGR: PLL1RGE=0b11 (8–16 MHz), PLL1VCOSEL=0 (wide 192–960 MHz), DIVP1EN=1
     * PLL1DIVR: DIVN1=49 (N=50), DIVP1=1 (P=2)
     *   VCO = 16 × 50 = 800 MHz; SYSCLK = 800/2 = 400 MHz
     */
    /*
     * PLLCKSELR: PLLSRC bits[1:0]=0 (HSI), DIVM1 bits[9:4]=4 (÷4→16 MHz ref)
     * PLLSRC encoding: 0=HSI, 1=CSI, 2=HSE, 3=NONE
     */
    RCC_PLLCKSELR = (4u << 4u);     /* PLLSRC=0 (HSI), DIVM1=4 at bits[9:4] */

    /*
     * PLLCFGR: PLL1RGE bits[3:2]=3 (8-16 MHz input), PLL1VCOSEL bit1=0 (wide VCO), DIVP1EN bit16
     */
    RCC_PLLCFGR = (1u << 16u)      /* DIVP1EN */
                | (3u << 2u)        /* PLL1RGE = 0b11 = 8–16 MHz range */
                | (0u << 1u);       /* PLL1VCOSEL = 0 = wide (192–960 MHz) */

    /*
     * PLL1DIVR: N1 bits[8:0]=49 (÷50→VCO=800MHz), P1 bits[15:9]=1 (÷2→400MHz)
     */
    RCC_PLL1DIVR = ((2u - 1u) << 9u)   /* P1 = P-1 = 1 (PLLP=2) at bits[15:9] */
                 | (50u - 1u);          /* N1 = N-1 = 49 (PLLN=50) at bits[8:0] */

    /* ── Step 6: Enable PLL1, wait for lock ─────────────────── */
    RCC_CR |= RCC_CR_PLL1ON;
    timeout = 1000000u;
    while ((RCC_CR & RCC_CR_PLL1RDY) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE001u, RCC_CR);
            while (1) {}
        }
    }

    /* ── Step 7: Set bus prescalers before switching SYSCLK ──── */
    /*
     * D1CFGR: HPRE=0b1000 (/2) → D1HCLK = 200 MHz
     * D2CFGR: D2PPRE1=0b100 (/2) → APB1=100 MHz; D2PPRE2=0b100 (/2) → APB2=100 MHz
     */
    RCC_D1CFGR = 0x8u;                                  /* HPRE bits[3:0]=8 → /2 → HCLK=200MHz */
    /*
     * D2PPRE1 at bits[6:4], DIV2 = 4<<4 = 0x40
     * D2PPRE2 at bits[10:8], DIV2 = 4<<8 = 0x400
     */
    RCC_D2CFGR = (4u << 4u) | (4u << 8u);              /* D2PPRE1=/2, D2PPRE2=/2 */

    /* ── Step 8: Switch SYSCLK to PLL1_P ────────────────────── */
    RCC_CFGR = (RCC_CFGR & ~0x7u) | 3u;    /* SW=3 → PLL1 */
    timeout = 1000000u;
    while (((RCC_CFGR >> 3u) & 0x7u) != 3u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE002u, RCC_CFGR);
            while (1) {}
        }
    }

    /* ── Step 9: Update SysTick for 400 MHz CPU clock ────────── */
    /*
     * SysTick uses FCLK = CPU = 400 MHz.
     * RVR = 400 MHz / 1000 Hz - 1 = 399999.
     */
    SYST_RVR = 399999u;
    SYST_CVR = 0u;

    /*
     * TIM2 is now clocked from APB1 = 100 MHz.
     * Since D2PPRE1 prescaler ≠ 1, TIM2 clock = APB1 × 2 = 200 MHz.
     * TIM2 counts in 1 ms = 200 000.
     */

    /* ── Step 10: Measure TIM2 ticks over 1 ms ────────────────── */
    uint32_t t0 = TIM2_CNT;
    delay_ms(1u);
    uint32_t t1 = TIM2_CNT;
    uint32_t delta = t1 - t0;   /* wraps correctly for uint32_t */

    /* Accept 180 000 – 220 000 (±10% of 200 000) */
    if (delta < 180000u || delta > 220000u) {
        ael_mailbox_fail(0xE003u, delta);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = delta;   /* measured TIM2 ticks per ms (expect ~200000) */
    while (1) {}
}
