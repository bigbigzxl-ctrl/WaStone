/*
 * stm32h750_qspi_flash — QUADSPI NOR flash JEDEC ID read
 *
 * YD-STM32H750VBT6 board has W25Q64 (Winbond 64 Mbit) on QUADSPI bank 1.
 * Reads JEDEC ID (0x9F) in 1-line SPI mode.
 * Expected: 0xEF 0x40 0x18 (Winbond, SPI Flash, 128 Mbit = W25Q128).
 *
 * GPIO (AF9=QUADSPI, AF10=QUADSPI_NCS, from MiniSTM32H7xx/06-SPIFlash_Test.ioc):
 *   PB2  = QUADSPI_CLK    AF9
 *   PB6  = QUADSPI_BK1_NCS AF10
 *   PD11 = QUADSPI_BK1_IO0 AF9
 *   PD12 = QUADSPI_BK1_IO1 AF9
 *   PD13 = QUADSPI_BK1_IO3 AF9
 *   PE2  = QUADSPI_BK1_IO2 AF9
 *
 * QUADSPI base: 0x52005000 (AHB3, D1 domain, RM0433 §23)
 * RCC_AHB3ENR bit 14 = QSPIEN
 *
 * Clock: D1HCLK = 64 MHz, PRESCALER=1 → QSPI_CLK = 32 MHz (≤ W25Q64 max 80 MHz)
 * ClockMode = 3 (CKMODE=1), SampleShift = 1, FSIZE=22 (8MB), CSHT=7 (8 cycles)
 *
 * Error codes:
 *   0xE001 = TCF timeout (transfer never completed)
 *   0xE002 = wrong JEDEC manufacturer byte (expected 0xEF)
 *   0xE003 = wrong JEDEC memory type byte (expected 0x40)
 *   0xE004 = wrong JEDEC capacity byte (expected 0x18 = 128 Mbit)
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE          0x58024400u
#define RCC_AHB3ENR       (*(volatile uint32_t *)(RCC_BASE + 0x0D4u))
#define RCC_AHB4ENR       (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_AHB3ENR_QSPIEN    (1u << 14u)
#define RCC_AHB4ENR_GPIOBEN   (1u << 1u)
#define RCC_AHB4ENR_GPIODEN   (1u << 3u)
#define RCC_AHB4ENR_GPIOEEN   (1u << 4u)

/* ── GPIO ────────────────────────────────────────────────────────── */
#define GPIOB_BASE    0x58020400u
#define GPIOB_MODER   (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OSPEEDR (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_AFRL    (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))
#define GPIOB_AFRH    (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

#define GPIOD_BASE    0x58020C00u
#define GPIOD_MODER   (*(volatile uint32_t *)(GPIOD_BASE + 0x00u))
#define GPIOD_OSPEEDR (*(volatile uint32_t *)(GPIOD_BASE + 0x08u))
#define GPIOD_AFRH    (*(volatile uint32_t *)(GPIOD_BASE + 0x24u))

#define GPIOE_BASE    0x58021000u
#define GPIOE_MODER   (*(volatile uint32_t *)(GPIOE_BASE + 0x00u))
#define GPIOE_OSPEEDR (*(volatile uint32_t *)(GPIOE_BASE + 0x08u))
#define GPIOE_AFRL    (*(volatile uint32_t *)(GPIOE_BASE + 0x20u))

/* ── QUADSPI (AHB3, 0x52005000) ──────────────────────────────────── */
#define QSPI_BASE   0x52005000u
#define QSPI_CR     (*(volatile uint32_t *)(QSPI_BASE + 0x00u))
#define QSPI_DCR    (*(volatile uint32_t *)(QSPI_BASE + 0x04u))
#define QSPI_SR     (*(volatile uint32_t *)(QSPI_BASE + 0x08u))
#define QSPI_FCR    (*(volatile uint32_t *)(QSPI_BASE + 0x0Cu))
#define QSPI_DLR    (*(volatile uint32_t *)(QSPI_BASE + 0x10u))
#define QSPI_CCR    (*(volatile uint32_t *)(QSPI_BASE + 0x14u))
#define QSPI_DR     (*(volatile uint32_t *)(QSPI_BASE + 0x20u))
#define QSPI_DR8    (*(volatile uint8_t  *)(QSPI_BASE + 0x20u))

/* SR bits */
#define QSPI_SR_TEF   (1u << 0u)   /* transfer error */
#define QSPI_SR_TCF   (1u << 1u)   /* transfer complete */
#define QSPI_SR_FTF   (1u << 2u)   /* FIFO threshold */
#define QSPI_SR_BUSY  (1u << 5u)
/* FCR bits */
#define QSPI_FCR_CTEF (1u << 0u)
#define QSPI_FCR_CTCF (1u << 1u)
/* CR bits */
#define QSPI_CR_EN    (1u << 0u)
#define QSPI_CR_ABORT (1u << 1u)
#define QSPI_CR_SSHIFT (1u << 4u)   /* sample shift half cycle */

/* ── SysTick ─────────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0u)
#define SYST_CSR_CLKSOURCE (1u << 2u)
#define SYST_CSR_COUNTFLAG (1u << 16u)

static void delay_ticks(uint32_t ticks)
{
    for (volatile uint32_t i = 0u; i < ticks; i++) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Enable clocks: GPIOB, GPIOD, GPIOE, QUADSPI */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN;
    (void)RCC_AHB4ENR;
    RCC_AHB3ENR |= RCC_AHB3ENR_QSPIEN;
    (void)RCC_AHB3ENR;

    /*
     * GPIO init — all QUADSPI pins: AF mode, push-pull, no pull, very high speed.
     *
     * PB2  (AFRL[11:8])  = AF9  → CLK
     * PB6  (AFRL[27:24]) = AF10 → NCS
     * PD11 (AFRH[15:12]) = AF9  → IO0
     * PD12 (AFRH[19:16]) = AF9  → IO1
     * PD13 (AFRH[23:20]) = AF9  → IO3
     * PE2  (AFRL[11:8])  = AF9  → IO2
     */
    /* PB2, PB6: MODER=10 (AF) */
    GPIOB_MODER  &= ~((0x3u << 4u) | (0x3u << 12u));
    GPIOB_MODER  |=  ((0x2u << 4u) | (0x2u << 12u));
    GPIOB_OSPEEDR |= ((0x3u << 4u) | (0x3u << 12u));
    GPIOB_AFRL   &= ~((0xFu << 8u) | (0xFu << 24u));
    GPIOB_AFRL   |=  ((0x9u << 8u) | (0xAu << 24u));   /* PB2=AF9, PB6=AF10 */

    /* PD11, PD12, PD13: MODER=10 */
    GPIOD_MODER  &= ~((0x3u << 22u) | (0x3u << 24u) | (0x3u << 26u));
    GPIOD_MODER  |=  ((0x2u << 22u) | (0x2u << 24u) | (0x2u << 26u));
    GPIOD_OSPEEDR |= ((0x3u << 22u) | (0x3u << 24u) | (0x3u << 26u));
    GPIOD_AFRH   &= ~((0xFu << 12u) | (0xFu << 16u) | (0xFu << 20u));
    GPIOD_AFRH   |=  ((0x9u << 12u) | (0x9u << 16u) | (0x9u << 20u));

    /* PE2: MODER=10 */
    GPIOE_MODER  &= ~(0x3u << 4u);
    GPIOE_MODER  |=  (0x2u << 4u);
    GPIOE_OSPEEDR |= (0x3u << 4u);
    GPIOE_AFRL   &= ~(0xFu << 8u);
    GPIOE_AFRL   |=  (0x9u << 8u);
    (void)GPIOE_MODER;

    /*
     * QUADSPI configuration:
     *   CR:  PRESCALER=1 (32 MHz), SSHIFT=1, EN=1
     *   DCR: FSIZE=22 (8 MB), CSHT=7 (8 cycles), CKMODE=1 (mode 3)
     */
    QSPI_CR  = (1u << 24u)      /* PRESCALER=1 → /2 → 32 MHz */
             | QSPI_CR_SSHIFT   /* sample shift */
             | QSPI_CR_EN;      /* enable */

    QSPI_DCR = (22u << 16u)     /* FSIZE=22 → 8 MB */
             | (7u  << 8u)      /* CSHT=7 → 8 cycles CS high */
             | (1u  << 0u);     /* CKMODE=1 (mode 3, CLK high when idle) */

    delay_ticks(1u);

    /* Clear any stale flags */
    QSPI_FCR = QSPI_FCR_CTEF | QSPI_FCR_CTCF;

    /*
     * JEDEC READ ID (0x9F):
     *   DLR = 2 (receive 3 bytes)
     *   CCR: FMODE=01 (indirect read), DMODE=01 (1 line),
     *        ADMODE=00 (no address), IMODE=01 (1 line), INSTRUCTION=0x9F
     *
     * Writing CCR triggers the transaction immediately.
     */
    QSPI_DLR = 2u;   /* 3 bytes - 1 */

    QSPI_CCR = (1u << 26u)   /* FMODE=01 indirect read */
             | (1u << 24u)   /* DMODE=01 (1 line) */
             | (0u << 18u)   /* DCYC=0 (no dummy cycles) */
             | (0u << 14u)   /* ABMODE=00 */
             | (0u << 10u)   /* ADMODE=00 (no address) */
             | (1u << 8u)    /* IMODE=01 (1 line) */
             | 0x9Fu;        /* INSTRUCTION=0x9F (READ JEDEC ID) */

    /* Read 3 bytes as they arrive */
    uint8_t id[3] = { 0u, 0u, 0u };
    uint32_t timeout;

    for (uint32_t i = 0u; i < 3u; i++) {
        /* Wait for byte available in FIFO (FTF or TCF) */
        timeout = 1000000u;
        while (((QSPI_SR & QSPI_SR_FTF) == 0u) &&
               ((QSPI_SR & QSPI_SR_TCF) == 0u)) {
            if (--timeout == 0u) {
                ael_mailbox_fail(0xE001u, (i << 16u) | QSPI_SR);
                while (1) {}
            }
        }
        id[i] = QSPI_DR8;
    }

    /* Wait TCF */
    timeout = 1000000u;
    while ((QSPI_SR & QSPI_SR_TCF) == 0u) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE001u, 0xFF000000u | QSPI_SR);
            while (1) {}
        }
    }
    QSPI_FCR = QSPI_FCR_CTCF;

    /* Verify JEDEC ID: 0xEF 0x40 0x18 for W25Q128 (actual chip on YD board) */
    if (id[0] != 0xEFu) { ael_mailbox_fail(0xE002u, id[0]); while (1) {} }
    if (id[1] != 0x40u) { ael_mailbox_fail(0xE003u, id[1]); while (1) {} }
    if (id[2] != 0x18u) { ael_mailbox_fail(0xE004u, id[2]); while (1) {} }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = ((uint32_t)id[0] << 16u) |
                           ((uint32_t)id[1] << 8u)  |
                           ((uint32_t)id[2]);        /* 0x00EF4017 */
    while (1) {}
}
