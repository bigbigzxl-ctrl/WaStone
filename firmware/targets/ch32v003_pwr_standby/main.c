/*
 * CH32V003 PWR Standby mode test — zero wiring
 *
 * On CH32V003, "standby" (PDDS=1 + WFE) behaves like stop mode:
 *   - Clocks halted, SRAM preserved, wakeup RETURNS from WFE (no full reset)
 *   - AWU (Auto Wakeup Unit) generates an internal event after timeout
 *
 * Test flow:
 *   1. Initialize mailbox to RUNNING.
 *   2. Configure AWU: LSI ~128 kHz, /512 prescaler, window=25 → ~100 ms
 *   3. Enter standby via __WFE() — MCU clocks stop.
 *   4. AWU fires after ~100 ms → MCU wakes, resumes after __WFE().
 *   5. Clear AWU flag, write PASS to mailbox.
 *
 * IMPORTANT: Do NOT configure PD1 as pull-down — PD1 is the SDI debug pin.
 *            Pulling PD1 LOW prevents WCH-Link from connecting after wakeup.
 *
 * AWU timing: LSI ~128 kHz, AWUPSC=/512, AWUWR=25
 *   Period = 512/128000 * 25 ≈ 100 ms
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable PWR clock */
    RCC->APB1PCENR |= RCC_PWREN;

    /* Enable LSI (internal low-speed oscillator ~128 kHz) */
    RCC->RSTSCKR |= (1u << 0);  /* LSION */
    while (!(RCC->RSTSCKR & (1u << 1)));  /* wait LSIRDY */

    /* AWU: prescaler = /512, window = 25 → ~100 ms wakeup */
    PWR->AWUPSC  = 0x08u;  /* /512 */
    PWR->AWUWR   = 25u;
    PWR->AWUCSR |= (1u << 1);  /* AWUEN */

    /* Configure GPIO as input pull-down to minimize standby current.
     * CRITICAL: skip PD1 (bits[7:4]) — it is the SDI debug pin.
     *           Set PD1 to floating input (0x4) instead of pull-down (0x8). */
    RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN;
    GPIOA->CFGLR = 0x88888888u;
    GPIOC->CFGLR = 0x88888888u;
    GPIOD->CFGLR = 0x88888848u;  /* PD1 = 0x4 (floating), rest = 0x8 (pull-down) */
    GPIOA->OUTDR = 0u;
    GPIOC->OUTDR = 0u;
    GPIOD->OUTDR = 0u;

    /* Clear wakeup flag before entering standby */
    PWR->CTLR |= (1u << 2);  /* CWUF */

    /* Set PDDS (bit1) = 1 → standby mode */
    PWR->CTLR |= (1u << 1);

    /* Set SLEEPDEEP */
    PFIC->SCTLR |= (1u << 2);  /* SLEEPDEEP */

    /* Enter standby — clocks halt, AWU will fire after ~100 ms */
    __WFE();

    /* ── Execution resumes here after AWU wakeup ────────────────────── */

    /* Clear AWU flag and PDDS */
    PWR->CTLR &= ~(1u << 1);   /* clear PDDS */
    PFIC->SCTLR &= ~(1u << 2); /* clear SLEEPDEEP */
    PWR->CTLR |= (1u << 2);    /* CWUF */
    PWR->AWUCSR &= ~(1u << 1); /* disable AWU */

    ael_mailbox_pass();

    /* Liveness */
    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    uint32_t toggle = 0u;
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        toggle++;
        *detail0 = toggle << 1;
    }

    return 0;
}
