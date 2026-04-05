# AEL Zephyr 接入 — STM32F103 Hybrid Mode 试点计划

**文档类型**：实施计划  
**日期**：2026-04-04  
**状态**：草案  
**关联备忘录**：docs/memo/AEL_Zephyr_方向备忘录_中文版.docx

---

## 一、背景与目标

### 1.1 整体方向

根据 AEL-Zephyr 方向备忘录，AEL 的 Zephyr 接入策略分为以下几个阶段：

| 阶段 | 内容 | 状态 |
|------|------|------|
| P0 | 定义 backend contract | 待开始 |
| P1 | Zephyr backend 最小骨架 | 待开始 |
| P2 | ESP32 对照试点 | **暂缓** |
| P3 | STM32 Hybrid mode 试点 | **本文档** |
| P4 | board registry 自动 backend 选择 | 待开始 |

P2（ESP32 对照试点）暂缓，优先推进 P3，原因：
- STM32F103 + ST-Link bench 已就绪
- Zephyr 对 STM32F103（`stm32_min_dev` / `bluepill` board）支持成熟
- ST-Link + OpenOCD 是 Zephyr 官方支持的 runner，路径清晰
- 可直接复用现有 AEL STM32F103 测试固件做对照

### 1.2 什么是 Hybrid Mode

Hybrid mode 不是把所有项目迁移到 Zephyr，而是在同一套 bench 上，**先用 Zephyr 验证板级与工具链，再切回 AEL 原生裸机闭环**。

```
Step 1: Zephyr 阶段
  west build -b stm32_min_dev (或 bluepill)
  west flash --runner openocd   ← ST-Link
  串口观测 Zephyr boot log      ← 确认板子/ST-Link/串口链路正常

Step 2: Zephyr debugserver 阶段
  west debugserver --runner openocd
  GDB attach → 读寄存器 → 验证调试通路

Step 3: AEL 原生裸机阶段
  编译 AEL 裸机固件（如 stm32f103c6_uart_roundtrip）
  ST-Link + OpenOCD 烧写（同一工具链）
  AEL observe_uart 观测 → verify AEL_IDLE 模式
  完整 AEL 闭环跑通

对照验证：
  三个阶段用同一块板 + 同一个 ST-Link
  Zephyr 阶段 PASS → 硬件 OK
  AEL 阶段 PASS → AEL Zephyr backend 闭环 OK
  任意阶段失败 → 快速定位是硬件还是软件问题
```

### 1.3 试点目标

1. 验证 Zephyr backend 骨架可以驱动 STM32F103 完成 build / flash / observe 全流程
2. 验证 AEL 的 observe_uart + verify 层可以无修改地复用于 Zephyr 产生的串口输出
3. 验证 Hybrid mode 能快速区分 HW 问题与 SW/AEL 问题
4. 为后续 backend contract 定义积累接口边界数据

---

## 二、硬件与环境要求

### 2.1 硬件

| 设备 | 要求 | 备注 |
|------|------|------|
| STM32F103C6T6 / C8T6 Bluepill | 已在 bench 上 | 现有板即可 |
| ST-Link V2（或 V3） | 已连接 | 用于 SWD 烧写与调试 |
| USB-UART 适配器 | 连接 PA9/PA10 | 用于串口观测，或复用 ESP32JTAG Web UART |
| GND 共地 | 必须 | ST-Link GND ↔ STM32 GND |

### 2.2 接线

```
ST-Link SWDIO  → STM32 SWDIO (PA13)
ST-Link SWCLK  → STM32 SWCLK (PA14)
ST-Link GND    → STM32 GND
ST-Link 3.3V   → STM32 3.3V  (可选，如板子无独立供电)

UART RX (host) → STM32 PA9  (USART1_TX)
UART TX (host) → STM32 PA10 (USART1_RX)
```

### 2.3 软件依赖

```bash
# Zephyr SDK 与 west
pip install west
west init ~/zephyrproject
west update
west zephyr-export

# Zephyr SDK toolchain（arm-zephyr-eabi）
# 参考 https://docs.zephyrproject.org/latest/develop/getting_started/

# OpenOCD（ST-Link runner）
sudo apt install openocd   # 或从源码编译

# AEL 环境（现有）
cd /nvme1t/work/codex/ai-embedded-lab
```

---

## 三、详细实施步骤

### Step 0：准备 Zephyr workspace（前置，一次性）

```bash
# 初始化 Zephyr workspace（如未初始化）
west init ~/zephyrproject
cd ~/zephyrproject
west update

# 验证 STM32F103 board 已支持
west boards | grep -i bluepill
# 预期输出: bluepill_f103c8 (或 stm32_min_dev)
```

验收标准：`west boards` 能列出 bluepill 或 stm32_min_dev。

---

### Step 1：Zephyr 最小程序 build + flash

使用 Zephyr 自带的 `hello_world` 或 `blinky` sample：

```bash
cd ~/zephyrproject/zephyr
west build -p always -b bluepill_f103c8 samples/hello_world \
  -- -DCONFIG_UART_CONSOLE=y
west flash --runner openocd
```

**AEL 集成方式**（Zephyr backend 骨架调用）：

```python
# ael/backends/zephyr_backend.py（骨架）
class ZephyrBackend(AELBackend):
    def build(self, board, sample_dir, config_args=None):
        cmd = ['west', 'build', '-p', 'always', '-b', board, sample_dir]
        if config_args:
            cmd += ['--'] + config_args
        return subprocess.run(cmd, check=True, capture_output=True)

    def flash(self, runner='openocd'):
        return subprocess.run(
            ['west', 'flash', '--runner', runner],
            check=True, capture_output=True
        )
```

验收标准：`west flash` 无报错，ST-Link 烧写成功，板子 LED 闪烁（blinky）或串口输出 `Hello World`（hello_world）。

---

### Step 2：串口观测接入 AEL observe_uart

Zephyr hello_world 的串口输出格式：

```
*** Booting Zephyr OS build vX.X.X ***
Hello World! bluepill_f103c8
```

在 AEL 中以 `expect_patterns` 方式验证：

```python
# 测试 plan 片段（Zephyr 阶段）
"observe_uart": {
    "enabled": true,
    "baud": 115200,
    "duration_s": 5,
    "expect_patterns": [
        "Booting Zephyr OS",
        "Hello World"
    ]
}
```

**关键验证点**：AEL 现有的 observe_uart 层（`esp32jtag_web_uart` 或 native serial）能否不修改地消费 Zephyr 的串口输出。

验收标准：observe_uart 能捕获到 `Booting Zephyr OS` 字符串，verify 返回 PASS。

---

### Step 3：Zephyr debugserver 验证调试通路

```bash
# 启动 debugserver
west debugserver --runner openocd
# OpenOCD 监听 3333 端口

# 另一个终端：GDB attach
arm-none-eabi-gdb build/zephyr/zephyr.elf
(gdb) target extended-remote :3333
(gdb) monitor halt
(gdb) info registers
(gdb) monitor resume
(gdb) detach
```

**AEL 集成方式**：

```python
def start_debugserver(self, runner='openocd'):
    proc = subprocess.Popen(
        ['west', 'debugserver', '--runner', runner],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    # 等待 OpenOCD ready（检测 "Info : Listening on port 3333"）
    return proc

def gdb_batch(self, elf, cmds):
    # 复用现有 AEL GDB batch 逻辑
    ...
```

验收标准：GDB 能 attach，`info registers` 返回有效值，`monitor resume` 后固件继续运行。

---

### Step 4：切回 AEL 原生裸机固件（Hybrid 核心）

使用现有已验证的 AEL 固件：

```
firmware/targets/stm32f103c6_uart_roundtrip/
```

用**相同的 ST-Link + OpenOCD** 烧写（不走 west，走 AEL 原生 GDB batch）：

```python
# AEL GDB batch 命令（现有，沿用规则 1 的黄金序列变体）
gdb_launch_cmds = [
    "monitor init",
    "monitor reset halt",
    "load",
    "monitor reset run",
    "disconnect"
]
```

然后运行已有测试：

```bash
python3 -m ael run \
  --board stm32f103c6t6_bluepill_like \
  --test tests/plans/stm32f103c6_uart_roundtrip_with_esp32jtag.json
```

（或新建一个 st-link 版本的 test plan，见 Step 5）

验收标准：AEL 闭环 PASS，observe_uart 捕获到 `AEL_IDLE count=`。

---

### Step 5：新建 ST-Link 版本 test plan（可选但建议）

如果现有 test plan 绑定了 esp32jtag，建议新建一个 ST-Link 版本：

```json
{
  "name": "stm32f103c6_uart_roundtrip_with_stlink",
  "board": "stm32f103c6t6_bluepill_like",
  "supported_instruments": ["stlink"],
  "bench_setup": {
    "instrument_roles": [
      {
        "role": "control_instrument",
        "instrument_id": "stlink_stm32_local",
        "required": true
      }
    ]
  },
  "observe_uart": {
    "enabled": true,
    "backend": "native_serial",
    "baud": 115200,
    "duration_s": 6,
    "expect_patterns": ["AEL_IDLE count="]
  }
}
```

---

## 四、AEL Zephyr Backend 骨架设计

### 4.1 文件结构

```
ael/
  backends/
    base.py              ← AELBackend 抽象基类（新建）
    idf_backend.py       ← 现有 IDF 逻辑的 wrapper（重构）
    zephyr_backend.py    ← 本次新建
    native_backend.py    ← 现有裸机逻辑的 wrapper（重构）
```

### 4.2 抽象基类

```python
# ael/backends/base.py
from abc import ABC, abstractmethod
from pathlib import Path

class AELBackend(ABC):

    @abstractmethod
    def detect_project_type(self, project_dir: Path) -> bool:
        """判断 project_dir 是否属于本 backend"""

    @abstractmethod
    def build(self, **kwargs) -> Path:
        """构建，返回 artifact ELF/BIN 路径"""

    @abstractmethod
    def flash(self, artifact: Path, **kwargs) -> None:
        """烧写到目标板"""

    @abstractmethod
    def start_debugserver(self, **kwargs):
        """启动调试服务器，返回 subprocess.Popen"""

    @abstractmethod
    def observe(self, duration_s: float, expect_patterns: list) -> dict:
        """采集串口/日志，返回观测结果"""

    @abstractmethod
    def verify(self, observation: dict, expectations: dict) -> dict:
        """验证观测结果，返回 pass/fail + 详情"""
```

### 4.3 Zephyr backend 最小实现

```python
# ael/backends/zephyr_backend.py
import subprocess
from pathlib import Path
from .base import AELBackend

class ZephyrBackend(AELBackend):

    def detect_project_type(self, project_dir: Path) -> bool:
        return (project_dir / 'CMakeLists.txt').exists() and \
               (project_dir / 'prj.conf').exists()

    def build(self, board: str, project_dir: Path,
              build_dir: Path = None, config_args: list = None) -> Path:
        build_dir = build_dir or Path('build')
        cmd = ['west', 'build', '-p', 'always', '-b', board,
               str(project_dir), '--build-dir', str(build_dir)]
        if config_args:
            cmd += ['--'] + config_args
        subprocess.run(cmd, check=True)
        return build_dir / 'zephyr' / 'zephyr.elf'

    def flash(self, artifact: Path = None, runner: str = 'openocd',
              build_dir: Path = None) -> None:
        cmd = ['west', 'flash', '--runner', runner]
        if build_dir:
            cmd += ['--build-dir', str(build_dir)]
        subprocess.run(cmd, check=True)

    def start_debugserver(self, runner: str = 'openocd',
                          build_dir: Path = None):
        cmd = ['west', 'debugserver', '--runner', runner]
        if build_dir:
            cmd += ['--build-dir', str(build_dir)]
        return subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)

    def observe(self, duration_s: float, expect_patterns: list) -> dict:
        # 复用 AEL 现有 observe_uart 逻辑
        raise NotImplementedError("接入现有 observe_uart 层")

    def verify(self, observation: dict, expectations: dict) -> dict:
        # 复用 AEL 现有 verify 逻辑
        raise NotImplementedError("接入现有 verify 层")
```

---

## 五、验收标准汇总

| 步骤 | 验收条件 |
|------|---------|
| Step 0 | `west boards` 列出 bluepill 或 stm32_min_dev |
| Step 1 | `west flash` 无报错，板子运行 Zephyr 程序 |
| Step 2 | AEL observe_uart 捕获 `Booting Zephyr OS`，verify PASS |
| Step 3 | GDB attach 成功，`info registers` 返回有效值 |
| Step 4 | AEL 原生裸机固件 flash + observe 闭环 PASS |
| Step 5（可选） | ST-Link test plan 新建并运行 PASS |
| 整体 | Zephyr 阶段与 AEL 阶段均 PASS，confirm Hybrid mode 可行 |

---

## 六、风险与缓解

| 风险 | 说明 | 缓解措施 |
|------|------|---------|
| west workspace 初始化慢 | 首次 `west update` 需下载大量依赖 | 提前在 bench 机器初始化，CI 做镜像缓存 |
| bluepill board 名称差异 | Zephyr 中 F103C6 vs C8T6 board 名可能不同 | 先 `west boards \| grep -i f103` 确认，必要时加 board overlay |
| OpenOCD ST-Link 驱动版本 | 不同 OpenOCD 版本对 ST-Link V2/V3 支持有差异 | 固定 OpenOCD 版本，记录到 bench 文档 |
| observe_uart 串口归属 | ST-Link 的 VCP 与 USB-UART 适配器在 /dev/tty* 下可能混淆 | 用 udevadm 确认 tty 归属，不依赖设备顺序 |
| AEL GDB batch 与 west debugserver 端口冲突 | 两者都默认用 3333 | 分步运行，不同时启动 |

---

## 七、后续计划

本次试点完成后，产出物用于：

1. 补充 `backend contract`（P0）的接口定义，基于真实边界而非猜测
2. 完善 `ZephyrBackend` 骨架中 `observe` 和 `verify` 的接入方式
3. 为 P4（board registry 自动 backend 选择）提供 STM32 + Zephyr 的判断逻辑样本
4. 如试点顺利，升级为 `scope='pattern'` 写入 Civilization Engine

---

*本文档由 AEL-Claude 生成，基于 AEL_Zephyr_方向备忘录_中文版.docx 及现有 STM32F103 bench 状态。*
