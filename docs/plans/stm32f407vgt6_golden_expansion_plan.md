# STM32F407VGT6 Golden Suite Expansion Plan

**目标：** 从当前 7 个测试扩展到 ~21 个，与 VET6 Golden Suite 覆盖率对齐
**板子：** STM32F407 Discovery（onboard ST-Link）
**参考：** VET6 Golden Suite (`packs/stm32f407vet6_golden.json`) — 21/21 PASS 2026-04-01

---

## 当前状态

| 已有（7 个）| 缺少（14 个）|
|------------|------------|
| stm32f407_mailbox | stm32f407_adc_temp |
| stm32f407_timer_mailbox | stm32f407_i2c_loopback |
| stm32f407_gpio_loopback | stm32f407_pwm_capture |
| stm32f407_uart_loopback | stm32f407_dma_m2m |
| stm32f407_spi_loopback | stm32f407_crc |
| stm32f407_exti_trigger | stm32f407_rng |
| stm32f407_adc_loopback | stm32f407_dac_adc |
| | stm32f407_fpu |
| | stm32f407_rtc |
| | stm32f407_uart_dma |
| | stm32f407_spi_dma |
| | stm32f407_can_loopback |
| | stm32f407_tim2_32bit |
| | stm32f407_flash_rw |
| | stm32f407_dac_dma |

---

## VGT6 vs VET6 关键差异

| 项目 | VET6 (custom) | VGT6 (Discovery) |
|------|--------------|-----------------|
| Flash | 512 KB (sectors 0–7) | **1 MB (sectors 0–11)** |
| Flash test sector | Sector 7 @ `0x08060000` | **Sector 11 @ `0x080E0000`** |
| Instrument | ESP32JTAG (SWD over WiFi) | **ST-Link (onboard USB)** |
| `skip_attach` in test plans | `false` | **`true`** |
| GDB sequence | `monitor a → attach 1 → load → attach 1 → detach` | **`load → monitor reset run → disconnect`** (st-util halts after load) |
| Mailbox addr | `0x2001FC00` | `0x2001FC00` (同) |
| SRAM | 192 KB | 192 KB (同) |
| Core | Cortex-M4 @ 16 MHz HSI | Cortex-M4 @ 16 MHz HSI (同) |

> **重要：** GDB 序列不同。ST-Link (st-util) 在 `load` 后目标保持 halted，必须加 `monitor reset run` 才能让固件运行。参见 memory: `reference_stlink_gdb_load_resume.md`。

---

## 固件复用策略

大部分 VET6 固件可以**直接复用**（同一颗 Cortex-M4，外设地址相同）。
已存在于 `firmware/targets/stm32f407_*/` 的固件：

| 目标 | 固件路径 | 能否直接用 |
|------|---------|----------|
| adc_temp | `firmware/targets/stm32f407_adc_temp/` | ✅ 直接用 |
| i2c_loopback | `firmware/targets/stm32f407_i2c_loopback/` | ✅ 直接用 |
| pwm_capture | `firmware/targets/stm32f407_pwm_capture/` | ✅ 直接用 |
| dma_m2m | `firmware/targets/stm32f407_dma_m2m/` | ✅ 直接用 |
| crc | `firmware/targets/stm32f407_crc/` | ✅ 直接用 |
| rng | `firmware/targets/stm32f407_rng/` | ✅ 直接用 |
| dac_adc | `firmware/targets/stm32f407_dac_adc/` | ✅ 直接用 |
| fpu | `firmware/targets/stm32f407_fpu/` | ✅ 直接用 |
| rtc | `firmware/targets/stm32f407_rtc/` | ✅ 直接用 |
| uart_dma | `firmware/targets/stm32f407_uart_dma/` | ✅ 直接用 |
| spi_dma | `firmware/targets/stm32f407_spi_dma/` | ✅ 直接用 |
| can_loopback | `firmware/targets/stm32f407_can_loopback/` | ✅ 直接用 |
| tim2_32bit | `firmware/targets/stm32f407_tim2_32bit/` | ✅ 直接用 |
| dac_dma | `firmware/targets/stm32f407_dac_dma/` | ✅ 直接用 |
| flash_rw | `firmware/targets/stm32f407vet6_flash_rw/` | ⚠️ **需新固件** — sector 11 (VGT6 1MB) vs sector 7 (VET6 512KB) |

> 只有 **flash_rw** 需要写新固件，其余 14 个只需新建 test plan JSON 即可。

---

## 接线清单

**已有线（不变）：**

| 信号 | 连接 | 测试 |
|------|------|------|
| GPIO loopback | PB0 → PB1 | gpio_loopback |
| UART loopback | PD5 → PD6 | uart_loopback, uart_dma |
| SPI loopback | PB15 → PB14 (MOSI→MISO) | spi_loopback, spi_dma |
| ADC external | PA5 → 电位计或固定电压 | adc_loopback |
| EXTI trigger | PC0 → 跳线/触发信号 | exti_trigger |

**需新增的线：**

| 信号 | 连接 | 测试 |
|------|------|------|
| I2C loopback | PB6↔PB10 (SCL), PB7↔PB11 (SDA) | i2c_loopback |
| PWM capture | PB8 → PB9 (TIM4 CH3→CH4) | pwm_capture |
| DAC/ADC | PA4（自回环，无需外部线） | dac_adc, dac_dma |

**无需接线：**
adc_temp, dma_m2m, crc, rng, fpu, rtc, can_loopback（LBKM内部），tim2_32bit, flash_rw

---

## 执行步骤

### Step 1：确认固件存在并能编译

```bash
# 抽查几个 targets
cd firmware/targets/stm32f407_uart_dma && make clean && make
cd firmware/targets/stm32f407_fpu     && make clean && make
cd firmware/targets/stm32f407_rtc     && make clean && make
```

如果 Makefile 引用 `stm32f407vet6_blink/stm32f407vet6.ld`，linker script 对 VGT6 仍然有效（VGT6 的前 512KB 布局与 VET6 相同，mailbox 在 SRAM 里不受影响）。

### Step 2：为 flash_rw 写新固件

复制 VET6 flash_rw 固件，修改一处：

```c
// VET6: Sector 7, 512KB board
#define FLASH_SECTOR   7u
#define FLASH_ADDR     0x08060000u

// VGT6: Sector 11, 1MB board  ← 改这两行
#define FLASH_SECTOR  11u
#define FLASH_ADDR     0x080E0000u
```

目标目录：`firmware/targets/stm32f407vgt6_flash_rw/`

### Step 3：为每个缺失测试创建 test plan JSON

模板（以 uart_dma 为例）：

```json
{
  "schema_version": "1.0",
  "test_kind": "baremetal_mailbox",
  "name": "stm32f407_uart_dma",
  "board": "stm32f407_discovery",
  "supported_instruments": ["stlink", "esp32jtag"],
  "covers": ["uart", "dma"],
  "build": {
    "project_dir": "firmware/targets/stm32f407_uart_dma",
    "artifact_stem": "stm32f407_uart_dma",
    "build_dir": "artifacts/build_stm32f407_uart_dma"
  },
  "mailbox_verify": { "settle_s": 5.0, "addr": "0x2001FC00", "skip_attach": true },
  "bench_setup": {
    "notes": "USART2 DMA loopback. PD5→PD6 wire required.",
    "peripheral_signals": [...]
  }
}
```

关键点：`"skip_attach": true`（ST-Link），`"board": "stm32f407_discovery"`。

### Step 4：更新 golden pack

编辑 `packs/stm32f407vgt6_golden.json`：
- 在 `stages.3` 下添加 14 个新测试
- 更新 `description`
- 保持 `"status": "golden"`（等跑完验证后更新 `verified_date`）

### Step 5：运行 pack

```bash
python3 -m ael pack --board stm32f407_discovery --pack packs/stm32f407vgt6_golden.json
```

### Step 6：验证并升级

21/21 PASS 后：
- 更新 `verified_date` 为实际日期
- 记录到 CE：`ExperienceAPI.add(...)` + `run_index.record_success()`
- 在 README 新增里程碑

---

## Civilization Engine 查询（开始新 session 前必做）

```python
# Step 1: 查 VGT6 历史
ExperienceAPI.query(keyword='stm32f407vgt6', domain='engineering')
ExperienceAPI.query(keyword='stm32f407_discovery', domain='engineering')

# Step 2: 查 HIGH_PRIORITY patterns
ExperienceAPI.query(keyword='HIGH_PRIORITY', domain='engineering')

# Step 3: 查 ST-Link GDB 序列
ExperienceAPI.query(keyword='stlink', domain='engineering')
ExperienceAPI.query(keyword='monitor reset run', domain='engineering')
```

必须命中并复用的 patterns：
- `3f13ca66` — HardFault_Handler (全 ARM Cortex-M 强制)
- `ef195d1e` → 已升级为 `3f13ca66`
- Memory `reference_stlink_gdb_load_resume.md` — ST-Link GDB 序列

---

## 已知陷阱

| 陷阱 | 说明 |
|------|------|
| ST-Link halts after load | 必须 `monitor reset run` 才能让固件跑；否则 mailbox 永远不会写入 |
| Flash sector | VGT6 用 sector 11，不是 sector 7；写错会擦错地方或写保护失败 |
| PA9/PA10 | Discovery 上这两个引脚接到 ST-Link UART bridge；UART 测试必须用 PD5/PD6 (USART2) |
| RNG PLL | 需要 PLL48CLK=48MHz；VGT6 @ 16MHz HSI 同样需要配 PLLM=16/PLLN=192/PLLQ=4 |
| I2C SWRST | I2C 初始化前必须 assert CR1.SWRST=1 再清除（pattern `db885cac`） |
| TIM PSC | 写 PSC 后必须 EGR.UG=1（pattern `27de4499`） |
| CAN filter | LBKM=1 内部回环；仍需配置至少一个 RX filter，否则 FIFO 永远为空 |
| HardFault_Handler | **所有固件必须有**（pattern `3f13ca66`）；否则 LOCKUP 导致整个 pack 级联失败 |

---

*Plan created: 2026-04-01*
*Reference: VET6 Golden Suite report — `reports/stm32f407vet6_golden_suite_report.md`*
