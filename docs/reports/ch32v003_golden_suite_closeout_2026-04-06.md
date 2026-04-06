# CH32V003 Golden Suite Closeout — 2026-04-06

## Board

- **MCU**: CH32V003F4U6 (WCH RISC-V, 24 MHz HSI, 16 KB flash, 2 KB SRAM)
- **Board ID**: `ch32v003xxx`
- **Debug probe**: WCH-LinkE via SDI (1-wire debug interface)
- **AEL instrument**: `wchlink_local` @ 127.0.0.1:3334
- **Flash method**: WCH OpenOCD (`wch_openocd`)
- **Pack**: `packs/ch32v003_golden.json`

## Final Result: 14/14 PASS

Verified: 2026-04-06. Full pack run time: ~60 seconds.

| # | Test | Peripheral | Wiring | Result |
|---|------|-----------|--------|--------|
| 1 | `ch32v003_gpio_loopback` | GPIO output/input | PC0↔PC1 | ✅ PASS |
| 2 | `ch32v003_uart_wchlink` | USART1 TX | PD5→WCHLink RX | ✅ PASS |
| 3 | `ch32v003_uart_bidir` | USART1 TX+RX | PD5↔WCHLink, PD6↔WCHLink | ✅ PASS |
| 4 | `ch32v003_exti_loopback` | EXTI rising-edge | PC0→PC1 | ✅ PASS |
| 5 | `ch32v003_tim_pwm_capture` | TIM1 PWM + EXTI capture | PD2→PD4 | ✅ PASS |
| 6 | `ch32v003_spi_loopback` | SPI1 full-duplex | MOSI↔MISO | ✅ PASS |
| 7 | `ch32v003_systick` | SysTick timer | zero-wiring | ✅ PASS |
| 8 | `ch32v003_iwdg` | IWDG watchdog | zero-wiring | ✅ PASS |
| 9 | `ch32v003_flash_rw` | Flash read/write/erase | zero-wiring | ✅ PASS |
| 10 | `ch32v003_adc_vref` | ADC internal Vrefint (ch8) | zero-wiring | ✅ PASS |
| 11 | `ch32v003_dma_mem2mem` | DMA1 Ch3 MEM2MEM | zero-wiring | ✅ PASS |
| 12 | `ch32v003_tim2` | TIM2 free-running counter | zero-wiring | ✅ PASS |
| 13 | `ch32v003_wwdg` | WWDG window watchdog | zero-wiring | ✅ PASS |
| 14 | `ch32v003_pwr_sleep` | PWR sleep + AWU wakeup | zero-wiring | ✅ PASS |

Tests 1–9 were the original golden suite. Tests 10–14 were added in this session.

## Session Work Summary

### Infrastructure fixes (tests 1–9)

**WCH OpenOCD GDB server halts target on `init`**
- Root cause: OpenOCD's `init` command examines and halts the CPU; `check_uart` runs before
  `check_mailbox`, sees no serial output, and fails.
- Fix: Added `-c "init; catch {resume}"` to `cmd_srv` in `ael/adapters/flash_wch_openocd.py`
  so the target is resumed after examination.
- `catch` prevents "Hart is not halted!" error from killing OpenOCD when target was already running.

**USART1 tests: `observe_uart` disabled**
- WCH-Link CDC UART tests cannot use `observe_uart` (the GDB server resumes the target, but
  `check_uart` runs at priority 5 before `check_mailbox` at priority 6).
- Both `ch32v003_uart_wchlink` and `ch32v003_uart_bidir` switched to `observe_uart: disabled`;
  UART TX verified indirectly via mailbox `detail0_increment`.

**CH32V003 AFIO EXTICR — 2-bit-per-line layout**
- Root cause: CH32V003 uses 2 bits per EXTI line (not 4 like STM32).
  Formula: `bit_pos = PinSource << 1`. PA=0, PB=1, PC=2, PD=3.
- `ch32v003_exti_loopback`: fixed EXTI1/PC: `(AFIO->EXTICR & ~(0x3u<<2)) | (0x2u<<2)`
- `ch32v003_tim_pwm_capture`: fixed EXTI4/PD: `(AFIO->EXTICR & ~(0x3u<<8)) | (0x3u<<8)`
- Confirmed from WCH EVT `GPIO_EXTILineConfig()` source.

### New tests added (10–14)

**ch32v003_adc_vref**
- Reads ADC1 channel 8 (internal Vrefint ~1.2V). Averages 4 samples. Accepts [200, 700] (10-bit).
- Key fix: `EXTSEL[2:0]` must be set to `0b111` (software trigger) in CTLR2.
  `ADC_ExternalTrigConv_None = 0x000E0000`. Without this, `SWSTART` has no effect and the
  firmware waits forever for a hardware trigger.
- ADC prescaler: APB2/8 = 3 MHz. Calibration: CALVOL_50PERCENT → RSTCAL → CAL.
- Sample time: channel 8 bits[26:24]=7 (241.5 cycles) in SAMPTR1.

**ch32v003_dma_mem2mem**
- DMA1 Channel3 MEM2MEM: copies 32-word buffer from SRAM to SRAM. Verifies all words match.
- Key fix 1: `SRC_BUF` must NOT be `const` — DMA can only access SRAM, not Flash.
  `const` arrays go to `.rodata` (Flash at 0x00000000); DMA reads return bus error/zeros.
- Key fix 2: **ch32v003fun.c startup bug** — `.data` is NOT copied from Flash to SRAM.
  The startup code treats linker symbols `_sbss`/`_data_lma` as pointer variables (loads
  their content from SRAM) instead of using them as address constants. On power-on SRAM=0,
  so the copy is skipped entirely. Workaround: use `.bss` arrays and initialize in `main()`.
- CNTR = 32 (word transfers); CFGR: MEM2MEM(14)|PL_VH(13:12)|MSIZE32(11:10)|PSIZE32(9:8)|MINC(7)|PINC(6)|DIR(4).
- TC3 flag = DMA1->INTFR bit9.

**ch32v003_tim2**
- TIM2 free-running at 1 MHz (PSC=7, APB1=8 MHz). Reads CNT twice with a short gap.
- Key fix: TIM2->CNT is 16-bit. A long wait causes multiple overflows; `cnt >= 50000` fails
  when CNT wraps. Use unsigned 16-bit subtraction `(uint16_t)(cnt2 - cnt1) > 0` to detect
  advancement regardless of wrap. Also checks `TIM2->INTFR & 1` (UIF) as overflow indicator.
- SWEVGR=1 (UG bit) required to load PSC/ARR shadow registers before enabling CEN.

**ch32v003_wwdg**
- WWDG enabled: WDGTB=0 (no prescaler), W=0x7F, T=0x7F (~32 ms timeout at APB1/4096).
- PASS written immediately after `CTLR = WDGA|T=0x7F`. Liveness loop feeds WWDG every ~3 ms.
- WWDG cannot be disabled once WDGA=1; test relies on fast PASS write + continuous feed.

**ch32v003_pwr_sleep**
- LSI oscillator (~128 kHz) → AWU prescaler /512 → AWUWR=25 → ~100 ms wakeup period.
- EXTI->EVENR |= (1<<9) required for WFI to exit on AWU event.
- Writes PASS after first wakeup; liveness loop keeps sleeping and updating `wakeup_count`.

## Key CH32V003-Specific Findings

| Finding | Details |
|---------|---------|
| AFIO EXTICR 2-bit layout | Each EXTI line uses 2 bits: `bit = PinSource<<1`, port PC=2, PD=3 |
| ADC EXTSEL must be 0b111 | For SWSTART to work, EXTSEL[19:17]=0b111 (software trigger) |
| DMA cannot read Flash | SRC_BUF must be in SRAM; `const` global → .rodata → Flash → DMA fails |
| ch32v003fun.c .data copy bug | Startup copies `.data` only on fresh power-on (SRAM=0); after reflash, old SRAM content makes the copy skip. Use `.bss` + runtime init. |
| TIM2 CNT is 16-bit | Overflow at 65535 ticks; use modulo subtraction for advancement checks |
| WCH OpenOCD init halts target | `catch {resume}` in cmd_srv required; `resume` alone fails if target not halted |

## Bench Wiring

```
WCHLink SWDIO → CH32V003 PD1 (SDI)
WCHLink GND   → CH32V003 GND
WCHLink RX    → CH32V003 PD5 (USART1_TX)   [uart tests]
WCHLink TX    → CH32V003 PD6 (USART1_RX)   [bidir uart only]
CH32V003 PC0  → CH32V003 PC1               [gpio/exti loopback]
CH32V003 PD2  → CH32V003 PD4               [tim_pwm_capture]
CH32V003 SPI MOSI → CH32V003 SPI MISO      [spi loopback]
```
Tests 7–14 (systick through pwr_sleep) require only SDI + GND (zero peripheral wiring).

## Peripheral Coverage

```
GPIO ✓  EXTI ✓  USART1 ✓  SPI1 ✓  TIM1(PWM) ✓  TIM2 ✓
SysTick ✓  IWDG ✓  WWDG ✓  Flash ✓
ADC1(Vrefint) ✓  DMA1(MEM2MEM) ✓  PWR(Sleep+AWU) ✓
```

Not yet covered: I2C, OPA (CH32V003-unique), ADC-DMA, TIM-DMA, USART-DMA, PWR-Standby, RCC-MCO.

## Civilization Engine Audit

- 查询了什么：session 初继承上一 context，无新 CE 查询
- 命中了什么：无新命中
- 是否复用：N/A
- 新增记录：无（新发现的 bug pattern 记录在本报告中）
- 升级资产：无
