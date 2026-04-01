# STM32F407VET6 Golden Test Suite — Validation Report

**Date:** 2026-04-01
**Board:** STM32F407VET6 (custom board, 512 KB Flash, Cortex-M4 @ 16 MHz HSI)
**Instrument:** ESP32JTAG @ 192.168.2.98:4242 (SWD over WiFi, BMDA)
**Pack:** `packs/stm32f407vet6_golden.json`
**Result:** ✅ **21/21 PASS**

---

## Test Results

| # | Test | Stage | Covers | Result |
|---|------|-------|--------|--------|
| 1 | stm32f407vet6_timer_mailbox | 0 | timer, cpu | PASS |
| 2 | stm32f407vet6_gpio_loopback | 1 | gpio | PASS |
| 3 | stm32f407vet6_uart_loopback | 1 | uart, usart | PASS |
| 4 | stm32f407vet6_spi_loopback | 1 | spi | PASS |
| 5 | stm32f407vet6_adc_temp | 2 | adc, temp_sensor | PASS |
| 6 | stm32f407vet6_exti_trigger | 2 | exti, gpio | PASS |
| 7 | stm32f407vet6_adc_loopback | 2 | adc, analog | PASS |
| 8 | stm32f407vet6_i2c_loopback | 3 | i2c | PASS |
| 9 | stm32f407vet6_pwm_capture | 3 | timer, pwm | PASS |
| 10 | stm32f407vet6_dma_m2m | 3 | dma | PASS |
| 11 | stm32f407vet6_crc | 3 | crc, cpu | PASS |
| 12 | stm32f407vet6_rng | 3 | rng, cpu | PASS |
| 13 | stm32f407vet6_dac_adc | 3 | dac, adc, analog | PASS |
| 14 | stm32f407vet6_fpu | 3 | fpu, cpu | PASS |
| 15 | stm32f407vet6_rtc | 3 | rtc, lsi | PASS |
| 16 | stm32f407vet6_uart_dma | 3 | uart, dma | PASS |
| 17 | stm32f407vet6_spi_dma | 3 | spi, dma | PASS |
| 18 | stm32f407vet6_can_loopback | 3 | can | PASS |
| 19 | stm32f407vet6_tim2_32bit | 3 | timer, tim2 | PASS |
| 20 | stm32f407vet6_flash_rw | 3 | flash | PASS |
| 21 | stm32f407vet6_dac_dma | 3 | dac, dma, adc, tim6 | PASS |

---

## Peripheral Coverage

| Peripheral | Test(s) | Method |
|-----------|---------|--------|
| TIM3 (basic timer) | timer_mailbox | IRQ-driven 1 Hz tick |
| GPIO | gpio_loopback | PE2↔PE3 loopback wire |
| USART2 | uart_loopback, uart_dma | PD5↔PD6 loopback wire |
| SPI2 | spi_loopback, spi_dma | PB15↔PB14 MOSI↔MISO wire |
| ADC1 | adc_temp, adc_loopback, dac_adc, dac_dma | Internal temp sensor + PA5 external |
| EXTI | exti_trigger | PC0→PC3 edge (LA-driven GPIO output) |
| I2C1/I2C2 | i2c_loopback | PB6/PB7 master ↔ PB10/PB11 slave |
| TIM4 (PWM+capture) | pwm_capture | PB8 CH3 output → PB9 CH4 input |
| DMA2 (M2M) | dma_m2m | SRAM→SRAM, 16 words |
| CRC unit | crc | SW cross-validation (poly 0x04C11DB7) |
| RNG | rng | PLL48CLK=48MHz; 32 samples, no repeats |
| DAC1 | dac_adc, dac_dma | PA4 self-loopback (DAC1_OUT=ADC1_IN4) |
| FPU (Cortex-M4) | fpu | 7 sub-tests: add/mul/div/sqrt/fma/cvt/acc |
| RTC + LSI | rtc | Internal 32kHz, PREDIV_A=127/PREDIV_S=249 |
| DMA1 (USART2) | uart_dma | S6 CH4 (TX) + S5 CH4 (RX) |
| DMA1 (SPI2) | spi_dma | S4 CH0 (TX) + S3 CH0 (RX) |
| CAN1 | can_loopback | Internal LBKM=1, no transceiver |
| TIM2 (32-bit) | tim2_32bit | ARR=0xFFFFFFFF, counter > 0xFFFF |
| Flash (internal) | flash_rw | Sector 7 erase + write + readback |
| DMA2 + TIM6 + DAC | dac_dma | Circular waveform → ADC1 verify |

---

## Bench Wiring

| Signal | Connection | Tests |
|--------|-----------|-------|
| SWD | ESP32JTAG P3 → MCU SWD | all |
| GPIO loopback | PE2 → PE3 | gpio_loopback |
| UART loopback | PD5 → PD6 | uart_loopback, uart_dma |
| SPI loopback | PB15 → PB14 (MOSI→MISO) | spi_loopback, spi_dma |
| ADC external | PA5 → ESP32JTAG DAC out | adc_loopback |
| EXTI trigger | PC0 → ESP32JTAG GPIO out | exti_trigger |
| I2C | PB6-PB7 ↔ PB10-PB11 (internal) | i2c_loopback |
| PWM capture | PB8 → PB9 (same timer, internal) | pwm_capture |
| DAC/ADC | PA4 (self-loopback, no wire) | dac_adc, dac_dma |

*No wiring needed for: timer_mailbox, adc_temp, dma_m2m, crc, rng, fpu, rtc, can_loopback, tim2_32bit, flash_rw*

---

## Deferred Tests (excluded from suite)

| Test | Reason |
|------|--------|
| stm32f407vet6_iwdg | EXCLUDED: board enters continuous IWDG-reset loop after test; blocks SWD without BOOT0 recovery |
| stm32f407vet6_tim2_pwm | LA frequency verification; TIM2_CLK measured at 42 MHz — APB1 prescaler artifact under investigation |
| stm32f407vet6_dma_toggle | DMA TEIF error — root cause unresolved |

---

## Key Engineering Notes

1. **HardFault_Handler mandatory**: All bare-metal firmware MUST define `HardFault_Handler` with `SYSRESETREQ` (AIRCR = 0x05FA0004). Without it, GDB load from stale PC → LOCKUP → SWD death cascades across entire pack run.

2. **DMA2 for M2M**: DMA1 cannot do memory-to-memory on F407. Use DMA2 for any M2M transfers.

3. **RNG requires PLL**: RNG clock source is PLL48CLK. At 16MHz HSI, configure PLLM=16, PLLN=192, PLLQ=4 for 48MHz.

4. **TIM prescaler double-buffering**: Always write `EGR.UG=1` after setting `PSC` to apply the new value immediately.

5. **I2C stuck BUSY**: Assert `I2C_CR1.SWRST=1` then clear before configuring I2C. Prevents stuck bus from previous GDB session.

6. **CAN loopback**: `BTR.LBKM=1` enables internal loopback; no external transceiver required. Must configure ≥1 RX filter or FIFO will remain empty.

7. **Flash sector choice**: VET6 has 512KB (sectors 0-7). Sector 7 (0x08060000, 128KB) is the last sector and safe for erase/program tests.

8. **GDB command sequence** (immutable, all STM32):
   ```
   monitor a → attach 1 → load → attach 1 → detach
   ```

---

## Civilization Engine Reference

- Golden run record: `0e8931af-d3b4-4de5-bb1a-7e7047c75fc3`
- HardFault pattern: `ef195d1e`
- I2C SWRST pattern: `db885cac`
- TIM PSC EGR pattern: `27de4499`
- BMDA flash pattern: `77469dc5`
