#include <stdint.h>

/* Linker symbols */
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

void Reset_Handler(void);
void HardFault_Handler(void);
static void Default_Handler(void);

/* Minimal vector table — Cortex-M33, STM32H563 */
__attribute__((section(".isr_vector"), used))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,            /*  0 Initial SP */
    (uint32_t)Reset_Handler,       /*  1 Reset */
    (uint32_t)Default_Handler,     /*  2 NMI */
    (uint32_t)HardFault_Handler,   /*  3 HardFault — must not be Default_Handler */
    (uint32_t)Default_Handler,     /*  4 MemManage */
    (uint32_t)Default_Handler,     /*  5 BusFault */
    (uint32_t)Default_Handler,     /*  6 UsageFault */
    (uint32_t)Default_Handler,     /*  7 SecureFault */
    0, 0, 0,                       /*  8-10 Reserved */
    (uint32_t)Default_Handler,     /* 11 SVCall */
    (uint32_t)Default_Handler,     /* 12 DebugMonitor */
    0,                             /* 13 Reserved */
    (uint32_t)Default_Handler,     /* 14 PendSV */
    (uint32_t)Default_Handler,     /* 15 SysTick */
};

void Reset_Handler(void) {
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0u;
    }

    main();

    while (1) {}
}

/* HIGH_PRIORITY 3f13ca66: HardFault must SYSRESETREQ to avoid SWD LOCKUP */
void HardFault_Handler(void) {
    /* SCB->AIRCR: VECTKEY=0x05FA, SYSRESETREQ=bit2 */
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

static void Default_Handler(void) {
    while (1) {}
}
