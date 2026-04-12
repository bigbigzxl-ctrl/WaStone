/*
 * AEL Stage 0 — nRF52840 nice!nano blinky + USB CDC
 *
 * Blinks P0.15 (blue LED) at 1 Hz.
 * Prints "AEL_BLINKY" every 500 ms over USB CDC for observation.
 * Monitors for 1200-baud host connection → enters UF2 bootloader.
 *
 * 1200-baud bootloader re-entry:
 *   Host opens /dev/ttyACM* at 1200 baud →
 *   USBD_MSG_CDC_ACM_LINE_CODING callback fires →
 *   GPREGRET=0x57 + NVIC_SystemReset() →
 *   Bootloader stays in DFU mode → NICENANO drive reappears.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/device.h>

/* ── Bootloader entry ─────────────────────────────────────────────────────── */
#define NRF_POWER_GPREGRET (*(volatile uint32_t *)(0x40000000UL + 0x51CUL))
#define UF2_MAGIC 0x57UL

static atomic_t _enter_bl_flag = ATOMIC_INIT(0);

static void enter_bootloader(void)
{
    NRF_POWER_GPREGRET = UF2_MAGIC;
    NVIC_SystemReset();
}

/* ── USB device context (new Zephyr USB stack) ────────────────────────────── */
USBD_DEVICE_DEFINE(ael_usbd,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   0x239A, 0x000F);

USBD_DESC_LANG_DEFINE(ael_lang);
USBD_DESC_MANUFACTURER_DEFINE(ael_mfr, "Adafruit Industries LLC");
USBD_DESC_PRODUCT_DEFINE(ael_product, "AEL nRF52840 Blinky");

USBD_DESC_CONFIG_DEFINE(ael_fs_cfg_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(ael_fs_config,
                           USB_SCD_SELF_POWERED,
                           125, &ael_fs_cfg_desc);

/* ── USBD message callback (fires on every CDC line coding change) ─────────── */
static void _usbd_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
    /* VBUS management for nRF52840 (can detect VBUS) */
    if (usbd_can_detect_vbus(ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY)    usbd_enable(ctx);
        if (msg->type == USBD_MSG_VBUS_REMOVED)  usbd_disable(ctx);
    }

    /* 1200-baud magic: host opens port at 1200 → enter bootloader */
    if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING) {
        uint32_t baud = 0;
        uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &baud);
        if (baud == 1200) {
            atomic_set(&_enter_bl_flag, 1);
        }
    }
}

/* ── USB init ─────────────────────────────────────────────────────────────── */
static int usb_init(void)
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

    /* CDC-ACM needs IAD — set code triple accordingly */
    usbd_device_set_code_triple(&ael_usbd, USBD_SPEED_FS,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);

    err = usbd_msg_register_cb(&ael_usbd, _usbd_msg_cb);
    if (err) return err;

    err = usbd_init(&ael_usbd);
    if (err) return err;

    if (!usbd_can_detect_vbus(&ael_usbd)) {
        return usbd_enable(&ael_usbd);
    }
    return 0;
}

/* ── LED ──────────────────────────────────────────────────────────────────── */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(void)
{
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    int ret = usb_init();
    if (ret != 0) {
        /* USB failed — still blink so the board is visible */
        printk("[BL] usb_init err=%d\n", ret);
    }

    /* Short delay for USB enumeration before first printk */
    k_msleep(1000);

    uint32_t count = 0;
    while (1) {
        if (atomic_get(&_enter_bl_flag)) {
            k_msleep(50);   /* let host close the port */
            enter_bootloader();
        }
        gpio_pin_toggle_dt(&led);
        printk("AEL_BLINKY count=%u (open at 1200 baud->bootloader)\n", count++);
        k_msleep(500);
    }
    return 0;
}
