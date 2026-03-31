/*
 * STM32F103RCT6 — AEL EXTI trigger test
 *
 * Observable behaviour:
 *   - PA0 (output push-pull) drives repeated rising edges
 *   - PA1 → EXTI1 (rising edge pending bit)
 *   - PASS after 10 EXTI1 detections
 *   - detail0 = detected edge count
 *
 * Wiring required: PA0 → PA1
 * Mailbox address: 0x2000BC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x40021000U
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

/* AFIO */
#define AFIO_BASE       0x40010000U
#define AFIO_EXTICR1    (*(volatile uint32_t *)(AFIO_BASE + 0x08U))

/* EXTI */
#define EXTI_BASE       0x40010400U
#define EXTI_IMR        (*(volatile uint32_t *)(EXTI_BASE + 0x00U))
#define EXTI_RTSR       (*(volatile uint32_t *)(EXTI_BASE + 0x08U))
#define EXTI_PR         (*(volatile uint32_t *)(EXTI_BASE + 0x14U))

/* GPIOA */
#define GPIOA_BASE      0x40010800U
#define GPIOA_CRL       (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0CU))

static void delay(volatile uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

int main(void)
{
    /* Enable GPIOA + AFIO clocks (APB2 bits 2 and 0) */
    RCC_APB2ENR |= (1U << 2) | (1U << 0);

    /*
     * PA0: output push-pull 50 MHz -> CRL[3:0] = 0x3
     * PA1: input pull-down         -> CRL[7:4] = 0x8, ODR1 = 0
     */
    GPIOA_CRL &= ~0xFFU;
    GPIOA_CRL |=  0x83U;
    GPIOA_ODR &= ~(1U << 1);

    /* AFIO_EXTICR1 bits [7:4] = 0x0 -> PA1 drives EXTI1 (default route) */
    AFIO_EXTICR1 &= ~(0xFU << 4);

    /* Enable EXTI1 rising edge */
    EXTI_PR    = (1U << 1);
    EXTI_IMR  |= (1U << 1);
    EXTI_RTSR |= (1U << 1);

    ael_mailbox_init();

    /*
     * Drive rising edges until EXTI1 has observed 10 pending events. The live
     * bench probe is slow enough that a fixed 10-pulse burst can underrun.
     */
    uint32_t exti_count = 0U;
    for (uint32_t i = 0U; i < 50U && exti_count < 10U; i++) {
        GPIOA_ODR &= ~(1U << 0);
        delay(40000U);
        GPIOA_ODR |=  (1U << 0);
        delay(40000U);

        if (EXTI_PR & (1U << 1)) {
            EXTI_PR = (1U << 1);
            exti_count++;
            AEL_MAILBOX->detail0 = exti_count;
        }
    }

    if (exti_count < 10U) {
        ael_mailbox_fail(0x01U, exti_count);
        while (1) {}
    }

    ael_mailbox_pass();
    while (1) {}

    return 0;
}
