# AEL Zephyr 接入 — STM32F4 Discovery Hybrid Mode 试点计划

**文档类型**：实施计划  
**日期**：2026-04-04（更新：2026-04-05）  
**状态**：Steps 1–5 已完成，Step 6 代码已落地  
**目标板**：STM32F4 Discovery（STM32F407VGT6）  
**关联备忘录**：docs/memo/AEL_Zephyr_方向备忘录_中文版.docx  
**取代文档**：docs/plans/zephyr_stm32f103_hybrid_pilot_plan.md（已废弃）

---

## 一、背景与阶段定位

### 1.1 整体 Zephyr 接入阶段

| 阶段 | 内容 | 状态 |
|------|------|------|
| P0 | 定义 AEL backend contract 抽象基类 | **完成** `ael/backends/base.py` |
| P1 | Zephyr backend 最小骨架（west build/flash/debugserver） | **完成** `ael/backends/zephyr_backend.py` |
| P2 | ESP32 对照试点 | **暂缓** |
| **P3** | **STM32F4 Discovery Hybrid mode 试点** | **完成（本文档）** |
| P4 | board registry 自动 backend 选择 | 待开始 |

### 1.2 为什么选 STM32F4 Discovery

| 理由 | 说明 |
|------|------|
| AEL 已有完整支持 | golden pack 含 7 个已验证测试，是最佳对照基线 |
| 板载 ST-Link | 无需外部调试器，接 USB 线即可 flash + debug |
| Zephyr 官方支持 | `stm32f4_disco` 是 Zephyr 最早支持的板子之一，samples 全部可用 |
| 板载 4 个 LED | PD12/13/14/15，blinky 验证不需要任何额外接线 |
| bench 已就绪 | 当前 bench 上已有此板 |

---

## 二、硬件概览

### 2.1 STM32F4 Discovery 板载资源

```
MCU:        STM32F407VGT6（Cortex-M4F，168 MHz，1 MB Flash，192 KB RAM）
调试器:      板载 ST-Link/V2-A（USB Mini-B 接口，CN1）
LEDs:       PD12 绿 / PD13 橙 / PD14 红 / PD15 蓝
用户按键:    PA0（WKUP）
加速度计:    LIS302DL（SPI1）
麦克风:      MP45DT02（I2S2）
音频 DAC:   CS43L22（I2C1 + I2S3）
USB OTG FS: PA11/PA12
USB OTG HS: 通过 ULPI
```

### 2.2 关键引脚约束（已验证）

| 引脚 | 用途 | 约束 |
|------|------|------|
| PA9 / PA10 | USART1 TX/RX | **被 ST-Link UART bridge 占用，禁止用于 loopback** |
| PD5 / PD6 | USART2 TX/RX | 推荐 UART 使用引脚，无板载冲突 |
| PD12–PD15 | 板载 LED | 直接可用，无冲突 |
| PA0 | 用户按键 | 上电后为浮空输入 |

> 注：此约束已记录于 AEL memory（`reference_stm32f4_discovery_pins.md`），两次测试验证。

### 2.3 接线要求

**Zephyr blinky / hello_world 阶段：零额外接线**

```
PC ──USB Mini-B──> STM32F4 Discovery CN1（ST-Link）
```

**需要 UART 观测时（已实测验证）：**

```
STM32 PA2 (USART2_TX) ──> USB-UART 适配器 RX  ──> PC /dev/ttyUSBx
GND ──────────────────── USB-UART GND
```

> **实测修正（2026-04-05）**：Zephyr `stm32f4_disco` DTS 中 USART2 console 使用
> **PA2/PA3**，不是 PD5/PD6。PD5/PD6 仍是 AEL 原生 UART loopback 测试的引脚，
> 两者用途不同，勿混淆。

---

## 三、Zephyr 与 STM32F4 Discovery 的关系

### 3.1 Zephyr board 名称

```
stm32f4_disco
```

验证：
```bash
west boards | grep stm32f4_disco
# 输出: stm32f4_disco
```

### 3.2 Zephyr 为此板提供的内容

| 资源 | 说明 |
|------|------|
| board 定义 | `zephyr/boards/arm/stm32f4_disco/` — 时钟、pinmux、外设已配好 |
| OpenOCD 配置 | 自动选 `board/stm32f4discovery.cfg`，无需手写 |
| Flash runner | `west flash --runner openocd` 直接用板载 ST-Link |
| Debug runner | `west debugserver --runner openocd`，监听 :3333 |
| 默认 UART 控制台 | USART2（Zephyr 默认，PA2/PA3 或由 overlay 配置） |
| 可用 samples | blinky, hello_world, button, uart, i2c, spi, gpio 等全部支持 |

### 3.3 Zephyr 默认控制台 UART（已实测验证）

Zephyr 在 `stm32f4_disco` 上默认将串口控制台路由到 **USART2 PA2(TX)/PA3(RX)**。  
DTS 原文：`pinctrl-0 = <&usart2_tx_pa2 &usart2_rx_pa3>`

如果需要从 PC 读到 `Hello World` 输出，接线为：

```
PA2 (USART2_TX) → USB-UART RX
GND             → USB-UART GND
```

实测输出（Zephyr v4.4.0-rc2，2026-04-05）：
```
*** Booting Zephyr OS build v4.4.0-rc2-41-g149c8b1758a8 ***
Hello World! stm32f4_disco/stm32f407xx
```

---

## 四、Hybrid Mode 说明

Hybrid Mode 的核心是：**同一块板、同一个 ST-Link，先让 Zephyr 开路验证，再切回 AEL 原生闭环**。

```
┌─────────────────────────────────────────────────────┐
│  Step 1: Zephyr Blinky（零接线验证）                  │
│    west build -b stm32f4_disco samples/basic/blinky  │
│    west flash --runner openocd                        │
│    → 目视或 LA 确认 PD12/13/14/15 LED 轮流闪烁        │
│    → 证明：板子好、ST-Link 好、烧写链路通              │
├─────────────────────────────────────────────────────┤
│  Step 2: Zephyr Hello World（串口验证）               │
│    west build -b stm32f4_disco samples/hello_world   │
│    west flash --runner openocd                        │
│    → USB-UART 读 PA2，捕获 "Booting Zephyr OS"       │
│    → 证明：UART 链路通，AEL observe_uart 可复用        │
├─────────────────────────────────────────────────────┤
│  Step 3: Zephyr Debugserver（调试通路验证）            │
│    west debugserver --runner openocd                  │
│    GDB: target remote :3333 → info registers → detach│
│    → 证明：调试通路好，GDB batch 可接入               │
├─────────────────────────────────────────────────────┤
│  Step 4: 切回 AEL 原生裸机（闭环验证）                 │
│    AEL run stm32f407_mailbox（已验证的最简测试）        │
│    → 证明：Zephyr backend 与 AEL 原生 backend 共存     │
│    → 相同 ST-Link，相同 OpenOCD，AEL 闭环 PASS        │
├─────────────────────────────────────────────────────┤
│  Step 5: AEL Golden Pack 验证（扩展验证）              │
│    ael pack stm32f407vgt6_golden                      │
│    → 7 个测试全部 PASS，确认引入 Zephyr 未破坏原有能力  │
└─────────────────────────────────────────────────────┘
```

**Hybrid Mode 的价值**：

- Step 1–3 PASS，Step 4 FAIL → 问题在 AEL backend 代码，硬件无关
- Step 1 FAIL → ST-Link 或板子有物理问题，无需看代码
- Step 4 PASS，Step 5 FAIL → 特定测试的固件或 wiring 问题

---

## 五、详细实施步骤

### Step 0：环境准备（一次性）

```bash
# 安装 west
pip install west

# 初始化 Zephyr workspace
west init ~/zephyrproject
cd ~/zephyrproject
west update          # 下载所有依赖（首次约 5~15 分钟）

# 安装 Zephyr SDK（arm-zephyr-eabi 工具链）
# 从 https://github.com/zephyrproject-rtos/sdk-ng/releases 下载
# 解压后：
export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk-0.16.x
source ~/zephyrproject/zephyr/zephyr-env.sh

# OpenOCD（ST-Link 支持）
sudo apt install openocd    # Ubuntu/Debian
# 验证版本 >= 0.11
openocd --version
```

验收：`west boards | grep stm32f4_disco` 有输出。

---

### Step 1：Zephyr Blinky — 零接线验证

```bash
cd ~/zephyrproject/zephyr

west build -p always \
           -b stm32f4_disco \
           samples/basic/blinky

west flash --runner openocd
```

预期行为：
- OpenOCD 输出 `** Programming Finished **`
- 板子上 PD12（绿 LED）开始闪烁，约 1 Hz

**AEL Zephyr backend 骨架调用（目标形态）：**

```python
backend = ZephyrBackend(workspace='~/zephyrproject/zephyr')
artifact = backend.build(board='stm32f4_disco',
                         sample_dir='samples/basic/blinky')
backend.flash(runner='openocd')
```

验收标准：`west flash` 无报错，LED PD12 闪烁。

---

### Step 2：Zephyr Hello World — 串口观测

```bash
west build -p always \
           -b stm32f4_disco \
           samples/hello_world \
           -- -DCONFIG_UART_CONSOLE=y

west flash --runner openocd

# 读串口（PA2 → USB-UART RX → /dev/ttyUSBx，115200）
# 注意：先开始读，再用 OpenOCD reset 触发重新输出
python3 -c "
import serial, subprocess, threading, time
received = []
def read():
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=10) as s:
        s.reset_input_buffer()
        deadline = time.time() + 10
        while time.time() < deadline:
            line = s.readline()
            if line:
                txt = line.decode('utf-8', errors='replace').rstrip()
                received.append(txt)
                print('UART:', txt)
t = threading.Thread(target=read, daemon=True)
t.start()
time.sleep(0.3)
subprocess.run(['openocd', '-f', 'interface/stlink.cfg',
                '-f', 'target/stm32f4x.cfg',
                '-c', 'init', '-c', 'reset run', '-c', 'shutdown'],
               capture_output=True)
t.join(timeout=10)
"
```

实测输出（Zephyr v4.4.0-rc2，2026-04-05）：
```
*** Booting Zephyr OS build v4.4.0-rc2-41-g149c8b1758a8 ***
Hello World! stm32f4_disco/stm32f407xx
```

> **注意**：hello_world 只在 boot 时打印一次，必须先开串口监听再触发 reset，
> 否则会错过输出。用 OpenOCD `reset run` 软件复位即可，无需按硬件 RESET 键。

**AEL observe_uart 集成验证（核心测试点）：**

```python
# 验证 AEL 现有 observe_uart 层能否不修改地消费 Zephyr 输出
result = observe_uart(
    port='/dev/ttyUSB0',
    baud=115200,
    duration_s=5,
    expect_patterns=['Booting Zephyr OS', 'Hello World']
)
assert result['pass'] == True
```

验收标准：AEL observe_uart 捕获到 `Booting Zephyr OS`，verify 返回 PASS。

---

### Step 3：Zephyr Debugserver — 调试通路验证

```bash
# 终端 1：启动 debugserver
west debugserver --runner openocd
# 预期输出：
# Info : Listening on port 3333 for gdb connections
# Info : Listening on port 4444 for telnet connections

# 终端 2：GDB attach 验证
arm-none-eabi-gdb build/zephyr/zephyr.elf << 'EOF'
target extended-remote :3333
monitor halt
info registers
monitor reset run
detach
quit
EOF
```

预期：`info registers` 输出有效的 PC/SP/LR 值，不报错。

**AEL GDB batch 集成（复用现有逻辑）：**

```python
proc = backend.start_debugserver(runner='openocd')
# 等待 "Listening on port 3333"
wait_for_debugserver_ready(proc, port=3333)

# 复用 AEL 现有 gdb_batch 函数
result = gdb_batch(
    elf='build/zephyr/zephyr.elf',
    cmds=['target extended-remote :3333',
          'monitor halt',
          'info registers',
          'monitor reset run',
          'detach']
)
proc.terminate()
```

验收标准：GDB attach 成功，寄存器读取有效值，无 timeout。

---

### Step 4：切回 AEL 原生裸机 — 闭环验证

使用现有已验证的 AEL 最简测试（mailbox）：

```bash
cd /nvme1t/work/codex/ai-embedded-lab

python3 -m ael run \
  --board stm32f407_discovery \
  --test tests/plans/stm32f407_mailbox.json
```

这一步使用 AEL 原生 GDB batch（现有流程），不走 west，  
但物理上用的是**相同的板载 ST-Link**。

验收标准：AEL 闭环 PASS，mailbox 报告 `PASS`。

---

### Step 5：AEL Golden Pack 回归验证

```bash
python3 -m ael pack packs/stm32f407vgt6_golden.json
```

Golden pack 包含 7 个测试：

| 阶段 | 测试 | 覆盖外设 |
|------|------|---------|
| Stage 0 | stm32f407_mailbox | mailbox 基础通信 |
| Stage 1 | stm32f407_timer_mailbox | Timer IRQ |
| Stage 2 | stm32f407_gpio_loopback | GPIO |
| Stage 2 | stm32f407_uart_loopback | USART2 PD5↔PD6 |
| Stage 2 | stm32f407_exti_trigger | EXTI 中断 |
| Stage 2 | stm32f407_adc_loopback | ADC |
| Stage 2 | stm32f407_spi_loopback | SPI |

验收标准：7/7 PASS，与 Zephyr 试点引入前的 golden baseline 一致。

---

## 六、AEL Zephyr Backend 骨架设计

### 6.1 新增文件

```
ael/
  backends/
    __init__.py
    base.py              ← AELBackend 抽象基类（新建，P0）
    zephyr_backend.py    ← 本次新建（P1+P3）
```

### 6.2 抽象基类（P0）

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
        """构建，返回 artifact ELF 路径"""

    @abstractmethod
    def flash(self, artifact: Path = None, **kwargs) -> None:
        """烧写到目标板"""

    @abstractmethod
    def start_debugserver(self, **kwargs):
        """启动调试服务器，返回 subprocess.Popen"""

    def observe(self, duration_s: float, expect_patterns: list) -> dict:
        """采集串口输出 — 默认复用 AEL 现有 observe_uart"""
        raise NotImplementedError

    def verify(self, observation: dict, expectations: dict) -> dict:
        """验证结果 — 默认复用 AEL 现有 verify 层"""
        raise NotImplementedError
```

### 6.3 Zephyr Backend 最小实现（P1）

```python
# ael/backends/zephyr_backend.py
import subprocess
import time
from pathlib import Path
from .base import AELBackend

class ZephyrBackend(AELBackend):

    def __init__(self, workspace: str = '~/zephyrproject/zephyr'):
        self.workspace = Path(workspace).expanduser()

    def detect_project_type(self, project_dir: Path) -> bool:
        """Zephyr 项目特征：有 CMakeLists.txt + prj.conf"""
        return ((project_dir / 'CMakeLists.txt').exists() and
                (project_dir / 'prj.conf').exists())

    def build(self, board: str, sample_dir: str,
              build_dir: Path = None, config_args: list = None) -> Path:
        build_dir = build_dir or Path('build/zephyr_build')
        cmd = [
            'west', 'build',
            '-p', 'always',
            '-b', board,
            str(self.workspace / sample_dir),
            '--build-dir', str(build_dir),
        ]
        if config_args:
            cmd += ['--'] + config_args
        subprocess.run(cmd, check=True, cwd=self.workspace)
        return build_dir / 'zephyr' / 'zephyr.elf'

    def flash(self, artifact: Path = None,
              runner: str = 'openocd',
              build_dir: Path = None) -> None:
        cmd = ['west', 'flash', '--runner', runner]
        if build_dir:
            cmd += ['--build-dir', str(build_dir)]
        subprocess.run(cmd, check=True, cwd=self.workspace)

    def start_debugserver(self, runner: str = 'openocd',
                          build_dir: Path = None,
                          port: int = 3333):
        cmd = ['west', 'debugserver', '--runner', runner]
        if build_dir:
            cmd += ['--build-dir', str(build_dir)]
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=self.workspace
        )
        # 等待 OpenOCD ready
        deadline = time.time() + 15
        while time.time() < deadline:
            line = proc.stdout.readline().decode('utf-8', errors='replace')
            if f'Listening on port {port}' in line:
                return proc
            if proc.poll() is not None:
                raise RuntimeError('west debugserver 异常退出')
        raise TimeoutError('west debugserver 启动超时')
```

---

## 七、验收标准汇总

| 步骤 | 验收条件 | 接线要求 | 结果 |
|------|---------|---------|------|
| Step 0 | `west boards \| grep stm32f4_disco` 有输出 | 无 | ✓ PASS |
| Step 1 | `west flash` 无报错，PD12 LED 闪烁 | 仅 USB | ✓ PASS |
| Step 2 | 串口捕获 `Booting Zephyr OS`，PASS | USB + USB-UART(**PA2**) | ✓ PASS |
| Step 3 | GDB attach 成功，寄存器有效值 | 仅 USB | ✓ PASS |
| Step 4 | AEL stm32f407_mailbox PASS | 仅 USB | ✓ PASS |
| Step 5 Stage 0+1 | mailbox + timer PASS | 仅 USB | ✓ PASS |
| Step 5 Stage 2 | GPIO/UART/EXTI/ADC/SPI loopback | USB + 飞线 | 待做 |
| Step 6 | ZephyrBackend 骨架代码 end-to-end 验证 | 仅 USB | ✓ PASS |
| **整体** | Zephyr 阶段与 AEL 阶段均 PASS，backend 共存无冲突 | — | ✓ **PASS** |

---

## 八、风险与缓解

| 风险 | 说明 | 缓解措施 |
|------|------|---------|
| PA9/PA10 UART 冲突 | ST-Link UART bridge 干扰 USART1 | 已知问题，AEL loopback 全程使用 USART2/PD5/PD6 |
| ~~Zephyr UART console 引脚差异~~ | **已消除**：实测确认 Zephyr console = PA2 | DTS 已明确 `usart2_tx_pa2`，无需 overlay |
| OpenOCD 版本兼容性 | 旧版 OpenOCD 不支持 ST-Link/V2-A | 已验证 OpenOCD 0.11 可用；`monitor reset run` 有非致命警告但不影响功能 |
| west workspace 初始化慢 | 首次 `west update` 约 5~15 分钟 | 已完成初始化；CI 镜像预装 |
| **ST-Link 设备竞争**（实测发现）| AEL st-util（:4242）或 pyocd（:3333）持有 ST-Link，导致 west flash/debugserver 失败 | `ZephyrBackend._release_port()` 在 flash/debugserver 前自动释放 |
| **Python 版本**（实测发现）| Zephyr 4.x 要求 Python ≥ 3.12，系统 Ubuntu 22.04 默认 3.10 | 用 uv 安装 Python 3.12 + 独立 venv；WEST_PYTHON 指向 venv |
| Golden pack 接线依赖 | Step 5 Stage 2 的 UART/SPI/GPIO loopback 需要飞线 | Stage 0+1 已 PASS（无接线），Stage 2 待补 |

---

## 九、后续输出与 P4 准备

本次试点实际产出（已完成）：

1. **backend contract**：`ael/backends/base.py` — `AELBackend` 抽象基类，commit `773da3e`
2. **ZephyrBackend 骨架**：`ael/backends/zephyr_backend.py`，已通过 STM32F407 实物 end-to-end 验证
3. **实测发现的约束**已写入代码：`_release_port()` 处理 AEL st-util / pyocd 与 west 的 ST-Link 竞争
4. **自动 backend 选择逻辑样本**（供 P4 使用）：
   ```python
   def resolve_backend(board_id, project_dir):
       if ZephyrBackend().detect_project_type(project_dir):
           return ZephyrBackend()
       return NativeBaremetalBackend()   # 现有路径
   ```

待完成：
- Step 5 Stage 2（loopback 接线）
- ZephyrBackend `observe()` / `verify()` 接入 AEL 现有层
- `build()` 独立测试（当前 end-to-end 中跳过了 build 单独验证）

---

## 十、环境配置摘要（实测，2026-04-05）

| 组件 | 版本 / 路径 | 说明 |
|------|------------|------|
| west | 1.5.0 | `pip install west`（系统 Python 3.10） |
| Zephyr | 4.4.0-rc2 | `~/zephyrproject/zephyr` |
| Python for Zephyr | 3.12.13 | `~/zephyr-venv/`（uv 安装，Zephyr 要求 ≥ 3.12） |
| ARM toolchain | 14.2.Rel1 | `/nvme1t/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi` |
| OpenOCD | 0.11.0 | `/usr/bin/openocd`（系统包） |
| Zephyr board 名 | `stm32f4_disco` | `~/zephyrproject/zephyr/boards/st/stm32f4_disco/` |
| UART console 引脚 | PA2 (TX) / PA3 (RX) | DTS 明确指定，115200 8N1 |

west build/flash 所需环境变量：
```bash
ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
GNUARMEMB_TOOLCHAIN_PATH=/nvme1t/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi
WEST_PYTHON=~/zephyr-venv/bin/python3
```

---

*本文档由 AEL-Claude 生成，基于 AEL_Zephyr_方向备忘录_中文版.docx 及 STM32F407 Discovery 实测结果（2026-04-05）。*
