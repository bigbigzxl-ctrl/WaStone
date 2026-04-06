"""
AEL flash adapter for WCHLink + wch-openocd (CH32V003/V103/etc.)

Flash sequence (all via OpenOCD TCL, no GDB):
  1. Run wch-openocd with `program <ELF> verify` + `wlink_reset_resume`
  2. Then (re)start wch-openocd as a persistent GDB server on gdb_port
     so that the subsequent check.mailbox_verify step can connect.

Probe config keys used:
  ip            : GDB server host          (default "127.0.0.1")
  gdb_port      : GDB server port          (default 3333)
  openocd_bin   : wch-openocd binary path  (default: _DEFAULT_OPENOCD)
  openocd_cfg   : wch-openocd config path  (default: _DEFAULT_OPENOCD_CFG)

Flash config keys used:
  post_load_settle_s   : seconds to wait after flash  (default 0)
  timeout_s            : overall flash timeout         (default 60)
"""

from __future__ import annotations

import os
import socket
import subprocess
import time
from pathlib import Path
from typing import Optional


_DEFAULT_OPENOCD     = "/nvme1t/wch-riscv-gcc/openocd/bin/openocd"
_DEFAULT_OPENOCD_CFG = "/nvme1t/wch-riscv-gcc/openocd/bin/wch-riscv.cfg"
_DEFAULT_PORT        = 3333


def _port_is_listening(host: str, port: int, timeout: float = 0.5) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def _wait_for_port(host: str, port: int, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if _port_is_listening(host, port):
            return True
        time.sleep(0.2)
    return False


def _kill_openocd_on_port(host: str, port: int) -> None:
    """Kill any wch-openocd process already listening on port (best-effort)."""
    try:
        result = subprocess.run(
            ["fuser", "-k", f"{port}/tcp"],
            capture_output=True,
        )
    except Exception:
        pass
    # Give a moment for the port to be released
    time.sleep(0.3)


def run(probe_cfg: dict, firmware_path: str, flash_cfg: dict = None, flash_json_path: str = None) -> bool:
    if not firmware_path or not os.path.exists(firmware_path):
        print("Flash/WCH: firmware not found")
        return False

    probe_cfg = probe_cfg or {}
    flash_cfg = flash_cfg or {}

    host        = str(probe_cfg.get("ip")          or "127.0.0.1").strip()
    port        = int(probe_cfg.get("gdb_port")     or _DEFAULT_PORT)
    openocd_bin = str(probe_cfg.get("openocd_bin") or _DEFAULT_OPENOCD).strip()
    openocd_cfg = str(probe_cfg.get("openocd_cfg") or _DEFAULT_OPENOCD_CFG).strip()
    timeout_s   = int(flash_cfg.get("timeout_s", 60))

    # ── kill any existing openocd holding the port ────────────────────────────
    if _port_is_listening(host, port):
        print(f"Flash/WCH: killing existing openocd on port {port}")
        _kill_openocd_on_port(host, port)

    # ── Step 1: flash via TCL program command ─────────────────────────────────
    cmd_flash = [
        openocd_bin,
        "-f", openocd_cfg,
        "-c", "gdb_port disabled; tcl_port disabled; telnet_port disabled",
        "-c", "init; halt",
        "-c", f"program {firmware_path} verify",
        "-c", "wlink_reset_resume; shutdown",
    ]

    print(f"Flash/WCH: flashing {firmware_path}")
    try:
        result = subprocess.run(
            cmd_flash,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired:
        print("Flash/WCH: wch-openocd flash timed out")
        return False
    except Exception as exc:
        print(f"Flash/WCH: flash error: {exc}")
        return False

    output = (result.stdout or "") + (result.stderr or "")
    print(output[:2000])

    if "Verified OK" not in output:
        print(f"Flash/WCH: flash failed (exit {result.returncode})")
        return False

    print("Flash/WCH: flash verified OK")

    # ── Step 2: start persistent GDB server for mailbox_verify ───────────────
    cmd_srv = [
        openocd_bin,
        "-f", openocd_cfg,
        "-c", f"gdb_port {port}; tcl_port disabled; telnet_port disabled",
    ]

    print(f"Flash/WCH: starting GDB server on port {port}")
    try:
        subprocess.Popen(
            cmd_srv,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except Exception as exc:
        print(f"Flash/WCH: failed to start GDB server: {exc}")
        return False

    if not _wait_for_port(host, port, timeout_s=8.0):
        print("Flash/WCH: GDB server did not start in time")
        return False

    print(f"Flash/WCH: GDB server ready at {host}:{port}")
    return True
