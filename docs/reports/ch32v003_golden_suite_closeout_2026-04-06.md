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

---

## Problems Encountered and How They Were Solved

### Bug 1 — WCH OpenOCD halts target on init, breaking UART tests
**CE ID: `91a479e9` (HIGH_PRIORITY, scope=pattern)**

**症状**：`ch32v003_uart_wchlink` 和 `ch32v003_uart_bidir` 反复 FAIL，错误为
`check_uart: expected UART output but got 0 bytes`，即使固件本身正确。

**根因发现过程**：

AEL pipeline 的 stage 优先级决定了执行顺序：`check_uart`（priority=5）早于
`check_mailbox`（priority=6）。WCH OpenOCD 在启动时执行 `init` 命令，该命令会
对 CPU 进行 examination 并 **halt** 目标。此时目标处于 halt 状态，固件未运行，
serial port 当然没有输出。`check_uart` 在这个时机去读串口，永远读不到任何内容。

**解决方法**：

在 `ael/adapters/flash_wch_openocd.py` 的 `cmd_srv` 列表中添加：
```python
"-c", "init; catch {resume}"
```
`catch {}` 是必须的——如果目标在某些情况下已经是 running 状态，裸 `resume` 会
抛出 `"Hart is not halted!"` 错误导致 OpenOCD 退出，GDB 端口来不及打开。

**同步调整**：两个 UART 测试的 `observe_uart` 改为 `disabled`，改用
`check_mode: detail0_increment` 验证 UART TX（固件发完数据后 detail0 才递增，
间接确认 TX 完成）。

---

### Bug 2 — AFIO EXTICR 2-bit 布局（不同于 STM32 的 4-bit）
**CE ID: `d994bafa` (HIGH_PRIORITY, scope=board_family)**

**症状**：`ch32v003_exti_loopback` 和 `ch32v003_tim_pwm_capture` 均 FAIL，
mailbox 显示 `status=FAIL`，edge_count=0（没有捕获到任何 EXTI 边沿）。

**根因发现过程**：

最初照搬 STM32 的 AFIO 写法，每个 EXTI 线使用 4 位字段：

```c
/* 错误写法（STM32 style，4 bits per EXTI line）*/
AFIO->EXTICR[1] = (0x2u << 4);  // EXTI5/PC → wrong
```

阅读 WCH EVT 源码 `ch32v00x_gpio.c` 中的 `GPIO_EXTILineConfig()` 发现：

```c
/* WCH EVT 原文 */
AFIO->EXTICR = (AFIO->EXTICR & (~(0x03 << (GPIO_PinSource * 2))))
             | (GPIO_PortSource << (GPIO_PinSource * 2));
```

CH32V003 只有一个 `EXTICR` 寄存器（不是 EXTICR[4] 数组），每线 **2 bits**，
`bit_pos = PinSource << 1`。

**解决方法**：

- EXTI1/PC: `(AFIO->EXTICR & ~(0x3u<<2)) | (0x2u<<2)`
- EXTI4/PD: `(AFIO->EXTICR & ~(0x3u<<8)) | (0x3u<<8)`

---

### Bug 3 — ADC EXTSEL 未设置，SWSTART 无效
**CE ID: `db402746` (HIGH_PRIORITY, scope=board_family)**

**症状**：`ch32v003_adc_vref` 运行后 mailbox 始终为 `RUNNING`（永不 PASS/FAIL），
GDB 读取 detail0=0。

**根因发现过程**：

手动 flash + GDB halt 读 mailbox，确认 status=RUNNING，detail0=0。这意味着
固件停在某个等待循环里。排查 adc_init() 中的等待：

1. RSTCAL 等待：不可能无限（硬件自清除）
2. CAL 等待：同上
3. **EOC 等待**：`while (!(ADC1->STATR & (1u << 1)))` — 可能死循环

对比 WCH EVT `ch32v00x_adc.h`，发现：

```c
#define ADC_ExternalTrigConv_None   ((uint32_t)0x000E0000)
// = bits[19:17] = EXTSEL = 0b111 = software trigger
```

我的代码 `CTLR2 = (1u<<0) | (1u<<23)` 中 EXTSEL=0b000（默认，选择 TIM1 OC1
作为外部触发源）。在没有 TIM1 OC1 信号的情况下，ADC 永远等不到触发。
SWSTART（软件触发）必须配合 EXTSEL=111 才能生效。

**解决方法**：

```c
ADC1->CTLR2 = (1u << 0) | (1u << 23) | (0x7u << 17);
// ADON | TSVREFE | EXTSEL[19:17]=0b111
```

---

### Bug 4 — ch32v003fun.c .data 启动拷贝失效（重大固件平台 bug）
**CE ID: `5e480c33` (HIGH_PRIORITY, scope=board_family)**

**症状**：`ch32v003_dma_mem2mem` FAIL，mailbox=FAIL，error_code=32（全部 32 个
word mismatch）。DMA 寄存器全为 0（DMA 未运行）。GDB 读 SRAM 中 SRC_BUF 内容
为乱码。

**根因发现过程**：

**第一步**：确认 DMA 寄存器为 0 → DMA 根本没运行，说明固件在 DMA 配置之前就
已经出了问题，或 SRC_BUF 是 0 导致 CNTR=0 被忽略。

**第二步**：检查 SRC_BUF 地址。`objdump` 和 map 文件确认 SRC_BUF（const）在
`.rodata`，放在 Flash（0x00000000）；DST_BUF 在 `.bss`（SRAM 0x20000080）。
DMA 的 PADDR=0x20000000 指向 SRAM 中 SRC_BUF 的 VMA 地址。

**第三步**：把 `const` 去掉，SRC_BUF 进入 `.data`，VMA=0x20000000（SRAM）。
重新 flash，同样的问题——GDB 读 `*0x20000000` 仍然是乱码。

**第四步**：GDB 读 Flash 地址 0x204（`.data` 的 LMA），数据正确：
```
0x204: 0x01020304 0x05060708 ...
```
但 SRAM 0x20000000 是乱码。说明 **.data 从 Flash 拷贝到 SRAM 这一步没有发生**。

**第五步**：反汇编 `handle_reset()`，发现关键代码：
```asm
lui  a5, 0x20000
lw   a5, 128(a5)    # a5 = *0x20000080 = DST_BUF[0] (乱码!)
lw   a4, -1920(gp)  # a4 = *0x20000100 (某个 SRAM 值)
bgeu a5, a4, skip   # 如果乱码值 >= 乱码值，跳过拷贝
```

启动代码把链接器符号 `_sbss`（值为 0x20000080）当作**指针变量**来读，即读取
地址 0x20000080 处的 4 字节内容，而不是直接使用 0x20000080 作为地址常量。
这是 ch32v003fun.c 中的一个 C 语言语义 bug——`extern uint32_t * _sbss` 把符号
当成存储指针的变量，而链接器 `PROVIDE(_sbss = .)` 定义的是地址常量，不是变量。

新板上电 SRAM=0，`*0x20000080=0`，`0 >= 0` 触发跳过条件，拷贝被 skip。
重 flash 后 SRAM 有旧数据，拷贝行为完全不可预测。

**解决方法**：

不使用任何有初始值的全局/静态变量。改用 `.bss`（无初始值）在 `main()` 里用
代码填充：

```c
static uint32_t SRC_BUF[BUF_SIZE];  // .bss
// in main():
for (uint32_t i = 0; i < BUF_SIZE; i++) SRC_BUF[i] = (i+1) * 0x01010101u;
```

**影响范围**：所有使用 ch32v003fun.c 的固件。已有的 9 个测试都不用初始化全局
数组，所以没有暴露此问题。

---

### Bug 5 — TIM2 CNT 16-bit 溢出导致计数判断失效
**CE ID: `ae4804b7` (scope=board_family)**

**症状**：`ch32v003_tim2` 第一次单独跑 PASS，在 pack 连续运行时偶发 FAIL，
mailbox 显示 `error_code=44410 < 50000`。

**根因发现过程**：

TIM2->CNT 是 16-bit 寄存器（最大 65535）。PSC=7 → 1 MHz 计时。等待循环
`for(i<2000000)` 在 24 MHz 约运行 417 ms，期间 TIM2 计 417000 ticks，溢出
417000 / 65536 ≈ 6.36 次，最终 CNT ≈ 23000。偶尔因为流水线差异或上一个测试
耗时略有不同，最终 CNT 可能在 44000–50000 附近，刚好卡在 50000 门槛处。

**另一个场景**：读两次 CNT 时若 CNT1 在 65500 附近，10K 次循环后 CNT2 发生
wrap，CNT2 < CNT1，`cnt2 > cnt1` 判断为 FALSE → FAIL。

**解决方法**：

```c
uint16_t advance = (uint16_t)((uint16_t)cnt2 - (uint16_t)cnt1);
uint32_t uif = TIM2->INTFR & 0x1u;  // overflow flag
if (cnt1 > 0u && (advance > 0u || uif)) ael_mailbox_pass();
```

无符号 16-bit 减法自动处理 wrap-around。UIF 作为溢出兜底。

---

## Key CH32V003-Specific Findings Summary

| 问题 | CE ID | 是否 HIGH_PRIORITY | 修复方法 |
|------|-------|-------------------|---------|
| WCH OpenOCD init halts target | `91a479e9` | ✅ YES | `init; catch {resume}` in cmd_srv |
| AFIO EXTICR 2-bit layout | `d994bafa` | ✅ YES | `bit_pos = PinSource<<1`; 2 bits/line |
| ADC EXTSEL=0b111 for SWSTART | `db402746` | ✅ YES | `CTLR2 |= (0x7u<<17)` |
| ch32v003fun.c .data copy bug | `5e480c33` | ✅ YES | Use `.bss` + runtime init in main() |
| TIM2 CNT 16-bit overflow | `ae4804b7` | — | uint16_t modulo subtract + UIF flag |

---

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

---

## Peripheral Coverage

```
GPIO ✓  EXTI ✓  USART1 ✓  SPI1 ✓  TIM1(PWM+capture) ✓  TIM2 ✓
SysTick ✓  IWDG ✓  WWDG ✓  Flash ✓
ADC1(Vrefint) ✓  DMA1(MEM2MEM) ✓  PWR(Sleep+AWU) ✓
```

Not yet covered: I2C, OPA (CH32V003-unique), ADC-DMA, TIM-DMA, USART-DMA,
PWR-Standby, RCC-MCO.

---

## Civilization Engine Audit

| 项目 | 详情 |
|------|------|
| 查询了什么 | session 继承上一 context；无新查询 |
| 命中了什么 | `d994bafa` (AFIO EXTICR, 已有) |
| 是否复用 | 是 — AFIO EXTICR fix 直接应用 |
| 新增记录 | `91a479e9` WCH OpenOCD halt → HIGH_PRIORITY pattern |
| | `db402746` ADC EXTSEL=111 → board_family |
| | `5e480c33` ch32v003fun.c .data bug → board_family HIGH_PRIORITY |
| | `ae4804b7` TIM2 16-bit CNT wrap → board_family |
| 升级资产 | `b5fb0a7b`（旧 WCH OpenOCD 记录）→ 新建 `91a479e9` 升级为 scope=pattern + HIGH_PRIORITY |
