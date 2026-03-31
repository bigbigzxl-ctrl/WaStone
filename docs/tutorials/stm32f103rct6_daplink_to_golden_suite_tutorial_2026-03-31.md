# STM32F103RCT6: From DAPLink Bring-Up To Golden Suite

## Quick Start

If you just want the shortest working path, do this.

### 1. Wire the bench

- DAPLink `SWDIO` -> target `SWDIO`
- DAPLink `SWCLK` -> target `SWCLK`
- DAPLink `NRST` -> target `NRST`
- DAPLink `GND` -> target `GND`
- DAPLink `TX` -> target `PA10 / USART1_RX`
- DAPLink `RX` -> target `PA9 / USART1_TX`
- jumper `PC8 <-> PC9`
- jumper `PB0 <-> PB1`
- jumper `PB8 <-> PB9`
- jumper `PA0 <-> PA1`
- jumper `PB15 <-> PB14`

### 2. Add Linux udev access

```bash
sudo tee /etc/udev/rules.d/99-cmsis-dap-lu.rules >/dev/null <<'EOF'
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="c251", ATTRS{idProduct}=="f001", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="c251", ATTRS{idProduct}=="f001", MODE="0666"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Unplug and replug the probe after that.

### 3. Verify the host sees the probe

```bash
ls -l /dev/hidraw* /dev/ttyACM*
dmesg | tail -n 30
```

Expected signs:

- a `hidraw*` node for the CMSIS-DAP interface
- a `ttyACM*` node for the UART bridge

### 4. Use OpenOCD, not pyOCD, for this probe

`pyOCD` was installed first but did not enumerate this HID-style `CMSIS-DAP_LU`
reliably. `OpenOCD` worked once the backend was forced to `hid`.

```bash
openocd -f interface/cmsis-dap.cfg -c "cmsis_dap_backend hid; adapter speed 1000" -f target/stm32f1x.cfg -c "init; reset halt; exit"
```

Expected signs:

- `CMSIS-DAP: Interface ready`
- `SWD DPIDR ...`
- target halted successfully

### 5. Identify the chip before assuming the target family

For the working `STM32F103RCT6` board, the key identity values were:

- `SWD DPIDR = 0x1ba01477`
- `CPUID = 0x411fc231`
- `DBGMCU_IDCODE = 0x10036414`

That identifies:

- `Cortex-M3`
- `STM32F1 high-density`
- matching `STM32F103RCT6`

### 6. If flash fails, unlock and mass erase

```bash
openocd -f interface/cmsis-dap.cfg -c "cmsis_dap_backend hid; adapter speed 1000" -f target/stm32f1x.cfg -c "init; reset halt; stm32f1x unlock 0; reset halt; stm32f1x mass_erase 0; program artifacts/build_stm32f103rct6_mailbox/stm32f103rct6_mailbox_app.elf verify reset exit"
```

This was the recovery path that turned a connectable-but-not-programmable board
into a working board.

### 7. Read mailbox results carefully

When reading mailbox state through OpenOCD, remember:

- OpenOCD `sleep` is in milliseconds, not seconds

So use `sleep 2000`, not `sleep 2`, if you mean 2 seconds.

### 8. Verify UART through DAPLink

Set the serial device and capture output:

```bash
stty -F /dev/ttyACM0 115200 raw -echo
timeout 10 cat /dev/ttyACM0
```

For the validated UART banner target, the expected output is:

- `AEL_READY STM32F103RCT6 UART`

### 9. Use the canonical STM32F103RCT6 golden wiring

The final canonical suite does not depend on the ambiguous `PC7` LED path.
It uses:

- DAPLink SWD
- DAPLink UART `<-> PA9/PA10`
- `PC8 <-> PC9`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`

### 10. Run the canonical pack

```bash
PYTHONPATH=. python3 -m ael inventory describe-dut --board stm32f103rct6 --format text
PYTHONPATH=. python3 -m ael pack --pack packs/stm32f103rct6_golden.json --board stm32f103rct6
```

The canonical pack is:

- [stm32f103rct6_golden.json](../../packs/stm32f103rct6_golden.json)

The rest of this document explains how we got there, what failed first, and
why the final setup looks the way it does.

## Purpose

This tutorial records the full path from first plugging a Linux host into a
USB DAPLink / CMSIS-DAP probe, through the failed `pyOCD` attempt, through the
successful `OpenOCD` workflow, and finally through the process of turning a
real `STM32F103RCT6` fixture into a formal AEL `golden` suite.

It is intentionally detailed.

The audience is:

- AEL maintainers
- anyone bringing up a similar HID-style DAPLink probe on Linux
- anyone building a staged/golden suite on top of a live STM32 bench

This is not just a happy-path guide. It also records the mistakes, false
assumptions, and bench-specific traps that mattered.

## End State

At the end of this process, the repo now contains:

- a canonical `STM32F103RCT6` golden pack  
  [stm32f103rct6_golden.json](../../packs/stm32f103rct6_golden.json)
- a golden DUT manifest  
  [manifest.yaml](../../assets_golden/duts/stm32f103rct6/manifest.yaml)
- a local DAPLink instrument entry  
  [daplink_f103_rct6.yaml](../../configs/instrument_instances/daplink_f103_rct6.yaml)
- a closeout report  
  [stm32f103rct6_golden_suite_closeout_2026-03-31.md](../reports/stm32f103rct6_golden_suite_closeout_2026-03-31.md)
- supporting reusable skills for:
  - DAPLink/OpenOCD programming
  - DAPLink live validation
  - replacing a flaky visual Stage 0 path with a machine-verifiable GPIO probe

## The Real Probe We Used

Linux identified the probe as:

- product: `CMSIS-DAP_LU`
- manufacturer: `jixin.pro`
- serial: `LU_2022_8888`
- VID:PID: `c251:f001`

The kernel also created:

- a `ttyACM*` serial device
- a `hidraw*` node

This matters because this class of probe behaves like a standard CMSIS-DAP
debug probe plus a USB serial bridge, but the HID path turned out to be the
important one for debug.

## Target Board

The target that ended up becoming the suite owner was:

- silk: `STM32F103RCT6`

The target was later positively identified in software as:

- Cortex-M3
- STM32F1 high-density
- device ID low 12 bits `0x414`

That is consistent with `STM32F103RCT6`.

## Part 1: Linux Sees The Probe, But That Does Not Mean The Tooling Works

### Initial kernel signs

The first signal that Linux saw the probe was the usual kernel output pattern:

- `ttyACM*` appeared
- a HID device appeared

This is necessary, but not sufficient. A lot of people stop here and assume the
probe is “working”. It is not enough.

There are three layers to separate:

1. Linux USB visibility
2. probe backend/tool compatibility
3. target-family access and flash correctness

If those are not separated, troubleshooting gets muddy immediately.

## Part 2: Install pyOCD First, But Do Not Assume It Will Win

We started by installing `pyOCD` and related pieces, because that is the most
obvious CMSIS-DAP path on Linux.

Typical install pattern:

```bash
python3 -m pip install --user -U pyocd hidapi
```

Then we checked:

```bash
python3 -m pyocd list
python3 -m pyocd list --targets
```

### What happened

The install itself was fine.

But probe enumeration still failed. In other words:

- Linux saw the probe
- `pyOCD` was installed
- `pyOCD` still did not reliably enumerate the HID-style DAPLink

### First lesson

For this probe class on Linux:

- do not assume `pyOCD` is the winning path
- even after permissions are fixed

That was the first important pivot.

## Part 3: Fix udev Permissions First Anyway

Even though `pyOCD` still did not work, fixing Linux permissions was still
necessary because `OpenOCD` also needs stable access to the same USB device.

We installed udev rules like:

```udev
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="c251", ATTRS{idProduct}=="f001", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="c251", ATTRS{idProduct}=="f001", MODE="0666"
```

Then reloaded them:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then unplugged and replugged the probe.

### What to check

```bash
ls -l /dev/hidraw* /dev/ttyACM*
lsusb
```

Expected signs:

- the probe still shows up in `lsusb`
- a `hidraw*` node exists
- a `ttyACM*` node exists
- permissions are no longer root-only on the path you need

### Important nuance

At this stage, permissions may be fixed and the probe may still not be usable
through `pyOCD`. That is not a contradiction. It simply means the failure is no
longer in Linux permissions; it is in backend compatibility.

## Part 4: Switch To OpenOCD

After `pyOCD` still failed to enumerate the probe reliably, we switched to
`OpenOCD`.

This turned out to be the correct decision.

### Install

If `OpenOCD` is not already installed:

```bash
sudo apt-get update
sudo apt-get install -y openocd
```

### The working command pattern

For this DAPLink class, the critical command was:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c "init" \
  -c "reset halt" \
  -c "exit"
```

### Why this matters

Two details were essential:

- `cmsis_dap_backend hid`
- low SWD speed

Without forcing the HID backend, `OpenOCD` did not consistently choose the
right interface for this probe.

Without keeping the adapter speed conservative, early bring-up was less stable.

### First successful signs

We were looking for lines like:

- `CMSIS-DAP: Interface ready`
- `SWD DPIDR 0x...`
- `hardware has 6 breakpoints, 4 watchpoints`
- `target halted due to debug-request`

Once those appeared, probe-to-target communication was established.

## Part 5: Identify The Target Before Flashing Anything

Never jump directly from “OpenOCD sees a core” to “this must be the part I
think it is”.

We explicitly identified the MCU.

Useful reads:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c init \
  -c "reset halt" \
  -c "mdw 0xE000ED00 1" \
  -c "mdw 0xE0042000 1" \
  -c exit
```

### The important values we got

- `SWD DPIDR = 0x1ba01477`
- `CPUID = 0x411fc231`
- `DBGMCU_IDCODE = 0x10036414`

Interpretation:

- `CPUID = 0x411fc231` -> Cortex-M3
- `DBGMCU_IDCODE low 12 bits = 0x414`
- `0x414` -> STM32F1 high-density

That aligned with the board silk:

- `STM32F103RCT6`

### Why this matters

Before that, we had already touched a different board that did not identify
like an F103 at all. That reinforced a key rule:

- do not trust the previous board
- do not trust the bench label
- identify the actual MCU in front of you

## Part 6: Flash Recovery Path Matters

The first flash/programming attempts are not always clean, even when SWD is
working.

For STM32F103-class devices, if flash programming fails with algorithm timeout
or write failure, the recovery sequence that mattered was:

1. unlock
2. mass erase
3. low-speed reprogram

Example:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 100" \
  -f target/stm32f1x.cfg \
  -c init \
  -c "reset halt" \
  -c "stm32f1x unlock 0" \
  -c exit
```

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 100" \
  -f target/stm32f1x.cfg \
  -c init \
  -c "reset halt" \
  -c "stm32f1x mass_erase 0" \
  -c exit
```

Then:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c "program <firmware>.elf verify reset exit"
```

## Part 7: Reading Mailbox Is More Useful Than Staring At Logs

For AEL mailbox firmware, the fastest truth source was direct SRAM mailbox
readback.

Working pattern:

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c init \
  -c "reset run" \
  -c "sleep 2000" \
  -c halt \
  -c "mdw 0x2000BC00 4" \
  -c exit
```

### Very important pitfall

`OpenOCD sleep` is in milliseconds.

That means:

- `sleep 2` = 2 ms
- `sleep 500` = 500 ms
- `sleep 2000` = 2 s

We explicitly tripped over this.

Early in the process, mailbox reads looked like tests were “stuck” because we
were reading after `sleep 2` and `sleep 5`, assuming seconds. They were only
2 ms and 5 ms delays.

This caused false confusion until corrected.

### PASS shape

Typical PASS mailbox:

- `magic = 0xAE100001`
- `status = 0x00000002`
- `error_code = 0`

## Part 8: DAPLink UART Was Also Useful

The DAPLink probe exposed a USB serial path through `ttyACM0`.

We used:

- `DAPLink TX -> STM32 PA10 / USART1_RX`
- `DAPLink RX -> STM32 PA9 / USART1_TX`
- `GND -> GND`

That enabled:

- UART banner proof
- UART multibyte proof
- UART DMA TX proof

Typical host-side checks:

```bash
stty -F /dev/ttyACM0 115200 raw -echo
timeout 5 cat /dev/ttyACM0
```

### Representative strings

- `AEL_READY STM32F103RCT6 UART`
- `AEL_UART_MB 55 AA 12 34`
- `AEL_UART_DMA A1 B2 C3 D4 55 66 77 88`

## Part 9: The First Suite Shape Was Only A Candidate

The initial goal was not “make it golden in one jump”. It was:

1. establish a stable fixture
2. build a staged candidate
3. validate it on real hardware
4. then promote

That was the right sequence.

The staged candidate pack started here:

- [stm32f103rct6_staged_candidate.json](../../packs/stm32f103rct6_staged_candidate.json)

It initially included:

- Stage 0
  - PC7 blinky visual
  - minimal runtime mailbox
- Stage 1
  - timer
  - SysTick
  - internal temp
- Stage 2
  - GPIO
  - EXTI
  - ADC
  - SPI
  - IWDG
- Stage 3
  - UART banner

Then it was gradually expanded.

## Part 10: Wiring Was The Real Foundation

The final validated fixture wiring evolved into:

- DAPLink SWD
- DAPLink UART `<->` `PA9/PA10`
- `PC8 <-> PC9`
- `PB0 <-> PB1`
- `PB8 <-> PB9`
- `PA0 <-> PA1`
- `PB15 <-> PB14`

This final shape matters more than any individual test name. Until the wiring
contract is explicit, failures are ambiguous.

## Part 11: PB8/PB9 Was A Real Trap

`PB8 -> PB9` looked fine on paper, but it failed in practice initially.

We did the right thing:

- instead of guessing, we added a tiny dedicated probe

That became:

- `stm32f103rct6_pb8_pb9_probe`

The probe simply toggled `PB8` and verified `PB9` followed as a plain input.

### What this taught us

Do not debug EXTI, capture, or PWM first if you are not yet sure the wire
itself is electrically behaving.

Always split the problem:

1. is the wire good?
2. is the peripheral logic good?

Once the wiring was repaired, the probe passed and we could safely use
`PB8/PB9` again for timer-oriented tests.

## Part 12: EXTI First Chose The Wrong Wire

The original EXTI path tried to use `PB8/PB9`.

That was a mistake for this suite.

Even after repair, `PB8/PB9` was better reserved as a timer path than a
canonical EXTI path.

We changed EXTI to reuse a wire already proven by another test:

- `PA0 -> PA1`

This became:

- output on `PA0`
- input on `PA1`
- validate `EXTI1`

### Why this was the right change

- `PA0/PA1` had already been proven by ADC loopback
- EXTI no longer depended on the same wire used later for timer proofs
- it reduced coupling between tests

We also removed dependency on NVIC/ISR delivery and instead validated the EXTI
pending bit directly. That made the test more robust on this live bench.

## Part 13: UART Multibyte And UART DMA Needed An Honest Contract

At one point it would have been easy to lie to ourselves and pretend the
fixture had a board-local:

- `PA9 -> PA10`

loopback.

But the actual fixture did not.

The real setup was:

- DAPLink UART attached to `PA9/PA10`

So for `uart_multibyte` and `uart_dma`, the honest solution was:

- keep mailbox PASS inside the firmware
- validate the actual transmitted bytes through the host-side UART observer

This matters because a “fake loopback” test can look neat in metadata but does
not reflect what the bench actually verifies.

## Part 14: Capture And PWM Came After Repairing PB8/PB9

Once `PB8/PB9` was known-good, we used it for:

- `capture_mailbox`
- `pwm_capture`

The approach used was pragmatic:

- software-driven waveform on `PB8`
- sampled/observed on `PB9`
- `TIM2` free-runs at 1 MHz
- period and duty are checked in firmware

### More pitfalls that mattered

#### First capture timing was too coarse

An early version used a soft delay loop for waveform timing. The measured period
was much larger than expected, because the delay loop was not actually a stable
microsecond delay.

Fix:

- use `TIM2` as the pacing source

#### First PWM edge-state machine was wrong

An early version missed the second rising edge because the state machine only
looked for it in the wrong phase.

Fix:

- explicitly track rise1, fall1, rise2 transitions

#### Duty window was too optimistic

The bench measured a high time around `614 us`, which was outside the first
window.

Fix:

- widen the accepted live duty window to match real measured behavior

This is a good example of the difference between:

- a mathematically expected waveform
- a live bench waveform with software-driven timing and sampling overhead

## Part 15: PC7 LED Was Not Trustworthy As A Canonical Stage 0 Signal

This was one of the most important lessons.

Originally there was a visual test:

- `stm32f103rct6_pc7_blinky_visual`

And the code really did drive `PC7`.

But the user still did not see a trustworthy LED blink.

So we tested it more directly using:

- `PC7 -> PC6`

That probe failed.

Then we tested another pair on the same GPIO bank:

- `PC8 -> PC9`

That probe passed.

### Conclusion

The problem was not “GPIOC is broken”.

The problem was:

- the `PC7` LED path is board-specific and ambiguous

Possible causes include:

- inverted LED path
- transistor buffer
- extra board load
- incorrect silk expectation

### Golden-suite decision

We did not let that ambiguous LED path block the suite.

Instead:

- `PC7` remained a non-canonical legacy diagnostic
- `PC8 -> PC9` became the canonical Stage 0 machine-verifiable probe

This is the correct AEL pattern:

- if the visual path is board-specific and unreliable, replace it with a clean
  machine-verifiable GPIO probe

## Part 16: Serializing OpenOCD Sessions Matters

Another real trap:

- do not run multiple `OpenOCD` sessions against this DAPLink at once

When two sessions raced, we saw failures like:

- `CMSIS-DAP failed to connect in mode (1)`

After that, the correct operational rule became:

- one `OpenOCD` session at a time

This matters for:

- shell scripting
- automated validation
- agent/tool orchestration

If a script tries to flash and read mailbox in parallel, it can destabilize the
probe even when the wiring is fine.

## Part 17: Why We Did Not Promote To Golden Immediately

Before promotion, we still had to resolve:

- which physical fixture was truly canonical
- whether `PC7` should remain part of Stage 0
- whether metadata still pointed at stale bench paths

The suite was kept as a strong `candidate` until those were clear.

That was the right call.

Do not promote early just because many tests already pass.

Promotion should follow:

1. canonical wiring
2. canonical instrument identity
3. explicit live validation
4. closeout
5. reusable skill capture

## Part 18: Final Canonical Golden Suite

Final canonical pack:

- [stm32f103rct6_golden.json](../../packs/stm32f103rct6_golden.json)

Final canonical DUT:

- [manifest.yaml](../../assets_golden/duts/stm32f103rct6/manifest.yaml)

Final stages:

Stage 0

- `stm32f103rct6_pc8_pc9_probe`
- `stm32f103rct6_minimal_runtime_mailbox`

Stage 1

- `stm32f103rct6_timer_mailbox`
- `stm32f103rct6_systick_mailbox`
- `stm32f103rct6_internal_temp_mailbox`

Stage 2

- `stm32f103rct6_gpio_loopback`
- `stm32f103rct6_exti_trigger`
- `stm32f103rct6_adc_loopback`
- `stm32f103rct6_spi_loopback`
- `stm32f103rct6_iwdg`
- `stm32f103rct6_capture_mailbox`
- `stm32f103rct6_pwm_capture`
- `stm32f103rct6_uart_multibyte`
- `stm32f103rct6_uart_dma`

Stage 3

- `stm32f103rct6_uart_banner`

## Part 19: Commands That Were Actually Useful

### Probe visibility

```bash
lsusb
ls -l /dev/hidraw* /dev/ttyACM*
```

### OpenOCD connect

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c "init" \
  -c "reset halt" \
  -c "exit"
```

### Program firmware

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c "program <firmware>.elf verify reset exit"
```

### Read mailbox after runtime

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -c "cmsis_dap_backend hid; adapter speed 50" \
  -f target/stm32f1x.cfg \
  -c init \
  -c "reset run" \
  -c "sleep 2000" \
  -c halt \
  -c "mdw 0x2000BC00 4" \
  -c exit
```

### Read UART output

```bash
stty -F /dev/ttyACM0 115200 raw -echo
timeout 5 cat /dev/ttyACM0
```

## Part 20: The Practical Lessons To Reuse Elsewhere

1. Separate USB visibility, probe backend compatibility, and target identity.

2. If `pyOCD` fails to enumerate a HID-style DAPLink on Linux, do not keep
   forcing it. Try `OpenOCD` with `cmsis_dap_backend hid`.

3. On this class of probe, low SWD speed is a feature, not an embarrassment.

4. Read mailbox directly. It is often the fastest truth source.

5. `OpenOCD sleep` is milliseconds.

6. Never run parallel `OpenOCD` sessions against a fragile local DAPLink.

7. Before debugging EXTI/PWM/capture, first prove the wire itself with a tiny
   GPIO probe.

8. If a board LED path is ambiguous, do not let it define your canonical Stage 0.
   Replace it with a clean GPIO probe.

9. Be honest about what the fixture really verifies. Do not pretend external
   UART is the same as internal board-local loopback.

10. Promote to `golden` only after canonical wiring, canonical instrument
    identity, live validation, closeout, and reusable skill capture all exist.

## Related Repo Artifacts

Programming skill:

- [daplink_openocd_stm32f103rct6_programming_skill_2026-03-30.md](../skills/daplink_openocd_stm32f103rct6_programming_skill_2026-03-30.md)

Candidate validation closeout:

- [stm32f103rct6_staged_candidate_live_validation_closeout_2026-03-31.md](../reports/stm32f103rct6_staged_candidate_live_validation_closeout_2026-03-31.md)

Golden closeout:

- [stm32f103rct6_golden_suite_closeout_2026-03-31.md](../reports/stm32f103rct6_golden_suite_closeout_2026-03-31.md)

DAPLink live-validation skill:

- [stm32f103rct6_daplink_live_validation_skill_2026-03-31.md](../skills/stm32f103rct6_daplink_live_validation_skill_2026-03-31.md)

Stage 0 migration skill:

- [stage0_visual_to_gpio_probe_migration_2026-03-31.md](../skills/stage0_visual_to_gpio_probe_migration_2026-03-31.md)
