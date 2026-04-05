/*
 * FreeRTOS configuration — STM32F407VGT6 (F4 Discovery)
 * 16 MHz HSI, Cortex-M4, 192 KB RAM, ARM_CM3 port (soft FP, no FPU context)
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configCPU_CLOCK_HZ    16000000UL
#define configTOTAL_HEAP_SIZE (20 * 1024)

#include "FreeRTOSConfig_base.h"

/* ARM_CM3 port: map port handler names to ST startup vector table symbols */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
