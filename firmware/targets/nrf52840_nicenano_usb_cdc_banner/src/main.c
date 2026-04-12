/*
 * AEL Stage 0 — nRF52840 nice!nano USB CDC banner
 *
 * Prints "AEL_READY nRF52840 count=N" over USB CDC every 500 ms.
 * Supports 1200-baud bootloader re-entry via ael_usb.h.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    uint32_t count = 0;
    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        printk("AEL_READY nRF52840 count=%u\n", count++);
        k_msleep(500);
    }
    return 0;
}
