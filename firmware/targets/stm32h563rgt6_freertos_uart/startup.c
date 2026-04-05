/*
 * Minimal Cortex-M33 startup for STM32H563RGT6 — FreeRTOS variant
 *
 * SVC_Handler, PendSV_Handler, SysTick_Handler are provided by the
 * FreeRTOS ARM_CM33_NTZ port (portasm.c / port.c).  They are declared
 * extern here so the linker resolves them from FreeRTOS objects.
 */
#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;
extern int main(void);

void Reset_Handler(void);
void HardFault_Handler(void);
static void Default_Handler(void) { while (1) {} }

/* FreeRTOS ARM_CM33_NTZ handlers — defined in portasm.c / port.c */
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

__attribute__((section(".isr_vector"), used))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)Default_Handler,   /* NMI */
    (uint32_t)HardFault_Handler,
    (uint32_t)Default_Handler,   /* MemManage */
    (uint32_t)Default_Handler,   /* BusFault */
    (uint32_t)Default_Handler,   /* UsageFault */
    (uint32_t)Default_Handler,   /* SecureFault */
    0, 0, 0,                     /* Reserved */
    (uint32_t)SVC_Handler,       /* SVCall  — FreeRTOS */
    (uint32_t)Default_Handler,   /* DebugMonitor */
    0,                           /* Reserved */
    (uint32_t)PendSV_Handler,    /* PendSV  — FreeRTOS */
    (uint32_t)SysTick_Handler,   /* SysTick — FreeRTOS */
};

void Reset_Handler(void) {
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0u;
    main();
    while (1) {}
}
