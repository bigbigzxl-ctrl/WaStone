/*
 * STM32F103RCT6 — AEL EXTI trigger test
 *
 * Observable behaviour:
 *   - PB0 (output push-pull) drives 10 rising edges
 *   - PB1 → EXTI1 (rising edge interrupt)
 *   - PASS after 10 EXTI1 interrupts
 *   - detail0 = interrupt count
 *
 * Wiring required: PB0 → PB1
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

/* GPIOB */
#define GPIOB_BASE      0x40010C00U
#define GPIOB_CRL       (*(volatile uint32_t *)(GPIOB_BASE + 0x00U))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0CU))

/* NVIC */
#define NVIC_ISER0      (*(volatile uint32_t *)0xE000E100U)

static volatile uint32_t exti_count  = 0U;
static volatile uint32_t test_passed = 0U;

void EXTI1_IRQHandler(void)
{
    if (EXTI_PR & (1U << 1)) {
        EXTI_PR = (1U << 1);   /* clear pending */
        exti_count++;
        AEL_MAILBOX->detail0 = exti_count;
        if (exti_count >= 10U && !test_passed) {
            ael_mailbox_pass();
            test_passed = 1U;
        }
    }
}

static void delay(volatile uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

int main(void)
{
    /* Enable GPIOB + AFIO clocks (APB2 bits 3 and 0) */
    RCC_APB2ENR |= (1U << 3) | (1U << 0);

    /*
     * PB0: output push-pull 50 MHz -> CRL[3:0] = 0x3
     * PB1: input pull-down         -> CRL[7:4] = 0x8, ODR1 = 0
     */
    GPIOB_CRL &= ~0xFFU;
    GPIOB_CRL |=  0x83U;
    GPIOB_ODR &= ~(1U << 1);

    /* AFIO_EXTICR1 bits [7:4] = 0x1 -> PB1 drives EXTI1 */
    AFIO_EXTICR1 &= ~(0xFU << 4);
    AFIO_EXTICR1 |=  (0x1U << 4);

    /* Enable EXTI1 rising edge */
    EXTI_PR    = (1U << 1);
    EXTI_IMR  |= (1U << 1);
    EXTI_RTSR |= (1U << 1);

    /* NVIC: EXTI1 = IRQ 7 */
    NVIC_ISER0 = (1U << 7);

    ael_mailbox_init();

    /* Drive 10 rising edges on PB0 */
    for (uint32_t i = 0U; i < 10U; i++) {
        GPIOB_ODR &= ~(1U << 0);
        delay(8000U);
        GPIOB_ODR |=  (1U << 0);
        delay(8000U);
    }

    /* Wait for ISR to declare PASS (should be near-instant) */
    while (!test_passed) {}

    while (1) {}

    return 0;
}
