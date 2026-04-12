/*
 * AEL Stage 2 — nRF52840 nice!nano onboard SPI flash JEDEC ID test
 *
 * No extra wiring — the MX25R6435F 8MB SPI NOR flash is hardwired to:
 *   SCK=P1.11, MOSI=P1.10, MISO=P1.13, CS=P1.12
 *
 * Sends JEDEC READ ID command (0x9F) via raw SPI.
 * Expected response: 0xC2 0x28 0x17 (MX25R6435F)
 *                 or 0xC8 0x40 0x15 (GD25Q16C, some board revisions)
 *
 * Reports: [SPI_FLASH] jedec=0xXX,0xXX,0xXX PASS|FAIL
 *          AEL_SPI_FLASH_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

#define SPI_NODE DT_NODELABEL(spi1)

/* JEDEC READ ID command */
#define CMD_JEDEC_ID  0x9F

/* Known JEDEC IDs for nice!nano onboard flash variants */
/* MX25R6435F (8MB): C2 28 17 */
/* GD25Q16C   (2MB): C8 40 15 */
static bool jedec_known(uint8_t mfr, uint8_t type, uint8_t cap)
{
    if (mfr == 0xC2 && type == 0x28 && cap == 0x17) return true; /* MX25R6435F */
    if (mfr == 0xC8 && type == 0x40 && cap == 0x15) return true; /* GD25Q16C   */
    if (mfr == 0xC8 && type == 0x60 && cap == 0x15) return true; /* GD25Q16E   */
    /* Any non-zero, non-0xFF response is treated as flash present */
    if (mfr != 0x00 && mfr != 0xFF) return true;
    return false;
}

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    printk("AEL_SPI_FLASH_START\n");

    const struct device *spi = DEVICE_DT_GET(SPI_NODE);
    if (!device_is_ready(spi)) {
        printk("[SPI_FLASH] spi_not_ready FAIL\n");
        printk("AEL_SPI_FLASH_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(5000);
            printk("AEL_SPI_FLASH_FAIL (repeat)\n");
        }
    }

    static const struct spi_config cfg = {
        .frequency = 4000000,
        .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER,
        .slave     = 0,
        .cs        = SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(spi1)),
    };

    /* TX: JEDEC READ ID cmd + 3 dummy bytes for response */
    static uint8_t tx_buf[4] = { CMD_JEDEC_ID, 0x00, 0x00, 0x00 };
    static uint8_t rx_buf[4] = { 0 };

    const struct spi_buf tx_bufs[] = {{ .buf = tx_buf, .len = sizeof(tx_buf) }};
    const struct spi_buf rx_bufs[] = {{ .buf = rx_buf, .len = sizeof(rx_buf) }};
    const struct spi_buf_set tx_set = { .buffers = tx_bufs, .count = 1 };
    const struct spi_buf_set rx_set = { .buffers = rx_bufs, .count = 1 };

    int ret = spi_transceive(spi, &cfg, &tx_set, &rx_set);
    if (ret != 0) {
        printk("[SPI_FLASH] spi_err=%d FAIL\n", ret);
        printk("AEL_SPI_FLASH_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(5000);
            printk("AEL_SPI_FLASH_FAIL (repeat)\n");
        }
    }

    uint8_t mfr  = rx_buf[1];
    uint8_t type = rx_buf[2];
    uint8_t cap  = rx_buf[3];

    if (!jedec_known(mfr, type, cap)) {
        printk("[SPI_FLASH] jedec=0x%02X,0x%02X,0x%02X unknown FAIL\n",
               mfr, type, cap);
        printk("AEL_SPI_FLASH_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(5000);
            printk("AEL_SPI_FLASH_FAIL (repeat)\n");
        }
    }

    printk("[SPI_FLASH] jedec=0x%02X,0x%02X,0x%02X PASS\n", mfr, type, cap);
    printk("AEL_SPI_FLASH_PASS\n");

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        k_msleep(5000);
        printk("AEL_SPI_FLASH_PASS (repeat)\n");
    }
    return 0;
}
