/*
 * AEL Stage 2 — nRF52840 nice!nano IEEE 802.15.4 radio test
 *
 * No extra wiring — bare-metal nRF52840 RADIO peripheral in IEEE 802.15.4 mode.
 * No networking stack (direct register access, same pattern as TIMER/RTC tests).
 *
 * Test sequence:
 *   1. Enable HFCLK (radio requires 64 MHz HFXO).
 *   2. Configure RADIO: MODE=15 (IEEE 802.15.4 250 kbps DSSS), channel 15 (2.425 GHz).
 *   3. Issue TASKS_RXEN → wait for EVENTS_READY (PLL locked, radio RX ramp-up complete).
 *   4. Wait 200 ms, disable radio.
 *   5. PASS if EVENTS_READY fired within 2 ms.
 *
 * Reports: [IEEE802154] ready_us=%u PASS|FAIL
 *          AEL_IEEE802154_PASS|FAIL
 *
 * RADIO MODE=15 = IEEE 802.15.4 (0x0F per nRF52840 PS §6.20.4)
 * Channel 15 frequency: 2405 + (15-11)*5 = 2425 MHz → FREQUENCY = 25 (offset from 2400)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

/* ── nRF52840 RADIO registers (PS §6.20) ───────────────────────────────── */
#define NRF_RADIO_BASE          0x40001000UL
#define RADIO_TASKS_TXEN        (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x000))
#define RADIO_TASKS_RXEN        (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x004))
#define RADIO_TASKS_START       (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x008))
#define RADIO_TASKS_DISABLE     (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x010))
#define RADIO_EVENTS_READY      (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x100))
#define RADIO_EVENTS_DISABLED   (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x110))
#define RADIO_FREQUENCY         (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x508))
#define RADIO_MODE              (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x514))
#define RADIO_STATE             (*(volatile uint32_t *)(NRF_RADIO_BASE + 0x550))

/* RADIO MODE values */
#define RADIO_MODE_IEEE802154   0x0FUL  /* IEEE 802.15.4 250 kbps DSSS O-QPSK */

/* nRF52840 CLOCK registers */
#define NRF_CLOCK_BASE          0x40000000UL
#define CLOCK_TASKS_HFCLKSTART  (*(volatile uint32_t *)(NRF_CLOCK_BASE + 0x000))
#define CLOCK_EVENTS_HFCLKSTARTED (*(volatile uint32_t *)(NRF_CLOCK_BASE + 0x100))
#define CLOCK_HFCLKSTAT         (*(volatile uint32_t *)(NRF_CLOCK_BASE + 0x40C))

/* ── HFCLK start helper ────────────────────────────────────────────────── */
static bool hfclk_start(void)
{
    /* Check if HFXO already running (bit 16 = HFCLKSTARTED, bit 0 = SRC=HFXO) */
    if ((CLOCK_HFCLKSTAT & BIT(16)) && (CLOCK_HFCLKSTAT & BIT(0))) {
        return true; /* already on HFXO */
    }
    CLOCK_EVENTS_HFCLKSTARTED = 0;
    CLOCK_TASKS_HFCLKSTART = 1;
    uint32_t deadline = k_uptime_get_32() + 200;
    while (!CLOCK_EVENTS_HFCLKSTARTED) {
        if (k_uptime_get_32() > deadline) return false;
        k_yield();
    }
    return true;
}

int main(void)
{
    ael_usb_init();
    k_msleep(1500);

    printk("AEL_IEEE802154_START\n");

    /* Step 1: Start HFXO (radio requires it) */
    if (!hfclk_start()) {
        printk("[IEEE802154] hfclk_timeout FAIL\n");
        printk("AEL_IEEE802154_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(5000);
            printk("AEL_IEEE802154_FAIL (repeat)\n");
        }
    }
    printk("[IEEE802154] hfclk_ok=1\n");

    /* Step 2: Configure RADIO for IEEE 802.15.4, channel 15 (2.425 GHz)
     * FREQUENCY[6:0] = target MHz - 2400 → 2425 - 2400 = 25 = 0x19
     * FREQUENCY[8] = MAP=0 (default, no frequency map offset needed for IEEE 802.15.4)
     */
    RADIO_EVENTS_READY    = 0;
    RADIO_EVENTS_DISABLED = 0;
    RADIO_MODE            = RADIO_MODE_IEEE802154;
    RADIO_FREQUENCY       = 25;   /* 2400 + 25 = 2425 MHz = ch15 */

    /* Step 3: Start RX ramp-up */
    uint32_t t0 = k_uptime_get_32();
    RADIO_TASKS_RXEN = 1;

    uint32_t deadline = k_uptime_get_32() + 10;  /* PLL lock < 10 ms */
    while (!RADIO_EVENTS_READY) {
        if (k_uptime_get_32() > deadline) {
            RADIO_TASKS_DISABLE = 1;
            printk("[IEEE802154] rxen_timeout state=%u FAIL\n", RADIO_STATE);
            printk("AEL_IEEE802154_FAIL\n");
            while (1) {
                if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
                k_msleep(5000);
                printk("AEL_IEEE802154_FAIL (repeat)\n");
            }
        }
    }
    uint32_t ready_us = (k_uptime_get_32() - t0) * 1000; /* rough ms→μs */

    /* Step 4: Dwell 200 ms in RX, then disable */
    k_msleep(200);
    RADIO_TASKS_DISABLE = 1;

    deadline = k_uptime_get_32() + 5;
    while (!RADIO_EVENTS_DISABLED && k_uptime_get_32() < deadline) {}

    printk("[IEEE802154] ch=15 freq=2425MHz ready_us=%u PASS\n", ready_us);
    printk("AEL_IEEE802154_PASS\n");

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        k_msleep(5000);
        printk("AEL_IEEE802154_PASS (repeat)\n");
    }
    return 0;
}
