#!/usr/bin/env python3
"""
STM32F407VET6 GPIO wire-scan via BMDA GDB.

Technique (HIGH_PRIORITY df19dd7c):
  - Configure driver pin: output push-pull
  - Configure input pin: input with pull-up (reads HIGH when floating)
  - Drive driver HIGH → input should read HIGH
  - Drive driver LOW  → input should read LOW  (wire confirmed)

Ports tested:
  PB6  → PB7   (I2C1 SDA/SCL - new wire)
  PB8  → PB9   (EXTI test - new wire)
  PC0  → PC1   (ADC loopback - new wire)
  PC2  → PC3   (GPIO loopback - existing)
  PA7  → PA6   (SPI MOSI→MISO - existing)
  PD5  → PD6   (UART loopback - GPIO mode test only)
"""

import subprocess
import sys
import re

BMDA_HOST = "192.168.2.98"
BMDA_PORT = 4242
GDB_BIN   = "arm-none-eabi-gdb"

# STM32F407 register addresses
RCC_AHB1ENR = 0x40023830

GPIOA_BASE = 0x40020000
GPIOB_BASE = 0x40020400
GPIOC_BASE = 0x40020800
GPIOD_BASE = 0x40020C00

# Offsets
OFF_MODER = 0x00
OFF_PUPDR = 0x0C
OFF_IDR   = 0x10
OFF_BSRR  = 0x18

# Wire pairs: (drv_base, drv_pin, in_base, in_pin, label)
WIRES = [
    (GPIOB_BASE, 6,  GPIOB_BASE, 7,  "PB6 → PB7  (I2C1 new)"),
    (GPIOB_BASE, 8,  GPIOB_BASE, 9,  "PB8 → PB9  (EXTI new)"),
    (GPIOC_BASE, 0,  GPIOC_BASE, 1,  "PC0 → PC1  (ADC loopback new)"),
    (GPIOC_BASE, 2,  GPIOC_BASE, 3,  "PC2 → PC3  (GPIO existing)"),
    (GPIOA_BASE, 7,  GPIOA_BASE, 6,  "PA7 → PA6  (SPI MOSI→MISO existing)"),
    (GPIOD_BASE, 5,  GPIOD_BASE, 6,  "PD5 → PD6  (UART loopback existing)"),
]

def build_gdb_script():
    lines = [
        f"target extended-remote {BMDA_HOST}:{BMDA_PORT}",
        "monitor swdp_scan",
        "attach 1",
        "monitor halt",
        # Enable GPIOA(0), GPIOB(1), GPIOC(2), GPIOD(3) clocks
        f"set *(unsigned int *)0x{RCC_AHB1ENR:08X} = "
        f"*(unsigned int *)0x{RCC_AHB1ENR:08X} | 0x0F",
    ]

    for drv_base, drv_pin, in_base, in_pin, label in WIRES:
        drv_moder = drv_base + OFF_MODER
        drv_bsrr  = drv_base + OFF_BSRR
        in_moder  = in_base  + OFF_MODER
        in_pupdr  = in_base  + OFF_PUPDR
        in_idr    = in_base  + OFF_IDR

        # Use a unique marker comment so we can parse output
        safe_label = label.replace(" ", "_").replace("(", "").replace(")", "")

        lines += [
            f"# --- {safe_label} ---",
            # driver pin: MODER[2*drv_pin+1:2*drv_pin] = 01 (output)
            f"set *(unsigned int *)0x{drv_moder:08X} = "
            f"(*(unsigned int *)0x{drv_moder:08X} & ~(3u << {2*drv_pin})) | (1u << {2*drv_pin})",
            # input pin: MODER bits = 00 (input)
            f"set *(unsigned int *)0x{in_moder:08X} = "
            f"*(unsigned int *)0x{in_moder:08X} & ~(3u << {2*in_pin})",
            # input pin PUPDR: bits = 01 (pull-up)
            f"set *(unsigned int *)0x{in_pupdr:08X} = "
            f"(*(unsigned int *)0x{in_pupdr:08X} & ~(3u << {2*in_pin})) | (1u << {2*in_pin})",
            # Drive driver HIGH
            f"set *(unsigned int *)0x{drv_bsrr:08X} = (1u << {drv_pin})",
            # Read IDR when HIGH (expect input bit = 1)
            f"print/x *(unsigned int *)0x{in_idr:08X}",
            # Drive driver LOW (BSRR upper 16 bits = reset)
            f"set *(unsigned int *)0x{drv_bsrr:08X} = (1u << {drv_pin + 16})",
            # Read IDR when LOW (expect input bit = 0)
            f"print/x *(unsigned int *)0x{in_idr:08X}",
        ]

    lines += ["detach", "quit"]
    return "\n".join(lines)


def parse_results(gdb_output):
    """Extract IDR print values in order and evaluate each wire."""
    # Each wire produces 2 prints: IDR_when_HIGH, IDR_when_LOW
    values = re.findall(r"\$\d+\s*=\s*(0x[0-9a-fA-F]+)", gdb_output)

    results = []
    for i, (drv_base, drv_pin, in_base, in_pin, label) in enumerate(WIRES):
        idx = i * 2
        if idx + 1 >= len(values):
            results.append((label, "UNKNOWN", "missing GDB output"))
            continue

        idr_high = int(values[idx],   16)
        idr_low  = int(values[idx+1], 16)

        bit_when_high = (idr_high >> in_pin) & 1
        bit_when_low  = (idr_low  >> in_pin) & 1

        if bit_when_high == 1 and bit_when_low == 0:
            status = "PASS"
            note   = f"HIGH={idr_high:#010x} bit{in_pin}=1  LOW={idr_low:#010x} bit{in_pin}=0"
        elif bit_when_high == 0 and bit_when_low == 0:
            status = "FAIL"
            note   = f"stuck LOW — short to GND? HIGH={idr_high:#010x}"
        elif bit_when_high == 1 and bit_when_low == 1:
            status = "FAIL"
            note   = f"stuck HIGH — open wire or short to VDD? LOW={idr_low:#010x}"
        else:
            status = "FAIL"
            note   = f"unexpected: HIGH={idr_high:#010x} LOW={idr_low:#010x}"

        results.append((label, status, note))

    return results


def main():
    script = build_gdb_script()

    # Write temp GDB script
    script_path = "/tmp/stm32f407vet6_wire_scan.gdb"
    with open(script_path, "w") as f:
        f.write(script)

    print(f"Connecting to BMDA at {BMDA_HOST}:{BMDA_PORT} ...")
    cmd = [GDB_BIN, "--batch", f"--command={script_path}"]
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=30
        )
    except subprocess.TimeoutExpired:
        print("ERROR: GDB timed out")
        sys.exit(1)

    output = result.stdout + result.stderr

    if "--debug" in sys.argv:
        print("=== RAW GDB OUTPUT ===")
        print(output)
        print("======================")

    results = parse_results(output)

    print()
    print("=" * 60)
    print("  STM32F407VET6 Wire Scan Results")
    print("=" * 60)
    all_pass = True
    for label, status, note in results:
        mark = "✓" if status == "PASS" else "✗"
        print(f"  {mark} {status:4s}  {label}")
        if status != "PASS":
            print(f"         → {note}")
            all_pass = False
        else:
            print(f"         → {note}")
    print("=" * 60)
    print(f"  Overall: {'ALL PASS' if all_pass else 'FAILURES DETECTED'}")
    print("=" * 60)

    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()
