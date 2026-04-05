/*
 * FreeRTOS configuration — STM32F103RCT6
 * 8 MHz HSI, Cortex-M3, 48 KB RAM, ARM_CM3 port
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configCPU_CLOCK_HZ    8000000UL
#define configTOTAL_HEAP_SIZE (16 * 1024)

#include "FreeRTOSConfig_base.h"

/* ARM_CM3 port: map port handler names to ST startup vector table symbols */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
