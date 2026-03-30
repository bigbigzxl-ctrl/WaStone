/*
 * STM32H750VBT6 startup — timer_mailbox variant
 *
 * Extends the minimal startup with vendor IRQs 0-54 so that
 * TIM6_DAC_IRQHandler (IRQ 54, vector index 70) is reachable.
 *
 * All other vendor IRQs go to Default_Handler.
 * Core exception handlers are identical to the minimal startup.
 *
 * IRQ numbering (RM0433 Table 159):
 *   IRQ 54 = TIM6_DAC  →  vector index 70  (= 16 + 54)
 */

#include <stdint.h>

extern int main(void);
extern void TIM6_DAC_IRQHandler(void);

extern uint32_t _estack;
extern uint32_t _etext;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

void Reset_Handler(void);
void Default_Handler(void);

void Reset_Handler(void)
{
    uint32_t *src = &_etext;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) { *dst++ = *src++; }
    dst = &_sbss;
    while (dst < &_ebss) { *dst++ = 0u; }
    (void)main();
    while (1) {}
}

void Default_Handler(void) { while (1) {} }

__attribute__((section(".isr_vector")))
void (*const vector_table[])(void) = {
    /* 0  */ (void (*)(void))(&_estack),
    /* 1  */ Reset_Handler,
    /* 2  */ Default_Handler,  /* NMI */
    /* 3  */ Default_Handler,  /* HardFault */
    /* 4  */ Default_Handler,  /* MemManage */
    /* 5  */ Default_Handler,  /* BusFault */
    /* 6  */ Default_Handler,  /* UsageFault */
    /* 7  */ 0,
    /* 8  */ 0,
    /* 9  */ 0,
    /* 10 */ 0,
    /* 11 */ Default_Handler,  /* SVCall */
    /* 12 */ Default_Handler,  /* DebugMon */
    /* 13 */ 0,
    /* 14 */ Default_Handler,  /* PendSV */
    /* 15 */ Default_Handler,  /* SysTick */
    /* IRQ 0 .. 53 → Default_Handler (indices 16 .. 69) */
    /* 16 */ Default_Handler, /* 17 */ Default_Handler,
    /* 18 */ Default_Handler, /* 19 */ Default_Handler,
    /* 20 */ Default_Handler, /* 21 */ Default_Handler,
    /* 22 */ Default_Handler, /* 23 */ Default_Handler,
    /* 24 */ Default_Handler, /* 25 */ Default_Handler,
    /* 26 */ Default_Handler, /* 27 */ Default_Handler,
    /* 28 */ Default_Handler, /* 29 */ Default_Handler,
    /* 30 */ Default_Handler, /* 31 */ Default_Handler,
    /* 32 */ Default_Handler, /* 33 */ Default_Handler,
    /* 34 */ Default_Handler, /* 35 */ Default_Handler,
    /* 36 */ Default_Handler, /* 37 */ Default_Handler,
    /* 38 */ Default_Handler, /* 39 */ Default_Handler,
    /* 40 */ Default_Handler, /* 41 */ Default_Handler,
    /* 42 */ Default_Handler, /* 43 */ Default_Handler,
    /* 44 */ Default_Handler, /* 45 */ Default_Handler,
    /* 46 */ Default_Handler, /* 47 */ Default_Handler,
    /* 48 */ Default_Handler, /* 49 */ Default_Handler,
    /* 50 */ Default_Handler, /* 51 */ Default_Handler,
    /* 52 */ Default_Handler, /* 53 */ Default_Handler,
    /* 54 */ Default_Handler, /* 55 */ Default_Handler,
    /* 56 */ Default_Handler, /* 57 */ Default_Handler,
    /* 58 */ Default_Handler, /* 59 */ Default_Handler,
    /* 60 */ Default_Handler, /* 61 */ Default_Handler,
    /* 62 */ Default_Handler, /* 63 */ Default_Handler,
    /* 64 */ Default_Handler, /* 65 */ Default_Handler,
    /* 66 */ Default_Handler, /* 67 */ Default_Handler,
    /* 68 */ Default_Handler, /* 69 */ Default_Handler,
    /* 70 */ TIM6_DAC_IRQHandler,  /* IRQ 54 */
};
