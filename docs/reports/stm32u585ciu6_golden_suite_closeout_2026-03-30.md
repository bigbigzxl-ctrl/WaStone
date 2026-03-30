# STM32U585CIU6 Golden Suite Closeout — 2026-03-30

## 结果

**14/14 PASS** — `packs/stm32u585ciu6_golden.json`，状态 `"golden"`，verified_date `2026-03-30`。

| 阶段 | 测试 | 结果 |
|------|------|------|
| Stage0 | blinky, minimal_runtime, uid, button_idle | 4/4 PASS |
| Stage1 | crc, rng, cordic, iwdg, timer | 5/5 PASS |
| Stage2 | gpio_loopback, uart_loopback, spi_loopback, pwm_capture, exti | 5/5 PASS |

跳过（记录在 CE）：
- ADC 相关 4 个测试：VDDA 硬件缺陷，ADC LDO 永不就绪（ADRDY timeout），两条 ADC 路径（ADC1 + ADC4）均确认
- i2c_loopback：需要物理接线 PB6↔PB10 (SCL) + PB7↔PB3 (SDA)，当前 bench 未接

---

## 新学到的东西（重点）

### 1. STM32 Timer PSC double-buffering — EGR.UG=1 必须写 `[27de4499]`

**现象：** pwm_capture 测试返回 `0xE003`（period=40000，期望 10000）。TIM3 PSC 写了 3，但 timer 实际以 PSC=0 运行。

**根因：** STM32 所有 timer 的 PSC（预分频）寄存器是 shadow-buffered（双缓冲）。写 PSC 只更新 preload 寄存器，实际生效要等 Update Event（UEV）。如果不主动触发 UEV，timer enable 后 PSC 仍为 0（÷1），时钟速率错误。

**修法：** PSC/ARR 写完之后、CR1 enable 之前，写 `EGR = 1`（offset 0x14，UG bit）强制触发 UEV，PSC 立即加载进 active 寄存器。

```c
TIM1_EGR = 1u;   /* UG: force update event to load PSC shadow */
TIM3_EGR = 1u;   /* UG: force update event to load PSC shadow */
TIM3_SR  = 0u;   /* clear UIF set by EGR write */
```

**跨板性：** 适用于所有 STM32（F1/F4/F7/H7/U5），PWM 生成和捕获 timer 均需要。
**CE 记录：** `27de4499`，scope=pattern，HIGH_PRIORITY

---

### 2. STM32U5 EXTI 新 IP — IMR1 必须置位 + secure alias `[35eeae70]`

**现象：** exti 测试返回 `0xE001`（0 edges detected）。RTSR1[4]=1 ✓，EXTICR2=0x01 ✓，wire 确认 GPIOB_IDR[4]=1 ✓，但 RPR1/FPR1 始终为 0。

**根因一（secure alias）：** STM32U585 TZEN=1，EXTI 属于 AHB3 secure 外设。使用 NS alias（0x46022000）写 RTSR1/FTSR1 不报错但被忽略（TrustZone 过滤）。必须使用 secure alias 0x56022000（CMSIS 中 `EXTI_BASE_S`）。

**根因二（IMR1 polling gate）：** STM32U5 的 EXTI IP 是新设计，RPR1/FPR1 的置位被 IMR1 gate：`IMR1[n]=0` 时即使边沿检测到，RPR1/FPR1 也不会置位。旧版 STM32（F4/L4）没有这个限制——pending bit 无条件置位，IMR 只控制中断触发。polling 模式下旧代码不设 IMR1 是正确的，但在 U5 上必须显式设置。

**修法：**
```c
#define EXTI_BASE   0x56022000u  /* secure alias */
EXTI_IMR1 |= (1u << 4u);        /* gate: required for RPR1/FPR1 to assert */
```

**跨板性：** 适用于所有 STM32U5 系列（U585 已确认，其他 U5 预期相同）。
**CE 记录：** `35eeae70`，scope=pattern，HIGH_PRIORITY

---

### 3. STM32U585 ADC VDDA 硬件缺陷诊断 `[7bdb110f]`

尝试路径：
1. ADC1（AHB2，0x42028000）→ ADRDY timeout
2. ADC4（AHB3，NS alias 0x46021000）→ ADRDY timeout
3. ADC4（AHB3，secure alias 0x56021000）→ ADRDY timeout
4. ADC4 内部 DAC 通道（无需外部 pin）→ ADRDY timeout
5. REVID=0x3007（Rev.C silicon），ADC4_CR 写入确认正常（ADVREGEN=1, CR=0x10000000）
6. 增加 DEEPPWD→0→ADVREGEN sequence → 仍然 timeout

**结论：** VDDA 硬件问题（此板单元），ADC LDO 物理上不就绪。与软件、地址、silicon rev 无关。CE 记录为 skip，不影响其他板。

---

### 4. STM32U585 TrustZone secure alias 规律

| 外设 | NS alias | Secure alias（正确） |
|------|----------|---------------------|
| EXTI | 0x46022000 | **0x56022000** |
| ADC4 | 0x46021000 | **0x56021000** |
| DAC1 | 0x46021800 | **0x56021800** |
| SPI1 | 0x40013000 | **0x50013000** |
| ADC4_CCR | 0x46021308 | **0x56021308** |

规律：AHB3 外设（0x46xxxxxx）的 secure alias = 0x56xxxxxx（高位 4→5）。AHB2 外设（0x42xxxxxx）的 secure alias = 0x52xxxxxx。

---

## Civilization Engine Audit

| 动作 | 内容 | EE ID |
|------|------|-------|
| 新增 pattern | STM32 Timer PSC double-buffering — EGR.UG=1 | `27de4499` |
| 新增 pattern | STM32U5 EXTI — IMR1 required + secure alias | `35eeae70` |
| 新增 task skip | ADC1/ADC4 VDDA 硬件缺陷，此板单元 | `7bdb110f` |
| 新增 task skip | I2C loopback 需要物理接线 | `77033976` |
| run_index 更新 | 14 条 golden_pack 成功记录 | — |
| CLAUDE.md 更新 | 两条 HIGH_PRIORITY 加入资产表 | — |

---

## 板级信息

- **Board:** STM32U585CIU6 (WeAct Studio，TZEN=1 default)
- **Silicon REVID:** 0x3007（Rev.C）
- **Clock:** MSI 4MHz（默认，未切换 PLL）
- **Instrument:** ESP32JTAG @ 192.168.2.98:4242
- **Bench wiring（Stage2）:** PA7↔PA6 (SPI), PA9↔PA10 (UART), PB0↔PB1 (GPIO), PA8→PB4 (PWM→EXTI)
- **Commit:** `124dc0f`（run_index golden pack 记录）；大提交 63 files 在前一个 commit
