# STM32F030C8T6 UART DMA Investigation

**Date:** 2026-04-03
**MCU:** `STM32F030C8T6`
**Primary bench:** local DAPLink
**Status:** deferred investigation, not part of any validated suite or pack

## Purpose

This document captures the current `UART DMA` work for `STM32F030C8T6` so the
next session can restart from a concrete baseline instead of re-deriving the
same failed paths.

This work is intentionally **not** part of the validated Golden Suite and is
also **not** part of the validated DAPLink UART observed side-pack.

## Code Locations

Local loopback attempt on canonical `PA9 <-> PA10` wiring:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma/main.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma/Makefile)
- [stm32f030c8t6_uart_dma.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma.json)

DAPLink-host-observed TX DMA attempt on `PA9 -> DAPLink RX`, `DAPLink TX -> PA10`:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/main.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/Makefile)
- [stm32f030c8t6_uart_dma_observed.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma_observed.json)

These files are committed for traceability, but they are currently WIP and are
not promoted into any suite.

## References Used

Repo reference that already works on another MCU:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f103rct6_uart_dma/main.c)
- [stm32f103rct6_uart_dma.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f103rct6_uart_dma.json)

ST official F0 reference used for alignment:

- `/tmp/STM32CubeF0/Projects/STM32F0308-Discovery/Examples/UART/UART_TwoBoards_ComDMA/Src/main.c`
- `/tmp/STM32CubeF0/Projects/STM32F0308-Discovery/Examples/UART/UART_TwoBoards_ComDMA/Src/stm32f0xx_hal_msp.c`
- `/tmp/STM32CubeF0/Projects/STM32F0308-Discovery/Examples/UART/UART_TwoBoards_ComDMA/Inc/main.h`

Useful peripheral-definition clue found in a public `stm32f030x8.h` mirror:

- `SYSCFG_CFGR1_USART1TX_DMA_RMP`
- `SYSCFG_CFGR1_USART1RX_DMA_RMP`

This mattered because it suggested that `USART1` DMA routing on `STM32F030x8`
may depend on `SYSCFG CFGR1` remap bits, not only on the default channel table.

## What Was Tried

### 0. TX-only host-observed narrowing pass

After the initial mixed mailbox / DMA investigation, the work was narrowed again
to the exact bench path the user requested:

- keep the DAPLink UART wiring
- ignore DUT RX for the first pass
- debug only `USART1 TX DMA`
- let DAPLink RX observe the output

To make that practical, `firmware/targets/stm32f030c8t6_uart_dma_observed/main.c`
was turned into a TX-only diagnostic firmware that:

- uses ordinary polling UART for diagnostic text
- tries four TX DMA variants in sequence
- repeats the results forever on the DAPLink UART bridge

The four variants are:

- `DMA_CH2`
- `DMA_CH2_KICK`
- `DMA_CH4`
- `DMA_CH4_KICK`

where:

- `CH2` = default official F030 mapping
- `CH4` = `SYSCFG_CFGR1_USART1TX_DMA_RMP` remap path
- `KICK` = software writes the first byte to `TDR` before enabling DMA for the remainder

This narrowed pass answered the key isolation question:

- the immediate blocker is on the TX DMA side, not the RX side

### 0b. RX-only host-stimulated narrowing pass

After the TX-only pass still showed no DMA movement, a matching RX-only probe was
added on the same DAPLink UART wiring:

- host sends `PING_DMA_RX` over `DAPLink TX -> PA10`
- firmware arms `DMA1 Channel3` on `USART1_RDR`
- firmware keeps reporting readiness over ordinary UART TX on `PA9`

Code and plan:

- `firmware/targets/stm32f030c8t6_uart_dma_rx_observed/main.c`
- `tests/plans/stm32f030c8t6_uart_dma_rx_observed.json`

Representative run:

- `runs/2026-04-03_15-42-09_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_rx_observed`

Observed UART output:

- repeated `AEL_UART_DMA_RX_READY`
- no `AEL_UART_DMA_RX_OK`

Live register snapshot after the failed host exchange:

- `USART1_CR1 = 0x0000000D`
- `USART1_CR3 = 0x00000040`
- `USART1_ISR = 0x006000F8`
- `SYSCFG_CFGR1 = 0x00000000`
- `DMA1_ISR = 0x00000081`
- `DMA1_CCR3 = 0x00000081`
- `DMA1_CNDTR3 = 0x0000000B`

Interpretation:

- ordinary UART host-to-DUT traffic is present at the fixture level
- DMA RX channel was armed
- receive count did not decrease
- no valid `Channel3` completion state was observed

So the narrowing result after checking both sides is:

- TX-only DMA did not start
- RX-only DMA did not start
- the current blocker is not a simple "TX-only" or "RX-only" bug
- it is the `USART1 <-> DMA` request path itself on this STM32F030C8T6 setup

Representative diagnostic run:

- `runs/2026-04-03_15-38-42_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`

Observed repeated UART diagnostics:

- `DMA_CH2 code=0000D402 dma_isr=00000000 cndtr=00000026 ccr=00000091 usart_isr=00600090 cfgr1=00000000`
- `DMA_CH2_KICK code=0000D402 dma_isr=00000000 cndtr=00000025 ccr=00000091 usart_isr=006000D0 cfgr1=00000000`
- `DMA_CH4 code=0000D402 dma_isr=00000000 cndtr=00000026 ccr=00000091 usart_isr=00600090 cfgr1=00000200`
- `DMA_CH4_KICK code=0000D402 dma_isr=00000000 cndtr=00000025 ccr=00000091 usart_isr=006000D0 cfgr1=00000200`

What these lines prove:

- normal polling UART TX is healthy on the DAPLink bridge
- `DMAT=1`
- both the default and remapped TX channel variants were armed
- `CCR.EN|DIR|MINC` were set (`0x91`)
- `CNDTR` never decremented
- `DMA_ISR` never raised `TCIF` or `TEIF`

That means the DMA request path to `USART1 TX` still did not start in any tested
TX-only variant.

### 1. Local loopback baseline

First attempt used the canonical local UART contract:

- `PA9 <-> PA10`
- `USART1`
- DMA TX on `DMA1 Channel2`
- polling RX path for proof

Goal:

- prove that `DMA1` drives `USART1_TDR`
- receive the same bytes back through the local loopback
- pass mailbox after exact match

Representative local-loopback failures ended in mailbox errors such as:

- `0xE172`
- `0xE174`

These encoded the fact that RX never completed and/or DMA transfer-complete
never arrived.

### 2. DAPLink host-observed TX DMA proof

Because plain host-observed UART TX already worked on this bench, a second
attempt narrowed scope to:

- `PA9 -> DAPLink RX`
- `DAPLink TX -> PA10`
- only prove TX over DMA
- let the host UART observer verify the emitted text

Expected line:

- `AEL_UART_DMA A1 B2 C3 D4 55 66 77 88`

Representative failed runs:

- `runs/2026-04-03_14-19-36_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`
- `runs/2026-04-03_14-32-31_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`
- `runs/2026-04-03_14-34-39_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`
- `runs/2026-04-03_14-37-30_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`

Common failure shape:

- `failure_class = uart_expected_patterns_missing`
- host observed `0` bytes in the clean failure cases
- mailbox showed `error_code = 0xD402`

`0xD402` in the current observed firmware means:

- DMA transfer-complete flag never asserted
- transfer-error flag never asserted
- software timed out waiting for DMA completion

## Specific Technical Variants Attempted

The following variants were implemented and exercised:

1. Default F030 mapping based on the official `UART_TwoBoards_ComDMA` example.

- `USART1`
- `PA9/PA10`
- `DMA1 Channel2` for TX
- `DMA1 Channel3` for RX in the broader official model

2. TX-DMA-only observed proof modeled after the repo's successful F103 host
   observed DMA TX test.

3. RAM-backed TX buffer instead of `const` flash-backed data.

This was used to eliminate the possibility that the problem was caused by DMA
reading from flash in this bare-metal setup.

4. Manual first-byte kick, then DMA for the remaining bytes.

This was used to test whether this exact F030 path needed an initial `TXE`
transition driven by software before DMA requests would begin.

5. `SYSCFG_CFGR1` remap experiment.

`SYSCFG_CFGR1_USART1TX_DMA_RMP` was set and TX was moved to `DMA1 Channel4`
to test the possibility that the default `Channel2` routing was not valid for
this exact device/configuration.

None of the above produced a passing DMA TX proof.

6. TX-only multi-variant diagnostic repeater on the DAPLink bridge.

This variant used normal UART text output to report DMA register state after
each attempted TX path, so the DMA experiment could be debugged without relying
on mailbox for the primary proof.

This did not produce a successful TX DMA transfer, but it did produce the most
conclusive isolation so far: TX DMA itself is the current failure point.

### Startup / linker trap discovered during the narrowing pass

One additional repo-local issue surfaced during this work:

- if the DMA diagnostic firmware used initialized RAM data (`.data`), the board
  could fault in `Reset_Handler` before reaching `main`

Reason:

- current `startup.c` copies `.data` using `uint32_t *`
- this firmware's `.data` load address landed on an unaligned flash address
- Cortex-M0 unaligned word access in the startup copy path triggered `HardFault`

Workaround used for the current diagnostic firmware:

- avoid initialized `.data`
- keep the DMA diagnostic frame in flash (`const`) instead

This is a separate startup/linker correctness issue, not the UART DMA request
issue itself.

## Live Register Findings

The most useful live readout came from attaching with `gdb-multiarch` after a
failing host-observed run and reading mailbox and peripheral registers.

Representative state after failure:

- mailbox magic present
- mailbox status = FAIL
- mailbox error = `0xD402`
- `USART1_CR1 = 0x0000000D`
- `USART1_CR3 = 0x00000080`
- `USART1_ISR` had `TXE=1` and `TC=1`
- DMA channel `CCR` showed enabled + memory increment + direction
- DMA `CNDTR` stayed unchanged
- DMA `ISR` never reported `TCIF` or `TEIF`

What this means:

- `USART1` was enabled
- `DMAT=1`
- transmitter idle state was healthy enough to report `TXE=1`
- software did arm the DMA channel
- but the DMA engine never consumed the transfer count

That is the key observation: the failure is not “USART1 dead” and not “wrong
baud only”. It is specifically that the expected DMA request path never started
moving data.

## Current Best Interpretation

The current evidence points to one of these unresolved causes:

1. The exact `STM32F030C8T6` DMA request routing still is not configured
   correctly for bare-metal register use.

2. The register sequence still misses a subtle precondition that the HAL/LL
   stack handles implicitly.

3. The DMA remap hypothesis is only partially correct, and the attempted
   `Channel2` and `Channel4` variants still do not match the true request path
   for this device.

What the evidence does **not** support:

- generic UART wiring failure
- generic DAPLink UART failure
- generic `USART1` enable failure

Those paths were already proven independently by:

- `stm32f030c8t6_uart_tx_probe`
- `stm32f030c8t6_uart_multibyte_observed`
- `stm32f030c8t6_uart_banner`

## Why This Is Deferred

The validated coverage already includes:

- canonical local-loopback Golden Suite
- validated DAPLink UART observed side-pack

`UART DMA` is the only remaining unstable UART feature in this setup. It is
therefore better kept as an explicit deferred item than silently included in a
pack that overclaims coverage.

## Recommended Restart Plan For Next Session

If work resumes on `UART DMA`, start here:

1. Re-read the current observed DMA target:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/main.c)

2. Compare it again against:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f103rct6_uart_dma/main.c)
- `/tmp/STM32CubeF0/Projects/STM32F0308-Discovery/Examples/UART/UART_TwoBoards_ComDMA/Src/main.c`
- `/tmp/STM32CubeF0/Projects/STM32F0308-Discovery/Examples/UART/UART_TwoBoards_ComDMA/Src/stm32f0xx_hal_msp.c`

3. Verify the true DMA request mapping from the exact `STM32F030x8`
   peripheral-definition source before changing code again.

4. Prefer a minimal proof that uses:

- one-shot TX only
- host-observed DAPLink UART
- explicit register dumps after failure

5. If bare-metal register work still stalls, try one controlled fallback:

- temporarily port the smallest possible official HAL/LL USART1 DMA TX proof to
  this repo for `STM32F030C8T6`

That fallback would answer an important question quickly:

- “is the problem our register sequence, or is there a deeper bench/device
  mismatch?”

## Bottom Line

As of this report:

- `UART DMA` work exists in the repo
- the work is documented and restartable
- the feature is not validated
- the feature is not part of any current suite or pack
- TX-only DAPLink UART diagnostics now clearly show the immediate failure is on
  `USART1 <-> DMA` request generation, not on the ordinary UART wiring path
