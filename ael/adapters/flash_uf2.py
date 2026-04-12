"""
AEL UF2 mass-storage flash adapter.

Copies a .uf2 firmware image to a mounted UF2 bootloader drive
(e.g. /media/aes/NICENANO) and waits for the drive to unmount,
which signals the board has rebooted into the new firmware.

flash_cfg fields consumed:
  uf2_mount_path   — path to the mounted UF2 drive (default: /media/aes/NICENANO)
  timeout_s        — seconds to wait for unmount after copy  (default: 30)
  post_load_settle_s — seconds to wait after unmount for USB re-enumeration (default: 3)

probe_cfg fields consumed (fallback):
  uf2_mount_path   — same as flash_cfg.uf2_mount_path

Firmware path resolution:
  firmware_path is expected to be a .uf2 file, OR a .elf/.bin path for which
  a sibling .uf2 exists (Zephyr places zephyr.uf2 next to zephyr.elf).

Verified on: nRF52840 nice!nano v1 (Adafruit UF2 bootloader 0.6.0)
"""

from __future__ import annotations

import os
import shutil
import time
from pathlib import Path
from typing import Any, Dict, Optional

_DEFAULT_MOUNT = "/media/aes/NICENANO"
_DEFAULT_TIMEOUT = 30.0
_DEFAULT_SETTLE  = 3.0


def _find_uf2(firmware_path: str) -> Optional[Path]:
    """Resolve .uf2 path from firmware_path (may be .elf, .bin, or .uf2)."""
    p = Path(firmware_path)
    if p.suffix == ".uf2" and p.exists():
        return p
    # Try sibling .uf2 (covers Zephyr zephyr.elf → zephyr.uf2)
    candidate = p.with_suffix(".uf2")
    if candidate.exists():
        return candidate
    return None


def _is_mounted(path: str) -> bool:
    """Return True if path is a mount point that exists."""
    p = Path(path)
    if not p.exists():
        return False
    try:
        return os.path.ismount(str(p))
    except Exception:
        return False


def _wait_for_unmount(mount_path: str, timeout_s: float) -> bool:
    """Poll until the UF2 drive unmounts (board rebooted). Returns True on success."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if not _is_mounted(mount_path):
            return True
        time.sleep(0.25)
    return False


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
    timeout_s  = float(flash_cfg.get("timeout_s", _DEFAULT_TIMEOUT))
    settle_s   = float(flash_cfg.get("post_load_settle_s", _DEFAULT_SETTLE))

    # ── Guard: drive must be mounted before we start ──────────────────────────
    if not _is_mounted(mount_path):
        print(f"Flash/UF2: drive not mounted at {mount_path}")
        print("Flash/UF2: put the board into bootloader mode (double-tap reset)")
        return False

    # ── Resolve .uf2 file ─────────────────────────────────────────────────────
    if not firmware_path:
        print("Flash/UF2: no firmware_path provided")
        return False

    uf2_path = _find_uf2(firmware_path)
    if uf2_path is None:
        print(f"Flash/UF2: no .uf2 file found near {firmware_path}")
        return False

    dest = Path(mount_path) / uf2_path.name
    print(f"Flash/UF2: copying {uf2_path.name} ({uf2_path.stat().st_size // 1024} KB) → {mount_path}")

    try:
        shutil.copy2(str(uf2_path), str(dest))
    except Exception as exc:
        print(f"Flash/UF2: copy failed: {exc}")
        return False

    # The OS may buffer the write; give it a moment to flush to the drive.
    # The UF2 bootloader should trigger reboot shortly after the write completes.
    time.sleep(0.5)

    # ── Wait for drive to unmount (= board rebooted) ──────────────────────────
    print(f"Flash/UF2: waiting for {mount_path} to unmount (board reboot) …")
    if not _wait_for_unmount(mount_path, timeout_s):
        print(f"Flash/UF2: drive still mounted after {timeout_s:.0f}s — flash may have failed")
        return False

    print(f"Flash/UF2: drive unmounted — board rebooting into firmware")

    # ── Post-reboot settle (USB CDC re-enumeration) ───────────────────────────
    if settle_s > 0.0:
        print(f"Flash/UF2: settling {settle_s:.1f}s for USB re-enumeration …")
        time.sleep(settle_s)

    print("Flash/UF2: done")
    return True
