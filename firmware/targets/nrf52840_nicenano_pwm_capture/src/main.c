/*
 * AEL Stage 2 — nRF52840 nice!nano PWM + capture loopback
 *
 * Wiring: P0.22 (PWM0 CH0 out) → P0.24 (GPIOTE in for counting)  [1 wire]
 *
 * Generates PWM at ~1 kHz on P0.22.
 * GPIOTE + PPI + TIMER1 count rising edges on P0.24 for 100 ms.
 * Expected edge count: ~100 (1 kHz × 100 ms).
 * Accepts ±30% tolerance.
 *
 * Reports: [PWM] edges=%u expected=~100 err_pct=%d PASS|FAIL
 *          AEL_PWM_CAPTURE_PASS|FAIL
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/pwm.h>

/* Direct register access for GPIOTE, PPI, TIMER1 */
#define NRF_GPIOTE_BASE      0x40006000UL
#define GPIOTE_CONFIG(n)     (*(volatile uint32_t *)(NRF_GPIOTE_BASE + 0x510 + (n)*4))
#define GPIOTE_EVENTS_IN(n)  (*(volatile uint32_t *)(NRF_GPIOTE_BASE + 0x100 + (n)*4))

#define NRF_PPI_BASE         0x4001F000UL
#define PPI_CHEN             (*(volatile uint32_t *)(NRF_PPI_BASE + 0x500))
#define PPI_CHENSET          (*(volatile uint32_t *)(NRF_PPI_BASE + 0x504))
#define PPI_CH_EEP(n)        (*(volatile uint32_t *)(NRF_PPI_BASE + 0x510 + (n)*8))
#define PPI_CH_TEP(n)        (*(volatile uint32_t *)(NRF_PPI_BASE + 0x514 + (n)*8))

#define NRF_TIMER1_BASE      0x40009000UL
#define TIMER1_TASKS_START   (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x000))
#define TIMER1_TASKS_STOP    (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x004))
#define TIMER1_TASKS_CLEAR   (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x00C))
#define TIMER1_TASKS_COUNT   (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x008))
#define TIMER1_TASKS_CAPTURE0 (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x040))
#define TIMER1_CC0           (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x540))
#define TIMER1_MODE          (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x504))
#define TIMER1_BITMODE       (*(volatile uint32_t *)(NRF_TIMER1_BASE + 0x508))

#define CAPTURE_PIN  24   /* P0.24 — input from PWM */
#define CAPTURE_PORT  0
#define GPIOTE_CH    0
#define PPI_CH       0

static const struct pwm_dt_spec pwm_out = PWM_DT_SPEC_GET(DT_ALIAS(ael_pwm_out));

int main(void)
{
    k_msleep(1500);
    printk("AEL_PWM_CAPTURE_START\n");

    if (!pwm_is_ready_dt(&pwm_out)) {
        printk("[PWM] device_not_ready FAIL\n");
        printk("AEL_PWM_CAPTURE_FAIL\n");
        return -1;
    }

    /* PWM: 1 kHz, 50% duty */
    uint32_t period_ns = 1000000;  /* 1 ms = 1 kHz */
    int ret = pwm_set_dt(&pwm_out, period_ns, period_ns / 2);
    if (ret != 0) {
        printk("[PWM] set_err=%d FAIL\n", ret);
        printk("AEL_PWM_CAPTURE_FAIL\n");
        return -1;
    }

    /* TIMER1 in counter mode */
    TIMER1_TASKS_STOP  = 1;
    TIMER1_TASKS_CLEAR = 1;
    TIMER1_MODE        = 1;   /* Counter mode */
    TIMER1_BITMODE     = 3;   /* 32-bit */

    /* GPIOTE channel 0: event on rising edge of P0.24 */
    GPIOTE_CONFIG(GPIOTE_CH) =
        (1UL << 0)                          /* MODE = Event */
        | ((uint32_t)CAPTURE_PIN  << 8)     /* PSEL */
        | ((uint32_t)CAPTURE_PORT << 13)    /* PORT */
        | (1UL << 16);                      /* POLARITY = LoToHi (rising) */

    /* PPI: GPIOTE_EVENTS_IN[0] → TIMER1_TASKS_COUNT */
    PPI_CH_EEP(PPI_CH) = (uint32_t)&GPIOTE_EVENTS_IN(GPIOTE_CH);
    PPI_CH_TEP(PPI_CH) = (uint32_t)&TIMER1_TASKS_COUNT;
    PPI_CHENSET = (1UL << PPI_CH);

    TIMER1_TASKS_START = 1;
    k_msleep(100);
    TIMER1_TASKS_STOP     = 1;
    TIMER1_TASKS_CAPTURE0 = 1;
    uint32_t edges = TIMER1_CC0;

    /* Disable PPI */
    PPI_CHEN &= ~(1UL << PPI_CH);

    /* Stop PWM */
    pwm_set_dt(&pwm_out, period_ns, 0);

    /* Expected ~100 edges (1 kHz × 100 ms), accept ±30% */
    int32_t expected = 100;
    int32_t err_pct  = (int32_t)((int64_t)((int32_t)edges - expected) * 100 / expected);

    bool pass = (err_pct >= -30 && err_pct <= 30);
    printk("[PWM] edges=%u expected=%d err_pct=%d %s\n",
           edges, expected, err_pct, pass ? "PASS" : "FAIL");

    while (1) {
        printk("AEL_PWM_CAPTURE_%s\n", pass ? "PASS" : "FAIL");
        k_msleep(3000);
    }
    return 0;
}
