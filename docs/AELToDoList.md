# AEL To-Do List

Future tasks, investigations, and open issues.
Add new items here; mark done with `[x]` and a commit/date.

---

## Open Investigations

### INV-001 — ESP32JTAG/目标板 SWD 连接被打坏的根因

**Status:** [ ] 待查

**核心 Bug：**
ESP32JTAG BMDA 固件有 bug —— 任何一次 GDB 连接失败（SWD scan 失败、attach 失败等），都会把 BMDA 内部 SWD 状态机搞坏，之后所有连接都失败，直到 ESP32JTAG 重启。
**注意**：也可能是目标板问题（STM32 SWD 接口进入异常状态），需要两者都重启才能恢复，未确定是哪一侧的根因。

**现象：**
1. AEL `ael pack` 的 flash resilience ladder 执行以下命令序列（每次 attempt）：
   - `monitor a`（或改为 `monitor swdp_scan` 后）— 连续多次快速失败
   - `monitor connect_srst enable` — ESP32JTAG 返回 `Target does not support this command`
2. 经过多次 GDB 连接/断开失败后，SWD 状态机进入损坏状态
3. 此后手动 `monitor swdp_scan` 也失败，只有重启 ESP32JTAG **和**目标板才能恢复

**怀疑根因（待确认哪侧）：**
- ESP32JTAG BMDA v2.0.0-rc2：连续快速失败的 SWD scan 后状态机未正确 reset
- 或：目标板 STM32 SWD 接口在某些条件下进入 lockup 状态
- `monitor connect_srst enable` 不支持但可能触发了内部状态变化

**要查的内容：**
- 单独重启 ESP32JTAG（不重启目标板）能否恢复 → 确定是哪侧的问题
- 单独重启目标板（不重启 ESP32JTAG）能否恢复
- BMDA ESP32JTAG 固件源码中 `swdp_scan` 失败路径的状态机处理逻辑
- 复现步骤：连续对 ESP32JTAG 执行 10+ 次失败的 `monitor swdp_scan`

**已知缓解：**
- ESP32JTAG 可通过 `POST https://<ip>/set_credentials`（保持原参数不变）软重启
- board config 加 `allowed_strategies: ["normal"]` 跳过 `connect_under_reset`（发 `monitor connect_srst enable`）
- `flash_bmda_gdbmi.py` `attempt_ok` 改为检查具体 load 失败关键词，不用 `"failed" not in output`

---

## Open Tasks

### TASK-002 — 多探头场景下的本地 GDB/OpenOCD 会话隔离

**Status:** [ ] 待实现

**问题：**
当前本地 ST-Link / DAPLink 会话管理主要按 `127.0.0.1:<port>` 判断和复用。
如果同一台 PC 上同时接了多个 ST-Link / DAPLink，对不同板子并行或交替操作时，可能错误复用、误杀、或连到错误目标。

**风险：**
- 复用到另一个探头/板子的旧会话
- 新会话因端口占用无法启动
- MCU 识别/flash 实际连到了错误目标
- cleanup 误杀不属于当前 run 的调试会话

**后续修复方向：**
- 用探头稳定身份做会话绑定：优先 USB serial，fallback 到 USB bus/device path
- 不再只靠端口识别会话；运行态保存 `(probe identity, port, pid, owner, target)`
- 仅当会话身份匹配当前探头时才允许 reuse
- cleanup 只回收当前 run 自己启动的会话
- DAPLink/OpenOCD 启动时增加精确 probe 选择，避免“first CMSIS-DAP wins”

### TASK-003 — firmware/targets/ 历史遗留无 board_id 目录清理

**Status:** [ ] 待实现（与 tests/plans 清理一并进行）

**背景：**
`firmware/targets/` 目前有 398 个目录，其中存在大量无具体 board_id 的历史遗留版本，
已被带型号的具体版本取代，但尚未清除：

| 待删目录前缀 | 已被取代为 |
|------------|----------|
| `stm32f103/` | `stm32f103c6/` + `stm32f103rct6/` |
| `stm32f407/` | `stm32f407vet6/` |
| `stm32f401/` | `stm32f401rct6/` |
| `stm32f411/` | `stm32f411ceu6/` |
| `stm32g431/` | `stm32g431cbu6/` |

**执行时机：**
与 `tests/plans/` 的对应清理一并进行，确保两侧同步删除，不留悬空引用。

**执行步骤：**
1. 确认上述目录在 `tests/plans/` 和 `packs/` 中均无有效引用
2. `git rm -r firmware/targets/stm32f103/ firmware/targets/stm32f407/` 等
3. 同步删除对应的 `tests/plans/stm32f103_*.json` 等
4. 验证所有现有 pack 仍可正常运行

---

### TASK-001 — ESP32JTAG Web UI / CLI 直接重启按钮

**Status:** [ ] 待实现

**需求：**
在 ESP32JTAG 的 Web UI 和/或 AEL CLI 上增加一个"直接重启 ESP32JTAG"的按钮或命令，
使用户无需手动断电即可快速触发软重启。

**背景：**
当前已知软重启机制：通过 `POST https://<ip>/set_credentials`（pbcfg 参数双切换）触发 ESP32 重启。
该机制已在 `_ProbeSoftResetRecoveryAdapter` 中用于自动恢复，但目前没有暴露给用户的直接入口。

**期望行为：**
- Web UI：在探头管理页面加一个 "Restart" / "重启" 按钮，点击后触发 pbcfg 切换重启
- AEL CLI：新增命令（如 `ael probe restart <ip>`），直接调用同样的重启序列，并等待 GDB 端口恢复后报告成功
- 重启完成后应显示确认信息（端口恢复时间、成功/失败）

---

### TASK-004 — CH32V003 设备恢复 + pwr_sleep AWU 测试修复

**Status:** [ ] 待实现

**背景：**
`pwr_sleep` 固件使用了 `__WFE()` 而未配置 `EXTI->EVENR bit9`，导致设备永久卡死在
WFE 休眠状态，WCH-Link SDI 接口无响应，无法 flash 新固件。已将 `pwr_sleep` 从
`ch32v003_golden.json` 中移除，golden 现为 13/13。

**恢复方法：**
用 WCH-LinkUtility（PC 端 WCH 官方工具）通过 ISP 模式强制写入任意可运行固件：
1. 安装 WCH-LinkUtility（Windows/Linux）
2. 选择 CH32V003，通过 WCH-Link USB 连接
3. ISP 模式下写入 `ch32v003_minimal_mailbox.elf`（不休眠）
4. 验证 `ael run --test ch32v003_minimal_mailbox.json` PASS

**pwr_sleep 正确修复方案（恢复后实施）：**
已知原因：`__WFI()` 唤醒需要 `EXTI->EVENR |= (1u << 9)` 打开 AWU 事件线。
修复后固件逻辑（已写好，见 `firmware/targets/ch32v003_pwr_sleep/main.c`）：
- `EXTI->EVENR |= (1u << 9)` — AWU 事件路由到 WFI
- `__WFI()` — 仅休眠一次（约 100 ms）
- 唤醒后写 PASS，进入 busy liveness 循环（不再休眠，debug halt 可靠）
- liveness 用 `SysTick->CNT >> 13` 持续更新 detail0

恢复后执行：
```
ael run --test tests/plans/ch32v003_pwr_sleep.json
# PASS → 将 pwr_sleep 加回 ch32v003_golden.json → golden 恢复 14/14
```

**附：CH32V003 stage2 待验证**
9 个 stage2 测试（I2C、SPI DMA、ADC DMA、TIM DMA 等）之前已 PASS，
设备恢复后可正式将 stage2 跑通，确认后可并入 golden 或维持独立 pack。

---

## Done

_（暂无）_
