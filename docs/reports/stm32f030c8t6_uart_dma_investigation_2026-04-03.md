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
- `DMA_CH2_LATE`
- `DMA_CH4_LATE`
- `DMA_CH2_IRQ`
- `DMA_CH4_IRQ`
- `DMA_CH2_IRQ_9600`

where:

- `CH2` = default official F030 mapping
- `CH4` = `SYSCFG_CFGR1_USART1TX_DMA_RMP` remap path
- `KICK` = software writes the first byte to `TDR` before enabling DMA for the remainder
- `LATE` = DMA channel is armed before `DMAT` is asserted
- `IRQ` = DMA channel interrupt plus `USART1` interrupt path, aligned more closely
  with the ST example note that UART IRQ must be enabled
- `IRQ_9600` = same IRQ path, but with `PA9/PA10` set to pull-up, high-speed,
  and `USART1` moved to `9600 baud` to mirror the ST example more closely

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

The RX narrowing pass was later expanded with these variants:

- `DMA_RX_CH3`
- `DMA_RX_CH5`
- `DMA_RX_CH3_LATE`
- `DMA_RX_CH5_LATE`
- `DMA_RX_CH3_IRQ`
- `DMA_RX_CH5_IRQ`

where:

- `CH3` = default official F030 RX mapping
- `CH5` = `SYSCFG_CFGR1_USART1RX_DMA_RMP` remap path
- `LATE` = DMA channel is armed before `DMAR` is asserted
- `IRQ` = DMA channel interrupt path enabled to test whether RX completion only
  becomes visible when the shared DMA IRQ is active

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

### 0c. DMA1 memory-to-memory control check

To separate "DMA engine broken" from "USART DMA request broken", a plain DMA1
memory-to-memory self-test was added:

- `firmware/targets/stm32f030c8t6_dma_m2m/main.c`
- `tests/plans/stm32f030c8t6_dma_m2m.json`

Representative run:

- `runs/2026-04-03_15-46-36_stm32f030c8t6_daplink_uart_stm32f030c8t6_dma_m2m`

Result:

- `PASS`

What this proves:

- `DMA1` clocking is fine
- `DMA1 Channel1` can execute a real transfer
- memory-to-memory mode works on this board

Therefore the current fault domain is narrower than "DMA1 broken":

- `DMA1` itself works
- ordinary `USART1` UART also works
- but `USART1 DMA request generation / routing` is still not functioning in the
  current UART DMA implementations

Representative diagnostic run:

- `runs/2026-04-03_15-38-42_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`

Observed repeated UART diagnostics:

- `DMA_CH2 code=0000D402 dma_isr=00000000 cndtr=00000026 ccr=00000091 usart_isr=00600090 cfgr1=00000000`
- `DMA_CH2_KICK code=0000D402 dma_isr=00000000 cndtr=00000025 ccr=00000091 usart_isr=006000D0 cfgr1=00000000`
- `DMA_CH4 code=0000D402 dma_isr=00000000 cndtr=00000026 ccr=00000091 usart_isr=00600090 cfgr1=00000200`
- `DMA_CH4_KICK code=0000D402 dma_isr=00000000 cndtr=00000025 ccr=00000091 usart_isr=006000D0 cfgr1=00000200`
- `DMA_CH2_LATE code=0000D402 dma_isr=00000000 cndtr=00000026 ccr=00000091 usart_isr=00600090 cfgr1=00000000`
- `DMA_CH4_LATE code=0000D402 dma_isr=00000000 cndtr=00000026 ccr=00000091 usart_isr=00600090 cfgr1=00000200`
- `DMA_CH2_IRQ code=0000D406 dma_isr=00000000 cndtr=00000026 ccr=0000009B usart_isr=00600090 cfgr1=00000000`
- `DMA_CH4_IRQ code=0000D406 dma_isr=00000000 cndtr=00000026 ccr=0000009B usart_isr=00600090 cfgr1=00000200`
- `DMA_CH2_IRQ_9600 code=0000D406 dma_isr=00000000 cndtr=00000026 ccr=0000009B usart_isr=00600090 cfgr1=00000000`

What these lines prove:

- normal polling UART TX is healthy on the DAPLink bridge
- `DMAT=1`
- both the default and remapped TX channel variants were armed
- `CCR.EN|DIR|MINC` were set (`0x91`)
- `CNDTR` never decremented
- `DMA_ISR` never raised `TCIF` or `TEIF`
- even with DMA interrupt enable + `USART1` interrupt enabled, no shared DMA IRQ
  ever fired (`0xD406`)
- matching the ST example's `9600 baud` and GPIO pull-up/high-speed style did
  not change the result

That means the DMA request path to `USART1 TX` still did not start in any tested
TX-only variant.

Representative manual proof run for the IRQ-enabled TX variants:

- manual flash of `firmware/targets/stm32f030c8t6_uart_dma_observed/build/stm32f030c8t6_uart_dma_observed_app.elf`
- serial capture in `/tmp/stm32f030_uart_dma_irq_diag.log`

This manual path was needed because the current AEL runner selected
`board.build.project_dir = firmware/targets/stm32f030c8t6` instead of the
test-local `build.project_dir`, causing a false `PASS` on the base blink target.
That runner issue is separate from the UART DMA problem.

### 0d. RX-only remap + IRQ narrowing pass

After TX IRQ still showed no DMA activity, the RX observed target was expanded
to cover:

- default `Channel3`
- remapped `Channel5`
- late `DMAR`
- DMA interrupt enabled on both channel groups

Representative manual proof run:

- manual flash of `artifacts/build_stm32f030c8t6_uart_dma_rx_observed/stm32f030c8t6_uart_dma_rx_observed_app.elf`
- serial capture in `/tmp/stm32f030_uart_dma_rx_irq_diag.log`

Representative observed lines:

- `DMA_RX_CH3 code=0000D513 dma_isr=00000000 cndtr=0000000B ccr=00000081 usart_isr=006000D0 cfgr1=00000000`
- `DMA_RX_CH5 code=0000D513 dma_isr=00000000 cndtr=0000000B ccr=00000081 usart_isr=006000F8 cfgr1=00000400`
- `DMA_RX_CH3_LATE code=0000D513 dma_isr=00000000 cndtr=0000000B ccr=00000081 usart_isr=006000F0 cfgr1=00000000`
- `DMA_RX_CH5_LATE code=0000D513 dma_isr=00000000 cndtr=0000000B ccr=00000081 usart_isr=006000F8 cfgr1=00000400`
- `DMA_RX_CH3_IRQ code=0000D517 dma_isr=00000000 cndtr=0000000B ccr=0000008B usart_isr=006000F0 cfgr1=00000000`
- `DMA_RX_CH5_IRQ code=0000D517 dma_isr=00000000 cndtr=0000000B ccr=0000008B usart_isr=006000F0 cfgr1=00000400`

What these lines add:

- RX remap to `Channel5` also does not start
- late `DMAR` assertion does not start RX DMA either
- enabling DMA channel interrupts does not produce a DMA interrupt
- `DMA_ISR` remains `0`
- `CNDTR` remains unchanged at `0x0B`

So the "maybe RX only needs IRQ/remap timing" hypothesis is also eliminated.

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

7. IRQ-enabled TX completion path.

- enabled `DMA_CCR_TCIE | DMA_CCR_TEIE`
- enabled the shared DMA NVIC line
- enabled `USART1_IRQn`
- after DMA TC, switched to `USART1 TCIE` completion handling

This still produced no DMA interrupt at all on either the default or remapped
TX path.

8. RX remap + IRQ completion path.

- tested both `Channel3` and remapped `Channel5`
- tested both early and late `DMAR` assertion
- enabled shared DMA NVIC lines for RX completion

This still produced no DMA interrupt and no receive count change.

9. ST-style minimal HAL TX-only proof.

To separate "our bare-metal sequence is wrong" from "DMA TX cannot work on this
fixture", a one-off minimal HAL proof was built in `/tmp/stm32f030_hal_dma_tx_probe`
using:

- ST official `stm32f0xx-hal-driver`
- ST official `cmsis-device-f0`
- `USART1`
- `DMA1 Channel2`
- `9600 baud`
- `PA9 -> DAPLink RX`

Observed host UART output:

- `AEL_HAL_UART_DMA_BEGIN`
- `AEL_HAL_UART_DMA_TX`
- `AEL_HAL_UART_DMA_OK`

Live debugger state after completion:

- `g_tx_done = 1`
- `g_tx_error = 0`
- `hdma_tx.Instance->CNDTR = 0`
- `USART1->ISR = 0x6000D0`

What this proves:

- `USART1 TX DMA` does work on this exact MCU and fixture
- DAPLink UART RX does observe the DMA-emitted frame
- the remaining bug is in the repo's bare-metal UART DMA implementation, not in
  the hardware path and not in ST's supported configuration model

10. Bare-metal one-shot `HALSEQ` probe.

To test whether the remaining gap was only ordering, a one-shot bare-metal probe
was added:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_halseq_probe/main.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_halseq_probe/Makefile)

This probe does only one path:

- configure `USART1` at `9600`
- arm `DMA1 Channel2` with `TCIE|TEIE`
- enable the DMA channel first
- clear `TCF`
- assert `DMAT`
- on DMA TC, disable `DMAT` and switch to `USART1 TCIE`
- explicitly wait for `USART1 TC=1` after the diagnostic preamble before
  starting DMA, to match the HAL proof's blocking transmit behavior more closely

Observed host UART output:

- `AEL_HALSEQ_BEGIN`
- `AEL_HALSEQ_FAIL dma_tc=00000000 dma_te=00000000 uart_tc=00000000 dma_isr=00000000 cndtr=00000013 ccr=0000009B usart_isr=00600010`

What this adds:

- a HAL-like ordering alone is not sufficient
- waiting for the previous polling transmit to reach `TC=1` still does not make
  bare-metal DMA start
- even the one-shot bare-metal `Channel2 + IRQ + 9600 + clear-TCF-before-DMAT`
  path still produces no DMA activity
- the missing ingredient is therefore deeper than simple high-level ordering

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

4. The bare-metal sequence still differs from the working ST HAL path in at
   least one material way, even though the broad outline appears similar.

5. That missing difference is not eliminated by converting the bare-metal path
   into a one-shot HAL-like `Channel2 + IRQ + 9600` sequence.

What the evidence does **not** support:

- generic UART wiring failure
- generic DAPLink UART failure
- generic `USART1` enable failure
- missing DMA engine clock or DMA controller failure
- "only TX is broken" as the sole explanation
- "only RX is broken" as the sole explanation
- "STM32F030C8T6 cannot do USART1 TX DMA on this bench"

Those paths were already proven independently by:

- `stm32f030c8t6_uart_tx_probe`
- `stm32f030c8t6_uart_multibyte_observed`
- `stm32f030c8t6_uart_banner`
- `stm32f030c8t6_dma_m2m`
- one-off `/tmp/stm32f030_hal_dma_tx_probe` HAL proof
- repo-local `stm32f030c8t6_uart_dma_halseq_probe` still failing

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

That fallback has now been exercised, and the answer is:

- the deeper bench/device path is valid
- the remaining problem is our bare-metal sequence

## Bottom Line

As of this report:

- `UART DMA` work exists in the repo
- the work is documented and restartable
- the feature is not validated
- the feature is not part of any current suite or pack
- TX-only DAPLink UART diagnostics now clearly show the immediate failure is on
  `USART1 <-> DMA` request generation, not on the ordinary UART wiring path
- DMA1 memory-to-memory pass shows the DMA engine itself is healthy
- a one-off ST HAL TX-only proof now shows that `USART1 TX DMA` does work on
  this exact bench, so the unresolved issue is narrowed to the repo's
  bare-metal implementation

## ST HAL RX DMA Proof

A matching ST HAL receive-side reference target now also exists in the repo:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_hal_observed/main.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_hal_observed/Makefile)
- [provenance.md](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_hal_observed/provenance.md)
- [stm32f030c8t6_uart_dma_rx_hal_observed.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma_rx_hal_observed.json)

Implementation notes:

- reuses the vendored ST F0 HAL and CMSIS device sources already formalized for
  the successful TX-side HAL reference target
- configures `USART1` on `PA9/PA10`
- configures `DMA1_Channel3` for `USART1_RX`
- starts `HAL_UART_Receive_DMA()` for the fixed frame `PING_DMA_RX_HAL`
- reports the result back over ordinary polling UART TX on `PA9`

Live validation was done on the current DAPLink UART wiring:

- `DAPLink RX -> PA9`
- `DAPLink TX -> PA10`
- `GND common`

Observed transcript:

```text
AEL_HAL_UART_DMA_RX_READY
AEL_HAL_UART_DMA_RX_OK
```

Important observation detail:

- to capture the full interaction reliably, the host serial port needed to be
  opened before issuing `monitor reset run`
- if the host observer attaches too late, the early `READY` banner can be
  missed and the run can look like a false no-output failure

What this proves:

- `USART1 RX DMA` also works on this exact `STM32F030C8T6 + DAPLink UART`
  bench when driven through the ST HAL path
- ordinary UART TX status reporting still works in the same target while RX DMA
  is active
- the remaining unresolved problem is therefore even narrower: the repo's
  bare-metal `USART1 <-> DMA` sequence, not the hardware bench and not the ST
  HAL configuration path

## Bare-Metal TX Observed Breakthrough

The deferred bare-metal TX-side observed target is now passing again:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/main.c)
- [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/startup.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/Makefile)
- [stm32f030c8t6_uart_dma_observed.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma_observed.json)

Validated run:

- `2026-04-03_17-04-47_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_observed`

The repaired target now proves:

- bare-metal `USART1 TX DMA` on `DMA1 Channel2`
- DAPLink host observation of the actual DMA-transmitted payload on `PA9`
- mailbox pass only after DMA transfer complete and final UART `TC`

Observed output on the host included:

```text
AEL_UART_DMA_DIAG_BEGIN
AEL_UART_DMA_TX_BARE
AEL_UART_DMA_OK
```

The key fix was not in the DMA request path itself. The main blocking bug was
startup:

- this target's `_etext` landed at an odd address (`0x08000729`)
- the inherited reset code copied `.data` using `uint32_t *`
- on Cortex-M0 that produced an unaligned access during reset and dropped into
  `HardFault`

That is why earlier runs looked like "no UART readiness" rather than a clean DMA
failure.

The concrete repair was:

- add a target-local [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_observed/startup.c)
  that still calls `SystemInit()` but copies `.data` bytewise instead of
  wordwise
- keep using ST's `system_stm32f0xx.c`
- simplify the target to a single `TX-only` bare-metal proof path
- keep emitting `AEL_UART_DMA_DIAG_BEGIN` in the steady-state success loop so
  late-attaching UART observers still see the expected readiness banner

What this changes in the overall interpretation:

- bare-metal `USART1 TX DMA` is now proven on this bench too
- the original broad "bare-metal UART DMA path is broken" conclusion was too
  wide
- the remaining unresolved area is now narrower again: receive-side bare-metal
  DMA and any combined TX/RX bare-metal test shape

## Bare-Metal RX Observed Breakthrough

The deferred bare-metal RX-side observed target is now passing as well:

- [main.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/main.c)
- [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/startup.c)
- [Makefile](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/Makefile)
- [stm32f030c8t6_uart_dma_rx_observed.json](/home/ali/work/ai-embedded-lab/tests/plans/stm32f030c8t6_uart_dma_rx_observed.json)

Validated run:

- `2026-04-03_17-08-45_stm32f030c8t6_daplink_uart_stm32f030c8t6_uart_dma_rx_observed`

The repaired target now proves:

- bare-metal `USART1 RX DMA` on `DMA1 Channel3`
- host-driven payload on `DAPLink TX -> PA10`
- mailbox pass only after DMA transfer complete and exact payload match

The same startup issue applied here too:

- the original target still inherited the old word-copy reset code
- the new target now uses a local byte-copy [startup.c](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_rx_observed/startup.c)
  plus ST's `system_stm32f0xx.c`

The receive-side proof was also simplified down to one direct path:

- `USART1_RDR -> DMA1_Channel3 -> rx_buf`
- no remap variants
- no mixed TX/RX DMA coupling
- repeated `AEL_UART_DMA_RX_READY` so late host observers still sync correctly

What this means now:

- ST HAL `TX DMA`: proven
- ST HAL `RX DMA`: proven
- bare-metal `TX DMA observed`: proven
- bare-metal `RX DMA observed`: proven

The original DMA investigation is therefore resolved for the split observed
paths. Any remaining future work would be about whether to add a combined
bare-metal TX+RX test shape, not whether this MCU and bench can perform UART DMA.
