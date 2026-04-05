# FreeRTOS Cortex-M UART Bring-up Checklist

Derived from `stm32f103rct6_freertos_uart` pilot (2026-04-05).  
CE asset: `58abbaa4` (scope=pattern, HIGH_PRIORITY).

---

## Three mandatory fixes before first boot

### 1. Interrupt handler name mapping

FreeRTOS ARM_CM3 port defines functions with non-standard names that do NOT match
the STM32 startup vector table. The scheduler will hang in `Default_Handler`
at the first `svc 0` instruction unless you remap them.

**ARM_CM3 port — requires FreeRTOSConfig.h mapping:**
```c
/* Add to FreeRTOSConfig.h */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
```

**ARM_CM33_NTZ port — names already match, NO mapping needed.**  
portasm.c directly defines `SVC_Handler`, `PendSV_Handler`; port.c defines `SysTick_Handler`.  
Startup vector table must declare them as `extern` (not hardcode `Default_Handler`).

**Diagnostic:** if the board outputs nothing after `vTaskStartScheduler()` and
GDB shows `CONTROL.SPSEL=0` (MSP, not PSP), the scheduler never started — handler
names are not wired correctly.

---

### 2. UART baud rate divisor (BRR) — use the correct formula

STM32 USART BRR with 16× oversampling:

```
USARTDIV = f_CLK / (16 × baud)
BRR = (floor(USARTDIV) << 4) | round(frac(USARTDIV) × 16)
```

| Board / Clock          | Target baud | USARTDIV | BRR value |
|------------------------|-------------|----------|-----------|
| STM32F103 @ 8 MHz HSI  | 115200      | 4.34     | `0x45`    |
| STM32H563 @ 64 MHz HSI | 115200      | 34.72    | `0x22B`   |

**Common mistake:** computing `f_CLK / baud = 69` for F103 and writing `0x0457`
(which gives 7200 baud). The DAPLink receives only `\x00` bytes.

**Diagnostic:** 7 seconds of `\x00` from `/dev/ttyACM0` with correct UART wiring =
baud rate mismatch. Verify with `arm-none-eabi-nm` that `USART1_BRR` is initialised.

---

### 3. Linking FreeRTOS under -nostdlib

`heap_4.c` calls `memset`. Bare-metal builds with `-nostdlib` have no libc, so
the linker fails with `undefined reference to 'memset'`.

**Fix — add to LDFLAGS:**
```makefile
LDFLAGS += -specs=nano.specs -lc
```

This pulls in newlib-nano, which provides `memset`/`memcpy` without the full C runtime.

---

## Minimum file set for a new FreeRTOS UART pilot

```
firmware/targets/<board>_freertos_uart/
├── main.c              # usart_init() + two tasks + HardFault_Handler (SYSRESETREQ)
├── FreeRTOSConfig.h    # board-specific: CPU_HZ, TICK_RATE, heap, priorities
│                       # + handler mapping if using ARM_CM3 port
├── startup.c           # vector table with SVC/PendSV/SysTick → FreeRTOS handlers
│   (or inherit from    # (copy from existing target, update handler references)
│    board base dir)
└── Makefile            # -mcpu=cortex-m{3|33} + FreeRTOS src paths + -specs=nano.specs -lc

tests/plans/<board>_freertos_uart.json
└── observe_uart:
    ├── port: /dev/ttyACM0 (or board-specific)
    ├── baud: 115200
    ├── duration_s: 8
    └── expect_patterns: ["<BOARD>_FREERTOS_A TICK", "<BOARD>_FREERTOS_B TICK"]
    # Do NOT include a startup banner in expect_patterns:
    # banner is printed during the 2s flash-settle window and is missed by observe.
```

---

## PASS criteria

- AEL `result=pass`
- Both TICK patterns matched in `observe_uart.log`
- `arm-none-eabi-nm` shows `T SVC_Handler`, `T PendSV_Handler`, `T SysTick_Handler`
  (all uppercase `T`, not `W` weak)
- GDB halt shows `CONTROL=0x2` (PSP active = tasks running)

---

## Common failure modes

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| No UART output; GDB PC deep in Default_Handler loop | SVC/PendSV/SysTick not wired to FreeRTOS | Add handler `#define` mapping or update startup vector table |
| UART outputs only `\x00` (hundreds of bytes) | Wrong BRR → wrong baud rate | Recalculate: `USARTDIV = f_CLK / (16 × baud)` |
| Linker: `undefined reference to memset` | heap_4.c needs libc | Add `-specs=nano.specs -lc` to LDFLAGS |
| Banner pattern missing; TICK patterns present | Banner printed during flash settle (2s) | Remove banner from `expect_patterns`; use periodic TICK only |
| GDB shows CONTROL=0x0 (MSP), no tasks switching | vTaskStartScheduler failed silently | Check heap size; assert fails silently without configASSERT |
