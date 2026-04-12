/*
 * AEL Pre-Stage-2 — nRF52840 nice!nano wire connectivity check
 *
 * Drives each output pin LOW then HIGH and reads the paired input pin.
 * Reports CONNECTED if input follows output, OPEN otherwise.
 *
 * Connections tested:
 *   P0.17 (OUT) ↔ P0.20 (IN)   — GPIO/UART loopback wire
 *   P0.22 (OUT) → P0.24 (IN)   — PWM capture wire
 *   P1.10 (OUT) → P1.13 (IN)   — SPI MOSI→MISO wire
 *
 * Reports:
 *   [WIRE] P0.17->P0.20 CONNECTED|OPEN
 *   [WIRE] P0.22->P0.24 CONNECTED|OPEN
 *   [WIRE] P1.10->P1.13 CONNECTED|OPEN
 *   AEL_WIRE_CHECK_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

static const struct device *gpio0;
static const struct device *gpio1;

static bool check_wire(const char *tag,
                        const struct device *out_dev, int out_pin,
                        const struct device *in_dev,  int in_pin)
{
    gpio_pin_configure(out_dev, out_pin, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(in_dev,  in_pin,  GPIO_INPUT);

    /* Drive LOW, read */
    gpio_pin_set(out_dev, out_pin, 0);
    k_busy_wait(50);
    int r0 = gpio_pin_get(in_dev, in_pin);

    /* Drive HIGH, read */
    gpio_pin_set(out_dev, out_pin, 1);
    k_busy_wait(50);
    int r1 = gpio_pin_get(in_dev, in_pin);

    /* Tristate output, check if input floats (connected → stays at last driven value) */
    gpio_pin_configure(out_dev, out_pin, GPIO_INPUT);

    bool connected = (r0 == 0 && r1 == 1);
    printk("[WIRE] %s %s\n", tag, connected ? "CONNECTED" : "OPEN");
    return connected;
}

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    if (!device_is_ready(gpio0) || !device_is_ready(gpio1)) {
        printk("[WIRE] gpio_not_ready FAIL\n");
        printk("AEL_WIRE_CHECK_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(3000);
        }
    }

    printk("AEL_WIRE_CHECK_START\n");

    bool all_ok = true;
    all_ok &= check_wire("P0.17->P0.20", gpio0, 17, gpio0, 20);
    all_ok &= check_wire("P0.22->P0.24", gpio0, 22, gpio0, 24);
    all_ok &= check_wire("P1.10->P1.13", gpio1, 10, gpio1, 13);

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        printk("AEL_WIRE_CHECK_%s\n", all_ok ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
