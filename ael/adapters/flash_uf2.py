"""
AEL UF2 mass-storage flash adapter.

Copies a .uf2 firmware image to a mounted UF2 bootloader drive
(e.g. /media/aes/NICENANO) and waits for the drive to unmount,
which signals the board has rebooted into the new firmware.

Auto-bootloader-entry (1200-baud touch):
  If the UF2 drive is NOT mounted, the adapter tries to find a USB-CDC
  device (/dev/ttyACM*) and open it at 1200 baud.  This triggers the
  Adafruit/nice!nano UF2 bootloader's "1200-baud magic" — the firmware
  writes GPREGRET=0x57 + calls NVIC_SystemReset(), causing the bootloader
  to stay in DFU mode and present the UF2 mass-storage drive.

  Requirement: the currently-running firmware must implement the 1200-baud
  bootloader-entry protocol (all AEL nRF52840 firmwares do from v2 onward).

flash_cfg fields consumed:
  uf2_mount_path      — path to the mounted UF2 drive (default: /media/aes/NICENANO)
  uf2_cdc_port        — explicit CDC port; auto-detected if omitted
  timeout_s           — seconds to wait total (default: 30)
  post_load_settle_s  — seconds after unmount for USB re-enumeration (default: 3)
  skip_1200_touch     — set true to skip auto-bootloader-entry (default: false)

probe_cfg fields consumed (fallback):
  uf2_mount_path   — same as flash_cfg.uf2_mount_path

Firmware path resolution:
  firmware_path is expected to be a .uf2 file, OR a .elf/.bin path for which
  a sibling .uf2 exists (Zephyr places zephyr.uf2 next to zephyr.elf).

Verified on: nRF52840 nice!nano v1 (Adafruit UF2 bootloader 0.6.0)
"""

from __future__ import annotations

import glob
import os
import shutil
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

_DEFAULT_MOUNT   = "/media/aes/NICENANO"
_DEFAULT_TIMEOUT = 30.0
_DEFAULT_SETTLE  = 3.0


def _find_uf2(firmware_path: str) -> Optional[Path]:
    """Resolve .uf2 path from firmware_path (may be .elf, .bin, or .uf2)."""
    p = Path(firmware_path)
    if p.suffix == ".uf2" and p.exists():
        return p
    candidate = p.with_suffix(".uf2")
    if candidate.exists():
        return candidate
    return None


def _is_mounted(path: str) -> bool:
    p = Path(path)
    if not p.exists():
        return False
    try:
        return os.path.ismount(str(p))
    except Exception:
        return False


def _wait_for_mount(mount_path: str, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if _is_mounted(mount_path):
            return True
        time.sleep(0.25)
    return False


def _wait_for_unmount(mount_path: str, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if not _is_mounted(mount_path):
            return True
        time.sleep(0.25)
    return False


def _find_cdc_ports() -> List[str]:
    """Return sorted list of candidate CDC/serial ports."""
    ports: List[str] = []
    ports.extend(sorted(glob.glob("/dev/ttyACM*")))
    ports.extend(sorted(glob.glob("/dev/ttyUSB*")))
    return ports


def _touch_1200_baud(port: str) -> None:
    """Open port at 1200 baud and immediately close — triggers Adafruit bootloader entry."""
    try:
        import serial  # type: ignore
        s = serial.Serial(port, baudrate=1200, timeout=0.1)
        time.sleep(0.1)
        s.close()
        print(f"Flash/UF2: 1200-baud touch sent to {port}")
    except Exception as exc:
        print(f"Flash/UF2: 1200-baud touch on {port} failed: {exc}")


def _try_enter_bootloader(mount_path: str, explicit_port: Optional[str],
                          timeout_s: float) -> bool:
    """
    Attempt to bring up the UF2 bootloader drive via the 1200-baud protocol.

    Returns True if the drive becomes mounted within timeout_s.
    """
    ports = [explicit_port] if explicit_port else _find_cdc_ports()
    if not ports:
        print("Flash/UF2: no CDC ports found — cannot send 1200-baud touch")
        print("Flash/UF2: enter bootloader manually (double-tap RST or power-cycle)")
        return False

    print(f"Flash/UF2: drive not mounted — trying 1200-baud bootloader touch on {ports}")
    for port in ports:
        _touch_1200_baud(port)

    print(f"Flash/UF2: waiting up to {timeout_s:.0f}s for {mount_path} to appear …")
    return _wait_for_mount(mount_path, timeout_s)


def run(
    probe_cfg: Dict[str, Any],
    firmware_path: str,
    flash_cfg: Optional[Dict[str, Any]] = None,
    flash_json_path: Optional[str] = None,
) -> bool:
    probe_cfg = probe_cfg or {}
    flash_cfg = flash_cfg or {}

    mount_path = (
        str(flash_cfg.get("uf2_mount_path") or "").strip()
        or str(probe_cfg.get("uf2_mount_path") or "").strip()
        or _DEFAULT_MOUNT
    )
    timeout_s        = float(flash_cfg.get("timeout_s", _DEFAULT_TIMEOUT))
    settle_s         = float(flash_cfg.get("post_load_settle_s", _DEFAULT_SETTLE))
    skip_touch       = bool(flash_cfg.get("skip_1200_touch", False))
    explicit_port    = str(flash_cfg.get("uf2_cdc_port") or "").strip() or None

    # ── Step 1: ensure bootloader drive is mounted ────────────────────────────
    if not _is_mounted(mount_path):
        if skip_touch:
            print(f"Flash/UF2: drive not mounted at {mount_path} and skip_1200_touch=true")
            return False
        if not _try_enter_bootloader(mount_path, explicit_port, timeout_s):
            print(f"Flash/UF2: could not bring up bootloader drive at {mount_path}")
            print("Flash/UF2: → double-tap RST pad to GND on the PCB, then retry")
            return False

    # ── Step 2: resolve .uf2 file ─────────────────────────────────────────────
    if not firmware_path:
        print("Flash/UF2: no firmware_path provided")
        return False

    uf2_path = _find_uf2(firmware_path)
    if uf2_path is None:
        print(f"Flash/UF2: no .uf2 file found near {firmware_path}")
        return False

    dest = Path(mount_path) / uf2_path.name
    print(f"Flash/UF2: copying {uf2_path.name} "
          f"({uf2_path.stat().st_size // 1024} KB) → {mount_path}")

    try:
        shutil.copy2(str(uf2_path), str(dest))
    except Exception as exc:
        print(f"Flash/UF2: copy failed: {exc}")
        return False

    time.sleep(0.5)   # let OS flush before the board resets

    # ── Step 3: wait for drive to unmount (board rebooted) ───────────────────
    print(f"Flash/UF2: waiting for {mount_path} to unmount …")
    if not _wait_for_unmount(mount_path, timeout_s):
        print(f"Flash/UF2: drive still mounted after {timeout_s:.0f}s — flash may have failed")
        return False

    print("Flash/UF2: drive unmounted — board rebooting into firmware")

    # ── Step 4: wait for USB CDC to re-enumerate ────────────────────────────
    # After UF2 flash the bootloader resets into the app.  We actively poll
    # for /dev/ttyACM* rather than sleeping a fixed interval, so the pipeline
    # never stalls waiting for a manual USB replug.
    print("Flash/UF2: waiting for firmware CDC port to appear …")
    cdc_deadline = time.monotonic() + max(timeout_s, settle_s + 10.0)
    found_port = None
    while time.monotonic() < cdc_deadline:
        ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
        if ports:
            found_port = ports[0]
            break
        time.sleep(0.5)

    if found_port:
        print(f"Flash/UF2: firmware running, CDC port ready: {found_port}")
    else:
        # Firmware didn't enumerate — prompt user to replug
        print("Flash/UF2: CDC port did not appear — please unplug and replug USB")
        print("Flash/UF2: waiting up to 60s for manual replug …")
        replug_deadline = time.monotonic() + 60.0
        while time.monotonic() < replug_deadline:
            ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
            if ports:
                found_port = ports[0]
                print(f"Flash/UF2: CDC port appeared after replug: {found_port}")
                break
            time.sleep(0.5)

    if settle_s > 0.0 and found_port:
        time.sleep(min(settle_s, 1.0))   # short settle after port appears

    print("Flash/UF2: done")
    return True
