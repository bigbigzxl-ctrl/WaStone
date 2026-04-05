# FreeRTOS UART Bring-up Template

Use this template when adding a new MCU to the FreeRTOS UART pilot series.
Reference: `stm32f103rct6_freertos_uart` (pilot #1), `stm32h563rgt6_freertos_uart` (pilot #2).
CE checklist: `docs/freertos_cortexm_bringup_checklist.md`

---

## Step 1 — Fill in board parameters

| Parameter | Value for your board |
|-----------|---------------------|
| `BOARD_ID` | e.g. `stm32h563rgt6` |
| `MCU_FAMILY` | e.g. `stm32h5` |
| `CPU_ARCH` | `cortex-m3` / `cortex-m4` / `cortex-m33` |
| `FREERTOS_PORT_DIR` | `portable/GCC/ARM_CM3` or `ARM_CM4F` or `ARM_CM33_NTZ/non_secure` |
| `CPU_CLOCK_HZ` | clock after reset (HSI default) |
| `USART_BASE` | e.g. `0x40013800` for USART1 |
| `USART_TX_PIN` | e.g. `PA9` |
| `USART_AF` | alternate function number (check reference manual) |
| `BRR_VALUE` | `floor(f_CLK / (16 × 115200)) << 4 | round(frac × 16)` |
| `UART_PORT_ON_HOST` | `/dev/ttyACM0` or `/dev/ttyUSB0` |
| `INSTRUMENT_TYPE` | `daplink` / `stlink` / `esp32jtag` |

---

## Step 2 — Create firmware files

### `firmware/targets/<BOARD_ID>_freertos_uart/FreeRTOSConfig.h`

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_16_BIT_TICKS              0
#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configUSE_MALLOC_FAILED_HOOK        0
#define configUSE_DAEMON_TASK_STARTUP_HOOK  0

#define configCPU_CLOCK_HZ      <CPU_CLOCK_HZ>UL
#define configTICK_RATE_HZ      1000UL

#define configMAX_PRIORITIES        5
#define configMINIMAL_STACK_SIZE    128   /* words */
#define configTOTAL_HEAP_SIZE       (16 * 1024)
#define configMAX_TASK_NAME_LEN     12

#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     0
#define configUSE_COUNTING_SEMAPHORES   0
#define configUSE_TIMERS                0
#define configUSE_TASK_NOTIFICATIONS    1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1

#define configUSE_TRACE_FACILITY            0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configGENERATE_RUN_TIME_STATS       0
#define configCHECK_FOR_STACK_OVERFLOW      0

#define configSUPPORT_STATIC_ALLOCATION     0
#define configSUPPORT_DYNAMIC_ALLOCATION    1

/* Interrupt priority — adjust <PRIORITY_BITS> per MCU (STM32F1=4, STM32H5=4) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - <PRIORITY_BITS>))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - <PRIORITY_BITS>))

#define INCLUDE_vTaskDelay          1
#define INCLUDE_vTaskDelete         1
#define INCLUDE_vTaskSuspend        1
#define INCLUDE_xTaskGetSchedulerState 1

/* ARM_CM3 port only — remove if using ARM_CM33_NTZ */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
```

> **ARM_CM33_NTZ note:** remove the three `#define` lines at the bottom — that port
> already uses the correct names internally.

### `firmware/targets/<BOARD_ID>_freertos_uart/main.c`

```c
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* --- peripheral register macros (fill in from RM) --- */
/* ... */
#define USART_BRR_115200   <BRR_VALUE>u

static void usart_init(void) { /* enable clocks, GPIO AF, BRR, CR1=TE|UE */ }
static void usart_putc(char c) { while (!(SR & TXE_BIT)) {} DR = c; }
static void usart_puts(const char *s) { while (*s) usart_putc(*s++); }

static void task_a(void *arg) {
    (void)arg;
    for (;;) {
        usart_puts("<BOARD_UPPER>_FREERTOS_A TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
static void task_b(void *arg) {
    (void)arg;
    for (;;) {
        usart_puts("<BOARD_UPPER>_FREERTOS_B TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

/* Required for all Cortex-M bare-metal: SYSRESETREQ on fault */
void HardFault_Handler(void) {
    volatile uint32_t *aircr = (volatile uint32_t *)0xE000ED0Cu;
    *aircr = 0x05FA0004u;
    while (1) {}
}

int main(void) {
    usart_init();
    /* banner intentionally NOT in expect_patterns: printed during flash settle */
    usart_puts("<BOARD_UPPER>_FREERTOS SCHEDULER STARTED\r\n");
    xTaskCreate(task_a, "TaskA", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(task_b, "TaskB", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    vTaskStartScheduler();
    for (;;) {}
}
```

### `firmware/targets/<BOARD_ID>_freertos_uart/Makefile`

```makefile
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF   ?= $(BUILD_DIR)/<BOARD_ID>_freertos_uart.elf
OUT_BIN   ?= $(BUILD_DIR)/<BOARD_ID>_freertos_uart.bin

FRTDIR = ../../third_party/FreeRTOS-Kernel
PORT   = $(FRTDIR)/<FREERTOS_PORT_DIR>

CFLAGS = -mcpu=<CPU_ARCH> -mthumb -O0 -g3 \
         -ffreestanding -fdata-sections -ffunction-sections \
         -Wall -Wextra \
         -I$(FRTDIR)/include -I$(PORT) -I.

LDFLAGS = -T <LINKER_SCRIPT> \
          -Wl,--gc-sections -nostartfiles -nostdlib \
          -specs=nano.specs -lc

OBJS = \
    $(BUILD_DIR)/startup.o \
    $(BUILD_DIR)/main.o \
    $(BUILD_DIR)/frt_tasks.o \
    $(BUILD_DIR)/frt_list.o \
    $(BUILD_DIR)/frt_queue.o \
    $(BUILD_DIR)/frt_port.o \
    $(BUILD_DIR)/frt_heap4.o

# ARM_CM33_NTZ needs portasm.o too:
# OBJS += $(BUILD_DIR)/frt_portasm.o
```

---

## Step 3 — Create test plan

```json
{
  "schema_version": "1.0",
  "test_kind": "observe_uart",
  "name": "<BOARD_ID>_freertos_uart",
  "board": "<BOARD_ID>",
  "supported_instruments": ["<INSTRUMENT_TYPE>"],
  "requires": { "mailbox": false, "datacapture": false },
  "labels": ["uart_observed", "freertos", "rtos"],
  "covers": ["usart", "uart_tx", "freertos_scheduler", "multitask"],
  "preflight": { "enabled": false },
  "build": {
    "project_dir": "firmware/targets/<BOARD_ID>_freertos_uart",
    "artifact_stem": "<BOARD_ID>_freertos_uart",
    "build_dir": "artifacts/build_<BOARD_ID>_freertos_uart"
  },
  "signal_checks": [],
  "observe_uart": {
    "enabled": true,
    "port": "<UART_PORT_ON_HOST>",
    "baud": 115200,
    "profile": "stm32",
    "duration_s": 8,
    "expect_patterns": [
      "<BOARD_UPPER>_FREERTOS_A TICK",
      "<BOARD_UPPER>_FREERTOS_B TICK"
    ]
  },
  "bench_setup": {
    "serial_console": { "port": "<UART_PORT_ON_HOST>", "baud": 115200, "required": true },
    "peripheral_signals": [
      { "role": "uart_tx", "dut_signal": "<TX_PIN>", "direction": "output" },
      { "role": "uart_rx", "dut_signal": "<RX_PIN>", "direction": "input" }
    ]
  }
}
```

---

## Step 4 — Verify and run

```bash
# Confirm handler symbols are strong (T not W):
arm-none-eabi-nm artifacts/build_<BOARD_ID>_freertos_uart/*.elf \
  | grep -E "SVC_Handler|PendSV_Handler|SysTick_Handler"
# Expected: T SVC_Handler   T PendSV_Handler   T SysTick_Handler

# Run AEL closed loop:
python3 -m ael run --test tests/plans/<BOARD_ID>_freertos_uart.json
# Expected: result=pass
```

---

## What varies per MCU family

| Item | Cortex-M3 (STM32F1) | Cortex-M33 (STM32H5) |
|------|--------------------|-----------------------|
| FreeRTOS port | `ARM_CM3` | `ARM_CM33_NTZ/non_secure` |
| Handler mapping macros | Required in FreeRTOSConfig.h | Not needed (port uses correct names) |
| Extra port source | none | `portasm.c` |
| USART ISR register | `USART1_SR` | `USART1_ISR` |
| USART data register | `USART1_DR` | `USART1_TDR` |
| BRR @ 115200 | `0x45` (8 MHz HSI) | `0x22B` (64 MHz HSI) |
| GPIO alternate function config | `CRH` register | `MODER` + `AFRH` |
| `-mcpu` flag | `cortex-m3` | `cortex-m33` |
