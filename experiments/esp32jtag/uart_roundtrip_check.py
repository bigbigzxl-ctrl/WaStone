#!/usr/bin/env python3
import argparse
import base64
import ssl
import sys
import time

import websocket


def _auth_header(user: str, password: str) -> list[str]:
    token = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
    return [f"Authorization: Basic {token}"]


def _recv_until(ws, deadline: float, patterns: list[str], capture: list[str]) -> str | None:
    stream = "".join(capture)
    while time.time() < deadline:
        try:
            chunk = ws.recv()
        except websocket.WebSocketTimeoutException:
            continue
        if chunk is None:
            continue
        if not isinstance(chunk, str):
            chunk = chunk.decode("utf-8", errors="replace")
        capture.append(chunk)
        stream += chunk
        print(f"RX: {chunk.rstrip()}")
        for pattern in patterns:
            if pattern in stream:
                return pattern
    return None


def _drain(ws, duration_s: float) -> None:
    deadline = time.time() + duration_s
    while time.time() < deadline:
        try:
            chunk = ws.recv()
        except websocket.WebSocketTimeoutException:
            continue
        if chunk is None:
            continue
        if not isinstance(chunk, str):
            chunk = chunk.decode("utf-8", errors="replace")
        print(f"DRAIN: {chunk.rstrip()}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--endpoint", default="wss://192.168.2.98/ws")
    ap.add_argument("--user", default="admin")
    ap.add_argument("--password", default="admin")
    ap.add_argument("--payload", default="PING 1234")
    ap.add_argument("--ready-timeout", type=float, default=6.0)
    ap.add_argument("--echo-timeout", type=float, default=4.0)
    ap.add_argument("--drain-s", type=float, default=0.4)
    args = ap.parse_args()

    headers = _auth_header(args.user, args.password)
    payload_line = args.payload + "\r\n"
    echo_expect = f"AEL_ECHO:{args.payload}"
    capture: list[str] = []

    print(f"Connecting: {args.endpoint}")
    ws = websocket.create_connection(
        args.endpoint,
        timeout=5.0,
        header=headers,
        sslopt={"cert_reqs": ssl.CERT_NONE},
    )
    ws.settimeout(0.2)

    try:
        ready = _recv_until(
            ws,
            time.time() + args.ready_timeout,
            ["AEL_READY STM32F103C6T6 UART_BRIDGE", "AEL_IDLE count="],
            capture,
        )
        if ready is None:
            print("FAIL: no READY/IDLE UART text received")
            return 1

        capture.clear()
        _drain(ws, args.drain_s)

        print(f"TX: {args.payload}")
        ws.send(payload_line)

        matched = _recv_until(
            ws,
            time.time() + args.echo_timeout,
            [echo_expect],
            capture,
        )
        if matched != echo_expect:
            print(f"FAIL: expected echo '{echo_expect}' not observed")
            return 1

        print("PASS: roundtrip echo observed")
        return 0
    finally:
        ws.close()


if __name__ == "__main__":
    sys.exit(main())
