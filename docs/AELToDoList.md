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

## Done

_（暂无）_
