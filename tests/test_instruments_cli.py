import json

import pytest

from ael import __main__ as ael_main


def test_meter_reachability_cli_success(monkeypatch, capsys):
    monkeypatch.setattr(
        "ael.instruments.provision.ensure_meter_reachable",
        lambda manifest, host=None, timeout_s=1.0: {
            "ok": True,
            "instrument_id": manifest.get("id"),
            "host": host or "192.168.4.1",
            "ping": {"ok": True},
        },
    )
    monkeypatch.setattr(
        "sys.argv",
        ["ael", "instruments", "meter-reachability", "--id", "esp32s3_dev_c_meter"],
    )

    with pytest.raises(SystemExit) as exc:
        ael_main.main()

    assert exc.value.code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["ok"] is True
    assert payload["instrument_id"] == "esp32s3_dev_c_meter"


def test_meter_reachability_cli_failure(monkeypatch, capsys):
    def _fail(manifest, host=None, timeout_s=1.0):
        raise RuntimeError(
            "meter esp32s3_dev_c_meter at 192.168.4.1 is unreachable and needs manual checking. "
            "Suggestion: add a meter reset feature."
        )

    monkeypatch.setattr("ael.instruments.provision.ensure_meter_reachable", _fail)
    monkeypatch.setattr(
        "sys.argv",
        ["ael", "instruments", "meter-reachability", "--id", "esp32s3_dev_c_meter"],
    )

    with pytest.raises(SystemExit) as exc:
        ael_main.main()

    assert exc.value.code == 1
    payload = json.loads(capsys.readouterr().out)
    assert payload["ok"] is False
    assert "manual checking" in payload["error"]


def test_run_cli_defers_control_instrument_selection_when_not_explicit(monkeypatch):
    captured = {}

    def _fake_run_cli(probe_path, board_id, test_path, wiring=None, output_mode="normal", until_stage="report"):
        captured.update(
            {
                "probe_path": probe_path,
                "board_id": board_id,
                "test_path": test_path,
                "wiring": wiring,
                "output_mode": output_mode,
                "until_stage": until_stage,
            }
        )
        return 0

    monkeypatch.setattr(ael_main, "run_cli", _fake_run_cli)
    monkeypatch.setattr(
        "sys.argv",
        ["ael", "run", "--board", "stm32f103_uart", "--test", "tests/plans/stm32f103_uart_banner.json"],
    )

    with pytest.raises(SystemExit) as exc:
        ael_main.main()

    assert exc.value.code == 0
    assert captured["probe_path"] is None
    assert captured["board_id"] == "stm32f103_uart"
    assert captured["test_path"] == "tests/plans/stm32f103_uart_banner.json"


def test_run_cli_uses_explicit_control_instrument_override(monkeypatch):
    captured = {}

    def _fake_run_cli(probe_path, board_id, test_path, wiring=None, output_mode="normal", until_stage="report"):
        captured.update(
            {
                "probe_path": probe_path,
                "board_id": board_id,
                "test_path": test_path,
            }
        )
        return 0

    monkeypatch.setattr(ael_main, "run_cli", _fake_run_cli)
    monkeypatch.setattr(
        "sys.argv",
        [
            "ael",
            "run",
            "--board",
            "stm32f103_uart",
            "--test",
            "tests/plans/stm32f103_uart_banner.json",
            "--control-instrument",
            "configs/instrument_instances/esp32jtag_stm32_golden.yaml",
        ],
    )

    with pytest.raises(SystemExit) as exc:
        ael_main.main()

    assert exc.value.code == 0
    assert captured["probe_path"] == "configs/instrument_instances/esp32jtag_stm32_golden.yaml"


def test_run_cli_uses_explicit_controller_alias_override(monkeypatch):
    captured = {}

    def _fake_run_cli(probe_path, board_id, test_path, wiring=None, output_mode="normal", until_stage="report"):
        captured.update(
            {
                "probe_path": probe_path,
                "board_id": board_id,
                "test_path": test_path,
            }
        )
        return 0

    monkeypatch.setattr(ael_main, "run_cli", _fake_run_cli)
    monkeypatch.setattr(
        "sys.argv",
        [
            "ael",
            "run",
            "--board",
            "stm32f103_uart",
            "--test",
            "tests/plans/stm32f103_uart_banner.json",
            "--controller",
            "configs/instrument_instances/esp32jtag_stm32_golden.yaml",
        ],
    )

    with pytest.raises(SystemExit) as exc:
        ael_main.main()

    assert exc.value.code == 0
    assert captured["probe_path"] == "configs/instrument_instances/esp32jtag_stm32_golden.yaml"


def test_instruments_detect_mcu_cli_uses_instance_and_target(monkeypatch, capsys):
    monkeypatch.setattr(
        "ael.__main__._load_instrument_instance_probe_cfg",
        lambda repo_root, instance_id: {
            "type": "daplink",
            "endpoint": "local:cmsis-dap-lu",
            "ip": "127.0.0.1",
            "gdb_port": 3333,
            "gdb_cmd": "arm-none-eabi-gdb",
        },
    )
    monkeypatch.setattr(
        "ael.__main__.mcu_detect.detect_mcu_from_probe_cfg",
        lambda probe_cfg, target: {
            "ok": True,
            "managed_session": False,
            "reused_session": True,
            "registers": {
                "0xe000ed00": 0x411FC231,
                "0xe0042000": 0x10036414,
            },
            "identity": {
                "family": "STM32F1 high-density",
                "part": "STM32F103RC",
                "flash_kb": 256,
            },
        },
    )
    monkeypatch.setattr(
        "sys.argv",
        ["ael", "instruments", "detect-mcu", "--id", "daplink_f103_rct6", "--target", "stm32f103rct6"],
    )

    with pytest.raises(SystemExit) as exc:
        ael_main.main()

    assert exc.value.code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["ok"] is True
    assert payload["identity"]["part"] == "STM32F103RC"
