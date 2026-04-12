/*
 * AEL Stage 2 — nRF52840 nice!nano I2C loopback (master + slave, same bus)
 *
 * Wiring: none — P0.28 (SCL) and P0.29 (SDA) shared between TWIM0 and TWIS1.
 * Note: add 4.7 kΩ pull-ups on SDA and SCL if bus floats.
 *
 * TWIM0 (master) writes 8 bytes to slave addr 0x42 (TWIS1).
 * TWIS1 (slave) reads the 8 bytes and returns them on a subsequent master read.
 * Verifies round-trip data matches.
 *
 * Reports: [I2C] write_ok=%d read_ok=%d match=%d PASS|FAIL
 *          AEL_I2C_LOOPBACK_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include "../../../nrf52840_nicenano_common/ael_bootloader.h"

AEL_BOOTLOADER_THREAD_DEFINE();

#define I2C_MASTER_NODE  DT_NODELABEL(i2c0)
#define I2C_SLAVE_NODE   DT_NODELABEL(i2c1)
#define SLAVE_ADDR       0x42
#define BUF_LEN          8

static const struct device *i2c_master;
static const struct device *i2c_slave_dev;

static uint8_t slave_rx_buf[BUF_LEN];
static uint8_t slave_tx_buf[BUF_LEN];
static int     slave_rx_len;

/* TWIS callback */
static int slave_write_requested(struct i2c_target_config *cfg)
{
    slave_rx_len = 0;
    return 0;
}
static int slave_write_received(struct i2c_target_config *cfg, uint8_t val)
{
    if (slave_rx_len < BUF_LEN) {
        slave_rx_buf[slave_rx_len++] = val;
        slave_tx_buf[slave_rx_len - 1] = val;  /* echo back */
    }
    return 0;
}
static int slave_read_requested(struct i2c_target_config *cfg, uint8_t *val)
{
    *val = slave_tx_buf[0];
    return 0;
}
static int slave_read_processed(struct i2c_target_config *cfg, uint8_t *val)
{
    static int idx = 1;
    if (idx < BUF_LEN) {
        *val = slave_tx_buf[idx++];
    }
    return 0;
}
static int slave_stop(struct i2c_target_config *cfg)
{
    return 0;
}

static const struct i2c_target_callbacks slave_cbs = {
    .write_requested = slave_write_requested,
    .write_received  = slave_write_received,
    .read_requested  = slave_read_requested,
    .read_processed  = slave_read_processed,
    .stop            = slave_stop,
};

static struct i2c_target_config slave_cfg = {
    .address   = SLAVE_ADDR,
    .callbacks = &slave_cbs,
};

int main(void)
{
    k_msleep(1500);

    i2c_master  = DEVICE_DT_GET(I2C_MASTER_NODE);
    i2c_slave_dev = DEVICE_DT_GET(I2C_SLAVE_NODE);

    if (!device_is_ready(i2c_master) || !device_is_ready(i2c_slave_dev)) {
        printk("[I2C] device_not_ready FAIL\n");
        printk("AEL_I2C_LOOPBACK_FAIL\n");
        return -1;
    }

    /* Register slave */
    int ret = i2c_target_register(i2c_slave_dev, &slave_cfg);
    if (ret != 0) {
        printk("[I2C] slave_register_err=%d FAIL\n", ret);
        printk("AEL_I2C_LOOPBACK_FAIL\n");
        return -1;
    }

    printk("AEL_I2C_LOOPBACK_START\n");

    uint8_t tx[BUF_LEN], rx[BUF_LEN];
    for (int i = 0; i < BUF_LEN; i++) tx[i] = (uint8_t)(0x10 + i);
    memset(rx, 0, sizeof(rx));

    /* Master write */
    int w_ret = i2c_write(i2c_master, tx, BUF_LEN, SLAVE_ADDR);
    k_msleep(5);

    /* Master read */
    int r_ret = i2c_read(i2c_master, rx, BUF_LEN, SLAVE_ADDR);

    int match = 0;
    if (w_ret == 0 && r_ret == 0) {
        for (int i = 0; i < BUF_LEN; i++) {
            if (rx[i] == tx[i]) match++;
        }
    }
    bool pass = (w_ret == 0 && r_ret == 0 && match == BUF_LEN);
    printk("[I2C] write_ok=%d read_ok=%d match=%d/%d %s\n",
           (w_ret == 0), (r_ret == 0), match, BUF_LEN, pass ? "PASS" : "FAIL");

    while (1) {
        printk("AEL_I2C_LOOPBACK_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
