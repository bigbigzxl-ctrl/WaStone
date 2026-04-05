"""
Regression test: ZephyrBackend hello_world end-to-end closed loop
==================================================================

Validates the full AEL-Zephyr pipeline on STM32F4 Discovery:

    flash  →  observe(PA2, 115200)  →  verify  →  ok=True

This test is intentionally minimal — it only proves the pipeline works,
not that the Zephyr sample itself is correct.  Run it after any change
to ael/backends/ to confirm the closed loop is intact.

Hardware required
-----------------
- STM32F4 Discovery (STM32F407VGT6) connected via USB (onboard ST-Link)
- USB-UART adapter: PA2 (USART2_TX) → RXD,  GND → GND  →  /dev/ttyUSB0
- Zephyr hello_world already built in ~/zephyrproject/zephyr/build/
  (run `west build -p always -b stm32f4_disco samples/hello_world`
   if the build directory is missing)

Skip conditions
---------------
- SKIP if /dev/ttyUSB0 is absent (no UART adapter)
- SKIP if ~/zephyrproject/zephyr/build/zephyr/zephyr.elf is absent
  (Zephyr workspace not initialised)

Proven boundaries (2026-04-05)
------------------------------
- ZephyrBackend.flash()           : releases :3333/:4242 before west flash
- ZephyrBackend.observe()         : delegates to observe_uart_log.run()
- ZephyrBackend.verify()          : delegates to check_eval.evaluate_uart_facts()
- firmware_ready_seen             : derived from missing_expect (same as adapter_registry)
- hello_world prints once at boot : must open serial BEFORE triggering reset
- Zephyr console pin on this board: PA2 (USART2_TX), NOT PD5 — see DTS
"""

import os
import subprocess
import threading
import time
from pathlib import Path

import pytest

from ael.backends.zephyr_backend import ZephyrBackend

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
UART_PORT   = "/dev/ttyUSB0"
UART_BAUD   = 115200
EXPECT      = ["Booting Zephyr OS", "Hello World"]
OBSERVE_S   = 10      # generous window; hello_world fires immediately at boot
RESET_DELAY = 0.5     # seconds to wait after starting observe before reset

_BACKEND    = ZephyrBackend()
_ELF        = _BACKEND.workspace / "build" / "zephyr" / "zephyr.elf"

# ---------------------------------------------------------------------------
# Skip guards
# ---------------------------------------------------------------------------
def _stlink_usb_present() -> bool:
    """Return True if an STMicroelectronics USB device (VID 0483) is visible."""
    try:
        import glob
        for vendor_file in glob.glob("/sys/bus/usb/devices/*/idVendor"):
            try:
                if open(vendor_file).read().strip().lower() == "0483":
                    return True
            except OSError:
                pass
    except Exception:
        pass
    return False


_skip_no_uart = pytest.mark.skipif(
    not Path(UART_PORT).exists(),
    reason=f"UART adapter not present at {UART_PORT}",
)
_skip_no_elf = pytest.mark.skipif(
    not _ELF.exists(),
    reason=f"Zephyr build not found: {_ELF}",
)
_skip_no_stlink = pytest.mark.skipif(
    not _stlink_usb_present(),
    reason="ST-Link USB device not detected (STM32F4 Discovery not connected)",
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _openocd_reset() -> None:
    """Trigger a software reset of the STM32F407 via OpenOCD."""
    subprocess.run(
        [
            "openocd",
            "-f", "interface/stlink.cfg",
            "-f", "target/stm32f4x.cfg",
            "-c", "init",
            "-c", "reset run",
            "-c", "shutdown",
        ],
        capture_output=True,
        timeout=10,
    )


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

@_skip_no_uart
@_skip_no_elf
@_skip_no_stlink
def test_flash_observe_verify_hello_world():
    """
    Full pipeline: flash → observe → verify → ok=True.

    This is the canonical regression for ZephyrBackend on STM32F4 Discovery.
    """
    backend = ZephyrBackend()

    # Step 1: flash (releases ST-Link from any stale AEL gdbserver first)
    backend.flash(runner="openocd")

    # Step 2: start observe in background, then reset board
    # hello_world only prints once at boot — serial must be open before reset.
    result_box: dict = {}

    def _observe():
        result_box["obs"] = backend.observe(
            port=UART_PORT,
            baud=UART_BAUD,
            duration_s=OBSERVE_S,
            expect_patterns=EXPECT,
            profile="stm32",
        )

    t = threading.Thread(target=_observe, daemon=True)
    t.start()
    time.sleep(RESET_DELAY)   # let observe open the serial port first
    _openocd_reset()
    t.join(timeout=OBSERVE_S + 5)

    obs = result_box.get("obs")
    assert obs is not None, "observe() did not return (thread hung)"

    # Step 3: verify
    verdict = backend.verify(obs)

    # --- assertions ---
    assert obs.get("ok") is True, (
        f"observe failed: {obs.get('error_summary')} | "
        f"missing={obs.get('missing_expect')}"
    )
    assert obs.get("missing_expect") == [], (
        f"Not all patterns matched. Missing: {obs['missing_expect']}"
    )
    assert verdict["ok"] is True, (
        f"verify failed: kind={verdict['failure_kind']} "
        f"class={verdict['failure_class']} msg={verdict['error_summary']}"
    )
    assert verdict["verify_substage"] == "uart.verify"


@_skip_no_uart
@_skip_no_elf
def test_verify_fails_on_wrong_pattern():
    """
    Smoke-test the verify path: if an expected pattern is absent,
    verify() must return ok=False with failure_class=uart_expected_patterns_missing.

    This test does NOT flash — it feeds a synthetic observe result directly
    into verify() to exercise the failure path without hardware I/O.
    """
    backend = ZephyrBackend()

    # Synthetic observation: observe 'succeeded' but a required pattern is missing
    fake_obs = {
        "ok": False,
        "bytes": 50,
        "lines": 2,
        "missing_expect": ["PATTERN_THAT_WILL_NEVER_APPEAR"],
        "firmware_ready_seen": False,
        "crash_detected": False,
        "reboot_loop_suspected": False,
        "download_mode_detected": False,
        "error_summary": "expected UART readiness patterns missing",
        "_cfg": {
            "port": UART_PORT,
            "baud": UART_BAUD,
        },
    }

    verdict = backend.verify(fake_obs)

    assert verdict["ok"] is False
    assert verdict["failure_class"] == "uart_expected_patterns_missing"
    assert verdict["verify_substage"] == "uart.verify"
