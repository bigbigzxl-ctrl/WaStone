/*
 * FreeRTOS base configuration — shared across all AEL ARM Cortex-M bare-metal targets.
 *
 * Usage in a board-specific FreeRTOSConfig.h:
 *
 *   #ifndef FREERTOS_CONFIG_H
 *   #define FREERTOS_CONFIG_H
 *
 *   // 1. Board-specific overrides BEFORE including base
 *   #define configCPU_CLOCK_HZ    64000000UL   // your clock
 *   #define configTOTAL_HEAP_SIZE (32 * 1024)  // your RAM budget
 *
 *   // 2. Pull in all common defaults
 *   #include "FreeRTOSConfig_base.h"
 *
 *   // 3. Port-specific handler mapping (ARM_CM3 / ARM_CM33_NTZ only)
 *   //    CM33_NTZ: omit — port.c defines the vectors directly
 *   //    CM3/CM4:  add the three macros below
 *   // #define vPortSVCHandler     SVC_Handler
 *   // #define xPortPendSVHandler  PendSV_Handler
 *   // #define xPortSysTickHandler SysTick_Handler
 *
 *   // 4. Port-specific security flags (CM33_NTZ only)
 *   // #define configRUN_FREERTOS_SECURE_ONLY  0
 *   // #define configENABLE_TRUSTZONE          0
 *   // #define configENABLE_MPU                0
 *   // #define configENABLE_FPU                0
 *
 *   #endif
 *
 * Board-specific values that MUST be set before including this file:
 *   configCPU_CLOCK_HZ    — CPU frequency in Hz
 *   configTOTAL_HEAP_SIZE — heap budget in bytes (size to available SRAM)
 */

#ifndef FREERTOS_CONFIG_BASE_H
#define FREERTOS_CONFIG_BASE_H

/* ── Defaults — override before including this file ──────────────────────── */

#ifndef configCPU_CLOCK_HZ
#  error "Define configCPU_CLOCK_HZ before including FreeRTOSConfig_base.h"
#endif

#ifndef configTOTAL_HEAP_SIZE
#  error "Define configTOTAL_HEAP_SIZE before including FreeRTOSConfig_base.h"
#endif

/* ── Tick ─────────────────────────────────────────────────────────────────── */
#ifndef configUSE_16_BIT_TICKS
#  define configUSE_16_BIT_TICKS   0          /* 32-bit tick counter */
#endif
#define configTICK_RATE_HZ         1000UL     /* 1 ms tick */

/* ── Scheduler ────────────────────────────────────────────────────────────── */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* ── Task limits ─────────────────────────────────────────────────────────── */
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128   /* words */
#define configMAX_TASK_NAME_LEN                 12

/* ── Features ────────────────────────────────────────────────────────────── */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/* ── Stats / trace (off) ─────────────────────────────────────────────────── */
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0
#define configCHECK_FOR_STACK_OVERFLOW          0

/* ── Memory allocation ───────────────────────────────────────────────────── */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* ── Cortex-M interrupt priority (4 priority bits) ──────────────────────── */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 4))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 4))

/* ── API inclusions ──────────────────────────────────────────────────────── */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetSchedulerState          1

#endif /* FREERTOS_CONFIG_BASE_H */
