# STM32F030C8T6 UART DMA Issue And Fix

**Date:** 2026-04-03
**MCU:** `STM32F030C8T6`
**Bench:** local DAPLink with UART bridge wiring

## Purpose

This document explains the actual `UART DMA` issue that blocked progress on the
`STM32F030C8T6`, what made it confusing, how it was diagnosed, and what was
changed to fix it.

This is the short operational version of the longer investigation log:

- [stm32f030c8t6_uart_dma_investigation_2026-04-03.md](/home/ali/work/ai-embedded-lab/docs/reports/stm32f030c8t6_uart_dma_investigation_2026-04-03.md)

## Scope

The relevant bare-metal targets are:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/main.c)
- [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/startup.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/Makefile)
- [stm32f030c8t6_uart_dma_observed.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma_observed.json)
- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/main.c)
- [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/startup.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/Makefile)
- [stm32f030c8t6_uart_dma_rx_observed.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma_rx_observed.json)

The matching ST HAL reference proofs are:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_hal_observed/main.c)
- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_hal_observed/main.c)

## Original Symptom

The original observed behavior looked like a UART DMA failure:

- `UART DMA` tests would not produce the expected UART-ready text
- mailbox-based runs often looked like "DMA did not start"
- host-observed runs often looked like "no UART output" or "expected readiness patterns missing"

That made it look as if:

- `USART1 TX DMA` was broken
- `USART1 RX DMA` was broken
- or the `USART1 <-> DMA` request path on `STM32F030C8T6` was misconfigured

That interpretation was reasonable at first, but incomplete.

## Why It Was Confusing

Two separate things were happening at different times:

1. Some early UART DMA implementations really were too broad and too noisy:

- multiple remap variants
- mixed TX/RX reasoning
- mailbox plus observed UART at the same time

2. A more basic fault was hiding underneath:

- some target-specific linker layouts placed `_etext` at a non-4-byte-aligned
  address
- the inherited reset code copied `.data` using `uint32_t *`
- on Cortex-M0, that unaligned access faults before `main()`

The second problem meant the program could die before:

- UART initialization
- DMA setup
- diagnostic banner output

So several "DMA failures" were actually pre-`main()` startup faults.

## Actual Root Cause

The direct root cause of the blocking failures on the repaired bare-metal UART
DMA targets was:

- target-local `_etext` landing at a non-word-aligned address
- inherited `Reset_Handler` copying `.data` with 32-bit pointer accesses
- Cortex-M0 raising an unaligned access fault during reset

In short:

- the code never reached `main()`
- therefore it never reached UART setup
- therefore it never reached DMA setup

This was a startup/linker correctness problem, not initially a DMA peripheral
problem.

## How It Was Confirmed

The diagnosis converged in a few steps.

First, ST HAL reference targets were used to answer the hardware question:

- `ST HAL TX DMA` passed
- `ST HAL RX DMA` passed

That proved:

- the bench wiring was valid
- DAPLink UART path was valid
- `USART1 TX DMA` and `USART1 RX DMA` both work on this exact MCU and fixture

Second, the bare-metal observed TX target was reduced to one direct path and
then inspected under GDB.

That showed:

- execution had fallen into `Default_Handler`
- the program had faulted before normal UART output

Third, symbol inspection exposed the alignment clue:

- `_etext` landed at an odd address such as `0x08000729`

That made the startup bug explicit:

- byte source was not word aligned
- word-copy reset code was invalid for that image layout

## The Fix

The concrete repair was the same on both bare-metal observed DMA targets.

### 1. Add target-local startup code

Each target now has its own [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/startup.c) or [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/startup.c) that:

- still calls `SystemInit()`
- copies `.data` bytewise instead of wordwise
- zeroes `.bss` normally

This removes the unaligned-access fault during reset.

### 2. Keep ST system initialization

Both repaired targets still compile ST's:

- `system_stm32f0xx.c`

This avoids changing clock and system setup variables at the same time as the
DMA fix.

### 3. Simplify each test to one direct DMA path

Instead of carrying many remap and sequencing variants in the passing proof,
each repaired target now validates one direct hardware path:

TX side:

- `DMA1 Channel2 -> USART1_TDR`
- host observes the actual DMA-transmitted payload on `PA9 -> DAPLink RX`

RX side:

- `USART1_RDR -> DMA1 Channel3 -> rx_buf`
- host sends payload on `DAPLink TX -> PA10`

This keeps the runtime claim honest and easy to interpret.

### 4. Emit repeated readiness text

For host-observed tests, the ready banner is emitted repeatedly so that a UART
observer attaching after flash-settle still sees the expected synchronization
pattern.

This fixes the secondary runner problem where:

- target was already alive
- but the host attached too late and missed a one-shot `READY`

## What Was Fixed Versus What Was Not

What is now fixed and proven:

- ST HAL `USART1 TX DMA`
- ST HAL `USART1 RX DMA`
- bare-metal observed `USART1 TX DMA`
- bare-metal observed `USART1 RX DMA`

What this does **not** mean:

- every old failed UART DMA attempt had exactly the same root cause
- every complex mixed TX/RX or remap-heavy test shape is now automatically good

What is proven is narrower and stronger:

- this MCU and this bench can perform UART DMA
- the repaired split bare-metal observed proofs are valid

## Validated Results

The repaired bare-metal DMA proofs are now formalized in:

- [stm32f030c8t6_daplink_uart_dma_observed.json](/home/ali/work/ai-embedded-lab/packs/stm32f030c8t6_daplink_uart_dma_observed.json)

Validated pack rerun:

- `pack_runs/2026-04-03_17-13-00_stm32f030c8t6_daplink_uart_dma_observed_stm32f030c8t6_daplink_uart`

Validated individual runs:

- `runs/2026-04-03_17-13-00_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`
- `runs/2026-04-03_17-13-15_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_rx_observed`

## Practical Lesson

For small Cortex-M targets, especially Cortex-M0:

- do not assume a shared word-copy reset stub is safe for every target-local
  linker layout
- if a target shows "no UART output" before claiming peripheral failure,
  confirm it is actually reaching `main()`
- if `_etext` is not guaranteed word aligned, byte-copy `.data` in reset or
  enforce alignment explicitly in the linker layout

For host-observed UART tests:

- keep a repeated ready banner if the runner may attach after flash settle

## Bottom Line

The key `UART DMA` blocker was not fundamentally a DMA hardware failure.

The blocking issue was:

- target-specific startup code performing an invalid word copy of `.data`

That startup bug produced misleading DMA-like symptoms by killing the firmware
before it reached the UART/DMA code.

Once startup was fixed and the proofs were reduced to one clean TX path and one
clean RX path, both bare-metal UART DMA observed tests passed.
