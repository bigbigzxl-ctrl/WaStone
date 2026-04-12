/* ch32v305_exti_loopback — PA1 drives PA2, EXTI2 counts rising edges
 * Board: CH32V305RBT6
 * Wiring: PA1 ↔ PA2 jumper
 * Clock: 96 MHz
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

volatile uint32_t exti_count = 0;

void EXTI2_IRQHandler(void) __attribute__((interrupt));
void EXTI2_IRQHandler(void) { exti_count++; EXTI->INTFR = EXTI_Line2; }

static void delay_us(uint32_t us) { for (uint32_t i = 0; i < us * 96; i++) __asm__("nop"); }

int main(void)
{
    ael_mailbox_init();

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO;

    /* PA1: output PP 2MHz; PA2: input pull-down for EXTI */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFF << 4))
                 | (0x2u << 4)    /* PA1: OUT_PP 2MHz */
                 | (0x8u << 8);   /* PA2: IN with pull (ODR=0 → pull-down) */
    GPIOA->OUTDR &= ~(1u << 2);   /* pull-down */

    /* AFIO EXTICR1: EXTI2 source = PA (bits[11:8] = 0x00) */
    AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~(0xFu << 8));

    EXTI->INTENR |= EXTI_Line2;
    EXTI->RTENR  |= EXTI_Line2;
    NVIC_EnableIRQ(EXTI2_IRQn);

    /* Generate 5 pulses */
    for (int i = 0; i < 5; i++) {
        GPIOA->BSHR = (1u << 1); delay_us(50);
        GPIOA->BCR  = (1u << 1); delay_us(50);
    }
    delay_us(100);

    if (exti_count != 5) { ael_mailbox_fail(1, exti_count); }
    else                 { ael_mailbox_pass(); }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
