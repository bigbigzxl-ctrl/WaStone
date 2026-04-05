/*
 * FreeRTOS configuration for STM32F407VGT6 (F4 Discovery)
 * 16 MHz HSI, Cortex-M4, ARM_CM3 port (no FPU context save needed)
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Tick type */
#define configUSE_16_BIT_TICKS                  0   /* 32-bit tick counter */

/* Scheduler */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Clock — 16 MHz HSI */
#define configCPU_CLOCK_HZ                      16000000UL
#define configTICK_RATE_HZ                      1000UL   /* 1 ms tick */

/* Task limits */
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128      /* words */
#define configTOTAL_HEAP_SIZE                   (20 * 1024)
#define configMAX_TASK_NAME_LEN                 12

/* Features */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/* Stats / trace (off) */
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0
#define configCHECK_FOR_STACK_OVERFLOW          0

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* Cortex-M interrupt priority (4 priority bits on M4) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 4))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 4))

/* API inclusions */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetSchedulerState          1

/* Map ARM_CM3 port handler names to ST startup vector table symbols */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
