from __future__ import annotations

import re
import subprocess
from typing import Callable

from ael.adapters import flash_bmda_gdbmi


_READ_ADDRS = (
    0xE000ED00,  # CPUID
    0xE0042000,  # DBGMCU_IDCODE for STM32F1/F4/G4 families
    0x1FFFF7E0,  # FLASH_SIZE on STM32F1
    0x1FFFF7E8,  # UID word 0 on STM32F1
)


def _gdb_read_words(gdb_cmd: str, ip: str, port: int, timeout_s: int = 15) -> dict[int, int]:
    args = [gdb_cmd, "-q", "--nx", "--batch", "-ex", f"target extended-remote {ip}:{int(port)}", "-ex", "monitor reset halt"]
    for addr in _READ_ADDRS:
        args.extend(["-ex", f"x/wx 0x{addr:08x}"])
    args.extend(["-ex", "detach"])
    res = subprocess.run(args, capture_output=True, text=True, timeout=max(1, int(timeout_s)))
    text = (res.stdout or "") + (res.stderr or "")
    out: dict[int, int] = {}
    for match in re.finditer(r"0x([0-9a-fA-F]{8})\s*:\s*(?:0x)?([0-9a-fA-F]+)", text):
        addr = int(match.group(1), 16)
        value = int(match.group(2), 16)
        out[addr] = value
    if res.returncode != 0:
        raise RuntimeError(text.strip() or f"{gdb_cmd} failed with exit code {res.returncode}")
    return out


def _classify_identity(registers: dict[int, int]) -> dict[str, object]:
    cpuid = int(registers.get(0xE000ED00, 0))
    dbgmcu_idcode = int(registers.get(0xE0042000, 0))
    flash_raw = int(registers.get(0x1FFFF7E0, 0))
    flash_kb = flash_raw & 0xFFFF
    dev_id = dbgmcu_idcode & 0xFFF
    family = ""
    part = ""
    if dev_id == 0x414:
        family = "STM32F1 high-density"
        if flash_kb == 256 and cpuid == 0x411FC231:
            part = "STM32F103RC"
    return {
        "cpuid": cpuid,
        "dbgmcu_idcode": dbgmcu_idcode,
        "flash_kb": flash_kb,
        "uid_word0": int(registers.get(0x1FFFF7E8, 0)),
        "family": family,
        "part": part,
    }


def detect_mcu_from_probe_cfg(
    probe_cfg: dict,
    *,
    target: str,
    emit: Callable[[str], None] = print,
    log_path: str = "",
    timeout_s: int = 15,
) -> dict[str, object]:
    ip = str(probe_cfg.get("ip") or "").strip()
    port = int(probe_cfg.get("gdb_port") or 0)
    gdb_cmd = str(probe_cfg.get("gdb_cmd") or "").strip()
    if not gdb_cmd:
        return {"ok": False, "error": "gdb_cmd not set"}
    if not target:
        return {"ok": False, "error": "target not set"}
    if not (flash_bmda_gdbmi._is_local_host(ip) and port > 0 and flash_bmda_gdbmi._uses_external_local_gdb_server(probe_cfg)):
        return {"ok": False, "error": "detect-mcu currently supports local DAPLink/OpenOCD gdb_remote instruments only"}

    bootstrap = {"managed": False}
    try:
        emit(f"Detect: acquiring DAPLink/OpenOCD session at {ip}:{port}")
        bootstrap = flash_bmda_gdbmi._ensure_local_daplink_gdb_server(
            probe_cfg,
            {"target": target},
            emit,
            flash_log_path=log_path,
        )
        if not bootstrap.get("ok", True):
            return {"ok": False, "error": str(bootstrap.get("error") or "failed to acquire DAPLink/OpenOCD session")}
        emit(f"Detect: reading MCU identity registers via {gdb_cmd}")
        registers = _gdb_read_words(gdb_cmd, ip, port, timeout_s=timeout_s)
        identity = _classify_identity(registers)
        return {
            "ok": True,
            "managed_session": bool(bootstrap.get("managed")),
            "reused_session": not bool(bootstrap.get("managed")),
            "registers": {f"0x{addr:08x}": value for addr, value in sorted(registers.items())},
            "identity": identity,
        }
    except Exception as exc:
        return {"ok": False, "error": str(exc)}
    finally:
        if bootstrap.get("managed"):
            flash_bmda_gdbmi._cleanup_managed_local_stlink_server(bootstrap, emit)
