/*
 * stm32h750_spi_loopback — SPI1 MISO↔MOSI loopback self-test
 *
 * SPI1 master sends 4 bytes; MISO↔MOSI bench wire returns them.
 * Verifies: SPI1 peripheral, GPIOB AF5, bench wire PB4↔PB5.
 *
 * Pin assignment (RM0433 Table 10, AF5):
 *   PB3 = SPI1_SCK  (AF5) — clock output, no bench wire needed
 *   PB4 = SPI1_MISO (AF5)  }
 *   PB5 = SPI1_MOSI (AF5)  } wired together: PB4↔PB5
 *
 * SPI1_BASE = 0x40013000 (APB2, D2 domain, RM0433 §57)
 * RCC_APB2ENR bit 12 = SPI1EN (RM0433 §8.7.40)
 *
 * SPI IP: SPIv2 (H7-specific, differs from STM32G4 in kernel clock).
 * Configuration: 8-bit, Master, Full-Duplex, SSM=1, CKMODE=00, MBR=/4.
 *
 * SPI kernel clock: H7 default = PLL1_Q (invalid when PLL not running on HSI).
 * Must switch RCC_D2CCIP1R SPI123SEL[14:12] to 0b100 (PER_CK = HSI = 64 MHz).
 * APB2 clock for registers = 64 MHz; SPI kernel (SCK source) = PER_CK = 64 MHz.
 * SPI clock = 64 MHz / MBR(/4) = 16 MHz.
 *
 * Transfer sequence (TSIZE=4 auto-stop):
 *   1. Configure CFG1/CFG2/CR2/CR1 → set SPE
 *   2. Set CSTART (starts clocking immediately once TXFIFO has data)
 *   3. Per byte: wait TXP → write TXDR8 → wait RXP → read RXDR8
 *
 * Error codes:
 *   bit 0: byte 0 (0x12) mismatch
 *   bit 1: byte 1 (0x34) mismatch
 *   bit 2: byte 2 (0xAB) mismatch
 *   bit 3: byte 3 (0xCD) mismatch
 *   0x10: TX FIFO timeout
 *   0x20: RX FIFO timeout
 *
 * All register addresses from RM0433 + verified against stm32h750xx.h.
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ───────────────────────────────────────────────────────── */
#define RCC_BASE            0x58024400u
#define RCC_AHB4ENR         (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x0F0u))
#define RCC_APB2RSTR        (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))
/*
 * RCC_D2CCIP1R offset 0x050 (RM0433 §8.7.37):
 *   SPI123SEL [14:12]:  000=PLL1_Q (default), 100=PER_CK
 * CKPERSEL [29:28] in RCC_D1CCIPR defaults to 00 → PER_CK = HSI = 64 MHz.
 */
#define RCC_D2CCIP1R        (*(volatile uint32_t *)(RCC_BASE + 0x050u))
#define RCC_AHB4ENR_GPIOBEN (1u << 1)
#define RCC_APB2ENR_SPI1EN  (1u << 12)
#define RCC_APB2RSTR_SPI1RST (1u << 12)
#define RCC_D2CCIP1R_SPI123SEL_MSK   (0x7u << 12u)
#define RCC_D2CCIP1R_SPI123SEL_PERCK (0x4u << 12u)   /* 0b100 = PER_CK */

/* ── GPIOB (base 0x58020400) ──────────────────────────────────── */
#define GPIOB_BASE    0x58020400u
#define GPIOB_MODER   (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OSPEEDR (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_AFRL    (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))

/* ── SPI1 (RM0433 §57, APB2, base 0x40013000) ─────────────────── */
#define SPI1_BASE   0x40013000u
#define SPI1_CR1    (*(volatile uint32_t *)(SPI1_BASE + 0x00u))
#define SPI1_CR2    (*(volatile uint32_t *)(SPI1_BASE + 0x04u))
#define SPI1_CFG1   (*(volatile uint32_t *)(SPI1_BASE + 0x08u))
#define SPI1_CFG2   (*(volatile uint32_t *)(SPI1_BASE + 0x0Cu))
#define SPI1_SR     (*(volatile uint32_t *)(SPI1_BASE + 0x14u))
#define SPI1_IFCR   (*(volatile uint32_t *)(SPI1_BASE + 0x18u))
/* Byte-width access to TXDR / RXDR for 8-bit transfers */
#define SPI1_TXDR8  (*(volatile uint8_t  *)(SPI1_BASE + 0x20u))
#define SPI1_RXDR8  (*(volatile uint8_t  *)(SPI1_BASE + 0x30u))

/* SPI CR1 bits (RM0433 §57.8.1) */
#define SPI_CR1_SPE    (1u << 0)
#define SPI_CR1_CSTART (1u << 9)
#define SPI_CR1_CSUSP  (1u << 10)
#define SPI_CR1_SSI    (1u << 12)

/* SPI SR bits (RM0433 §57.8.7 / stm32h750xx.h) */
#define SPI_SR_RXP   (1u << 0)
#define SPI_SR_TXP   (1u << 1)
#define SPI_SR_EOT   (1u << 3)
#define SPI_SR_MODF  (1u << 9)    /* bit 9 per CMSIS stm32h750xx.h */
#define SPI_SR_SUSP  (1u << 11)
/* IFCR: write 1 to clear corresponding SR flag */
#define SPI_IFCR_MODFC (1u << 9)  /* MODFC: clear MODF flag */

/* SPI CFG1: DSIZE[4:0]=7 (8-bit), MBR[30:28]=001 (/4 → 16 MHz) */
#define SPI_CFG1_VAL  ((7u << 0u) | (1u << 28u))

/*
 * SPI CFG2: MASTER=1 (bit22), SSM=1 (bit26) — software slave management.
 * SSM=1 means NSS is controlled by SSI bit in CR1, not by any NSS pin.
 * CR1.SSI=1 → internal NSS = HIGH (slave deselected from the master's view).
 * This eliminates any MODF caused by a floating/unconfigured NSS pin.
 * No dedicated NSS pin needed for loopback.
 */
#define SPI_CFG2_VAL  ((1u << 22u) | (1u << 26u))   /* MASTER | SSM */

/* ── SysTick ─────────────────────────────────────────────────── */
#define SYST_CSR       (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR       (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR       (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

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

    /* Enable GPIOB and SPI1 clocks */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC_AHB4ENR;
    RCC_APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC_APB2ENR;

    /*
     * Switch SPI1 kernel clock from PLL1_Q (default, invalid without PLL)
     * to PER_CK = HSI = 64 MHz.  Must be done before SPE=1.
     */
    RCC_D2CCIP1R = (RCC_D2CCIP1R & ~RCC_D2CCIP1R_SPI123SEL_MSK)
                 | RCC_D2CCIP1R_SPI123SEL_PERCK;
    (void)RCC_D2CCIP1R;

    /* Reset SPI1 to clear any stale state from previous runs */
    RCC_APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    (void)RCC_APB2RSTR;
    RCC_APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
    (void)RCC_APB2RSTR;
    delay_ticks(2u);   /* 2 ms: allow kernel clock to stabilise after reset */

    ael_mailbox_init();

    /*
     * GPIO: PB3=SCK, PB4=MISO, PB5=MOSI — all AF5.
     * AFRL bits: PB3[15:12], PB4[19:16], PB5[23:20].
     * MODER bits: PB3[7:6], PB4[9:8], PB5[11:10] = 10 (AF).
     */
    GPIOB_AFRL &= ~((0xFu << 12u) | (0xFu << 16u) | (0xFu << 20u));
    GPIOB_AFRL |=  ((5u  << 12u) | (5u  << 16u) | (5u  << 20u));
    GPIOB_MODER &= ~((0x3u << 6u) | (0x3u << 8u) | (0x3u << 10u));
    GPIOB_MODER |=  ((0x2u << 6u) | (0x2u << 8u) | (0x2u << 10u));
    GPIOB_OSPEEDR |= ((0x3u << 6u) | (0x3u << 8u) | (0x3u << 10u));
    (void)GPIOB_MODER;

    /*
     * SPI1 configuration — HAL-order init (RM0433 §57.4.9):
     *   1. SSI=1 in CR1 FIRST (before CFG1/CFG2, before SPE=1).
     *      With SSM=1: SSI=1 → internal NSS held HIGH → no MODF on SPE.
     *   2. Write CFG1, CFG2 (SPE must be 0 to write these).
     *   3. Write CR2 (TSIZE=4).
     *   4. Write CR1 with SSI=1 | SPE=1 atomically.
     *   5. Write CSTART (starts clocking once TX FIFO has data).
     *
     * H750 SPIv2: MODF (SR bit 9) fires spuriously on first SPE=1 if the
     * kernel clock edge races with SPE.  Clear MODF via IFCR bit 9 (MODFC)
     * while SPE=0, then re-enable.  Loop up to 5 times.
     */
    {
        uint32_t modf_tries = 0u;
        for (;;) {
            SPI1_CR1  = SPI_CR1_SSI;      /* SSI=1 first, SPE=0 */
            SPI1_CFG1 = SPI_CFG1_VAL;
            SPI1_CFG2 = SPI_CFG2_VAL;
            SPI1_CR2  = 4u;               /* TSIZE = 4 frames */
            SPI1_CR1  = SPI_CR1_SSI | SPI_CR1_SPE;  /* enable with SSI=1 */
            delay_ticks(1u);
            if ((SPI1_SR & SPI_SR_MODF) == 0u) {
                break;                    /* SPE stable, no MODF */
            }
            /* MODF fired — SPE auto-cleared; clear flag and retry */
            SPI1_IFCR = SPI_IFCR_MODFC;
            (void)SPI1_SR;
            modf_tries++;
            if (modf_tries >= 5u) {
                ael_mailbox_fail(0x30u, (SPI1_SR << 16u) | modf_tries);
                while (1) {}
            }
            delay_ticks(2u);
        }
    }

    /*
     * Write SSI|SPE|CSTART in a single 32-bit store — avoids the read-modify-write
     * race that |= would introduce (MODF auto-clears SPE; if MODF fires between the
     * read and the write of |=, CSTART lands with SPE=0 → second MODF).
     * Per RM0433, writing SPE=1 and CSTART=1 simultaneously is valid.
     */
    SPI1_CR1 = SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART;

    static const uint8_t tx[4] = { 0x12u, 0x34u, 0xABu, 0xCDu };
    uint8_t rx[4] = { 0u, 0u, 0u, 0u };
    uint32_t err = 0u;

    for (uint32_t i = 0u; i < 4u; i++) {
        /* Wait for TX FIFO space */
        uint32_t timeout = 1000000u;
        while (((SPI1_SR & SPI_SR_TXP) == 0u) && (--timeout > 0u)) {}
        if ((SPI1_SR & SPI_SR_TXP) == 0u) { err = 0x10u; goto done; }
        SPI1_TXDR8 = tx[i];

        /* Wait for received byte */
        timeout = 1000000u;
        while (((SPI1_SR & SPI_SR_RXP) == 0u) && (--timeout > 0u)) {}
        if ((SPI1_SR & SPI_SR_RXP) == 0u) {
            /* Include SR+CR1 in detail for diagnosis */
            err = 0x20u | (i << 8u);
            goto done;
        }
        rx[i] = SPI1_RXDR8;
    }

    /* Wait for EOT */
    {
        uint32_t timeout = 1000000u;
        while (((SPI1_SR & SPI_SR_EOT) == 0u) && (--timeout > 0u)) {}
    }

    /* Verify */
    for (uint32_t i = 0u; i < 4u; i++) {
        if (rx[i] != tx[i]) { err |= (1u << i); }
    }

done:
    SPI1_CR1 |= SPI_CR1_CSUSP;
    delay_ticks(1u);
    SPI1_CR1 &= ~SPI_CR1_SPE;

    if (err == 0u) {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = ((uint32_t)rx[0] << 24u) |
                               ((uint32_t)rx[1] << 16u) |
                               ((uint32_t)rx[2] << 8u)  |
                               ((uint32_t)rx[3]);
        while (1) {}
    } else {
        ael_mailbox_fail(err,
            ((uint32_t)SPI1_SR << 16u) |
            ((uint32_t)rx[0] << 8u) | (uint32_t)rx[1]);
        while (1) {}
    }
}
