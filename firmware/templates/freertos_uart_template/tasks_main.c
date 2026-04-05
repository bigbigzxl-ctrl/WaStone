/*
 * tasks_main.c — AEL FreeRTOS UART demo (shared task logic)
 *
 * Board-independent.  Depends only on bsp_uart.h (board supplies bsp_uart.c).
 *
 * Two tasks alternate printing at 500 ms / 700 ms.  AEL observe_uart verifies:
 *   expect_patterns: ["<BOARD_TAG>_FREERTOS_A TICK", "<BOARD_TAG>_FREERTOS_B TICK"]
 *
 * BOARD_TAG is defined by the board's Makefile:
 *   -DBOARD_TAG=\"STM32F103RCT6\"
 *
 * To use for a new board:
 *   1. Create firmware/targets/<board>_freertos_uart/
 *   2. Write bsp_uart.c implementing bsp_uart_init() and bsp_uart_puts()
 *   3. Write FreeRTOSConfig.h: set configCPU_CLOCK_HZ + configTOTAL_HEAP_SIZE,
 *      then #include "FreeRTOSConfig_base.h" (from firmware/templates/freertos_uart_template/)
 *   4. Write Makefile: compile tasks_main.c + bsp_uart.c + FreeRTOS kernel
 *   5. Write test plan: expect_patterns = ["<BOARD_TAG>_FREERTOS_A TICK", ...]
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_uart.h"

#ifndef BOARD_TAG
#  error "Define BOARD_TAG via compiler flag: -DBOARD_TAG=\\\"YOUR_BOARD\\\""
#endif

/* ── FreeRTOS tasks ──────────────────────────────────────────────────────── */

static void task_a(void *arg)
{
    (void)arg;
    for (;;) {
        bsp_uart_puts(BOARD_TAG "_FREERTOS_A TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_b(void *arg)
{
    (void)arg;
    for (;;) {
        bsp_uart_puts(BOARD_TAG "_FREERTOS_B TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

/* ── HardFault handler — SYSRESETREQ to avoid SWD lockup ─────────────────── */

void HardFault_Handler(void)
{
    volatile uint32_t *aircr = (volatile uint32_t *)0xE000ED0Cu;
    *aircr = 0x05FA0004u;
    while (1) {}
}

/* ── libc stub for ST ASM startup __libc_init_array ─────────────────────── */

void _init(void) {}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void)
{
    bsp_uart_init();
    bsp_uart_puts(BOARD_TAG "_FREERTOS SCHEDULER STARTED\r\n");

    xTaskCreate(task_a, "TaskA", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(task_b, "TaskB", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();
    for (;;) {}
}
