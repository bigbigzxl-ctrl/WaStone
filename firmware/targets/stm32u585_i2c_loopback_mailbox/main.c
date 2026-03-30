/*
 * stm32u585_i2c_loopback_mailbox — I2C1 master ↔ I2C2 slave loopback
 * STM32U585CIU6, MSI 4MHz
 *
 * Wires: PB6(I2C1_SCL) ↔ PB10(I2C2_SCL)
 *        PB7(I2C1_SDA) ↔ PB3 (I2C2_SDA)
 *
 * I2C1 = Master, I2C2 = Slave (address 0x52)
 * Master writes 4 bytes, slave receives them.
 * Master reads 4 bytes, slave sends them back.
 * Verifies round-trip data integrity.
 *
 * TIMINGR for 100kHz @ 4MHz PCLK1: 0x00100F12
 *   PRESC=0, SCLDEL=1, SDADEL=0, SCLH=15, SCLL=18
 *
 * FAIL codes:
 *   0xE001 — slave ADDR (write) timeout
 *   0xE002 — slave RXNE timeout during master write
 *   0xE003 — slave ADDR (read) timeout
 *   0xE004 — master RX data mismatch
 *   0xE005 — master TXIS timeout
 *   0xE006 — master TC timeout after write
 *   0xE007 — master RXNE timeout after read start
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB1ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))

/* GPIOB */
#define GPIOB_BASE      0x42020400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04u))
#define GPIOB_OSPEEDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x08u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_AFRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x20u))
#define GPIOB_AFRH      (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

/* I2C1 (APB1) */
#define I2C1_BASE       0x40005400u
#define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00u))
#define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04u))
#define I2C1_TIMINGR    (*(volatile uint32_t *)(I2C1_BASE + 0x10u))
#define I2C1_ISR        (*(volatile uint32_t *)(I2C1_BASE + 0x18u))
#define I2C1_ICR        (*(volatile uint32_t *)(I2C1_BASE + 0x1Cu))
#define I2C1_RXDR       (*(volatile uint32_t *)(I2C1_BASE + 0x24u))
#define I2C1_TXDR       (*(volatile uint32_t *)(I2C1_BASE + 0x28u))

/* I2C2 (APB1) */
#define I2C2_BASE       0x40005800u
#define I2C2_CR1        (*(volatile uint32_t *)(I2C2_BASE + 0x00u))
#define I2C2_CR2        (*(volatile uint32_t *)(I2C2_BASE + 0x04u))
#define I2C2_OAR1       (*(volatile uint32_t *)(I2C2_BASE + 0x08u))
#define I2C2_TIMINGR    (*(volatile uint32_t *)(I2C2_BASE + 0x10u))
#define I2C2_ISR        (*(volatile uint32_t *)(I2C2_BASE + 0x18u))
#define I2C2_ICR        (*(volatile uint32_t *)(I2C2_BASE + 0x1Cu))
#define I2C2_RXDR       (*(volatile uint32_t *)(I2C2_BASE + 0x24u))
#define I2C2_TXDR       (*(volatile uint32_t *)(I2C2_BASE + 0x28u))

/* I2C ISR bits */
#define I2C_ISR_TXE     (1u << 0u)
#define I2C_ISR_TXIS    (1u << 1u)
#define I2C_ISR_RXNE    (1u << 2u)
#define I2C_ISR_ADDR    (1u << 3u)
#define I2C_ISR_NACKF   (1u << 4u)
#define I2C_ISR_STOPF   (1u << 5u)
#define I2C_ISR_TC      (1u << 6u)
#define I2C_ISR_DIR     (1u << 16u) /* 1=read (slave transmit), 0=write (slave receive) */

/* I2C ICR bits */
#define I2C_ICR_ADDRCF  (1u << 3u)
#define I2C_ICR_STOPCF  (1u << 5u)

/* Slave address (7-bit) */
#define SLAVE_ADDR      0x52u
/* OAR1: enable, 7-bit, OA1[7:1]=SLAVE_ADDR */
#define OAR1_VAL        ((SLAVE_ADDR << 1u) | (1u << 15u))

/* TIMINGR: 100kHz @ 4MHz PCLK1
 * PRESC=0, SCLDEL=1, SDADEL=0, SCLH=15(0xF), SCLL=18(0x12) */
#define I2C_TIMINGR     0x00100F12u

/* Master CR2 helpers */
/* Write N bytes to slave: SADD[7:1]=SLAVE_ADDR, NBYTES=N, AUTOEND, START */
#define I2C_CR2_WRITE(n) \
    (((SLAVE_ADDR) << 1u) | ((n) << 16u) | (1u << 25u) | (1u << 13u))
/* Read N bytes from slave: DIR=1, SADD[7:1]=SLAVE_ADDR, NBYTES=N, AUTOEND, START */
#define I2C_CR2_READ(n) \
    (((SLAVE_ADDR) << 1u) | (1u << 10u) | ((n) << 16u) | (1u << 25u) | (1u << 13u))

#define DATA_LEN        4u
#define TIMEOUT         1000000u

static const uint8_t tx_data[DATA_LEN] = {0x11u, 0x22u, 0x33u, 0x44u};
static uint8_t rx_slave[DATA_LEN];
static uint8_t rx_master[DATA_LEN];

int main(void)
{
    ael_mailbox_init();

    /* Clocks */
    RCC_AHB2ENR1 |= (1u << 1u);               /* GPIOBEN */
    RCC_APB1ENR1 |= (1u << 21u) | (1u << 22u); /* I2C1EN | I2C2EN */
    volatile uint32_t dummy = RCC_APB1ENR1;
    (void)dummy;

    /* GPIO setup: open-drain AF4 with pull-up for I2C pins
     * I2C1: PB6=SCL(AFRL), PB7=SDA(AFRL)
     * I2C2: PB10=SCL(AFRH), PB3=SDA(AFRL)
     */
    {
        /* Pins 3,6,7 in AFRL; pin 10 in AFRH */
        const uint32_t pins = (1u<<3u)|(1u<<6u)|(1u<<7u)|(1u<<10u);

        /* MODER = AF (10) for pins 3,6,7,10 */
        GPIOB_MODER &= ~((3u<<6u)|(3u<<12u)|(3u<<14u)|(3u<<20u));
        GPIOB_MODER |=  ((2u<<6u)|(2u<<12u)|(2u<<14u)|(2u<<20u));
        /* OTYPER = open-drain */
        GPIOB_OTYPER |= pins;
        /* OSPEEDR = high speed */
        GPIOB_OSPEEDR|= ((3u<<6u)|(3u<<12u)|(3u<<14u)|(3u<<20u));
        /* PUPDR = pull-up */
        GPIOB_PUPDR &= ~((3u<<6u)|(3u<<12u)|(3u<<14u)|(3u<<20u));
        GPIOB_PUPDR |=  ((1u<<6u)|(1u<<12u)|(1u<<14u)|(1u<<20u));
        /* AFRL: PB3→AF4[15:12], PB6→AF4[27:24], PB7→AF4[31:28] */
        GPIOB_AFRL &= ~((0xFu<<12u)|(0xFu<<24u)|(0xFu<<28u));
        GPIOB_AFRL |=  ((4u  <<12u)|(4u  <<24u)|(4u  <<28u));
        /* AFRH: PB10→AF4[11:8] */
        GPIOB_AFRH &= ~(0xFu << 8u);
        GPIOB_AFRH |=  (4u   << 8u);
    }

    /* Configure I2C2 (slave) first — disable PE before config */
    I2C2_CR1     = 0u;
    I2C2_TIMINGR = I2C_TIMINGR;
    I2C2_OAR1    = OAR1_VAL;
    I2C2_CR1     = (1u << 0u);   /* PE=1, NOSTRETCH=0 (slave must stretch) */

    /* Configure I2C1 (master) */
    I2C1_CR1     = 0u;
    I2C1_TIMINGR = I2C_TIMINGR;
    I2C1_CR1     = (1u << 0u);   /* PE=1 */

    /* --- Phase 1: Master WRITE 4 bytes → Slave receives --- */

    /* Master: start write transfer */
    I2C1_CR2 = I2C_CR2_WRITE(DATA_LEN);

    /* Interleave: push TX byte to master and receive on slave */
    for (uint32_t i = 0u; i < DATA_LEN; i++) {
        /* Wait master TXIS (transmit buffer empty) */
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (I2C1_ISR & I2C_ISR_TXIS) break;
        }
        if (t >= TIMEOUT) {
            /* detail0: I2C1_ISR (check NACKF=bit4, BUSY=bit15, ARLO=bit9) */
            ael_mailbox_fail(0xE005u, I2C1_ISR);
            while (1) {}
        }
        I2C1_TXDR = tx_data[i];

        /* Wait slave RXNE */
        for (t = 0u; t < TIMEOUT; t++) {
            if (I2C2_ISR & I2C_ISR_RXNE) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE002u, i);
            while (1) {}
        }
        rx_slave[i] = (uint8_t)(I2C2_RXDR & 0xFFu);
    }

    /* Wait master STOP (AUTOEND generates it) and slave STOPF */
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (I2C2_ISR & I2C_ISR_STOPF) break;
        }
        I2C2_ICR = I2C_ICR_STOPCF;
    }

    /* --- Phase 2: Master READ 4 bytes ← Slave sends rx_slave[] back --- */

    /* Preload slave TX buffer with the received data */
    I2C2_TXDR = rx_slave[0];

    /* Master: start read transfer */
    I2C1_CR2 = I2C_CR2_READ(DATA_LEN);

    /* Slave: wait ADDR match, clear it, then feed bytes */
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (I2C2_ISR & I2C_ISR_ADDR) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE003u, 0u);
            while (1) {}
        }
        I2C2_ICR = I2C_ICR_ADDRCF;
    }

    /* Feed remaining slave TX bytes; master reads each */
    for (uint32_t i = 0u; i < DATA_LEN; i++) {
        /* Master: wait RXNE */
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (I2C1_ISR & I2C_ISR_RXNE) break;
        }
        if (t >= TIMEOUT) {
            ael_mailbox_fail(0xE007u, i);
            while (1) {}
        }
        rx_master[i] = (uint8_t)(I2C1_RXDR & 0xFFu);

        /* Feed next byte to slave TX if not last */
        if (i + 1u < DATA_LEN) {
            uint32_t ts;
            for (ts = 0u; ts < TIMEOUT; ts++) {
                if (I2C2_ISR & I2C_ISR_TXIS) break;
            }
            I2C2_TXDR = rx_slave[i + 1u];
        }
    }

    /* Wait slave STOPF */
    {
        uint32_t t;
        for (t = 0u; t < TIMEOUT; t++) {
            if (I2C2_ISR & I2C_ISR_STOPF) break;
        }
        I2C2_ICR = I2C_ICR_STOPCF;
    }

    /* Verify round-trip */
    for (uint32_t i = 0u; i < DATA_LEN; i++) {
        if (rx_master[i] != tx_data[i]) {
            AEL_MAILBOX->detail0 = ((uint32_t)tx_data[i] << 8u) | rx_master[i];
            ael_mailbox_fail(0xE004u, ((uint32_t)i << 16u) |
                             ((uint32_t)tx_data[i] << 8u) | rx_master[i]);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
