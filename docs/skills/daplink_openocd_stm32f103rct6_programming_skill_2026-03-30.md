# DAPLink OpenOCD STM32F103RCT6 Programming

## Purpose

Use this skill when a Linux host sees a USB DAPLink / CMSIS-DAP probe and you
need to identify and program an STM32-class board over SWD without relying on
the repo's normal bench adapters.

## Trigger

Apply this when:

- `lsusb` shows a CMSIS-DAP style probe such as `CMSIS-DAP_LU`
- Linux creates a `ttyACM*` device and a HID interface
- `pyOCD` does not enumerate the probe reliably
- you need direct `OpenOCD` + SWD access to a target board

## Source Case

Source probe:

- USB VID:PID `c251:f001`
- product string `CMSIS-DAP_LU`
- manufacturer `jixin.pro`
- serial `LU_2022_8888`

Source target:

- silk: `STM32F103RCT6`

Observed path:

- `pyOCD` and `hidapi` were installed successfully on Linux
- `pyOCD list` still did not enumerate this HID-based probe
- `OpenOCD` initially chose the wrong CMSIS-DAP interface unless the backend
  was forced to HID

## Core Rule

For HID-style DAPLink probes on Linux, do not assume `pyOCD` will be the most
reliable path even after permissions are fixed.

If `pyOCD list` still shows no probes, switch to `OpenOCD` and force:

- `cmsis_dap_backend hid`

For STM32F103-class targets, use low SWD speed plus unlock/mass-erase recovery
if flash programming times out on the first try.

## Procedure

1. Confirm Linux sees the probe.

   Indicators:
   - `lsusb` shows the probe VID:PID
   - the kernel creates `ttyACM*`
   - a HID interface exists for the same device

2. Fix udev access first.

   Example rule for this probe:
   - `SUBSYSTEM=="hidraw", ATTRS{idVendor}=="c251", ATTRS{idProduct}=="f001", MODE="0666"`
   - `SUBSYSTEM=="tty", ATTRS{idVendor}=="c251", ATTRS{idProduct}=="f001", MODE="0666"`

3. If `pyOCD` still does not enumerate the probe, switch to `OpenOCD`.

4. For this DAPLink class on Linux, force the HID backend.

   Working pattern:

   ```bash
   openocd \
     -f interface/cmsis-dap.cfg \
     -c "cmsis_dap_backend hid; adapter speed 50" \
     -f target/stm32f1x.cfg \
     -c "init" -c "reset halt" -c "exit"
   ```

5. Identify the target before flashing.

   Useful reads:

   ```bash
   openocd \
     -f interface/cmsis-dap.cfg \
     -c "cmsis_dap_backend hid; adapter speed 1000; reset_config srst_only srst_nogate connect_assert_srst" \
     -f target/stm32f1x.cfg \
     -c init \
     -c "reset halt" \
     -c "mdw 0xE000ED00 1" \
     -c "mdw 0xE0042000 1" \
     -c "mdw 0x1FFFF7E0 1" \
     -c "mdw 0x1FFFF7E8 1" \
     -c exit
   ```

6. For STM32F103RCT6, treat these values as a positive identification:

   - `SWD DPIDR = 0x1ba01477`
   - `CPUID = 0x411fc231` -> Cortex-M3
   - `DBGMCU_IDCODE = 0x10036414`
   - low 12 bits `0x414` -> STM32F1 high-density

7. If first flash attempt fails with:

   - `timeout waiting for algorithm`
   - `flash write failed`

   then recover in this order:

   ```bash
   openocd \
     -f interface/cmsis-dap.cfg \
     -c "cmsis_dap_backend hid; adapter speed 100; reset_config srst_only srst_nogate connect_assert_srst" \
     -f target/stm32f1x.cfg \
     -c init \
     -c "reset halt" \
     -c "stm32f1x unlock 0" \
     -c exit
   ```

   ```bash
   openocd \
     -f interface/cmsis-dap.cfg \
     -c "cmsis_dap_backend hid; adapter speed 100; reset_config srst_only srst_nogate connect_assert_srst" \
     -f target/stm32f1x.cfg \
     -c init \
     -c "reset halt" \
     -c "stm32f1x mass_erase 0" \
     -c exit
   ```

8. Retry programming at low speed.

   Working pattern:

   ```bash
   openocd \
     -f interface/cmsis-dap.cfg \
     -c "cmsis_dap_backend hid; adapter speed 50" \
     -f target/stm32f1x.cfg \
     -c "program <firmware>.elf verify reset exit"
   ```

9. For mailbox validation, let the firmware run briefly, then halt and read the
   mailbox region directly.

   Working pattern:

   ```bash
   openocd \
     -f interface/cmsis-dap.cfg \
     -c "cmsis_dap_backend hid; adapter speed 50" \
     -f target/stm32f1x.cfg \
     -c init \
     -c "reset run" \
     -c "sleep 500" \
     -c halt \
     -c "mdw 0x2000BC00 4" \
     -c exit
   ```

   PASS signature for the F103 mailbox firmware:
   - `magic = 0xAE100001`
   - `status = 0x00000002`
   - `error_code = 0`

## Reusable Lesson

For Linux-hosted DAPLink bring-up, separate three layers:

- USB/udev visibility
- probe backend choice (`pyOCD` vs `OpenOCD`)
- target-family identification before programming

For HID-style CMSIS-DAP probes that enumerate as `CMSIS-DAP_LU`, `OpenOCD`
with `cmsis_dap_backend hid` can succeed even when `pyOCD` does not enumerate
the probe.

For STM32F103RCT6, use ID reads to confirm `0x414` high-density identity before
writing flash, and recover flash timeouts with `unlock -> mass_erase -> low
speed reprogram`.
