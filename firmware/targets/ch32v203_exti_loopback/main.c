/* ch32v203_exti_loopback — PA2 drives PA3, EXTI3 counts rising edges */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"
volatile uint32_t exti_count = 0;
void EXTI3_IRQHandler(void) __attribute__((interrupt));
void EXTI3_IRQHandler(void) { exti_count++; EXTI->INTFR = EXTI_Line3; }
static void delay_us(uint32_t us) { for (uint32_t i=0;i<us*72;i++) __asm__("nop"); }
int main(void)
{
    ael_mailbox_init();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO;
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFF<<8)) | (0x3<<8) | (0x8<<12);
    AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~(0xF<<12)); /* PA3 → EXTI3 */
    EXTI->INTENR |= EXTI_Line3;
    EXTI->RTENR  |= EXTI_Line3;
    NVIC_EnableIRQ(EXTI3_IRQn);
    /* Generate 5 pulses */
    for (int i = 0; i < 5; i++) {
        GPIOA->BSHR = (1<<2); delay_us(50);
        GPIOA->BCR  = (1<<2); delay_us(50);
    }
    delay_us(100);
    if (exti_count != 5) { ael_mailbox_fail(1, exti_count); }
    else { ael_mailbox_pass(); }
    uint32_t tick = 0;
    while (1) { AEL_MAILBOX->detail0 = ++tick; for (volatile int i=0;i<720000;i++); }
}
