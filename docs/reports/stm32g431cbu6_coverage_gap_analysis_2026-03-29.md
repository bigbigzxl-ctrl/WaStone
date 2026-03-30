# STM32G431CBU6 Golden Suite — Coverage Gap Analysis

**Date:** 2026-03-29
**Pack:** `packs/stm32g431cbu6_golden.json`
**Current State:** 17 tests, 4 stages, status=golden

---

## 1. 当前覆盖地图

| 外设 / 功能 | 测试名 | Stage | 接线 |
|------------|--------|-------|------|
| GPIO 输出签名 | `stm32g431_gpio_signature` | 3 | PA2→LA P0.3 |
| GPIO 输入回环 | `stm32g431_gpio_loopback` | 3 | PA8↔PA6 |
| USART1 polling 回环 | `stm32g431_uart_loopback` | 2 | PA9↔PA10 |
| USART1 DMA 回环 | `stm32g431_uart_dma` | 2 | PA9↔PA10, DMA1 CH4/5 + DMAMUX1 |
| SPI1 MISO↔MOSI 回环 | `stm32g431_spi` | 2 | PB4↔PB5 |
| TIM3 IRQ 计数 | `stm32g431_timer_mailbox` | 1 | 无 |
| TIM1 PWM 输出 | `stm32g431_pwm` | 3 | PA8→PA6（捕获用），PA2→LA |
| TIM1 输入捕获 | `stm32g431_capture` | 2 | PA4→LA P0.2 |
| ADC1 内部温度传感器 | `stm32g431_internal_temp_mailbox` | 1 | 无 |
| ADC1 外部通道 | `stm32g431_adc` | 3 | PB1→PB0 |
| ADC2（DAC readback） | `stm32g431_dac_mailbox` | 3 | 无（PA4 内部路由） |
| DAC1_OUT1 | `stm32g431_dac_mailbox` | 3 | 无（→ADC2_IN17 内部） |
| IWDG + LSI | `stm32g431_iwdg` | 2 | 无 |
| EXTI 外部中断 | `stm32g431_exti` | 2 | PA8↔PA6 |
| FDCAN1 内部回环 (Layer 1) | `stm32g431_fdcan_loopback` | 2 | 无 |
| DMA1 + DMAMUX1 | `stm32g431_uart_dma` | 2 | 含于 UART DMA 测试 |
| 综合接线验证 | `stm32g431_wiring_verify` | 2 | 全部跳线 |
| Blinky 目视 | `stm32g431_blinky_visual` | 0 | 无 |
| Minimal Runtime mailbox | `stm32g431_minimal_runtime_mailbox` | 0 | 无 |

**已使用 bench pin：** PA2, PA3, PA4, PA6, PA8, PA9, PA10, PB0, PB1, PB3, PB4, PB5

---

## 2. 空缺分析

### 2A. 无需新接线（no-wire）— 可立即实现

| 外设 | G4 特有 | 说明 | 优先级 |
|------|---------|------|--------|
| **CORDIC** | ✓ | 硬件三角函数加速器（cos/sin/phase/modulus）。G4 系列独有，其他 STM32 家族无此外设。测试：写 CORDIC_WDATA1，读 CORDIC_RDATA1，与软件参考值比较。 | ★★★ |
| **FMAC** | ✓ | 滤波器数学加速器（FIR/IIR/BIQUAD）。G4 系列独有。测试：配置 FIR，写入已知数据，验证输出。 | ★★★ |
| **WWDG** | - | 窗口看门狗。与 IWDG 互补：WWDG 要求在时间窗口内喂狗，过早/过晚均复位。测试：在窗口内喂狗 → PASS，验证超窗口行为（可用 mailbox 序列）。 | ★★ |
| **RTC + LSE/LSI** | - | RTC 秒计数器。使用 LSI（无外部晶振需求）。测试：初始化 RTC → 等待 WUTF 唤醒标志 → 验证计数递增。 | ★★ |
| **CRC 单元** | - | 硬件 CRC32（Poly=0x04C11DB7）。测试：写入已知字节序列，读 CRC_DR，与软件参考值比较。1 页数据即可。 | ★★ |
| **Flash 页读写** | - | 内置 Flash 编程（页擦 + 字写入 + 读回）。使用末尾保留页（远离代码区）。 | ★ |
| **低功耗唤醒** | - | STOP2 模式进入 + RTC/EXTI 唤醒。验证 MCU 可正常休眠并从指定源唤醒。 | ★ |

**推荐首选：CORDIC + FMAC**——这是 STM32G4 相对于 G0/G0x/F4 的核心差异化硬件，不覆盖等于遗漏了 G4 的标志性能力。

### 2B. 需少量新接线

| 外设 | 所需接线 | 可用 pin | 说明 | 优先级 |
|------|---------|---------|------|--------|
| **FDCAN1 Layer 2**（引脚回环） | PA12↔PA11（1 根线） | 均空闲 | 在片外引脚级验证 FDCAN TX/RX 物理路径。无需 CAN transceiver（3.3V TTL 直连）。延续 Layer 1 分层测试逻辑。 | ★★★ |
| **I2C loopback** | PB8/PB9↔PB10/PB11（2 根线） | 均空闲 | I2C1 master (PB8/PB9) ↔ I2C2 slave (PB10/PB11) 同片回环。G431 上 I2C 完全未覆盖。 | ★★ |
| **DAC1_OUT2** | PA5（单 pin，ADC1 内部 readback） | PA5 空闲 | DAC1 第二通道验证。PA5=DAC1_OUT2=ADC1_IN12，可内部路由，无外部接线。 | ★ |
| **USART2 或 USART3** | 需 TX↔RX 跳线 | PB6/PB7 等空闲 | 增加第二路 UART 覆盖。bench 已有 PA9↔PA10 USART1，新增 USART2/3 意义有限，除非项目需求。 | ☆ |
| **SPI2 / SPI3** | 需 MISO↔MOSI 跳线 | PB12-15 等空闲 | SPI1 已覆盖，再加 SPI2/3 收益递减。 | ☆ |

### 2C. 复杂 / 选做

| 外设 | 原因 |
|------|------|
| USB Full-Speed | 需要 host 侧驱动，bench 侧需 USB hub 或 PC 枚举，配置复杂 |
| OPAMP (OPA1-3) | 需要模拟电路支持（分压、负载），bench 不具备 |
| COMP (比较器) | 需要外部精确参考电压 |
| DMA2 | G431 DMA2 仅 2 通道，覆盖意义低于已有 DMA1（6 通道），重复性高 |
| ADC2 独立外部通道 | ADC2 已通过 DAC readback 覆盖，再加外部接线收益有限 |

---

## 3. 空闲 Pin 资源表

| Pin | AF / 功能 | 状态 |
|-----|----------|------|
| PA5 | DAC1_OUT2 / ADC1_IN12 | 空闲 |
| PA7 | TIM1_CH1N / SPI1_MOSI | 空闲 |
| PA11 | FDCAN1_RX | 空闲 |
| PA12 | FDCAN1_TX | 空闲 |
| PA15 | TIM2_CH1 / SPI1_NSS | 空闲 |
| PB6 | USART2_TX / TIM4_CH1 | 空闲 |
| PB7 | USART2_RX / TIM4_CH2 | 空闲 |
| PB8 | I2C1_SCL / TIM4_CH3 | 空闲 |
| PB9 | I2C1_SDA / TIM4_CH4 | 空闲 |
| PB10 | I2C2_SCL / USART3_TX | 空闲 |
| PB11 | I2C2_SDA / USART3_RX | 空闲 |
| PB12–PB15 | SPI2 / TIM1 / others | 空闲 |
| PC13–PC15 | GPIO / RTC | 空闲 |
| PF0–PF1 | OSC / GPIO | 空闲 |

---

## 4. 推荐下一步路线图

### Phase A — no-wire，加入 Stage 1（或新 Stage 4）

```
stm32g431_cordic     — CORDIC cos 计算验证（G4 独有）
stm32g431_fmac       — FMAC FIR 滤波验证（G4 独有）
stm32g431_crc        — CRC32 硬件校验
stm32g431_wwdg       — 窗口看门狗（IWDG 的补充）
```

### Phase B — 加 3 根线，补全通信覆盖

```
PA12↔PA11:  stm32g431_fdcan_pin_loopback  — FDCAN Layer 2
PB8↔PB10,
PB9↔PB11:  stm32g431_i2c_loopback        — I2C master/slave
```

### Phase C — 选做

```
stm32g431_rtc        — RTC + LSI 秒计数
stm32g431_flash_rw   — Flash 页擦写
stm32g431_dac_out2   — DAC1_OUT2 (PA5) readback via ADC1
```

---

## 5. Civilization Engine Audit

**查询了什么：** `stm32g431cbu6`, `CORDIC`, `FMAC`, `coverage`
**命中了什么：** 无直接命中（首次 G431 CORDIC/FMAC 分析）
**是否复用：** 否（新分析）
**新增记录：** 无（分析报告，不记录新 EE 条目）
**升级资产：** 无
