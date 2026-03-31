/*
 * STM32F407VET6 — Pre-Stage2 Wiring Verification
 *
 * Phase 1 — LA connectivity (~60 s):
 *   Toggles PC0, PC1, and PA5 at ~1 Hz so the host LA script can confirm:
 *     P1: LA P0.0 ↔ PC0 (or PC1)
 *     P2: LA P0.1 ↔ PA5
 *   detail0 counts milliseconds elapsed; status = AEL_STATUS_RUNNING.
 *
 * Phase 2 — Loopback tests (mailbox-reported):
 *   P3: PC2 (output) → PC3 (input)  GPIO loopback   [ERR bits 0-1]
 *   P4: PD5 (USART2_TX) ↔ PD6 (USART2_RX) loopback [ERR bit  2  ]
 *   P5: PA7 (output) → PA6 (input)  GPIO loopback   [ERR bits 3-4]
 *
 * Error bitmask (reported via ael_mailbox_fail):
 *   bit 0  ERR_P3_HIGH  PC2=H, PC3 read L  (open wire or short to GND)
 *   bit 1  ERR_P3_LOW   PC2=L, PC3 read H  (short to VDD)
 *   bit 2  ERR_P4       USART2 byte mismatch / timeout
 *   bit 3  ERR_P5_HIGH  PA7=H, PA6 read L
 *   bit 4  ERR_P5_LOW   PA7=L, PA6 read H
 *
 * Clock: 16 MHz HSI (no PLL — simplest BRR for UART).
 * SysTick: 1 kHz (RVR = 15999 at 16 MHz).
 * Mailbox: 0x2001FC00 (SRAM1 top 1 KB, compatible with AEL stm32f407 tests).
 *
 * Bench wiring required:
 *   LA P0 GND  → STM32 GND   (GND first)
 *   LA P0.0    → STM32 PC3   (P1 + P3 loopback sink)
 *   LA P0.1    → STM32 PA5   (P2)
 *   PC2        → PC3         (P3 GPIO loopback source)
 *   PD5        → PD6         (P4 UART loopback)
 *   PA7        → PA6         (P5 GPIO loopback)
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* ── RCC ──────────────────────────────────────────────────────────────────── */
#define RCC_BASE    0x40023800U
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40U))

/* ── GPIOA (PA5, PA6, PA7) ────────────────────────────────────────────────── */
#define GPIOA_BASE   0x40020000U
#define GPIOA_MODER  (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_PUPDR  (*(volatile uint32_t *)(GPIOA_BASE + 0x0CU))
#define GPIOA_IDR    (*(volatile uint32_t *)(GPIOA_BASE + 0x10U))
#define GPIOA_ODR    (*(volatile uint32_t *)(GPIOA_BASE + 0x14U))
#define GPIOA_BSRR   (*(volatile uint32_t *)(GPIOA_BASE + 0x18U))

/* ── GPIOC (PC2, PC3) ─────────────────────────────────────────────────────── */
#define GPIOC_BASE   0x40020800U
#define GPIOC_MODER  (*(volatile uint32_t *)(GPIOC_BASE + 0x00U))
#define GPIOC_PUPDR  (*(volatile uint32_t *)(GPIOC_BASE + 0x0CU))
#define GPIOC_IDR    (*(volatile uint32_t *)(GPIOC_BASE + 0x10U))
#define GPIOC_ODR    (*(volatile uint32_t *)(GPIOC_BASE + 0x14U))
#define GPIOC_BSRR   (*(volatile uint32_t *)(GPIOC_BASE + 0x18U))

/* ── GPIOD (PD5 USART2_TX, PD6 USART2_RX) ────────────────────────────────── */
#define GPIOD_BASE   0x40020C00U
#define GPIOD_MODER  (*(volatile uint32_t *)(GPIOD_BASE + 0x00U))
#define GPIOD_AFRL   (*(volatile uint32_t *)(GPIOD_BASE + 0x20U))

/* ── USART2 (APB1, PD5 TX / PD6 RX, AF7) ─────────────────────────────────── */
#define USART2_BASE  0x40004400U
#define USART2_SR    (*(volatile uint32_t *)(USART2_BASE + 0x00U))
#define USART2_DR    (*(volatile uint32_t *)(USART2_BASE + 0x04U))
#define USART2_BRR   (*(volatile uint32_t *)(USART2_BASE + 0x08U))
#define USART2_CR1   (*(volatile uint32_t *)(USART2_BASE + 0x0CU))

#define USART_SR_RXNE  (1U << 5)
#define USART_SR_TXE   (1U << 7)
#define USART_CR1_RE   (1U << 2)
#define USART_CR1_TE   (1U << 3)
#define USART_CR1_UE   (1U << 13)

/*
 * 115200 baud @ 16 MHz HSI, APB1 = HCLK (no prescaler at reset), OVER8=0:
 *   USARTDIV = 16 000 000 / (16 × 115200) = 8.6805
 *   mantissa = 8, fraction = round(0.6805 × 16) = 11 → BRR = (8<<4)|11 = 0x8B
 */
#define USART2_BRR_VAL  0x8BU

/* ── SysTick ──────────────────────────────────────────────────────────────── */
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018U)
#define SYST_CSR_ENABLE    (1U << 0)
#define SYST_CSR_CLKSOURCE (1U << 2)   /* use processor clock */
#define SYST_CSR_COUNTFLAG (1U << 16)

/* ── Debug snapshots (readable via GDB from SRAM) ────────────────────────── */
static volatile uint32_t dbg_gpioc_moder;
static volatile uint32_t dbg_gpioc_odr;
static volatile uint32_t dbg_gpioa_odr;
static volatile uint32_t dbg_level;

/* ── Error bits ───────────────────────────────────────────────────────────── */
#define ERR_P3_HIGH  (1U << 0)
#define ERR_P3_LOW   (1U << 1)
#define ERR_P4       (1U << 2)
#define ERR_P5_HIGH  (1U << 3)
#define ERR_P5_LOW   (1U << 4)

/* ── SysTick-based delay ──────────────────────────────────────────────────── */

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0U; i < ms; i++) {
        SYST_CVR = 0U;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0U) {}
    }
}

/* ── P3: PC2 (output) → PC3 (input pull-down) GPIO loopback ─────────────── */

static uint32_t test_p3_gpio(void)
{
    uint32_t err = 0U;

    /* PC2: output push-pull (MODER[5:4] = 01) */
    GPIOC_MODER &= ~(3U << 4U);
    GPIOC_MODER |=  (1U << 4U);

    /* PC3: input (MODER[7:6] = 00), pull-down (PUPDR[7:6] = 10) */
    GPIOC_MODER &= ~(3U << 6U);
    GPIOC_PUPDR &= ~(3U << 6U);
    GPIOC_PUPDR |=  (2U << 6U);

    /* Drive PC2 HIGH, wait 2 ms, read PC3 */
    GPIOC_BSRR = (1U << 2U);         /* set PC2 */
    delay_ms(2U);
    if ((GPIOC_IDR & (1U << 3U)) == 0U) {
        err |= ERR_P3_HIGH;
    }

    /* Drive PC2 LOW, wait 2 ms, read PC3 */
    GPIOC_BSRR = (1U << (2U + 16U)); /* clear PC2 */
    delay_ms(2U);
    if ((GPIOC_IDR & (1U << 3U)) != 0U) {
        err |= ERR_P3_LOW;
    }

    return err;
}

/* ── P4: USART2 PD5(TX) ↔ PD6(RX) loopback ──────────────────────────────── */

static uint32_t test_p4_uart(void)
{
    /* PD5 USART2_TX AF7: MODER[11:10]=10, AFRL[23:20]=7
     * PD6 USART2_RX AF7: MODER[13:12]=10, AFRL[27:24]=7  */
    GPIOD_MODER &= ~(0xFU << 10U);
    GPIOD_MODER |=  (0xAU << 10U);
    GPIOD_AFRL  &= ~(0xFFU << 20U);
    GPIOD_AFRL  |=  (0x77U << 20U);

    RCC_APB1ENR |= (1U << 17U);       /* USART2EN */
    (void)RCC_APB1ENR;                /* read-back ensures clock is on */

    USART2_CR1 = 0U;
    USART2_BRR = USART2_BRR_VAL;
    USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* Send 0x5A */
    uint32_t t = 1000000U;
    while ((USART2_SR & USART_SR_TXE) == 0U) {
        if (--t == 0U) return ERR_P4;
    }
    USART2_DR = 0x5AU;

    /* Receive 0x5A */
    t = 1000000U;
    while ((USART2_SR & USART_SR_RXNE) == 0U) {
        if (--t == 0U) return ERR_P4;
    }
    if ((USART2_DR & 0xFFU) != 0x5AU) {
        return ERR_P4;
    }

    return 0U;
}

/* ── P5: PA7 (output) → PA6 (input pull-down) GPIO loopback ─────────────── */

static uint32_t test_p5_gpio(void)
{
    uint32_t err = 0U;

    /* PA7: output push-pull (MODER[15:14] = 01) */
    GPIOA_MODER &= ~(3U << 14U);
    GPIOA_MODER |=  (1U << 14U);

    /* PA6: input (MODER[13:12] = 00), pull-down (PUPDR[13:12] = 10) */
    GPIOA_MODER &= ~(3U << 12U);
    GPIOA_PUPDR &= ~(3U << 12U);
    GPIOA_PUPDR |=  (2U << 12U);

    /* Drive PA7 HIGH, wait 2 ms, read PA6 */
    GPIOA_BSRR = (1U << 7U);
    delay_ms(2U);
    if ((GPIOA_IDR & (1U << 6U)) == 0U) {
        err |= ERR_P5_HIGH;
    }

    /* Drive PA7 LOW, wait 2 ms, read PA6 */
    GPIOA_BSRR = (1U << (7U + 16U));
    delay_ms(2U);
    if ((GPIOA_IDR & (1U << 6U)) != 0U) {
        err |= ERR_P5_LOW;
    }

    return err;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void)
{
    /* SysTick @ 1 kHz from 16 MHz HSI: reload = 16000 - 1 = 15999 */
    SYST_RVR = 15999U;
    SYST_CVR = 0U;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Enable GPIOA (bit 0), GPIOC (bit 2), GPIOD (bit 3) clocks */
    RCC_AHB1ENR |= (1U << 0U) | (1U << 2U) | (1U << 3U);
    (void)RCC_AHB1ENR;

    ael_mailbox_init();

    /* ── Phase 1: toggle PC0, PC1, PA5 for LA P1/P2 verification ───────── */
    /*
     * PC0 output push-pull (MODER[1:0] = 01)
     * PC1 output push-pull (MODER[3:2] = 01)
     * PA5 output push-pull (MODER[11:10] = 01)
     * All three toggle together every 500 ticks.
     * Identify which LA channel corresponds to PC0 or PC1.
     */
    GPIOC_MODER &= ~(0xFU << 0U);   /* clear PC0 and PC1 mode bits */
    GPIOC_MODER |=  (0x5U << 0U);   /* PC0=01, PC1=01 (output) */

    GPIOA_MODER &= ~(3U << 10U);
    GPIOA_MODER |=  (1U << 10U);

    /* Drive all HIGH initially */
    GPIOC_BSRR = (1U << 0U) | (1U << 1U);  /* set PC0, PC1 */
    GPIOA_BSRR = (1U << 5U);

    uint32_t level    = 1U;
    uint32_t tick_ms  = 0U;

    /* 60 000 ms = 60 s: enough for 20+ LA snapshots */
    while (tick_ms < 60000U) {
        delay_ms(1U);
        tick_ms++;
        AEL_MAILBOX->detail0 = tick_ms;

        /* Snapshot GPIO state for GDB inspection */
        dbg_gpioc_moder = GPIOC_MODER;  /* expect bits[3:0]=0101 (PC0,PC1 out) */
        dbg_gpioc_odr   = GPIOC_ODR;   /* bits[1:0] toggle together */
        dbg_gpioa_odr   = GPIOA_ODR;   /* bit[5] toggles */
        dbg_level       = level;

        /* Toggle every 500 ms */
        if ((tick_ms % 500U) == 0U) {
            level ^= 1U;
            if (level) {
                GPIOC_BSRR = (1U << 0U) | (1U << 1U);          /* PC0, PC1 set   */
                GPIOA_BSRR = (1U << 5U);                        /* PA5 set        */
            } else {
                GPIOC_BSRR = (1U << 16U) | (1U << 17U);        /* PC0, PC1 clear */
                GPIOA_BSRR = (1U << (5U + 16U));                /* PA5 clear      */
            }
        }
    }

    /* ── Phase 2: loopback tests ─────────────────────────────────────────── */
    AEL_MAILBOX->detail0 = 0xAA000000U;  /* phase-2 sentinel */

    uint32_t err = 0U;
    err |= test_p3_gpio();
    err |= test_p4_uart();
    err |= test_p5_gpio();

    /* ── Report ──────────────────────────────────────────────────────────── */
    if (err == 0U) {
        ael_mailbox_pass();
        uint32_t n = 0U;
        while (1) {
            delay_ms(1U);
            AEL_MAILBOX->detail0 = ++n;
        }
    } else {
        ael_mailbox_fail(err, 0U);
        while (1) {}
    }
}
