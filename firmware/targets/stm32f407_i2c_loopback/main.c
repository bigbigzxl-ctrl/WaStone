/*
 * STM32F407 — I2C1 Master / I2C2 Slave Loopback
 *
 * I2C1 master: PB6 (SCL, AF4), PB7  (SDA, AF4)
 * I2C2 slave:  PB10(SCL, AF4), PB11 (SDA, AF4)
 *
 * External wiring required: PB6↔PB10 (SCL bus), PB7↔PB11 (SDA bus).
 * Pull-up: STM32 internal ~40 kΩ via PUPDR=01 on all four pins.
 *
 * Protocol:
 *   1. Master WRITE: send tx_buf[4] = {0xA1,0xB2,0xC3,0xD4} to slave addr 0x42.
 *   2. Slave stores received bytes in slave_buf[4].
 *   3. Master READ: read 4 bytes back from slave (slave echoes slave_buf).
 *   4. Master verifies rx_buf == tx_buf.
 *
 * Clock: 16 MHz HSI, APB1 = 16 MHz.
 *   CCR = 80  (SM 100 kHz)
 *   TRISE = 17 (1000 ns rise time spec)
 *
 * Mailbox at 0x2001FC00 (STM32F407 SRAM1 top − 1 KB).
 *   PASS: 4/4 bytes verified.
 *   FAIL: error_code = step that timed out (ERR_* constants).
 *         detail0    = diagnostic SR snapshot on ADDR timeout.
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

/* ---- RCC ------------------------------------------------------------------ */
#define RCC_BASE            0x40023800u
#define RCC_AHB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x30u))
#define RCC_APB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x40u))
#define RCC_AHB1ENR_GPIOBEN (1u << 1)
#define RCC_APB1ENR_I2C1EN  (1u << 21)
#define RCC_APB1ENR_I2C2EN  (1u << 22)

/* ---- GPIOB ---------------------------------------------------------------- */
#define GPIOB_BASE   0x40020400u
#define GPIOB_MODER  (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OTYPER (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_PUPDR  (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_AFRL   (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))
#define GPIOB_AFRH   (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

/* ---- I2C1 (master) base 0x40005400 ---------------------------------------- */
#define I2C1_BASE   0x40005400u
#define I2C1_CR1    (*(volatile uint32_t *)(I2C1_BASE + 0x00u))
#define I2C1_CR2    (*(volatile uint32_t *)(I2C1_BASE + 0x04u))
#define I2C1_DR     (*(volatile uint32_t *)(I2C1_BASE + 0x10u))
#define I2C1_SR1    (*(volatile uint32_t *)(I2C1_BASE + 0x14u))
#define I2C1_SR2    (*(volatile uint32_t *)(I2C1_BASE + 0x18u))
#define I2C1_CCR    (*(volatile uint32_t *)(I2C1_BASE + 0x1Cu))
#define I2C1_TRISE  (*(volatile uint32_t *)(I2C1_BASE + 0x20u))

/* ---- I2C2 (slave)  base 0x40005800 ---------------------------------------- */
#define I2C2_BASE   0x40005800u
#define I2C2_CR1    (*(volatile uint32_t *)(I2C2_BASE + 0x00u))
#define I2C2_CR2    (*(volatile uint32_t *)(I2C2_BASE + 0x04u))
#define I2C2_OAR1   (*(volatile uint32_t *)(I2C2_BASE + 0x08u))
#define I2C2_DR     (*(volatile uint32_t *)(I2C2_BASE + 0x10u))
#define I2C2_SR1    (*(volatile uint32_t *)(I2C2_BASE + 0x14u))
#define I2C2_SR2    (*(volatile uint32_t *)(I2C2_BASE + 0x18u))
#define I2C2_CCR    (*(volatile uint32_t *)(I2C2_BASE + 0x1Cu))
#define I2C2_TRISE  (*(volatile uint32_t *)(I2C2_BASE + 0x20u))

/* I2C CR1 bits */
#define I2C_CR1_PE    (1u << 0)
#define I2C_CR1_START (1u << 8)
#define I2C_CR1_STOP  (1u << 9)
#define I2C_CR1_ACK   (1u << 10)
#define I2C_CR1_SWRST (1u << 15)

/* I2C SR1 bits */
#define I2C_SR1_SB    (1u << 0)
#define I2C_SR1_ADDR  (1u << 1)
#define I2C_SR1_BTF   (1u << 2)
#define I2C_SR1_RXNE  (1u << 6)
#define I2C_SR1_TXE   (1u << 7)

/* I2C SR2 bits */
#define I2C_SR2_BUSY  (1u << 1)

/* SysTick */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

/* Error codes */
#define ERR_WRITE_SB    1u
#define ERR_SWRITE_ADDR 2u
#define ERR_MWRITE_ADDR 3u
#define ERR_WRITE_DATA  4u
#define ERR_WRITE_BTF   5u
#define ERR_READ_SB     6u
#define ERR_SREAD_ADDR  7u
#define ERR_SREAD_TXE   8u
#define ERR_MREAD_ADDR  9u
#define ERR_READ_DATA   10u
#define ERR_MISMATCH    11u

#define N_BYTES     4u
#define I2C_TIMEOUT 200000u
#define SLAVE_ADDR  0x42u

static const uint8_t tx_buf[N_BYTES] = { 0xA1u, 0xB2u, 0xC3u, 0xD4u };
static uint8_t slave_buf[N_BYTES];
static uint8_t rx_buf[N_BYTES];

/* HardFault_Handler: force SYSRESETREQ instead of LOCKUP.
 * After GDB load, CPU may resume from old halt PC → HardFault.
 * Without this, Cortex-M4 enters LOCKUP (SWD cannot halt). */
void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0u; i < ms; i++) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

static int wait_flag(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t t = I2C_TIMEOUT;
    while ((*reg & mask) == 0u) {
        if (--t == 0u) return -1;
    }
    return 0;
}

int main(void)
{
    /* SysTick: 1 ms tick at 16 MHz HSI */
    SYST_RVR = 15999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    /* Enable GPIOB, I2C1, I2C2 clocks */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC_AHB1ENR;
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN | RCC_APB1ENR_I2C2EN;
    (void)RCC_APB1ENR;

    /* GPIO: PB6, PB7 (I2C1), PB10, PB11 (I2C2)
     * Mode: AF (10), OType: open-drain (1), PuPd: pull-up (01)
     * PB6=[13:12], PB7=[15:14], PB10=[21:20], PB11=[23:22] */
    GPIOB_MODER &= ~((3u << 12) | (3u << 14) | (3u << 20) | (3u << 22));
    GPIOB_MODER |=  ((2u << 12) | (2u << 14) | (2u << 20) | (2u << 22));
    GPIOB_OTYPER |= (1u << 6) | (1u << 7) | (1u << 10) | (1u << 11);
    GPIOB_PUPDR &= ~((3u << 12) | (3u << 14) | (3u << 20) | (3u << 22));
    GPIOB_PUPDR |=  ((1u << 12) | (1u << 14) | (1u << 20) | (1u << 22));
    /* AFRL: AF4 for PB6 [27:24], AF4 for PB7 [31:28] */
    GPIOB_AFRL &= ~((0xFu << 24) | (0xFu << 28));
    GPIOB_AFRL |=  ((0x4u << 24) | (0x4u << 28));
    /* AFRH: AF4 for PB10 [11:8], AF4 for PB11 [15:12] */
    GPIOB_AFRH &= ~((0xFu << 8) | (0xFu << 12));
    GPIOB_AFRH |=  ((0x4u << 8) | (0x4u << 12));

    /* I2C2 slave init (SWRST clears BUSY from prior GDB reset) */
    I2C2_CR1   = I2C_CR1_SWRST; delay_ms(1u); I2C2_CR1 = 0u;
    I2C2_CR2   = 16u;
    I2C2_CCR   = 80u;
    I2C2_TRISE = 17u;
    I2C2_OAR1  = (SLAVE_ADDR << 1u) | (1u << 14u);
    I2C2_CR1   = I2C_CR1_PE; (void)I2C2_CR1;
    I2C2_CR1  |= I2C_CR1_ACK; (void)I2C2_CR1;
    delay_ms(2u);

    /* I2C1 master init */
    I2C1_CR1   = I2C_CR1_SWRST; delay_ms(1u); I2C1_CR1 = 0u;
    I2C1_CR2   = 16u;
    I2C1_CCR   = 80u;
    I2C1_TRISE = 17u;
    I2C1_CR1   = I2C_CR1_PE | I2C_CR1_ACK;

    ael_mailbox_init();

    /* Continuous-retry loop */
    while (1) {
        uint32_t err     = 0u;
        uint32_t diag_sr = 0u;

        /* Re-init I2C2 slave */
        I2C2_CR1   = I2C_CR1_SWRST; delay_ms(1u); I2C2_CR1 = 0u;
        I2C2_CR2   = 16u; I2C2_CCR = 80u; I2C2_TRISE = 17u;
        I2C2_OAR1  = (SLAVE_ADDR << 1u) | (1u << 14u);
        I2C2_CR1   = I2C_CR1_PE; (void)I2C2_CR1;
        I2C2_CR1  |= I2C_CR1_ACK; (void)I2C2_CR1;

        /* Re-init I2C1 master */
        I2C1_CR1   = I2C_CR1_SWRST; delay_ms(1u); I2C1_CR1 = 0u;
        I2C1_CR2   = 16u; I2C1_CCR = 80u; I2C1_TRISE = 17u;
        I2C1_CR1   = I2C_CR1_PE | I2C_CR1_ACK;
        delay_ms(2u);

        /* ==== WRITE PHASE ==== */
        I2C1_CR1 |= I2C_CR1_START;
        if (wait_flag(&I2C1_SR1, I2C_SR1_SB) != 0) { err = ERR_WRITE_SB; goto iter_fail; }

        I2C1_DR = (SLAVE_ADDR << 1u) | 0u;

        {
            uint32_t t = I2C_TIMEOUT;
            while ((I2C2_SR1 & I2C_SR1_ADDR) == 0u) {
                if (--t == 0u) {
                    err     = ERR_SWRITE_ADDR;
                    diag_sr = (I2C2_SR1 & 0xFFu)
                            | ((I2C2_SR2 & 0xFFu) << 8u)
                            | ((I2C1_SR1 & 0xFFFFu) << 16u);
                    goto iter_fail;
                }
            }
        }
        (void)I2C2_SR1; (void)I2C2_SR2;

        if (wait_flag(&I2C1_SR1, I2C_SR1_ADDR) != 0) { err = ERR_MWRITE_ADDR; goto iter_fail; }
        (void)I2C1_SR1; (void)I2C1_SR2;

        for (uint32_t i = 0u; i < N_BYTES; i++) {
            if (wait_flag(&I2C1_SR1, I2C_SR1_TXE)  != 0) { err = ERR_WRITE_DATA; goto iter_fail; }
            I2C1_DR = tx_buf[i];
            if (wait_flag(&I2C2_SR1, I2C_SR1_RXNE) != 0) { err = ERR_WRITE_DATA; goto iter_fail; }
            slave_buf[i] = (uint8_t)I2C2_DR;
        }

        if (wait_flag(&I2C1_SR1, I2C_SR1_BTF) != 0) { err = ERR_WRITE_BTF; goto iter_fail; }
        I2C1_CR1 |= I2C_CR1_STOP;
        { uint32_t t = I2C_TIMEOUT; while (I2C1_SR2 & I2C_SR2_BUSY) { if (--t == 0u) { err = ERR_WRITE_BTF; goto iter_fail; } } }
        delay_ms(1u);

        /* ==== READ PHASE ==== */
        I2C1_CR1 |= I2C_CR1_ACK;
        I2C1_CR1 |= I2C_CR1_START;
        if (wait_flag(&I2C1_SR1, I2C_SR1_SB) != 0) { err = ERR_READ_SB; goto iter_fail; }

        I2C1_DR = (SLAVE_ADDR << 1u) | 1u;

        if (wait_flag(&I2C2_SR1, I2C_SR1_ADDR) != 0) { err = ERR_SREAD_ADDR; goto iter_fail; }
        (void)I2C2_SR1; (void)I2C2_SR2;
        if (wait_flag(&I2C2_SR1, I2C_SR1_TXE) != 0) { err = ERR_SREAD_TXE; goto iter_fail; }
        I2C2_DR = slave_buf[0];

        if (wait_flag(&I2C1_SR1, I2C_SR1_ADDR) != 0) { err = ERR_MREAD_ADDR; goto iter_fail; }
        (void)I2C1_SR1; (void)I2C1_SR2;

        for (uint32_t i = 0u; i < 2u; i++) {
            if (wait_flag(&I2C1_SR1, I2C_SR1_RXNE) != 0) { err = ERR_READ_DATA; goto iter_fail; }
            rx_buf[i] = (uint8_t)I2C1_DR;
            if (wait_flag(&I2C2_SR1, I2C_SR1_TXE) != 0) { err = ERR_SREAD_TXE; goto iter_fail; }
            I2C2_DR = slave_buf[i + 1u];
        }

        if (wait_flag(&I2C2_SR1, I2C_SR1_TXE) != 0) { err = ERR_SREAD_TXE; goto iter_fail; }
        I2C2_DR = slave_buf[3];

        if (wait_flag(&I2C1_SR1, I2C_SR1_BTF) != 0) { err = ERR_READ_DATA; goto iter_fail; }
        I2C1_CR1 &= ~I2C_CR1_ACK;
        I2C1_CR1 |=  I2C_CR1_STOP;
        rx_buf[2] = (uint8_t)I2C1_DR;

        if (wait_flag(&I2C1_SR1, I2C_SR1_RXNE) != 0) { err = ERR_READ_DATA; goto iter_fail; }
        rx_buf[3] = (uint8_t)I2C1_DR;
        I2C2_SR1 &= ~(1u << 10u);  /* clear AF */

        /* ==== VERIFY ==== */
        {
            uint32_t matched = 0u;
            for (uint32_t i = 0u; i < N_BYTES; i++) {
                if (rx_buf[i] == tx_buf[i]) { matched++; }
            }
            if (matched != N_BYTES) { err = ERR_MISMATCH; diag_sr = matched; goto iter_fail; }
        }

        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = 0x5A5Au;
        delay_ms(200u);
        continue;

    iter_fail:
        ael_mailbox_fail(err, diag_sr);
        delay_ms(200u);
    }
}
