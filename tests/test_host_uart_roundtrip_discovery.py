from __future__ import annotations

from unittest.mock import patch

from ael.adapter_registry import AdapterRegistry
import json


class _FakeSerial:
    def __init__(self, port, baudrate, timeout=0.1, rtscts=False, dsrdtr=False):
        self.port = port
        self._chunks = [
            b"AEL_USB_READY\r\n",
            b"PONG\r\n",
        ]
        self.written = bytearray()
        self.closed = False
        self.dtr = False

    def read(self, size):
        if self._chunks:
            return self._chunks.pop(0)
        return b""

    def write(self, data):
        self.written.extend(data)
        return len(data)

    def flush(self):
        return None

    def close(self):
        self.closed = True


def test_uart_roundtrip_can_discover_usb_port(tmp_path):
    registry = AdapterRegistry()
    adapter = registry.get("check.uart_roundtrip")
    raw_log = tmp_path / "uart_raw.log"
    out_json = tmp_path / "uart_out.json"
    step = {
        "type": "check.uart_roundtrip",
        "inputs": {
            "host_uart_cfg": {
                "enabled": True,
                "baud": 115200,
                "ready_patterns": ["AEL_USB_READY"],
                "tx": "PING\r\n",
                "expect_patterns": ["PONG"],
                "startup_wait_s": 0.1,
                "ready_timeout_s": 0.2,
                "response_timeout_s": 0.2,
                "port_discovery": {
                    "vid": "0x1209",
                    "pid": "0x0001",
                    "serial_number": "AEL401USBTEST",
                    "manufacturer_contains": "OpenAI AEL",
                    "product_contains": "STM32F401 USB CDC Test",
                    "timeout_s": 0.1,
                    "poll_s": 0.01,
                },
            },
            "raw_log_path": str(raw_log),
            "out_json": str(out_json),
        },
    }
    discovery = {
        "ok": True,
        "devices": [
            {
                "identity_kind": "usb_serial",
                "identity_value": "AEL401USBTEST",
                "serial_number": "AEL401USBTEST",
                "device_path": "/dev/ttyACM9",
                "by_id_path": "/dev/serial/by-id/usb-OpenAI_AEL_STM32F401_USB_CDC_Test_AEL401USBTEST-if00",
                "vid": 0x1209,
                "pid": 0x0001,
                "manufacturer": "OpenAI AEL",
                "product": "STM32F401 USB CDC Test",
            }
        ],
        "rejected": [],
        "duplicate_device_identities": [],
    }

    with patch("serial.Serial", _FakeSerial), patch(
        "ael.instruments.usb_uart_bridge_daemon.discover_usb_uart_devices",
        return_value=discovery,
    ):
        result = adapter.execute(step, {}, {})

    assert result["ok"] is True
    payload = json.loads(out_json.read_text(encoding="utf-8"))
    assert payload["port"].endswith("AEL401USBTEST-if00")
    assert payload["matched"]["ready"]["AEL_USB_READY"] == 1
    assert payload["missing_expect"] == []
    assert "PONG" in raw_log.read_text(encoding="utf-8")
