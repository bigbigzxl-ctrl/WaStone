/*
 * AEL Stage 2 — nRF52840 nice!nano UART loopback
 *
 * Wiring: P0.20 (TX) ←→ P0.17 (RX)  [1 wire]
 *
 * Sends 32 bytes on UART1, receives them back, verifies.
 * Reports: [UART] rx=%d/%d ok=%d PASS|FAIL
 *          AEL_UART_LOOPBACK_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

#define UART_NODE  DT_NODELABEL(uart1)
#define BUF_LEN    32

static const struct device *uart1_dev;
static volatile int rx_idx;
static uint8_t rx_buf[BUF_LEN];

static void uart_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    if (!uart_irq_update(dev)) return;
    while (uart_irq_rx_ready(dev) && rx_idx < BUF_LEN) {
        uint8_t c;
        if (uart_fifo_read(dev, &c, 1) == 1) {
            rx_buf[rx_idx++] = c;
        }
    }
}

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    uart1_dev = DEVICE_DT_GET(UART_NODE);
    if (!device_is_ready(uart1_dev)) {
        printk("[UART] device_not_ready FAIL\n");
        printk("AEL_UART_LOOPBACK_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(3000);
        }
    }

    uart_irq_callback_set(uart1_dev, uart_cb);
    uart_irq_rx_enable(uart1_dev);

    uint8_t tx_buf[BUF_LEN];
    for (int i = 0; i < BUF_LEN; i++) tx_buf[i] = (uint8_t)(0xA0 + i);

    printk("AEL_UART_LOOPBACK_START\n");

    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));

    for (int i = 0; i < BUF_LEN; i++) {
        uart_poll_out(uart1_dev, tx_buf[i]);
    }

    /* Wait up to 500 ms for all bytes */
    for (int w = 0; w < 50 && rx_idx < BUF_LEN; w++) {
        k_msleep(10);
    }

    int ok = 0;
    for (int i = 0; i < BUF_LEN; i++) {
        if (rx_buf[i] == tx_buf[i]) ok++;
    }
    bool pass = (rx_idx == BUF_LEN && ok == BUF_LEN);
    printk("[UART] rx=%d/%d ok=%d %s\n", rx_idx, BUF_LEN, ok, pass ? "PASS" : "FAIL");

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        printk("AEL_UART_LOOPBACK_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
