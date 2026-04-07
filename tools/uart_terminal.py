#!/usr/bin/env python3
"""Interactive UART terminal over ESP32JTAG WebSocket bridge.

Usage:
    python3 tools/uart_terminal.py [--host 192.168.2.99] [--port 443]

Type any text and press Enter to send.  The target echoes it back as AEL_ECHO:<line>.
Press Ctrl-C to exit.
"""
import argparse
import base64
import ssl
import sys
import threading
import time

try:
    import websocket
except ImportError:
    sys.exit("ERROR: websocket-client not installed. Run: pip install websocket-client")


def _auth_headers(user: str, password: str) -> list[str]:
    token = base64.b64encode(f"{user}:{password}".encode()).decode()
    return [f"Authorization: Basic {token}"]


def _recv_thread(ws: websocket.WebSocket, stop: threading.Event) -> None:
    """Print all incoming data from the WebSocket."""
    while not stop.is_set():
        try:
            chunk = ws.recv()
        except websocket.WebSocketTimeoutException:
            continue
        except Exception:
            if not stop.is_set():
                print("\n[DISCONNECTED]", flush=True)
            stop.set()
            break
        if chunk is None:
            continue
        if isinstance(chunk, bytes):
            chunk = chunk.decode("utf-8", errors="replace")
        # Print received text, stripping trailing newline so our prompt stays clean
        lines = chunk.splitlines()
        for line in lines:
            print(f"\r[RX] {line}          ")
        sys.stdout.write("> ")
        sys.stdout.flush()


def main() -> int:
    ap = argparse.ArgumentParser(description="UART terminal over ESP32JTAG WebSocket")
    ap.add_argument("--host",     default="192.168.2.99")
    ap.add_argument("--port",     type=int, default=443)
    ap.add_argument("--user",     default="admin")
    ap.add_argument("--password", default="admin")
    ap.add_argument("--path",     default="/ws")
    ap.add_argument("--no-tls",   action="store_true", help="Use ws:// instead of wss://")
    args = ap.parse_args()

    scheme  = "ws" if args.no_tls else "wss"
    ws_url  = f"{scheme}://{args.host}:{args.port}{args.path}"
    headers = _auth_headers(args.user, args.password)

    print(f"[CONNECTING] {ws_url}")
    try:
        ws = websocket.create_connection(
            ws_url,
            timeout=5.0,
            header=headers,
            sslopt={"cert_reqs": ssl.CERT_NONE},
            skip_utf8_validation=True,
        )
    except Exception as exc:
        print(f"[ERROR] Cannot connect: {exc}")
        return 1

    ws.settimeout(0.2)
    print("[CONNECTED]  Type a message and press Enter.  Ctrl-C to quit.")
    print("-" * 60)

    stop = threading.Event()
    recv_t = threading.Thread(target=_recv_thread, args=(ws, stop), daemon=True)
    recv_t.start()

    # Small delay so the boot banner arrives before we show the prompt
    time.sleep(0.5)

    try:
        while not stop.is_set():
            sys.stdout.write("> ")
            sys.stdout.flush()
            try:
                line = input()
            except EOFError:
                break
            if stop.is_set():
                break
            ws.send(line + "\r\n")
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        try:
            ws.close()
        except Exception:
            pass
        print("\n[DISCONNECTED]")

    return 0


if __name__ == "__main__":
    sys.exit(main())
