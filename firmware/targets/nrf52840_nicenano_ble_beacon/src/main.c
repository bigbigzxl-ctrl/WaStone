/*
 * AEL Stage 2 — nRF52840 nice!nano BLE beacon test
 *
 * Starts BLE legacy advertising with AEL manufacturer-specific payload.
 * Verifies: bt_enable() returns 0, bt_le_adv_start() returns 0.
 * Runs advertising for 5 s (nRF52840 2.4 GHz radio is active), then stops.
 * No second device needed — confirms radio stack initializes and TX path works.
 *
 * Reports: [BLE] adv_started=1 PASS|FAIL
 *          AEL_BLE_BEACON_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

/* Advertising data: Flags + AEL manufacturer payload */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA,
                  0xFF, 0xFF,          /* company id: test/internal */
                  'A', 'E', 'L',       /* AEL marker */
                  0x52, 0x84),         /* nRF52840 */
};

/* Scan response: device name */
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    printk("AEL_BLE_BEACON_START\n");

    int ret = bt_enable(NULL);
    if (ret != 0) {
        printk("[BLE] bt_enable err=%d FAIL\n", ret);
        printk("AEL_BLE_BEACON_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(5000);
            printk("AEL_BLE_BEACON_FAIL (repeat)\n");
        }
    }
    printk("[BLE] bt_enable ok\n");

    /* Start legacy non-connectable advertising */
    ret = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (ret != 0) {
        printk("[BLE] adv_start err=%d FAIL\n", ret);
        printk("AEL_BLE_BEACON_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(5000);
            printk("AEL_BLE_BEACON_FAIL (repeat)\n");
        }
    }

    printk("[BLE] adv_started=1\n");

    /* Advertise for 5 s — radio is actively transmitting */
    k_msleep(5000);

    bt_le_adv_stop();

    printk("[BLE] adv_started=1 PASS\n");
    printk("AEL_BLE_BEACON_PASS\n");

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        k_msleep(5000);
        printk("AEL_BLE_BEACON_PASS (repeat)\n");
    }
    return 0;
}
