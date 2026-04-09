"""
observe_litex_csr.py — AEL adapter: read LiteX Wishbone CSRs via litex_server.

Provides a thin wrapper around litex_client.RemoteClient for use in AEL
experiment scripts. Connects to a running litex_server instance and reads
a named CSR address map, returning a structured result dict.

Typical usage:
    from ael.adapters import observe_litex_csr
    result = observe_litex_csr.run(board_cfg, csr_reads=[...])

Works with any LiteX SoC (PA StarLite, etc.) as long as litex_server
is running and accessible on the configured host:port.
"""

from __future__ import annotations

import sys
import time
from typing import Any

sys.path.insert(0, '/home/aes/.local/lib/python3.10/site-packages')

# ---------------------------------------------------------------------------
# AEL semantic state CSR layout (pa_starlite_platform, ael_debug @ 0xf0001000)
# ---------------------------------------------------------------------------
AEL_CSR = {
    "ael_sys_status":   0xf0001000,  # [0]=cpu_running [2]=fault_active
    "ael_last_pc":      0xf0001004,
    "ael_last_exc":     0xf0001008,
    "ael_event_data":   0xf000100c,
    "ael_event_status": 0xf0001010,
    "ael_spi_state":    0xf0001014,
    "ael_task_id":           0xf000101c,
    "ael_task_state":        0xf0001020,  # [15:0]=task_state [31:16]=event_id
    "ael_block_reason":      0xf0001024,
    "ael_task_block_cycles": 0xf0001028,  # blocked cycle counter (new)
    "ael_deadlock_threshold":0xf000102c,  # deadlock threshold RW (new)
    "ael_ctrl":              0xf0001030,  # moved after new CSRs
    "leds_out":              0xf0000000,
}

TASK_STATES  = {0: "UNKNOWN", 1: "RUNNING", 2: "READY", 3: "BLOCKED", 4: "SLEEPING"}
BLOCK_REASONS = {0: "NONE", 1: "MUTEX", 2: "QUEUE", 3: "SEMAPHORE", 4: "TIMEOUT"}
EVENTS = {
    0x0000: "EV_NONE",       0x0001: "EV_CPU_FAULT",
    0x0002: "EV_ILLEGAL_INSTR",
    0x0100: "EV_SPI_TX_DONE", 0x0101: "EV_SPI_RX_DONE",
    0x0102: "EV_SPI_RX_TIMEOUT", 0x0103: "EV_SPI_ERROR",
    0x1000: "EV_TASK_SWITCH",
}

# VexRiscv debug bus
VEXRISCV_DEBUG_BASE = 0xf00f_0000
HALT_SET   = 1 << 17
HALT_CLEAR = 1 << 25
RESET_SET  = 1 << 16
RESET_CLEAR = 1 << 24


def _connect(board_cfg: dict[str, Any]):
    """Open RemoteClient using board_cfg litex_server host/port."""
    from litex.tools.litex_client import RemoteClient
    host = board_cfg.get("litex_server_host", "localhost")
    port = int(board_cfg.get("litex_server_port", 1234))
    wb = RemoteClient(host=host, port=port)
    wb.open()
    return wb


def read_semantic_state(wb) -> dict[str, Any]:
    """Read all AEL debug CSRs and return decoded state dict."""
    task_state = wb.read(AEL_CSR["ael_task_state"])
    event_id   = (task_state >> 16) & 0xFFFF
    task_low   = task_state & 0xFFFF
    sys_status = wb.read(AEL_CSR["ael_sys_status"])
    last_pc    = wb.read(AEL_CSR["ael_last_pc"])
    last_exc   = wb.read(AEL_CSR["ael_last_exc"])
    task_id    = wb.read(AEL_CSR["ael_task_id"])
    block      = wb.read(AEL_CSR["ael_block_reason"])
    leds       = wb.read(AEL_CSR["leds_out"])
    return {
        "cpu_running":   bool(sys_status & 0x1),
        "fault_active":  bool(sys_status & 0x4),
        "last_pc":       last_pc,
        "last_exc":      last_exc,
        "event_id":      event_id,
        "event_name":    EVENTS.get(event_id, f"0x{event_id:04x}"),
        "task_id":       task_id,
        "task_state":    TASK_STATES.get(task_low, f"0x{task_low:x}"),
        "task_state_raw": task_low,
        "block_reason":    BLOCK_REASONS.get(block, f"0x{block:x}"),
        "block_raw":       block,
        "block_cycles":    wb.read(AEL_CSR["ael_task_block_cycles"]),
        "deadlock_suspect":bool(sys_status & 0x20),   # sys_status[5]
        "leds":            leds & 0x3,
        "sys_status":    sys_status,
    }


def cpu_reset(wb) -> None:
    """Reset VexRiscv CPU via debug bus — firmware restarts from address 0."""
    wb.write(VEXRISCV_DEBUG_BASE, RESET_SET)
    time.sleep(0.05)
    wb.write(VEXRISCV_DEBUG_BASE, RESET_CLEAR)
    time.sleep(0.1)


def cpu_halt(wb) -> None:
    wb.write(VEXRISCV_DEBUG_BASE, HALT_SET)
    time.sleep(0.05)


def cpu_resume(wb) -> None:
    wb.write(VEXRISCV_DEBUG_BASE, HALT_CLEAR)
    time.sleep(0.05)


def run(
    board_cfg: dict[str, Any],
    csr_reads: list[dict[str, Any]] | None = None,
    poll_s: float = 0.0,
    reset_cpu: bool = False,
) -> dict[str, Any]:
    """
    Read LiteX CSRs from a running litex_server.

    Args:
        board_cfg:  Board config dict with optional litex_server_host/port.
        csr_reads:  List of {name, addr} dicts. If None, reads AEL semantic state.
        poll_s:     If > 0, wait this many seconds then re-read (single snapshot).
        reset_cpu:  If True, reset CPU before reading.

    Returns:
        dict with keys: ok, state (decoded AEL state), raw (addr→value), errors.
    """
    errors: list[str] = []

    try:
        wb = _connect(board_cfg)
    except Exception as exc:
        return {"ok": False, "errors": [f"litex_server connect failed: {exc}"],
                "state": {}, "raw": {}}

    try:
        if reset_cpu:
            cpu_reset(wb)

        if poll_s > 0:
            time.sleep(poll_s)

        state = read_semantic_state(wb)

        raw: dict[str, int] = {}
        if csr_reads:
            for entry in csr_reads:
                name = entry.get("name", f"0x{entry['addr']:08x}")
                raw[name] = wb.read(entry["addr"])
        else:
            for name, addr in AEL_CSR.items():
                raw[name] = wb.read(addr)

    except Exception as exc:
        errors.append(f"CSR read error: {exc}")
        wb.close()
        return {"ok": False, "errors": errors, "state": {}, "raw": {}}

    wb.close()

    return {
        "ok":     len(errors) == 0,
        "state":  state,
        "raw":    raw,
        "errors": errors,
    }
