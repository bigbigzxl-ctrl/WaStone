#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
import time

import serial


DEFAULT_PORT = "/dev/serial/by-id/usb-Arm_DAPLink_CMSIS-DAP_29e7f7fcfc57322e01-if00"
DEFAULT_BAUD = 115200


def _readline(ser: serial.Serial, deadline: float) -> str:
    buf = bytearray()
    while time.time() < deadline:
        chunk = ser.read(1)
        if not chunk:
            continue
        buf.extend(chunk)
        if chunk == b"\n":
            return buf.decode("utf-8", errors="replace").strip()
    raise TimeoutError("serial read timed out")


def _wait_for_ready(ser: serial.Serial, timeout_s: float) -> str:
    deadline = time.time() + timeout_s
    last = ""
    while time.time() < deadline:
        line = _readline(ser, deadline)
        last = line
        if "READY" in line:
            return line
    raise TimeoutError(f"did not receive READY banner, last line={last!r}")


def _expect_line(ser: serial.Serial, command: str, expected: str, timeout_s: float) -> str:
    ser.write(command.encode("utf-8"))
    ser.flush()
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        line = _readline(ser, deadline)
        if line == expected:
            return line
    raise TimeoutError(f"expected {expected!r} after {command!r}")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Smoke-test STM32F401 Black Pill UART via DAPLink CDC ACM")
    ap.add_argument("--port", default=DEFAULT_PORT)
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--timeout-s", type=float, default=4.0)
    args = ap.parse_args(argv)

    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            banner = _wait_for_ready(ser, args.timeout_s)
            pong = _expect_line(ser, "PING\n", "PONG", args.timeout_s)
            echo = _expect_line(ser, "ECHO hello-from-pc\n", "ECHO hello-from-pc", args.timeout_s)
    except Exception as exc:
        print(f"UART smoke FAIL: {exc}")
        return 1

    print(f"UART smoke PASS: banner={banner!r} pong={pong!r} echo={echo!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
