"""
check.mailbox_verify — AEL pipeline adapter

Reads the AEL debug mailbox from a target over the existing GDB/BMP endpoint
and verifies magic + status fields.  Produces a structured JSON artifact and
returns a standard AEL adapter result dict.

Expected inputs (from step["inputs"]):
    probe_ip     str   IP address of the BMP/GDB server
    probe_port   int   GDB server port (default 4242)
    target_id    int   GDB attach target ID (default 1)
    addr         str   Mailbox address, hex string (default "0x20007F00")
    settle_s     float seconds to wait after flash before reading (default 0)
    out_json     str   path where the result artifact will be written

Pass criteria:
    magic  == 0xAE100001
    status == 2  (STATUS_PASS)
"""

from __future__ import annotations

import json
import re
import subprocess
import time
from pathlib import Path
from typing import Any, Dict


MAILBOX_MAGIC  = 0xAE100001
STATUS_NAMES   = {0: "EMPTY", 1: "RUNNING", 2: "PASS", 3: "FAIL"}


def _write_json(path: str | Path, payload: dict) -> None:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def _gdb_read_mailbox(
    endpoint: str,
    target_id: int,
    addr: int,
    gdb_cmd: str = "arm-none-eabi-gdb",
    skip_attach: bool = False,
    halt_before_read: bool = False,
    attach_monitor_cmd: str = "monitor swdp_scan",
    detach_resume_target: str = "",
) -> Dict[str, Any]:
    """Run arm-none-eabi-gdb in batch mode and parse x/4xw output.

    skip_attach=True: omit 'monitor swdp_scan' + 'attach' (for st-util / non-BMDA servers).
    halt_before_read=True: send 'monitor halt' before the memory read.
    detach_resume_target: if set (e.g. 'wch_riscv.cpu.0'), configure the OpenOCD
        gdb-detach event to call resume so the MCU keeps running after disconnect.
    """
    cmds = [
        "set pagination off",
        "set confirm off",
        f"target extended-remote {endpoint}",
    ]
    if detach_resume_target:
        cmds += [f"monitor {detach_resume_target} configure -event gdb-detach {{resume}}"]
    if not skip_attach:
        cmds += [attach_monitor_cmd, f"attach {target_id}"]
    if halt_before_read:
        cmds += ["monitor halt"]
    disconnect_cmd = "disconnect" if skip_attach else "detach"
    cmds += [
        f"x/4xw {addr:#010x}",
        "monitor resume",
        disconnect_cmd,
        "quit",
    ]
    args = [gdb_cmd, "--batch"] + [
        item for cmd in cmds for item in ("-ex", cmd)
    ]

    try:
        proc = subprocess.run(args, capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "gdb_timeout", "stdout": "", "stderr": ""}
    except FileNotFoundError:
        return {"ok": False, "error": "gdb_not_found", "stdout": "", "stderr": ""}

    stdout = proc.stdout
    stderr = proc.stderr

    # Parse: "0x20007f00:\t0xae100001\t0x00000002\t0x00000000\t0x00000000"
    words: list[int] = []
    for line in stdout.splitlines():
        if f"{addr:#010x}" in line.lower():
            found = re.findall(r"0x[0-9a-fA-F]+", line)
            words = [int(x, 16) for x in found[1:]]  # skip the address word
            break

    if len(words) < 4:
        return {
            "ok": False,
            "error": "parse_failed",
            "stdout": stdout,
            "stderr": stderr,
        }

    return {
        "ok": True,
        "magic":      words[0],
        "status":     words[1],
        "error_code": words[2],
        "detail0":    words[3],
        "stdout":     stdout,
        "stderr":     stderr,
    }


def _extract_toggle_count(detail0: int) -> int:
    """Extract bits[15:1] toggle_count from detail0."""
    return (detail0 >> 1) & 0x7FFF


def _execute_detail0_increment(
    inputs: dict,
    addr_str: str,
    addr: int,
    endpoint: str,
    target_id: int,
    gdb_cmd: str,
    out_json: str | None,
    skip_attach: bool,
    halt_before_read: bool,
    attach_monitor_cmd: str,
    post_resume_openocd_bin: str | None = None,
    post_resume_openocd_cfg: str | None = None,
    probe_port: int = 4242,
    detach_resume_target: str = "",
) -> Dict[str, Any]:
    """Verify detail0.toggle_count increments between two reads.

    Both reads happen in a SINGLE GDB session using 'shell sleep' so the
    MCU is only halted for the reads and runs freely between them.
    """
    increment_wait_s = float(inputs.get("increment_wait_s", 2.0))

    cmds = [
        "set pagination off",
        "set confirm off",
        f"target extended-remote {endpoint}",
    ]
    if detach_resume_target:
        cmds += [f"monitor {detach_resume_target} configure -event gdb-detach {{resume}}"]
    if not skip_attach:
        cmds += [attach_monitor_cmd, f"attach {target_id}"]
    if halt_before_read:
        cmds += ["monitor halt"]

    addr_hex = f"{addr:#010x}"
    disconnect_cmd = "disconnect" if skip_attach else "detach"
    cmds += [
        f"x/4xw {addr_hex}",           # T1 read
        "monitor resume",
        f"shell sleep {int(increment_wait_s)}",   # MCU runs freely
        "monitor halt",
        f"x/4xw {addr_hex}",           # T2 read
        "monitor resume",
        disconnect_cmd,
        "quit",
    ]

    args = [gdb_cmd, "--batch"] + [
        item for cmd in cmds for item in ("-ex", cmd)
    ]
    try:
        proc = subprocess.run(args, capture_output=True, text=True,
                              timeout=increment_wait_s + 30)
    except subprocess.TimeoutExpired:
        return {"ok": False, "error_summary": "gdb timeout in detail0_increment",
                "failure_kind": "mailbox_read_error"}
    except FileNotFoundError:
        return {"ok": False, "error_summary": "gdb not found",
                "failure_kind": "mailbox_read_error"}

    # Parse both x/4xw outputs — find all lines matching the address
    stdout = proc.stdout
    readings: list[list[int]] = []
    for line in stdout.splitlines():
        if addr_hex in line.lower():
            found = re.findall(r"0x[0-9a-fA-F]+", line)
            words = [int(x, 16) for x in found[1:]]
            if len(words) >= 4:
                readings.append(words)

    if post_resume_openocd_bin and post_resume_openocd_cfg:
        _post_resume_openocd(post_resume_openocd_bin, post_resume_openocd_cfg, probe_port)

    if len(readings) < 2:
        return {"ok": False,
                "error_summary": f"could not parse two mailbox reads (got {len(readings)})",
                "failure_kind": "mailbox_read_error"}

    w1, w2 = readings[0], readings[1]
    magic_ok = w1[0] == MAILBOX_MAGIC
    if not magic_ok:
        return {"ok": False,
                "error_summary": f"magic mismatch: {w1[0]:#010x}",
                "failure_kind": "mailbox_verify_mismatch"}

    tc1 = _extract_toggle_count(w1[3])
    tc2 = _extract_toggle_count(w2[3])
    led_state = w2[3] & 0x1
    incremented = tc2 != tc1

    result = {
        "ok":               incremented,
        "addr":             addr_str,
        "endpoint":         endpoint,
        "magic":            f"{w1[0]:#010x}",
        "magic_ok":         magic_ok,
        "status_t1":        STATUS_NAMES.get(w1[1], w1[1]),
        "status_t2":        STATUS_NAMES.get(w2[1], w2[1]),
        "detail0_t1":       w1[3],
        "detail0_t2":       w2[3],
        "toggle_count_t1":  tc1,
        "toggle_count_t2":  tc2,
        "led_state":        led_state,
        "incremented":      incremented,
    }
    if out_json:
        _write_json(out_json, result)

    if not incremented:
        return {
            "ok": False,
            "error_summary": f"toggle_count did not increment (t1={tc1} t2={tc2}); MCU not running",
            "failure_kind": "mailbox_verify_mismatch",
            "result": result,
        }
    return {"ok": True, "result": result}


def _post_resume_openocd(openocd_bin: str, openocd_cfg: str, probe_port: int) -> None:
    """Kill the persistent GDB server and run a fresh OpenOCD to resume the target."""
    try:
        subprocess.run(["fuser", "-k", f"{probe_port}/tcp"],
                       capture_output=True, timeout=5)
        time.sleep(0.5)
    except Exception:
        pass
    try:
        subprocess.run(
            [openocd_bin, "-f", openocd_cfg,
             "-c", "gdb_port disabled; tcl_port disabled; telnet_port disabled",
             "-c", "init",
             "-c", "wlink_reset_resume; shutdown"],
            capture_output=True, timeout=15,
        )
    except Exception:
        pass


def execute(step: dict, plan: dict, ctx: Any) -> Dict[str, Any]:  # noqa: ARG001
    inputs    = step.get("inputs", {}) if isinstance(step, dict) else {}
    probe_ip  = inputs.get("probe_ip", "")
    probe_port = int(inputs.get("probe_port", 4242))
    gdb_cmd = str(inputs.get("gdb_cmd") or "arm-none-eabi-gdb")
    target_id = int(inputs.get("target_id", 1))
    addr_str    = inputs.get("addr", "0x20007F00")
    settle_s    = float(inputs.get("settle_s", 0.0))
    out_json    = inputs.get("out_json")
    skip_attach      = bool(inputs.get("skip_attach", False))
    halt_before_read = bool(inputs.get("halt_before_read", False))
    attach_monitor_cmd = str(inputs.get("attach_monitor_cmd", "monitor swdp_scan"))
    check_mode  = inputs.get("check_mode", "pass")
    post_resume_openocd_bin = inputs.get("post_resume_openocd_bin")
    post_resume_openocd_cfg = inputs.get("post_resume_openocd_cfg")
    detach_resume_target    = str(inputs.get("detach_resume_target") or "")

    if not probe_ip:
        return {"ok": False, "error_summary": "check.mailbox_verify: probe_ip not set"}

    if settle_s > 0.0:
        time.sleep(settle_s)

    addr     = int(addr_str, 16)
    endpoint = f"{probe_ip}:{probe_port}"

    if check_mode == "detail0_increment":
        return _execute_detail0_increment(
            inputs, addr_str, addr, endpoint, target_id, gdb_cmd, out_json,
            skip_attach, halt_before_read, attach_monitor_cmd,
            post_resume_openocd_bin=post_resume_openocd_bin,
            post_resume_openocd_cfg=post_resume_openocd_cfg,
            probe_port=probe_port,
            detach_resume_target=detach_resume_target,
        )

    raw      = _gdb_read_mailbox(
        endpoint,
        target_id,
        addr,
        gdb_cmd=gdb_cmd,
        skip_attach=skip_attach,
        halt_before_read=halt_before_read,
        attach_monitor_cmd=attach_monitor_cmd,
        detach_resume_target=detach_resume_target,
    )

    if not raw.get("ok"):
        result = {
            "ok":           False,
            "addr":         addr_str,
            "endpoint":     endpoint,
            "error":        raw.get("error", "unknown"),
            "gdb_stdout":   raw.get("stdout", ""),
            "gdb_stderr":   raw.get("stderr", ""),
        }
        if out_json:
            _write_json(out_json, result)
        return {
            "ok": False,
            "error_summary": f"mailbox read failed: {raw.get('error')}",
            "failure_kind": "mailbox_read_error",
            "result": result,
        }

    magic_ok  = raw["magic"] == MAILBOX_MAGIC
    status    = raw["status"]
    pass_ok   = magic_ok and status == 2  # STATUS_PASS

    result = {
        "ok":           pass_ok,
        "addr":         addr_str,
        "endpoint":     endpoint,
        "magic":        f"{raw['magic']:#010x}",
        "magic_ok":     magic_ok,
        "status":       status,
        "status_name":  STATUS_NAMES.get(status, f"UNKNOWN({status})"),
        "error_code":   f"{raw['error_code']:#010x}",
        "detail0":      raw["detail0"],
    }
    if out_json:
        _write_json(out_json, result)

    if post_resume_openocd_bin and post_resume_openocd_cfg:
        _post_resume_openocd(post_resume_openocd_bin, post_resume_openocd_cfg, probe_port)

    if not pass_ok:
        if not magic_ok:
            reason = f"magic mismatch: got {raw['magic']:#010x}, expected {MAILBOX_MAGIC:#010x}"
        else:
            reason = f"status={STATUS_NAMES.get(status, status)} (expected PASS)"
        return {
            "ok": False,
            "error_summary": f"mailbox verify failed: {reason}",
            "failure_kind": "mailbox_verify_mismatch",
            "result": result,
        }

    return {"ok": True, "result": result}
