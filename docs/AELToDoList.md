# AEL To-Do List

Future tasks, investigations, and open issues.
Add new items here; mark done with `[x]` and a commit/date.

---

## Open Investigations

### INV-001 — ESP32JTAG BMDA SWD 被 resilience ladder 打坏的根因

**Status:** [ ] 待查

**现象：**
1. AEL `ael pack` 的 flash resilience ladder 执行以下命令序列（每次 attempt）：
   - `monitor a`（或改为 `monitor swdp_scan` 后）— 连续多次快速失败
   - `monitor connect_srst enable` — ESP32JTAG 返回 `Target does not support this command`
   - `monitor freq <value>` — 频率设置
2. 经过 4 次 attempt × 多个 test（共约 12+ 次 GDB 连接/断开）后，ESP32JTAG 的 SWD 状态机进入损坏状态
3. 此后手动 `monitor swdp_scan` 也失败，只有重启 ESP32JTAG 才能恢复

**怀疑根因：**
- `monitor connect_srst enable` 在 ESP32JTAG BMDA 上不支持但可能改变了内部状态
- 或：BMDA ESP32JTAG v2.0.0-rc2 对连续快速失败的 SWD scan 有 bug（状态机未正确 reset）

**要查的内容：**
- `ael/instruments/interfaces/esp32jtag.py` 中 resilience ladder 的具体命令序列
- BMDA ESP32JTAG 固件源码中 `connect_srst` 和 `swdp_scan` 的处理逻辑
- 是否可以在 resilience ladder 中禁用 `connect_srst` attempt（ESP32JTAG 不支持）
- 复现步骤：连续对 ESP32JTAG 执行 10+ 次 `monitor swdp_scan`，观察是否必然损坏

**临时缓解：**
- board config 改用 `monitor swdp_scan`（不用 `monitor a`）——仍不足够
- 需要在 AEL flash ladder 中跳过 ESP32JTAG 不支持的 attempt（`connect_under_reset`）

---

## Open Tasks

_（暂无）_

---

## Done

_（暂无）_
