/*
 * stm32h563rgt6_exti_trigger
 *
 * PC8 (push-pull output) → wire → PC9 (EXTI9 rising edge).
 * Drives PC8 HIGH, polls EXTI RPR1 bit9 for rising edge event.
 * Clears flag, drives LOW, drives HIGH again — verifies second trigger.
 *
 * STM32H563 EXTI (new IP, RM0481):
 *   EXTI_BASE = 0x44022000
 *   RTSR1 = +0x00, FTSR1 = +0x04, IMR1 = +0x80, RPR1 = +0x0C
 *   EXTICR3 (+0x68): EXTI9 source at bits[11:8] = port C (0x2)
 *
 * Clocks: GPIOC = AHB2 bit2.
 * No NVIC needed — polling RPR1.
 *
 * FAIL codes:
 *   0xE001 — RPR1 bit9 not set after first PC8 HIGH
 *   0xE002 — RPR1 bit9 not set after second PC8 HIGH
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

#define GPIOC_BASE      0x42020800u
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00u))
#define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x18u))

/* STM32H563 EXTI new IP */
#define EXTI_BASE       0x44022000u
#define EXTI_RTSR1      (*(volatile uint32_t *)(EXTI_BASE + 0x00u))
#define EXTI_IMR1       (*(volatile uint32_t *)(EXTI_BASE + 0x80u))
#define EXTI_RPR1       (*(volatile uint32_t *)(EXTI_BASE + 0x0Cu))
#define EXTI_EXTICR3    (*(volatile uint32_t *)(EXTI_BASE + 0x68u))

#define TIMEOUT  500000u

static uint32_t wait_rpr1_bit9(void)
{
    for (uint32_t t = 0; t < TIMEOUT; t++) {
        if (EXTI_RPR1 & (1u << 9)) return 1u;
    }
    return 0u;
}

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOC (AHB2 bit2) */
    RCC_AHB2ENR |= (1u << 2);
    (void)RCC_AHB2ENR;

    /* PC8: output (MODER bits[17:16]=01), drive LOW initially */
    GPIOC_MODER = (GPIOC_MODER & ~(3u << 16)) | (1u << 16);
    GPIOC_BSRR  = (1u << 24);   /* BR8 = PC8 LOW */

    /* PC9: input (MODER bits[19:18]=00) */
    GPIOC_MODER &= ~(3u << 18);

    /* EXTI9: select port C in EXTICR3 bits[11:8] = 0x2 */
    EXTI_EXTICR3 = (EXTI_EXTICR3 & ~(0xFu << 8)) | (0x2u << 8);

    /* Enable rising trigger on EXTI9 */
    EXTI_RTSR1 |= (1u << 9);

    /* Unmask EXTI9 in IMR1 (HIGH_PRIORITY 35eeae70: IMR1 required for RPR1) */
    EXTI_IMR1 |= (1u << 9);

    /* Clear any stale pending */
    EXTI_RPR1 = (1u << 9);

    /* --- Trigger 1: drive PC8 HIGH --- */
    GPIOC_BSRR = (1u << 8);
    if (!wait_rpr1_bit9()) {
        ael_mailbox_fail(0xE001u, EXTI_RPR1);
        while (1) {}
    }
    EXTI_RPR1 = (1u << 9);   /* clear by writing 1 */

    /* --- Trigger 2: drive LOW then HIGH again --- */
    GPIOC_BSRR = (1u << 24);  /* PC8 LOW */
    for (volatile uint32_t d = 0; d < 1000u; d++) {}
    GPIOC_BSRR = (1u << 8);   /* PC8 HIGH */
    if (!wait_rpr1_bit9()) {
        ael_mailbox_fail(0xE002u, EXTI_RPR1);
        while (1) {}
    }
    EXTI_RPR1 = (1u << 9);

    AEL_MAILBOX->detail0 = 2u;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
