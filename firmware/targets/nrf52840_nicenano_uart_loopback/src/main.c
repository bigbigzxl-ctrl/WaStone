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

#define UART_NODE  DT_NODELABEL(uart1)
#define BUF_LEN    32
#define TX_TIMEOUT K_MSEC(500)
#define RX_TIMEOUT K_MSEC(500)

static const struct device *uart1_dev;

static int uart_tx_buf(const uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        uart_poll_out(uart1_dev, buf[i]);
    }
    return len;
}

static int uart_rx_buf(uint8_t *buf, int len, k_timeout_t timeout)
{
    int received = 0;
    uint32_t deadline = k_uptime_get_32() + k_ticks_to_ms_ceil32(timeout.ticks);
    while (received < len) {
        if (k_uptime_get_32() > deadline) break;
        unsigned char c;
        if (uart_poll_in(uart1_dev, &c) == 0) {
            buf[received++] = c;
        }
    }
    return received;
}

static bool run_loopback(void)
{
    uint8_t tx[BUF_LEN], rx[BUF_LEN];
    for (int i = 0; i < BUF_LEN; i++) {
        tx[i] = (uint8_t)(0xA0 ^ i);
    }
    memset(rx, 0, sizeof(rx));

    uart_tx_buf(tx, BUF_LEN);
    int got = uart_rx_buf(rx, BUF_LEN, RX_TIMEOUT);

    int ok = 0;
    for (int i = 0; i < got; i++) {
        if (rx[i] == tx[i]) ok++;
    }
    bool pass = (ok == BUF_LEN);
    printk("[UART] rx=%d/%d ok=%d %s\n", got, BUF_LEN, ok, pass ? "PASS" : "FAIL");
    return pass;
}

int main(void)
{
    k_msleep(1500);

    uart1_dev = DEVICE_DT_GET(UART_NODE);
    if (!device_is_ready(uart1_dev)) {
        printk("[UART] device_not_ready FAIL\n");
        printk("AEL_UART_LOOPBACK_FAIL\n");
        return -1;
    }

    printk("AEL_UART_LOOPBACK_START\n");
    bool pass = run_loopback();

    while (1) {
        printk("AEL_UART_LOOPBACK_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
