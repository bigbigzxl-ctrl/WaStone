/*
 * AEL Stage 0 — nRF52840 nice!nano blinky
 *
 * Blinks the blue LED (P0.15, active-HIGH) at 1 Hz.
 * Verified by: operator visual inspection.
 * No USB CDC used in this test — observation is visual only.
 *
 * Board: nrf52840dk/nrf52840 + app.overlay (P0.15 LED, code@0x1000)
 * UF2 family: 0x239A00B3 (Adafruit nRF52840 bootloader)
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

int main(void)
{
    if (!gpio_is_ready_dt(&led)) {
        return -1;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(500);
    }
    return 0;
}
