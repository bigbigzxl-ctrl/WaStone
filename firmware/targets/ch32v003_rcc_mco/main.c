/*
 * CH32V003 RCC MCO test — zero wiring
 *
 * Configures MCO output on PC4 (AF_PP) with SYSCLK as source.
 * Verification:
 *   1. Read back RCC_CFGR0 MCO field — must match written value.
 *   2. Use TIM2 at 1 MHz to measure ~1 ms delay via SysTick.
 *      If SYSCLK is stable at ~24 MHz, TIM2 should count ~1000 ticks
 *      during a SysTick-timed 1 ms window. Accept 500–1500 ticks.
 *
 * PC4 is ADC_CH2 / MCO pin. Configure as AF_PP 50 MHz.
 *
 * PASS if MCO config bits correct AND TIM2 count in expected range.
 * detail0 = TIM2 count during 1 ms << 1.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOC clock */
    RCC->APB2PCENR |= RCC_IOPCEN;

    /* PC4 = AF_PP 50 MHz (MCO output pin)
     * CFGLR bits[19:16] for PC4: 0xB = AF PP 50 MHz */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFu << 16)) | (0xBu << 16);

    /* Set MCO = SYSCLK (RCC_CFGR0 bits[26:24] = 0b100) */
    RCC->CFGR0 = (RCC->CFGR0 & ~(0x7u << 24)) | (0x4u << 24);

    /* Verify MCO bits read back correctly */
    uint32_t mco_bits = (RCC->CFGR0 >> 24) & 0x7u;

    /* Enable TIM2 clock */
    RCC->APB1PCENR |= RCC_TIM2EN;
    RCC->APB1PRSTR |=  RCC_TIM2RST;
    RCC->APB1PRSTR &= ~RCC_TIM2RST;

    /* TIM2: PSC=23 → 1 MHz tick (24 MHz / 24 = 1 MHz) */
    TIM2->PSC   = 23u;
    TIM2->ATRLR = 0xFFFFu;
    TIM2->SWEVGR = 0x0001u;
    TIM2->INTFR  = 0u;
    TIM2->CTLR1  = 0x0001u;  /* CEN */

    /* Use SysTick to time 1 ms (24000 cycles @ 24 MHz) */
    SysTick->SR   = 0u;
    SysTick->CMP  = 24000u - 1u;  /* 1 ms */
    SysTick->CNT  = 0u;
    SysTick->CTLR = 0xFu;  /* enable, auto-reload, use HCLK, start */

    uint16_t cnt_start = (uint16_t)TIM2->CNT;

    /* Wait for SysTick to count 1 ms */
    while (!(SysTick->SR & 0x1u));

    uint16_t cnt_end = (uint16_t)TIM2->CNT;
    uint16_t elapsed = (uint16_t)(cnt_end - cnt_start);

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = (uint32_t)elapsed << 1;

    /* PASS if MCO configured correctly (mco_bits == 4) AND
     * TIM2 counted ~1000 ticks in 1 ms (accept 500–1500) */
    if (mco_bits == 4u && elapsed >= 500u && elapsed <= 1500u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(mco_bits, elapsed);

    /* Liveness: keep updating detail0 */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 = (uint32_t)(uint16_t)TIM2->CNT << 1;
    }

    return 0;
}
