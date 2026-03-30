#include <stdint.h>

/* Linker symbols */
extern uint32_t _estack;
extern uint32_t _sidata;   /* LMA of .data in flash */
extern uint32_t _sdata;    /* VMA start of .data in SRAM */
extern uint32_t _edata;    /* VMA end of .data in SRAM */
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

void Reset_Handler(void);
static void Default_Handler(void);

/* Minimal vector table — 16 ARM core vectors + enough IRQ slots.
   STM32U585 has up to 137 external interrupts; for blinky we only
   need the first few to be valid. Pad with Default_Handler. */
__attribute__((section(".isr_vector"), used))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,          /*  0 Initial SP */
    (uint32_t)Reset_Handler,     /*  1 Reset */
    (uint32_t)Default_Handler,   /*  2 NMI */
    (uint32_t)Default_Handler,   /*  3 HardFault */
    (uint32_t)Default_Handler,   /*  4 MemManage */
    (uint32_t)Default_Handler,   /*  5 BusFault */
    (uint32_t)Default_Handler,   /*  6 UsageFault */
    (uint32_t)Default_Handler,   /*  7 SecureFault (Cortex-M33) */
    0, 0, 0,                     /*  8-10 Reserved */
    (uint32_t)Default_Handler,   /* 11 SVCall */
    (uint32_t)Default_Handler,   /* 12 DebugMonitor */
    0,                           /* 13 Reserved */
    (uint32_t)Default_Handler,   /* 14 PendSV */
    (uint32_t)Default_Handler,   /* 15 SysTick */
};

void Reset_Handler(void) {
    /* Copy .data from flash to SRAM */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero .bss */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0u;
    }

    main();

    /* Should never return */
    while (1) {}
}

static void Default_Handler(void) {
    while (1) {}
}
