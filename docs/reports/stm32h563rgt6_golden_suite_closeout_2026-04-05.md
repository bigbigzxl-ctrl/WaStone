# STM32H563RGT6 Golden Suite — Closeout Report

**Date:** 2026-04-05  
**Pack:** `packs/stm32h563rgt6_golden.json`  
**Run window:** 2026-04-05 03:53 → 03:59 UTC  
**Result: 46/46 PASS**

---

## Final Golden Suite (46 tests)

### Connectivity / Loopback (6)

| Test | Result |
|------|--------|
| `gpio_loopback` | PASS |
| `uart_loopback` | PASS |
| `exti_trigger` | PASS |
| `spi_loopback` | PASS |
| `pwm_capture` | PASS |
| `i2c_loopback` | PASS |

### Analog (3)

| Test | Result |
|------|--------|
| `adc_loopback` | PASS |
| `dac_adc` | PASS |
| `adc_temp_mailbox` | PASS |

### Core / Runtime (3)

| Test | Result |
|------|--------|
| `minimal_runtime_mailbox` | PASS |
| `fpu_mailbox` | PASS |
| `mpu_mailbox` | PASS |

### DMA / Cache / Memory (5)

| Test | Result |
|------|--------|
| `dma_m2m_mailbox` | PASS |
| `gpdma2_m2m_mailbox` | PASS |
| `icache_mailbox` | PASS |
| `dcache_mailbox` | PASS |
| `ramcfg_mailbox` | PASS |

### Timers (5)

| Test | Result |
|------|--------|
| `timer_mailbox` | PASS |
| `lptim_mailbox` | PASS |
| `lptim_multi_mailbox` | PASS |
| `lptim2_mailbox` | PASS |
| `tim151617_mailbox` | PASS |

### Crypto / Math Accelerators (4)

| Test | Result |
|------|--------|
| `crc_mailbox` | PASS |
| `hash_mailbox` | PASS |
| `cordic_mailbox` | PASS |
| `fmac_mailbox` | PASS |

### ID / RNG / Sensor (3)

| Test | Result |
|------|--------|
| `uid_mailbox` | PASS |
| `rng_mailbox` | PASS |
| `dts_mailbox` | PASS |

### Communication Peripherals (7)

| Test | Result |
|------|--------|
| `lpuart_mailbox` | PASS |
| `fdcan_mailbox` | PASS |
| `usb_drd_mailbox` | PASS |
| `i3c1_mailbox` | PASS |
| `cec_mailbox` | PASS |
| `ucpd1_mailbox` | PASS |
| `crs_mailbox` | PASS |

### System / Security (7)

| Test | Result |
|------|--------|
| `sbs_mailbox` | PASS |
| `sau_mailbox` | PASS |
| `dbgmcu_mailbox` | PASS |
| `dwt_mailbox` | PASS |
| `flash_optsr_mailbox` | PASS |
| `bkpsram_mailbox` | PASS |
| `vrefbuf_mailbox` | PASS |

### RTC / Tamper / Watchdog (3)

| Test | Result |
|------|--------|
| `rtc_mailbox` | PASS |
| `tamp_mailbox` | PASS |
| `wwdg_mailbox` | PASS |

---

## Excluded Tests (not in golden suite)

| Test | Reason |
|------|--------|
| `pka_mailbox` | PKA SRAM ECC hardware issue — see `stm32h563rgt6_pka_mailbox_investigation_2026-04-05.md` |
| `flash_scratch_mailbox` | Transient flash failures in pipeline; previously passed 13×. Excluded pending stability confirmation |
| `sai_mailbox` | Not required (SAI not in scope) |

---

## Peripheral Coverage

The golden suite covers the following STM32H563 peripherals:

GPIO, USART/UART, LPUART, SPI, I2C, I3C, EXTI, TIM (basic + advanced), LPTIM, PWM capture,
ADC, DAC, DTS, DMA (GPDMA1 + GPDMA2), ICache, DCache, RAMCFG, CRC, HASH, CORDIC, FMAC,
FPU, MPU, SAU, RNG, UID, USB DRD FS, FDCAN, CEC, UCPD, CRS, SBS, VREFBUF, DBGMCU, DWT,
RTC, TAMP, WWDG, BKPSRAM, Flash option bytes, PKA (suspended)

---

## Civilization Engine Audit

查询了什么：`stm32h563rgt6`, `HIGH_PRIORITY`  
命中了什么：无直接相关  
是否复用：否（golden suite 验证，无需复用 pattern）  
新增记录：无  
升级资产：无
