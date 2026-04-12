/*
 * ael_usb.c — shared USB CDC init for nRF52840 nice!nano test firmwares
 *
 * Sets up the new Zephyr USB device stack with a single CDC-ACM interface.
 * Registers a message callback that detects 1200-baud opens and sets
 * ael_bl_flag, enabling the application to re-enter the UF2 bootloader.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include "ael_usb.h"

/* Shared flag — set when host opens CDC port at 1200 baud */
atomic_t ael_bl_flag = ATOMIC_INIT(0);

/* ── USB device descriptors ───────────────────────────────────────────────── */
USBD_DEVICE_DEFINE(ael_usbd,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   0x239A, 0x0099);   /* Adafruit VID, AEL generic test PID */

USBD_DESC_LANG_DEFINE(ael_lang);
USBD_DESC_MANUFACTURER_DEFINE(ael_mfr, "Adafruit Industries LLC");
USBD_DESC_PRODUCT_DEFINE(ael_product, "AEL nRF52840");

USBD_DESC_CONFIG_DEFINE(ael_fs_cfg_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(ael_fs_config,
                           USB_SCD_SELF_POWERED,
                           125, &ael_fs_cfg_desc);

/* ── USBD message callback ────────────────────────────────────────────────── */
static void _ael_usbd_msg_cb(struct usbd_context *const ctx,
                              const struct usbd_msg *msg)
{
    /* VBUS management — nRF52840 can detect VBUS */
    if (usbd_can_detect_vbus(ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY)    usbd_enable(ctx);
        if (msg->type == USBD_MSG_VBUS_REMOVED)  usbd_disable(ctx);
    }

    /* 1200-baud bootloader re-entry */
    if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING) {
        uint32_t baud = 0;
        uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &baud);
        if (baud == 1200) {
            atomic_set(&ael_bl_flag, 1);
        }
    }
}

/* ── Public init function ─────────────────────────────────────────────────── */
int ael_usb_init(void)
{
    int err;

    err = usbd_add_descriptor(&ael_usbd, &ael_lang);
    if (err) return err;
    err = usbd_add_descriptor(&ael_usbd, &ael_mfr);
    if (err) return err;
    err = usbd_add_descriptor(&ael_usbd, &ael_product);
    if (err) return err;

    err = usbd_add_configuration(&ael_usbd, USBD_SPEED_FS, &ael_fs_config);
    if (err) return err;

    err = usbd_register_all_classes(&ael_usbd, USBD_SPEED_FS, 1, NULL);
    if (err) return err;

    /* CDC-ACM requires IAD triple */
    usbd_device_set_code_triple(&ael_usbd, USBD_SPEED_FS,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);

    err = usbd_msg_register_cb(&ael_usbd, _ael_usbd_msg_cb);
    if (err) return err;

    err = usbd_init(&ael_usbd);
    if (err) return err;

    /* Always enable unconditionally.
     *
     * On nRF52840, usbd_can_detect_vbus() returns true and the normal
     * pattern is to enable only on USBD_MSG_VBUS_READY.  However, after a
     * software reset (NVIC_SystemReset from UF2 bootloader), the host does
     * not re-assert VBUS — it was never removed — so the VBUS_READY event
     * never fires and USB stays down until a physical replug.
     *
     * Calling usbd_enable() here unconditionally avoids that stall.
     * The VBUS_REMOVED callback will still call usbd_disable() if VBUS is
     * truly lost, so battery-powered scenarios are handled correctly.
     */
    return usbd_enable(&ael_usbd);
}
