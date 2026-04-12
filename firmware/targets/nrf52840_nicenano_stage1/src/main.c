/*
 * AEL Stage 1 — nRF52840 nice!nano on-chip self-tests
 *
 * Runs 6 self-tests sequentially and prints results over USB CDC.
 * Each test line: [TAG] metric=value PASS|FAIL
 * Final line: AEL_STAGE1_PASS or AEL_STAGE1_FAIL
 *
 * Tests:
 *   TEMP  — nRF52840 on-die temperature sensor (TEMP peripheral)
 *   RNG   — hardware random number generator (entropy driver)
 *   TIMER — TIMER0 32-bit free-run, 1s count vs SysTick
 *   RTC   — RTC1 @32.768 kHz, 1s tick count accuracy
 *   FLASH — NVMC erase+write+read last page (4096 B)
 *   CRYPTO— AES-ECB hardware, known plaintext→ciphertext check
 *
 * Board: nrf52840dk/nrf52840 + app.overlay (USB CDC, code@0x1000)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/crypto/crypto.h>
#include <zephyr/sys/math_extras.h>
#include <string.h>
#include <stdint.h>

/* ── TEMP peripheral (direct register access) ─────────────────────────────── */
#define NRF_TEMP_BASE       0x4000C000UL
#define TEMP_TASKS_START    (*(volatile uint32_t *)(NRF_TEMP_BASE + 0x000))
#define TEMP_EVENTS_DATARDY (*(volatile uint32_t *)(NRF_TEMP_BASE + 0x100))
#define TEMP_TEMP           (*(volatile uint32_t *)(NRF_TEMP_BASE + 0x508))

static bool test_temp(void)
{
    TEMP_EVENTS_DATARDY = 0;
    TEMP_TASKS_START = 1;

    uint32_t deadline = k_uptime_get_32() + 200;
    while (!TEMP_EVENTS_DATARDY) {
        if (k_uptime_get_32() > deadline) {
            printk("[TEMP] timeout FAIL\n");
            return false;
        }
    }
    /* TEMP register: value in 0.25 °C units, signed */
    int32_t raw = (int32_t)TEMP_TEMP;
    int32_t temp_c4 = raw;          /* already in 0.25 °C */
    int32_t temp_c = temp_c4 / 4;

    if (temp_c < 0 || temp_c > 85) {
        printk("[TEMP] temp_c=%d out_of_range FAIL\n", temp_c);
        return false;
    }
    printk("[TEMP] temp_c=%d.%02d PASS\n", temp_c, (temp_c4 % 4) * 25);
    return true;
}

/* ── RNG (entropy driver) ─────────────────────────────────────────────────── */
#define RNG_BUF_LEN 64

static bool test_rng(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(rng));
    if (!device_is_ready(dev)) {
        printk("[RNG] device_not_ready FAIL\n");
        return false;
    }

    uint8_t buf[RNG_BUF_LEN];
    int ret = entropy_get_entropy(dev, buf, sizeof(buf));
    if (ret != 0) {
        printk("[RNG] entropy_err=%d FAIL\n", ret);
        return false;
    }

    /* Simple bit-balance check: expect 35–93% ones in 64 bytes */
    uint32_t ones = 0;
    for (int i = 0; i < RNG_BUF_LEN; i++) {
        ones += __builtin_popcount(buf[i]);
    }
    uint32_t total_bits = RNG_BUF_LEN * 8;
    /* 35..93% → 179..477 ones */
    if (ones < 179 || ones > 477) {
        printk("[RNG] ones=%u/%u balance_fail FAIL\n", ones, total_bits);
        return false;
    }
    printk("[RNG] ones=%u/%u PASS\n", ones, total_bits);
    return true;
}

/* ── TIMER0 (direct register access, 1 s reference via k_uptime) ─────────── */
#define NRF_TIMER0_BASE     0x40008000UL
#define TIMER0_TASKS_START  (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x000))
#define TIMER0_TASKS_STOP   (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x004))
#define TIMER0_TASKS_CLEAR  (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x00C))
#define TIMER0_TASKS_CAPTURE0 (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x040))
#define TIMER0_CC0          (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x540))
#define TIMER0_MODE         (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x504))
#define TIMER0_BITMODE      (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x508))
#define TIMER0_PRESCALER    (*(volatile uint32_t *)(NRF_TIMER0_BASE + 0x510))

static bool test_timer(void)
{
    /* TIMER0: 16 MHz / 2^0 = 16 MHz, 32-bit mode */
    TIMER0_TASKS_STOP  = 1;
    TIMER0_TASKS_CLEAR = 1;
    TIMER0_MODE        = 0;   /* Timer mode */
    TIMER0_BITMODE     = 3;   /* 32-bit */
    TIMER0_PRESCALER   = 0;   /* 16 MHz */
    TIMER0_TASKS_START = 1;

    uint32_t t0 = k_uptime_get_32();
    k_msleep(1000);
    uint32_t t1 = k_uptime_get_32();

    TIMER0_TASKS_CAPTURE0 = 1;
    uint32_t timer_ticks = TIMER0_CC0;
    TIMER0_TASKS_STOP = 1;

    uint32_t elapsed_ms = t1 - t0;
    /* Expected ticks: 16,000,000 per second */
    uint32_t expected = (uint32_t)((uint64_t)elapsed_ms * 16000);
    int32_t  err_ppm  = (int32_t)(((int64_t)timer_ticks - expected) * 1000000 / expected);

    if (err_ppm < -50000 || err_ppm > 50000) {
        printk("[TIMER] ticks=%u expected=%u err_ppm=%d FAIL\n",
               timer_ticks, expected, err_ppm);
        return false;
    }
    printk("[TIMER] ticks=%u err_ppm=%d PASS\n", timer_ticks, err_ppm);
    return true;
}

/* ── RTC1 (direct register, 32768 Hz LFCLK, 1 s reference) ──────────────── */
#define NRF_CLOCK_BASE      0x40000000UL
#define CLOCK_TASKS_LFCLKSTART (*(volatile uint32_t *)(NRF_CLOCK_BASE + 0x008))
#define CLOCK_EVENTS_LFCLKSTARTED (*(volatile uint32_t *)(NRF_CLOCK_BASE + 0x104))

#define NRF_RTC1_BASE       0x40011000UL
#define RTC1_TASKS_START    (*(volatile uint32_t *)(NRF_RTC1_BASE + 0x000))
#define RTC1_TASKS_STOP     (*(volatile uint32_t *)(NRF_RTC1_BASE + 0x004))
#define RTC1_TASKS_CLEAR    (*(volatile uint32_t *)(NRF_RTC1_BASE + 0x008))
#define RTC1_COUNTER        (*(volatile uint32_t *)(NRF_RTC1_BASE + 0x504))
#define RTC1_PRESCALER      (*(volatile uint32_t *)(NRF_RTC1_BASE + 0x508))

static bool test_rtc(void)
{
    /* Start LFCLK if not already running */
    CLOCK_EVENTS_LFCLKSTARTED = 0;
    CLOCK_TASKS_LFCLKSTART = 1;
    uint32_t deadline = k_uptime_get_32() + 1000;
    while (!CLOCK_EVENTS_LFCLKSTARTED) {
        if (k_uptime_get_32() > deadline) {
            printk("[RTC] lfclk_start_timeout FAIL\n");
            return false;
        }
    }

    /* RTC1: prescaler=0 → 32768 Hz */
    RTC1_TASKS_STOP  = 1;
    RTC1_TASKS_CLEAR = 1;
    RTC1_PRESCALER   = 0;
    RTC1_TASKS_START = 1;

    uint32_t t0 = k_uptime_get_32();
    k_msleep(1000);
    uint32_t t1 = k_uptime_get_32();

    uint32_t rtc_ticks = RTC1_COUNTER;
    RTC1_TASKS_STOP = 1;

    uint32_t elapsed_ms = t1 - t0;
    uint32_t expected   = (uint32_t)((uint64_t)elapsed_ms * 32768 / 1000);
    int32_t  err_ppm    = (expected > 0)
        ? (int32_t)(((int64_t)rtc_ticks - expected) * 1000000 / expected)
        : 999999;

    if (err_ppm < -100000 || err_ppm > 100000) {
        printk("[RTC] ticks=%u expected=%u err_ppm=%d FAIL\n",
               rtc_ticks, expected, err_ppm);
        return false;
    }
    printk("[RTC] ticks=%u err_ppm=%d PASS\n", rtc_ticks, err_ppm);
    return true;
}

/* ── NVMC flash R/W (last 4 KB page via Zephyr flash driver) ─────────────── */
#define FLASH_TEST_OFFSET  0x000EF000   /* last 4 KB of test_storage partition */
#define FLASH_TEST_SIZE    256

static bool test_flash(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash));
    if (!device_is_ready(dev)) {
        printk("[FLASH] device_not_ready FAIL\n");
        return false;
    }

    static uint8_t wbuf[FLASH_TEST_SIZE];
    static uint8_t rbuf[FLASH_TEST_SIZE];

    for (int i = 0; i < FLASH_TEST_SIZE; i++) {
        wbuf[i] = (uint8_t)(i ^ 0xA5);
    }

    int ret = flash_erase(dev, FLASH_TEST_OFFSET, 4096);
    if (ret != 0) {
        printk("[FLASH] erase_err=%d FAIL\n", ret);
        return false;
    }

    ret = flash_write(dev, FLASH_TEST_OFFSET, wbuf, FLASH_TEST_SIZE);
    if (ret != 0) {
        printk("[FLASH] write_err=%d FAIL\n", ret);
        return false;
    }

    ret = flash_read(dev, FLASH_TEST_OFFSET, rbuf, FLASH_TEST_SIZE);
    if (ret != 0) {
        printk("[FLASH] read_err=%d FAIL\n", ret);
        return false;
    }

    if (memcmp(wbuf, rbuf, FLASH_TEST_SIZE) != 0) {
        printk("[FLASH] data_mismatch FAIL\n");
        return false;
    }
    printk("[FLASH] rw_bytes=%d PASS\n", FLASH_TEST_SIZE);
    return true;
}

/* ── AES-ECB (hardware crypto) ───────────────────────────────────────────── */
/* NIST FIPS-197 Appendix B test vector */
static const uint8_t _ecb_key[16] = {
    0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
    0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c
};
static const uint8_t _ecb_plain[16] = {
    0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
    0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34
};
static const uint8_t _ecb_cipher[16] = {
    0x39,0x25,0x84,0x1d,0x02,0xdc,0x09,0xfb,
    0xdc,0x11,0x85,0x97,0x19,0x6a,0x0b,0x32
};

static bool test_crypto(void)
{
    const struct device *dev = device_get_binding(CONFIG_CRYPTO_NRF_ECB_DRV_NAME);
    if (!dev) {
        /* Try DT-based lookup */
        dev = DEVICE_DT_GET_ANY(nordic_nrf_ecb);
    }
    if (!dev || !device_is_ready(dev)) {
        printk("[CRYPTO] device_not_ready FAIL\n");
        return false;
    }

    struct cipher_ctx ctx = {
        .keylen = 16,
        .key.bit_stream = (uint8_t *)_ecb_key,
        .flags = CAP_RAW_KEY | CAP_SYNC_OPS,
    };
    struct cipher_pkt pkt = {
        .in_buf      = (uint8_t *)_ecb_plain,
        .in_len      = 16,
        .out_buf_max = 16,
    };
    static uint8_t out[16];
    pkt.out_buf = out;

    int ret = cipher_begin_session(dev, &ctx, CRYPTO_CIPHER_ALGO_AES,
                                   CRYPTO_CIPHER_MODE_ECB,
                                   CRYPTO_CIPHER_OP_ENCRYPT);
    if (ret != 0) {
        printk("[CRYPTO] session_err=%d FAIL\n", ret);
        return false;
    }

    ret = cipher_block_op(&ctx, &pkt);
    cipher_free_session(dev, &ctx);

    if (ret != 0) {
        printk("[CRYPTO] encrypt_err=%d FAIL\n", ret);
        return false;
    }

    if (memcmp(out, _ecb_cipher, 16) != 0) {
        printk("[CRYPTO] ciphertext_mismatch FAIL\n");
        return false;
    }
    printk("[CRYPTO] aes_ecb_ok=1 PASS\n");
    return true;
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Short boot delay so USB CDC enumerates before first printk */
    k_msleep(1500);

    printk("AEL_STAGE1_START board=nRF52840\n");

    bool pass = true;
    pass &= test_temp();
    pass &= test_rng();
    pass &= test_timer();
    pass &= test_rtc();
    pass &= test_flash();
    pass &= test_crypto();

    if (pass) {
        printk("AEL_STAGE1_PASS\n");
    } else {
        printk("AEL_STAGE1_FAIL\n");
    }

    /* Repeat summary every 5 s so observe_uart always catches it */
    while (1) {
        k_msleep(5000);
        printk("AEL_STAGE1_%s (repeat)\n", pass ? "PASS" : "FAIL");
    }
    return 0;
}
