/*
 * stm32h750_i2c_loopback — I2C1 Master / I2C2 Slave loopback
 *
 * I2C1 master: PB6 (SCL, AF4), PB7 (SDA, AF4)
 * I2C2 slave:  PB10(SCL, AF4), PB11(SDA, AF4)
 * Bench wiring required: PB6↔PB10, PB7↔PB11
 *
 * H750 uses I2Cv2 (different from F4 I2Cv1):
 *   - TIMINGR replaces CCR/TRISE
 *   - CR2 holds SADD, RD_WRN, NBYTES, START, STOP, AUTOEND
 *   - ISR/ICR replace SR1/SR2
 *   - No hardware SWRST in CR1; reset via RCC_APB1LRSTR
 *
 * TIMINGR = 0xF0420F13:
 *   PRESC=15 → tpresc = 16/64MHz = 250 ns
 *   SCLDEL=4, SDADEL=2
 *   SCLH=15  → tSCLH = 16×250 = 4000 ns  (spec ≥ 4000 ns ✓)
 *   SCLL=19  → tSCLL = 20×250 = 5000 ns  (spec ≥ 4700 ns ✓)
 *   fSCL ≈ 100 kHz
 *
 * Protocol:
 *   1. Master WRITE 4 bytes {0xA1,0xB2,0xC3,0xD4} to slave addr 0x42.
 *   2. Slave stores received bytes; Master sends STOP.
 *   3. Master READ 4 bytes from slave.
 *   4. Slave sends stored bytes; Master generates STOP/NACK.
 *   5. Verify rx == tx.
 *
 * Clock stretching (NOSTRETCH=0 default in slave): slave stretches SCL
 * when not ready, enabling single-CPU interleaved polling of both.
 *
 * Error codes:
 *   0xE001 = BUSY stuck after RCC reset
 *   0x10+i = slave ADDR timeout, phase i (0=write,1=read)
 *   0x20+i = master write TXIS timeout, byte i
 *   0x30+i = slave write RXNE timeout, byte i
 *   0x40   = master TC timeout after write
 *   0x50+i = slave read TXIS timeout, byte i
 *   0x60+i = master read RXNE timeout, byte i
 *   0x70   = master TC timeout after read
 *   0x80   = data mismatch (detail0 = first bad byte idx<<8 | rx)
 */

#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ─────────────────────────────────────────────────────────── */
#define RCC_BASE          0x58024400u
#define RCC_AHB4ENR       (*(volatile uint32_t *)(RCC_BASE + 0x0E0u))
#define RCC_APB1LENR      (*(volatile uint32_t *)(RCC_BASE + 0x0E8u))
#define RCC_APB1LRSTR     (*(volatile uint32_t *)(RCC_BASE + 0x090u))
#define RCC_AHB4ENR_GPIOBEN  (1u << 1u)
#define RCC_APB1LENR_I2C1EN  (1u << 21u)
#define RCC_APB1LENR_I2C2EN  (1u << 22u)
#define RCC_APB1LRSTR_I2C1RST (1u << 21u)
#define RCC_APB1LRSTR_I2C2RST (1u << 22u)

/* ── GPIOB ───────────────────────────────────────────────────────── */
#define GPIOB_BASE    0x58020400u
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
#define I2C1_OAR1     (*(volatile uint32_t *)(I2C1_BASE + 0x08u))
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
#define I2C_CR1_PE          (1u << 0u)
#define I2C_CR1_NOSTRETCH   (1u << 17u)   /* 0=stretch (slave default) */

/* I2C CR2 (master) */
#define I2C_CR2_RD_WRN      (1u << 10u)   /* 0=write, 1=read */
#define I2C_CR2_START       (1u << 13u)
#define I2C_CR2_STOP        (1u << 14u)
#define I2C_CR2_NACK        (1u << 15u)   /* slave NACK */
#define I2C_CR2_AUTOEND     (1u << 25u)   /* auto STOP after NBYTES */

/* I2C ISR bits */
#define I2C_ISR_TXE         (1u << 0u)
#define I2C_ISR_TXIS        (1u << 1u)   /* TX interrupt: write TXDR */
#define I2C_ISR_RXNE        (1u << 2u)
#define I2C_ISR_ADDR        (1u << 3u)   /* address matched (slave) */
#define I2C_ISR_STOPF       (1u << 5u)
#define I2C_ISR_TC          (1u << 6u)   /* transfer complete (master, AUTOEND=0) */
#define I2C_ISR_BUSY        (1u << 15u)
#define I2C_ISR_DIR         (1u << 16u)  /* slave DIR: 0=write, 1=read */

/* I2C ICR bits */
#define I2C_ICR_ADDRCF      (1u << 3u)
#define I2C_ICR_STOPCF      (1u << 5u)
#define I2C_ICR_NACKCF      (1u << 4u)

/* OAR1 */
#define I2C_OAR1_OA1EN      (1u << 15u)

/*
 * TIMINGR for 100 kHz SM @ PCLK1 = 64 MHz (HSI):
 *   PRESC=15 → tpresc = 250 ns
 *   SCLDEL=4, SDADEL=2, SCLH=15, SCLL=19
 */
#define I2C_TIMINGR_100K    0xF0420F13u

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

#define TIMEOUT 500000u
#define N_BYTES 4u
#define SLAVE_ADDR 0x42u

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
    /* SysTick: 64 MHz HSI, 1 ms/tick */
    SYST_RVR = 63999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    /* Enable GPIOB clock */
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC_AHB4ENR;

    /* Enable I2C1 + I2C2 clocks */
    RCC_APB1LENR |= RCC_APB1LENR_I2C1EN | RCC_APB1LENR_I2C2EN;
    (void)RCC_APB1LENR;

    /* RCC reset both I2C peripherals to clear any stale BUSY state */
    RCC_APB1LRSTR |= RCC_APB1LRSTR_I2C1RST | RCC_APB1LRSTR_I2C2RST;
    (void)RCC_APB1LRSTR;
    RCC_APB1LRSTR &= ~(RCC_APB1LRSTR_I2C1RST | RCC_APB1LRSTR_I2C2RST);
    (void)RCC_APB1LRSTR;
    delay_ticks(2u);

    /*
     * GPIO: PB6(I2C1_SCL), PB7(I2C1_SDA), PB10(I2C2_SCL), PB11(I2C2_SDA)
     * All: AF4, open-drain, pull-up, high speed.
     * AFRL: PB6[27:24]=4, PB7[31:28]=4
     * AFRH: PB10[11:8]=4, PB11[15:12]=4
     */
    /* MODER = 10 (AF) for PB6[13:12], PB7[15:14], PB10[21:20], PB11[23:22] */
    GPIOB_MODER &= ~((0x3u << 12u) | (0x3u << 14u) | (0x3u << 20u) | (0x3u << 22u));
    GPIOB_MODER |=  ((0x2u << 12u) | (0x2u << 14u) | (0x2u << 20u) | (0x2u << 22u));
    /* OTYPER = 1 (open-drain) */
    GPIOB_OTYPER |= (1u << 6u) | (1u << 7u) | (1u << 10u) | (1u << 11u);
    /* OSPEEDR = 11 (very high speed) */
    GPIOB_OSPEEDR |= (0x3u << 12u) | (0x3u << 14u) | (0x3u << 20u) | (0x3u << 22u);
    /* PUPDR = 01 (pull-up) */
    GPIOB_PUPDR &= ~((0x3u << 12u) | (0x3u << 14u) | (0x3u << 20u) | (0x3u << 22u));
    GPIOB_PUPDR |=  ((0x1u << 12u) | (0x1u << 14u) | (0x1u << 20u) | (0x1u << 22u));
    /* AFRL: PB6=AF4, PB7=AF4 */
    GPIOB_AFRL &= ~((0xFu << 24u) | (0xFu << 28u));
    GPIOB_AFRL |=  ((0x4u << 24u) | (0x4u << 28u));
    /* AFRH: PB10=AF4, PB11=AF4 */
    GPIOB_AFRH &= ~((0xFu << 8u) | (0xFu << 12u));
    GPIOB_AFRH |=  ((0x4u << 8u) | (0x4u << 12u));
    (void)GPIOB_MODER;

    /*
     * I2C2 slave init (init before master so slave is ready for first START):
     *   TIMINGR must be set while PE=0.
     *   OA1[7:1]=SLAVE_ADDR (7-bit), OA1EN=1.
     *   PE=1, NOSTRETCH=0 (stretch SCL when not ready — required for polling).
     */
    I2C2_CR1     = 0u;                 /* PE=0, clear all */
    I2C2_TIMINGR = I2C_TIMINGR_100K;
    I2C2_OAR1    = (SLAVE_ADDR << 1u) | I2C_OAR1_OA1EN;
    I2C2_CR1     = I2C_CR1_PE;         /* enable slave, NOSTRETCH=0 */

    delay_ticks(1u);

    /*
     * I2C1 master init:
     *   TIMINGR must be set while PE=0.
     *   PE=1.
     */
    I2C1_CR1     = 0u;
    I2C1_TIMINGR = I2C_TIMINGR_100K;
    I2C1_CR1     = I2C_CR1_PE;

    delay_ticks(1u);

    /* Check bus is not BUSY */
    if ((I2C1_ISR & I2C_ISR_BUSY) != 0u) {
        ael_mailbox_fail(0xE001u, I2C1_ISR);
        while (1) {}
    }

    static const uint8_t tx_buf[N_BYTES] = { 0xA1u, 0xB2u, 0xC3u, 0xD4u };
    uint8_t slave_buf[N_BYTES] = { 0u };
    uint8_t rx_buf[N_BYTES]    = { 0u };

    /* ================================================================
     * WRITE PHASE: master writes N_BYTES to slave
     * CR2: SADD[7:1]=SLAVE_ADDR, RD_WRN=0, NBYTES=N_BYTES, START=1
     * AUTOEND=0 so TC fires after last byte → we control STOP timing.
     * ================================================================ */
    I2C1_CR2 = ((SLAVE_ADDR << 1u) & 0x3FFu)    /* SADD[7:1] */
             | (N_BYTES << 16u)                   /* NBYTES */
             | I2C_CR2_START;                     /* generate START, write */

    /* Slave: wait ADDR match, check DIR=0 (write), clear ADDR */
    if (wait_isr(&I2C2_ISR, I2C_ISR_ADDR) < 0) {
        ael_mailbox_fail(0x10u, I2C1_ISR);
        while (1) {}
    }
    if ((I2C2_ISR & I2C_ISR_DIR) != 0u) {  /* DIR should be 0 for write */
        ael_mailbox_fail(0x11u, I2C2_ISR);
        while (1) {}
    }
    I2C2_ICR = I2C_ICR_ADDRCF;   /* clear address flag, SCL released */

    /* Interleaved: for each byte, wait master TXIS → write, then slave RXNE → read */
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

    /* Master: wait TC (all bytes sent, no AUTOEND) then send STOP */
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

    delay_ticks(1u);

    /* ================================================================
     * READ PHASE: master reads N_BYTES from slave (slave echoes slave_buf)
     * CR2: SADD[7:1]=SLAVE_ADDR, RD_WRN=1, NBYTES=N_BYTES, START=1
     * ================================================================ */
    I2C1_CR2 = ((SLAVE_ADDR << 1u) & 0x3FFu)
             | (N_BYTES << 16u)
             | I2C_CR2_RD_WRN
             | I2C_CR2_START;

    /* Slave: wait ADDR match, check DIR=1 (read), clear ADDR */
    if (wait_isr(&I2C2_ISR, I2C_ISR_ADDR) < 0) {
        ael_mailbox_fail(0x10u | 1u, I2C2_ISR);
        while (1) {}
    }
    if ((I2C2_ISR & I2C_ISR_DIR) == 0u) {  /* DIR should be 1 for read */
        ael_mailbox_fail(0x12u, I2C2_ISR);
        while (1) {}
    }
    I2C2_ICR = I2C_ICR_ADDRCF;

    /* Interleaved: slave waits TXIS → writes byte; master waits RXNE → reads */
    for (uint32_t i = 0u; i < N_BYTES; i++) {
        if (wait_isr(&I2C2_ISR, I2C_ISR_TXIS) < 0) {
            ael_mailbox_fail(0x50u | i, I2C2_ISR);
            while (1) {}
        }
        I2C2_TXDR = slave_buf[i];

        if (wait_isr(&I2C1_ISR, I2C_ISR_RXNE) < 0) {
            ael_mailbox_fail(0x60u | i, I2C2_ISR);
            while (1) {}
        }
        rx_buf[i] = (uint8_t)I2C1_RXDR;
    }

    /* Master: wait TC, then send STOP */
    if (wait_isr(&I2C1_ISR, I2C_ISR_TC) < 0) {
        ael_mailbox_fail(0x70u, I2C1_ISR);
        while (1) {}
    }
    I2C1_CR2 |= I2C_CR2_STOP;

    /* Slave: clear STOPF and NACK flag */
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

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = ((uint32_t)rx_buf[0] << 24u) |
                           ((uint32_t)rx_buf[1] << 16u) |
                           ((uint32_t)rx_buf[2] << 8u)  |
                           ((uint32_t)rx_buf[3]);
    while (1) {}
}
