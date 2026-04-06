/*
 * CH32V003 LED blink mailbox test
 *
 * PA1 blinks at 1 Hz (toggle every 500 ms)
 * PA2 blinks at 2 Hz (toggle every 250 ms)
 *
 * Uses SysTick interrupt with __attribute__((interrupt("WCH-Interrupt-fast")))
 * per the official WCH EVT examples (SYSTICK/SYSTICK_Interrupt).
 *
 * Key findings vs first attempt:
 *  - Interrupt attribute must be interrupt("WCH-Interrupt-fast"), not interrupt()
 *  - Delay polling must set CTLR bit 5 (INIT/CNT-reset) alongside bit 0 (enable)
 *  - SR flag must be cleared at the END of the handler
 *  - NVIC_EnableIRQ before writing CTLR
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#define SYSTEM_CORE_CLOCK 24000000UL   /* HSI default: 24 MHz */

#include "ael_mailbox.h"
#include "ch32v003fun.h"

void SystemInit(void) {}

/* ── 1 ms tick counter ─────────────────────────────────────────────────── */
static volatile uint32_t g_tick_ms = 0;

/* WCH-specific fast interrupt attribute — required for correct ISR entry/exit */
void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    g_tick_ms++;
    SysTick->SR = 0;   /* clear flag at END of handler, as per WCH examples */
}

/* ── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOA clock (APB2) */
    RCC->APB2PCENR |= RCC_IOPAEN;

    /*
     * PA1 (bits [7:4]) and PA2 (bits [11:8]):
     *   CNF[1:0]=00 (push-pull output), MODE[1:0]=01 (10 MHz)
     *   → nibble value 0x1 for each pin
     */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFFu << 4)) | (0x11u << 4);

    /* Both LEDs off */
    GPIOA->OUTDR &= ~((1u << 1) | (1u << 2));

    /*
     * SysTick at 1 ms per WCH EVT SYSTICK_Interrupt example:
     *   1. Enable NVIC FIRST
     *   2. Clear SR
     *   3. CMP = SystemCoreClock - 1  (1 s match; we want 1 ms so CMP = HCLK-1 is wrong)
     *      Correction: for 1 ms at 24 MHz with HCLK source → CMP = 24000 - 1
     *   4. CNT = 0
     *   5. CTLR = 0xF  (enable | int-enable | HCLK | auto-reload)
     */
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->SR   = 0;
    SysTick->CMP  = 24000u - 1u;   /* 1 ms at 24 MHz HCLK */
    SysTick->CNT  = 0;
    SysTick->CTLR = 0xFu;          /* STE|STIE|HCLK|auto-reload */

    __enable_irq();

    ael_mailbox_pass();

    uint32_t last_pa1 = 0;
    uint32_t last_pa2 = 0;

    while (1)
    {
        uint32_t now = g_tick_ms;

        if ((now - last_pa1) >= 500u) {        /* 1 Hz → toggle every 500 ms */
            GPIOA->OUTDR ^= (1u << 1);
            last_pa1 = now;
        }
        if ((now - last_pa2) >= 250u) {        /* 2 Hz → toggle every 250 ms */
            GPIOA->OUTDR ^= (1u << 2);
            last_pa2 = now;
        }
    }

    return 0;
}
