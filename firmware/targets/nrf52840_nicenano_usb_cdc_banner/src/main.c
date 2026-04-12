/*
 * AEL Stage 0 — nRF52840 nice!nano USB CDC banner
 *
 * Prints "AEL_READY nRF52840" over USB CDC every 500 ms indefinitely.
 * observe_uart catches the pattern at any time after boot.
 *
 * Board: nrf52840dk/nrf52840 + app.overlay (USB CDC console, code@0x1000)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    uint32_t count = 0;

    while (1) {
        printk("AEL_READY nRF52840 count=%u\n", count++);
        k_msleep(500);
    }
    return 0;
}
