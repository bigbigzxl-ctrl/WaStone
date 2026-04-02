import unittest
import types
from tempfile import TemporaryDirectory
from pathlib import Path
from unittest.mock import patch

from ael.adapters import observe_uart_log


class TestObserveUartBridge(unittest.TestCase):
    def test_capture_via_esp32jtag_web_uart_tolerates_invalid_text_frame_bytes(self):
        class FakeTimeout(Exception):
            pass

        class FakeWs:
            def __init__(self):
                self.calls = 0

            def settimeout(self, _timeout):
                return None

            def recv_data(self):
                self.calls += 1
                if self.calls == 1:
                    return (1, b"\x81AEL_READY STM32F103C6T6 UART_BRIDGE\r\n")
                raise FakeTimeout()

            def close(self):
                return None

        fake_ws = FakeWs()
        fake_mod = types.SimpleNamespace(
            ABNF=types.SimpleNamespace(OPCODE_TEXT=1),
            WebSocketTimeoutException=FakeTimeout,
            create_connection=lambda *args, **kwargs: fake_ws,
        )

        with patch.dict("sys.modules", {"websocket": fake_mod}):
            data, err = observe_uart_log._capture_via_esp32jtag_web_uart(
                "https://192.168.2.98:443",
                0.01,
                0.0,
            )

        self.assertIsNone(err)
        self.assertIsNotNone(data)
        self.assertIn(b"AEL_READY STM32F103C6T6 UART_BRIDGE", data)

    def test_run_uses_bridge_endpoint_when_present(self):
        with TemporaryDirectory() as td:
            raw_log = Path(td) / "uart.log"
            cfg = {
                "enabled": True,
                "port": "/dev/ttyUSB0",
                "baud": 115200,
                "duration_s": 1,
                "expect_patterns": ["AEL_READY STM32F103 UART"],
                "bridge_endpoint": "127.0.0.1:8767",
            }
            with patch(
                "ael.adapters.observe_uart_log._capture_via_bridge",
                return_value=(b"AEL_READY STM32F103 UART\r\n", None),
            ) as mocked:
                result = observe_uart_log.run(cfg, str(raw_log))

        mocked.assert_called_once()
        self.assertTrue(result["ok"])
        self.assertEqual(result["bridge_endpoint"], "127.0.0.1:8767")
        self.assertEqual(result["port"], "/dev/ttyUSB0")
        self.assertEqual(result["missing_expect"], [])


    def test_run_uses_esp32jtag_web_uart_backend_when_selected(self):
        with TemporaryDirectory() as td:
            raw_log = Path(td) / "uart.log"
            cfg = {
                "enabled": True,
                "port": "s3jtag_internal_web_uart",
                "baud": 115200,
                "duration_s": 1,
                "expect_patterns": ["AEL_READY RP2040 UART"],
                "backend": "esp32jtag_web_uart",
                "bridge_endpoint": "https://192.168.4.1:443",
            }
            with patch(
                "ael.adapters.observe_uart_log._capture_via_esp32jtag_web_uart",
                return_value=(b"AEL_READY RP2040 UART\r\n", None),
            ) as mocked:
                result = observe_uart_log.run(cfg, str(raw_log))

        mocked.assert_called_once_with("https://192.168.4.1:443", 1.0, 0.2)
        self.assertTrue(result["ok"])
        self.assertEqual(result["backend"], "esp32jtag_web_uart")
        self.assertEqual(result["bridge_endpoint"], "https://192.168.4.1:443")


if __name__ == "__main__":
    unittest.main()
