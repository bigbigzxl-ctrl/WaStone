/*
 * ael_usb.h — shared USB CDC init + 1200-baud bootloader re-entry
 *
 * Include in any nRF52840 Zephyr test firmware.
 * Call ael_usb_init() early in main().
 * Poll ael_bl_flag in main loop and call ael_enter_bootloader() when set.
 *
 * Requires in prj.conf:
 *   CONFIG_USB_DEVICE_STACK_NEXT=y
 *   CONFIG_USBD_CDC_ACM_CLASS=y
 *   CONFIG_UART_LINE_CTRL=y
 *   CONFIG_HWINFO=y
 *
 * Requires ael_usb.c compiled alongside (add to CMakeLists.txt):
 *   target_sources(app PRIVATE
 *       src/main.c
 *       ../../../nrf52840_nicenano_common/ael_usb.c)
 */

#pragma once

#include <zephyr/kernel.h>

#define _NRF_POWER_GPREGRET (*(volatile uint32_t *)(0x40000000UL + 0x51CUL))
#define _AEL_DFU_MAGIC      0x57UL

/* Set to 1 by USB callback when host opens port at 1200 baud. */
extern atomic_t ael_bl_flag;

/* Initialize USB CDC and register the 1200-baud bootloader callback. */
int ael_usb_init(void);

/* Write GPREGRET magic and reset into UF2 bootloader. Never returns. */
static inline void ael_enter_bootloader(void)
{
    _NRF_POWER_GPREGRET = _AEL_DFU_MAGIC;
    NVIC_SystemReset();
}
