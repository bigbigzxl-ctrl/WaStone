from unittest.mock import ANY, patch

from ael.instruments import mcu_detect


def test_detect_mcu_reuses_existing_daplink_session():
    probe_cfg = {
        "type": "daplink",
        "endpoint": "local:cmsis-dap-lu",
        "ip": "127.0.0.1",
        "gdb_port": 3333,
        "gdb_cmd": "arm-none-eabi-gdb",
    }

    class Result:
        returncode = 0
        stdout = (
            "0xe000ed00:\t0x411fc231\n"
            "0xe0042000:\t0x10036414\n"
            "0x1ffff7e0:\t0x00000100\n"
            "0x1ffff7e8:\t0x05d8ff36\n"
        )
        stderr = ""

    with patch(
        "ael.instruments.mcu_detect.flash_bmda_gdbmi._ensure_local_daplink_gdb_server",
        return_value={"ok": True, "managed": False},
    ) as ensure_server, patch(
        "ael.instruments.mcu_detect.subprocess.run",
        return_value=Result(),
    ) as run_gdb, patch(
        "ael.instruments.mcu_detect.flash_bmda_gdbmi._cleanup_managed_local_stlink_server"
    ) as cleanup:
        payload = mcu_detect.detect_mcu_from_probe_cfg(probe_cfg, target="stm32f103rct6", emit=lambda _line: None)

    assert payload["ok"] is True
    assert payload["reused_session"] is True
    assert payload["managed_session"] is False
    assert payload["identity"]["family"] == "STM32F1 high-density"
    assert payload["identity"]["part"] == "STM32F103RC"
    ensure_server.assert_called_once()
    run_gdb.assert_called_once()
    cleanup.assert_not_called()


def test_detect_mcu_cleans_up_managed_daplink_session():
    probe_cfg = {
        "type": "daplink",
        "endpoint": "local:cmsis-dap-lu",
        "ip": "127.0.0.1",
        "gdb_port": 3333,
        "gdb_cmd": "arm-none-eabi-gdb",
    }

    class Result:
        returncode = 0
        stdout = (
            "0xe000ed00:\t0x411fc231\n"
            "0xe0042000:\t0x10036414\n"
            "0x1ffff7e0:\t0x00000100\n"
            "0x1ffff7e8:\t0x05d8ff36\n"
        )
        stderr = ""

    bootstrap = {"ok": True, "managed": True, "kind": "daplink", "pid": 2468}
    with patch(
        "ael.instruments.mcu_detect.flash_bmda_gdbmi._ensure_local_daplink_gdb_server",
        return_value=bootstrap,
    ), patch(
        "ael.instruments.mcu_detect.subprocess.run",
        return_value=Result(),
    ), patch(
        "ael.instruments.mcu_detect.flash_bmda_gdbmi._cleanup_managed_local_stlink_server"
    ) as cleanup:
        payload = mcu_detect.detect_mcu_from_probe_cfg(probe_cfg, target="stm32f103rct6", emit=lambda _line: None)

    assert payload["ok"] is True
    assert payload["managed_session"] is True
    cleanup.assert_called_once_with(bootstrap, ANY)
