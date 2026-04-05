/*
 * FreeRTOS configuration — STM32H563RGT6
 * 64 MHz HSI, Cortex-M33, 256 KB SRAM1, ARM_CM33_NTZ port
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configCPU_CLOCK_HZ    64000000UL
#define configTOTAL_HEAP_SIZE (32 * 1024)

#include "FreeRTOSConfig_base.h"

/* ARM_CM33_NTZ: port.c/portasm.c define SVC/PendSV/SysTick directly —
   no handler-name mapping macros needed here.
   All code runs non-secure; TrustZone and MPU are not used. */
#define configRUN_FREERTOS_SECURE_ONLY  0
#define configENABLE_TRUSTZONE          0
#define configENABLE_MPU                0
#define configENABLE_FPU                0

#endif /* FREERTOS_CONFIG_H */
