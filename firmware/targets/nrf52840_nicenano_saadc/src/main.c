/*
 * AEL Stage 2 — nRF52840 nice!nano SAADC test
 *
 * Wiring: none — reads P0.31 (AIN7), the on-board VBATT voltage divider.
 * When powered from USB (5 V), VBATT divider reads ~VCC/2 ≈ 1.65 V.
 *
 * SAADC config: AIN7, gain=1/6, ref=600 mV internal, 12-bit.
 * Raw ADC = (1.65 / (0.6 × 6)) × 4095 ≈ 1879 counts.
 * Acceptance window: 100–4000 (sanity check: non-zero, not railed).
 *
 * Reports: [SAADC] raw=%d mv_approx=%d PASS|FAIL
 *          AEL_SAADC_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/adc.h>
#include "../../nrf52840_nicenano_common/ael_usb.h"

static const struct adc_dt_spec adc_ch7 =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

int main(void)
{
    ael_usb_init();
    k_msleep(1500);
    printk("AEL_SAADC_START\n");

    if (!adc_is_ready_dt(&adc_ch7)) {
        printk("[SAADC] device_not_ready FAIL\n");
        printk("AEL_SAADC_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(3000);
        }
    }

    int ret = adc_channel_setup_dt(&adc_ch7);
    if (ret != 0) {
        printk("[SAADC] channel_setup_err=%d FAIL\n", ret);
        printk("AEL_SAADC_FAIL\n");
        while (1) {
            if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
            k_msleep(3000);
        }
    }

    /* Take 4 samples and average */
    int32_t total = 0;
    int samples   = 4;
    for (int s = 0; s < samples; s++) {
        int16_t raw = 0;
        struct adc_sequence seq = {
            .channels    = BIT(adc_ch7.channel_id),
            .buffer      = &raw,
            .buffer_size = sizeof(raw),
            .resolution  = 12,
        };
        ret = adc_read_dt(&adc_ch7, &seq);
        if (ret != 0) {
            printk("[SAADC] read_err=%d FAIL\n", ret);
            printk("AEL_SAADC_FAIL\n");
            while (1) {
                if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
                k_msleep(3000);
            }
        }
        total += raw;
        k_msleep(10);
    }
    int32_t raw_avg = total / samples;

    /* Convert to mV: Vref=600 mV, gain=1/6 → full-scale = 3600 mV */
    int32_t mv_approx = (int32_t)((int64_t)raw_avg * 3600 / 4095);

    bool pass = (raw_avg >= 100 && raw_avg <= 4000);
    printk("[SAADC] raw=%d mv_approx=%d %s\n", raw_avg, mv_approx, pass ? "PASS" : "FAIL");

    while (1) {
        if (atomic_get(&ael_bl_flag)) { k_msleep(50); ael_enter_bootloader(); }
        printk("AEL_SAADC_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
