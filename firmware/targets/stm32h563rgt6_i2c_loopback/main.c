/*
 * stm32h563rgt6_i2c_loopback
 *
 * I2C1 master: PB6 (SCL, AF4), PB7 (SDA, AF4)
 * I2C2 slave:  PB10(SCL, AF4), PB12(SDA, AF4)
 * Bench wiring: PB6↔PB10 (SCL), PB7↔PB12 (SDA)
 *
 * H563 I2Cv2 (same IP as H750/U5):
 *   TIMINGR replaces CCR/TRISE.
 *   CR2 holds SADD, RD_WRN, NBYTES, START, STOP, AUTOEND.
 *   ISR/ICR replace SR1/SR2.
 *   Reset via RCC_APB1LRSTR (no hardware SWRST in CR1).
 *
 * TIMINGR = 0xF0420F13: 100 kHz @ PCLK1 = 64 MHz (HSI, reset default).
 *   PRESC=15 → tpresc = 250 ns
 *   SCLDEL=4, SDADEL=2, SCLH=15, SCLL=19
 *
 * HIGH_PRIORITY db885cac: RCC reset both I2C before config to clear
 * stale BUSY state from prior GDB loads.
 *
 * Protocol:
 *   Write 4 bytes {0xA1,0xB2,0xC3,0xD4} to slave (addr 0x42) → STOP.
 *   Read 4 bytes back from slave → STOP/NACK.
 *   Verify rx == tx.
 *
 * RCC (H563):
 *   RCC_BASE=0x44020C00, RCC_AHB2ENR=+0x08C (bit1=GPIOB)
 *   RCC_APB1LENR=+0x09C (bit21=I2C1, bit22=I2C2)
 *   RCC_APB1LRSTR=+0x098
 *
 * FAIL codes:
 *   0xE001 — I2C1 BUSY stuck after RCC reset
 *   0x10   — slave ADDR timeout (write phase)
 *   0x11   — slave DIR wrong (write phase)
 *   0x12   — slave DIR wrong (read phase)
 *   0x1x   — slave ADDR timeout (read phase) [0x10|1]
 *   0x20|i — master TXIS timeout, byte i
 *   0x30|i — slave RXNE timeout, byte i
 *   0x40   — master TC timeout after write
 *   0x41   — slave STOPF timeout after write
 *   0x50|i — slave TXIS timeout (read phase), byte i
 *   0x60|i — master RXNE timeout (read phase), byte i
 *   0x70   — master TC timeout after read
 *   0x71   — slave STOPF timeout after read
 *   0x80   — data mismatch, detail0 = (byte_idx<<8)|rx
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE          0x44020C00u
#define RCC_AHB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB1LRSTR     (*(volatile uint32_t *)(RCC_BASE + 0x098u))
#define RCC_APB1LENR      (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))

/* ── GPIOB ───────────────────────────────────────────────────────── */
#define GPIOB_BASE    0x42020400u
#define GPIOB_MODER   (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OTYPER  (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_OSPEEDR (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_PUPDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_AFRL    (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))
#define GPIOB_AFRH    (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

/* ── I2C1 master (APB1L, 0x40005400) ─────────────────────────────── */
#define I2C1_BASE     0x40005400u
#define I2C1_CR1      (*(volatile uint32_t *)(I2C1_BASE + 0x00u))
#define I2C1_CR2      (*(volatile uint32_t *)(I2C1_BASE + 0x04u))
#define I2C1_TIMINGR  (*(volatile uint32_t *)(I2C1_BASE + 0x10u))
#define I2C1_ISR      (*(volatile uint32_t *)(I2C1_BASE + 0x18u))
#define I2C1_ICR      (*(volatile uint32_t *)(I2C1_BASE + 0x1Cu))
#define I2C1_RXDR     (*(volatile uint32_t *)(I2C1_BASE + 0x24u))
#define I2C1_TXDR     (*(volatile uint32_t *)(I2C1_BASE + 0x28u))

/* ── I2C2 slave (APB1L, 0x40005800) ──────────────────────────────── */
#define I2C2_BASE     0x40005800u
#define I2C2_CR1      (*(volatile uint32_t *)(I2C2_BASE + 0x00u))
#define I2C2_CR2      (*(volatile uint32_t *)(I2C2_BASE + 0x04u))
#define I2C2_OAR1     (*(volatile uint32_t *)(I2C2_BASE + 0x08u))
#define I2C2_TIMINGR  (*(volatile uint32_t *)(I2C2_BASE + 0x10u))
#define I2C2_ISR      (*(volatile uint32_t *)(I2C2_BASE + 0x18u))
#define I2C2_ICR      (*(volatile uint32_t *)(I2C2_BASE + 0x1Cu))
#define I2C2_RXDR     (*(volatile uint32_t *)(I2C2_BASE + 0x24u))
#define I2C2_TXDR     (*(volatile uint32_t *)(I2C2_BASE + 0x28u))

/* I2C CR1 */
#define I2C_CR1_PE        (1u << 0u)

/* I2C CR2 */
#define I2C_CR2_RD_WRN    (1u << 10u)
#define I2C_CR2_START     (1u << 13u)
#define I2C_CR2_STOP      (1u << 14u)
#define I2C_CR2_AUTOEND   (1u << 25u)

/* I2C ISR */
#define I2C_ISR_TXE       (1u << 0u)
#define I2C_ISR_TXIS      (1u << 1u)
#define I2C_ISR_RXNE      (1u << 2u)
#define I2C_ISR_ADDR      (1u << 3u)
#define I2C_ISR_STOPF     (1u << 5u)
#define I2C_ISR_TC        (1u << 6u)
#define I2C_ISR_BUSY      (1u << 15u)
#define I2C_ISR_DIR       (1u << 16u)

/* I2C ICR */
#define I2C_ICR_ADDRCF    (1u << 3u)
#define I2C_ICR_STOPCF    (1u << 5u)
#define I2C_ICR_NACKCF    (1u << 4u)

/* I2C OAR1 */
#define I2C_OAR1_OA1EN    (1u << 15u)

/* TIMINGR for 100 kHz SM @ PCLK1 = 64 MHz (HSI reset default) */
#define I2C_TIMINGR_100K  0xF0420F13u

#define TIMEOUT     500000u
#define N_BYTES     4u
#define SLAVE_ADDR  0x42u

static int wait_isr(volatile uint32_t *isr, uint32_t mask)
{
    uint32_t t = TIMEOUT;
    while ((*isr & mask) == 0u) {
        if (--t == 0u) return -1;
    }
    return 0;
}

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable GPIOB (AHB2 bit1), I2C1+I2C2 (APB1L bit21+bit22) */
    RCC_AHB2ENR  |= (1u << 1u);
    RCC_APB1LENR |= (1u << 21u) | (1u << 22u);
    (void)RCC_AHB2ENR;
    (void)RCC_APB1LENR;

    /* 2. RCC reset both I2C to clear stale BUSY (HIGH_PRIORITY db885cac) */
    RCC_APB1LRSTR |= (1u << 21u) | (1u << 22u);
    (void)RCC_APB1LRSTR;
    for (volatile uint32_t d = 0; d < 200u; d++) {}
    RCC_APB1LRSTR &= ~((1u << 21u) | (1u << 22u));
    (void)RCC_APB1LRSTR;
    for (volatile uint32_t d = 0; d < 200u; d++) {}

    /* 3. GPIO config:
     *   PB6=I2C1_SCL(AF4), PB7=I2C1_SDA(AF4)
     *   PB10=I2C2_SCL(AF4), PB12=I2C2_SDA(AF4)
     *   All: AF mode, open-drain, pull-up, high speed.
     */
    /* MODER: AF=10 for PB6[13:12], PB7[15:14], PB10[21:20], PB12[25:24] */
    GPIOB_MODER &= ~((0x3u << 12u) | (0x3u << 14u) |
                     (0x3u << 20u) | (0x3u << 24u));
    GPIOB_MODER |=  ((0x2u << 12u) | (0x2u << 14u) |
                     (0x2u << 20u) | (0x2u << 24u));
    /* OTYPER: open-drain for PB6, PB7, PB10, PB12 */
    GPIOB_OTYPER |= (1u << 6u) | (1u << 7u) | (1u << 10u) | (1u << 12u);
    /* OSPEEDR: very high speed (11) */
    GPIOB_OSPEEDR &= ~((0x3u << 12u) | (0x3u << 14u) |
                       (0x3u << 20u) | (0x3u << 24u));
    GPIOB_OSPEEDR |=  ((0x3u << 12u) | (0x3u << 14u) |
                       (0x3u << 20u) | (0x3u << 24u));
    /* PUPDR: pull-up (01) */
    GPIOB_PUPDR &= ~((0x3u << 12u) | (0x3u << 14u) |
                     (0x3u << 20u) | (0x3u << 24u));
    GPIOB_PUPDR |=  ((0x1u << 12u) | (0x1u << 14u) |
                     (0x1u << 20u) | (0x1u << 24u));
    /* AFRL: PB6[27:24]=AF4, PB7[31:28]=AF4 */
    GPIOB_AFRL &= ~((0xFu << 24u) | (0xFu << 28u));
    GPIOB_AFRL |=  ((0x4u << 24u) | (0x4u << 28u));
    /* AFRH: PB10[11:8]=AF4, PB12[19:16]=AF4 */
    GPIOB_AFRH &= ~((0xFu << 8u) | (0xFu << 16u));
    GPIOB_AFRH |=  ((0x4u << 8u) | (0x4u << 16u));
    (void)GPIOB_MODER;

    /* 4. I2C2 slave init (before master so slave is ready) */
    I2C2_CR1     = 0u;                           /* PE=0 while configuring */
    I2C2_TIMINGR = I2C_TIMINGR_100K;
    I2C2_OAR1    = (SLAVE_ADDR << 1u) | I2C_OAR1_OA1EN;
    I2C2_CR1     = I2C_CR1_PE;                   /* enable; NOSTRETCH=0 (clock stretch) */

    for (volatile uint32_t d = 0; d < 1000u; d++) {}

    /* 5. I2C1 master init */
    I2C1_CR1     = 0u;
    I2C1_TIMINGR = I2C_TIMINGR_100K;
    I2C1_CR1     = I2C_CR1_PE;

    for (volatile uint32_t d = 0; d < 1000u; d++) {}

    /* Check bus not BUSY */
    if (I2C1_ISR & I2C_ISR_BUSY) {
        ael_mailbox_fail(0xE001u, I2C1_ISR);
        while (1) {}
    }

    static const uint8_t tx_buf[N_BYTES] = { 0xA1u, 0xB2u, 0xC3u, 0xD4u };
    uint8_t slave_buf[N_BYTES] = { 0u };
    uint8_t rx_buf[N_BYTES]    = { 0u };

    /* ================================================================
     * WRITE PHASE: master writes N_BYTES to slave addr 0x42
     * AUTOEND=0 so TC fires → we send explicit STOP.
     * ================================================================ */
    I2C1_CR2 = ((SLAVE_ADDR << 1u) & 0x3FFu)
             | (N_BYTES << 16u)
             | I2C_CR2_START;

    /* Slave: wait ADDR, verify DIR=0 (write), clear */
    if (wait_isr(&I2C2_ISR, I2C_ISR_ADDR) < 0) {
        ael_mailbox_fail(0x10u, I2C1_ISR);
        while (1) {}
    }
    if (I2C2_ISR & I2C_ISR_DIR) {
        ael_mailbox_fail(0x11u, I2C2_ISR);
        while (1) {}
    }
    I2C2_ICR = I2C_ICR_ADDRCF;

    /* Interleaved byte exchange */
    for (uint32_t i = 0u; i < N_BYTES; i++) {
        if (wait_isr(&I2C1_ISR, I2C_ISR_TXIS) < 0) {
            ael_mailbox_fail(0x20u | i, I2C1_ISR);
            while (1) {}
        }
        I2C1_TXDR = tx_buf[i];

        if (wait_isr(&I2C2_ISR, I2C_ISR_RXNE) < 0) {
            ael_mailbox_fail(0x30u | i, I2C2_ISR);
            while (1) {}
        }
        slave_buf[i] = (uint8_t)I2C2_RXDR;
    }

    /* Master: wait TC, send STOP */
    if (wait_isr(&I2C1_ISR, I2C_ISR_TC) < 0) {
        ael_mailbox_fail(0x40u, I2C1_ISR);
        while (1) {}
    }
    I2C1_CR2 |= I2C_CR2_STOP;

    /* Slave: clear STOPF */
    if (wait_isr(&I2C2_ISR, I2C_ISR_STOPF) < 0) {
        ael_mailbox_fail(0x41u, I2C2_ISR);
        while (1) {}
    }
    I2C2_ICR = I2C_ICR_STOPCF;

    for (volatile uint32_t d = 0; d < 1000u; d++) {}

    /* ================================================================
     * READ PHASE: master reads N_BYTES back from slave
     * ================================================================ */
    I2C1_CR2 = ((SLAVE_ADDR << 1u) & 0x3FFu)
             | (N_BYTES << 16u)
             | I2C_CR2_RD_WRN
             | I2C_CR2_START;

    /* Slave: wait ADDR, verify DIR=1 (read), clear */
    if (wait_isr(&I2C2_ISR, I2C_ISR_ADDR) < 0) {
        ael_mailbox_fail(0x10u | 1u, I2C2_ISR);
        while (1) {}
    }
    if (!(I2C2_ISR & I2C_ISR_DIR)) {
        ael_mailbox_fail(0x12u, I2C2_ISR);
        while (1) {}
    }
    I2C2_ICR = I2C_ICR_ADDRCF;

    /* Interleaved: slave writes, master reads */
    for (uint32_t i = 0u; i < N_BYTES; i++) {
        if (wait_isr(&I2C2_ISR, I2C_ISR_TXIS) < 0) {
            ael_mailbox_fail(0x50u | i, I2C2_ISR);
            while (1) {}
        }
        I2C2_TXDR = slave_buf[i];

        if (wait_isr(&I2C1_ISR, I2C_ISR_RXNE) < 0) {
            ael_mailbox_fail(0x60u | i, I2C1_ISR);
            while (1) {}
        }
        rx_buf[i] = (uint8_t)I2C1_RXDR;
    }

    /* Master: wait TC, send STOP */
    if (wait_isr(&I2C1_ISR, I2C_ISR_TC) < 0) {
        ael_mailbox_fail(0x70u, I2C1_ISR);
        while (1) {}
    }
    I2C1_CR2 |= I2C_CR2_STOP;

    /* Slave: clear STOPF + NACK */
    if (wait_isr(&I2C2_ISR, I2C_ISR_STOPF) < 0) {
        ael_mailbox_fail(0x71u, I2C2_ISR);
        while (1) {}
    }
    I2C2_ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;

    /* ================================================================
     * VERIFY
     * ================================================================ */
    for (uint32_t i = 0u; i < N_BYTES; i++) {
        if (rx_buf[i] != tx_buf[i]) {
            ael_mailbox_fail(0x80u, (i << 8u) | rx_buf[i]);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = ((uint32_t)rx_buf[0] << 24u) |
                           ((uint32_t)rx_buf[1] << 16u) |
                           ((uint32_t)rx_buf[2] << 8u)  |
                           ((uint32_t)rx_buf[3]);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
