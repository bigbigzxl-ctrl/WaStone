/*
 * AEL Stage 2 — nRF52840 nice!nano GPIO loopback
 *
 * Wiring required:
 *   P0.06 (OUT) ←→ P0.08 (IN)   — pair A
 *   P0.17 (OUT) ←→ P0.20 (IN)   — pair B
 *
 * Toggles OUT pin and reads it back from IN pin.
 * Reports: [GPIO_A] ok=N/N PASS|FAIL
 *          [GPIO_B] ok=N/N PASS|FAIL
 *          AEL_GPIO_LOOPBACK_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

#define GPIO0_NODE  DT_NODELABEL(gpio0)

static const struct device *gpio0;

#define PIN_A_OUT  6   /* P0.06 */
#define PIN_A_IN   8   /* P0.08 */
#define PIN_B_OUT  17  /* P0.17 */
#define PIN_B_IN   20  /* P0.20 */
#define N_ITERS    20

static bool loopback_pair(const char *tag, int out_pin, int in_pin)
{
    gpio_pin_configure(gpio0, out_pin, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, in_pin,  GPIO_INPUT);

    int ok = 0;
    for (int i = 0; i < N_ITERS; i++) {
        int val = (i & 1);
        gpio_pin_set(gpio0, out_pin, val);
        k_busy_wait(10);
        int read = gpio_pin_get(gpio0, in_pin);
        if (read == val) ok++;
    }
    bool pass = (ok == N_ITERS);
    printk("[%s] ok=%d/%d %s\n", tag, ok, N_ITERS, pass ? "PASS" : "FAIL");
    return pass;
}

int main(void)
{
    k_msleep(1500);

    gpio0 = DEVICE_DT_GET(GPIO0_NODE);
    if (!device_is_ready(gpio0)) {
        printk("[GPIO] device_not_ready FAIL\n");
        printk("AEL_GPIO_LOOPBACK_FAIL\n");
        return -1;
    }

    printk("AEL_GPIO_LOOPBACK_START\n");
    bool pass = true;
    pass &= loopback_pair("GPIO_A", PIN_A_OUT, PIN_A_IN);
    pass &= loopback_pair("GPIO_B", PIN_B_OUT, PIN_B_IN);

    while (1) {
        printk("AEL_GPIO_LOOPBACK_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
