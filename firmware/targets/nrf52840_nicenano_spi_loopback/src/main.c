/*
 * AEL Stage 2 — nRF52840 nice!nano SPI loopback
 *
 * Wiring: P1.10 (MOSI) ←→ P1.13 (MISO)   [1 wire]
 *
 * Sends 32 bytes via SPI1, receives on MISO (shorted to MOSI), verifies.
 * Reports: [SPI] rx_match=%d/%d PASS|FAIL
 *          AEL_SPI_LOOPBACK_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/spi.h>
#include <string.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

#define SPI_NODE DT_NODELABEL(spi1)
#define BUF_LEN  32

static const struct spi_config spi_cfg = {
    .frequency = 1000000,
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER,
    .slave     = 0,
    .cs        = {
        .gpio  = SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(spi1)),
        .delay = 0,
    },
};

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    const struct device *spi_dev = DEVICE_DT_GET(SPI_NODE);
    if (!device_is_ready(spi_dev)) {
        printk("[SPI] device_not_ready FAIL\n");
        printk("AEL_SPI_LOOPBACK_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(3000);
        }
    }

    uint8_t tx_buf[BUF_LEN], rx_buf[BUF_LEN];
    for (int i = 0; i < BUF_LEN; i++) tx_buf[i] = (uint8_t)(0x55 ^ i);
    memset(rx_buf, 0, sizeof(rx_buf));

    struct spi_buf tx = { .buf = tx_buf, .len = BUF_LEN };
    struct spi_buf rx = { .buf = rx_buf, .len = BUF_LEN };
    struct spi_buf_set tx_set = { .buffers = &tx, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx, .count = 1 };

    printk("AEL_SPI_LOOPBACK_START\n");

    int ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
    bool pass = false;
    if (ret != 0) {
        printk("[SPI] transceive_err=%d FAIL\n", ret);
    } else {
        int match = 0;
        for (int i = 0; i < BUF_LEN; i++) {
            if (rx_buf[i] == tx_buf[i]) match++;
        }
        pass = (match == BUF_LEN);
        printk("[SPI] rx_match=%d/%d %s\n", match, BUF_LEN, pass ? "PASS" : "FAIL");
    }

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        printk("AEL_SPI_LOOPBACK_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
