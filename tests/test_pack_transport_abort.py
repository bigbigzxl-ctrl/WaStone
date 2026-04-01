"""Tests for pack abort-on-transport-error behaviour.

Covers the three layers introduced to fix the 17-minute timeout when the
instrument is offline:

  1. preflight.run() emits failure_kind="transport_error" in the info dict
     when the monitor check fails with probe_transport_unhealthy.
  2. _PreflightAdapter.execute() passes failure_kind through to its return dict.
  3. run_pack() aborts the test loop after the first transport_error instead of
     grinding through all remaining tests.
"""
from __future__ import annotations

import json
import types
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest


# ---------------------------------------------------------------------------
# Layer 1 — preflight.run() failure_kind propagation
# ---------------------------------------------------------------------------

class TestPreflightRunFailureKind:
    """preflight.run() must include failure_kind=transport_error in the returned
    info dict when the GDB monitor is unreachable (ping also fails)."""

    def _make_probe_cfg(self, ip="192.168.2.63", port=4242):
        return {"ip": ip, "gdb_port": port, "gdb_cmd": "arm-none-eabi-gdb"}

    def test_transport_unhealthy_sets_failure_kind(self):
        from ael.adapters import preflight

        probe_cfg = self._make_probe_cfg()
        with (
            patch.object(preflight, "_ping", return_value=False),
            patch.object(preflight, "_check_tcp", return_value=False),
            patch.object(preflight, "_monitor_targets", return_value=(False, [], "probe_transport_unhealthy")),
            patch.object(preflight, "_la_self_test", return_value=False),
            patch.object(preflight, "_fetch_port_config", return_value={}),
        ):
            ok, info = preflight.run(probe_cfg)

        assert ok is False
        assert info.get("failure_kind") == "transport_error"

    def test_busy_or_stuck_sets_instrument_not_ready_not_transport_error(self):
        """probe_busy_or_stuck sets instrument_not_ready (triggers recovery), NOT
        transport_error (which would abort the pack). Pack abort must NOT fire."""
        from ael.adapters import preflight

        probe_cfg = self._make_probe_cfg()
        with (
            patch.object(preflight, "_ping", return_value=True),
            patch.object(preflight, "_check_tcp", return_value=True),
            patch.object(preflight, "_monitor_targets", return_value=(False, [], "probe_busy_or_stuck")),
            patch.object(preflight, "_la_self_test", return_value=False),
            patch.object(preflight, "_fetch_port_config", return_value={}),
        ):
            ok, info = preflight.run(probe_cfg)

        assert ok is False
        assert info.get("failure_kind") == "instrument_not_ready"
        assert info.get("failure_kind") != "transport_error"

    def test_monitor_ok_no_failure_kind(self):
        """Successful preflight must not include failure_kind."""
        from ael.adapters import preflight

        probe_cfg = self._make_probe_cfg()
        with (
            patch.object(preflight, "_ping", return_value=True),
            patch.object(preflight, "_check_tcp", return_value=True),
            patch.object(preflight, "_monitor_targets", return_value=(True, ["STM32H750VBT6"], None)),
            patch.object(preflight, "_la_self_test", return_value=True),
            patch.object(preflight, "_fetch_port_config", return_value={}),
        ):
            ok, info = preflight.run(probe_cfg)

        assert ok is True
        assert "failure_kind" not in info


# ---------------------------------------------------------------------------
# Layer 2 — _PreflightAdapter.execute() passes failure_kind through
# ---------------------------------------------------------------------------

class TestPreflightAdapterPassthrough:
    """_PreflightAdapter must surface failure_kind at the top level of its
    return dict so runner.py can write it to the step result and ultimately to
    the top-level failure_kind in result.json."""

    def _make_adapter(self):
        from ael.adapter_registry import _PreflightAdapter
        return _PreflightAdapter()

    def _make_step(self, probe_cfg=None, bench_setup=None):
        return {
            "inputs": {
                "probe_cfg": probe_cfg or {},
                "bench_setup": bench_setup or {},
                "out_json": None,
                "output_mode": "silent",
                "log_path": None,
            }
        }

    def test_transport_error_surfaced_at_top_level(self):
        import ael.instruments.native_api_dispatch as dispatch

        adapter = self._make_adapter()
        step = self._make_step(probe_cfg={"ip": "1.2.3.4"})

        failing_payload = {
            "status": "error",
            "error": {
                "code": "preflight_failed",
                "message": "unreachable",
                "details": {
                    "preflight": {
                        "ping_ok": False,
                        "tcp_ok": False,
                        "monitor_ok": False,
                        "la_ok": False,
                        "failure_kind": "transport_error",
                    }
                },
            },
        }

        with patch.object(dispatch, "preflight_probe", return_value=failing_payload):
            result = adapter.execute(step, {}, {})

        assert result["ok"] is False
        assert result.get("failure_kind") == "transport_error"

    def test_no_failure_kind_when_not_transport(self):
        """If info dict has no failure_kind, adapter must not inject one."""
        import ael.instruments.native_api_dispatch as dispatch

        adapter = self._make_adapter()
        step = self._make_step(probe_cfg={"ip": "1.2.3.4"})

        failing_payload = {
            "status": "error",
            "error": {
                "code": "preflight_failed",
                "message": "la failed",
                "details": {
                    "preflight": {
                        "ping_ok": True,
                        "tcp_ok": True,
                        "monitor_ok": True,
                        "la_ok": False,
                    }
                },
            },
        }

        with patch.object(dispatch, "preflight_probe", return_value=failing_payload):
            result = adapter.execute(step, {}, {})

        assert result["ok"] is False
        assert "failure_kind" not in result

    def test_success_no_failure_kind(self):
        import ael.instruments.native_api_dispatch as dispatch

        adapter = self._make_adapter()
        step = self._make_step(probe_cfg={"ip": "1.2.3.4"})

        ok_payload = {"status": "ok", "data": {"preflight": {"ping_ok": True}}}

        with patch.object(dispatch, "preflight_probe", return_value=ok_payload):
            result = adapter.execute(step, {}, {})

        assert result["ok"] is True
        assert "failure_kind" not in result


# ---------------------------------------------------------------------------
# Layer 3 — run_pack() aborts on transport_error
# ---------------------------------------------------------------------------

class TestPackTransportAbort:
    """run_pack() must stop iterating tests as soon as it sees
    failure_kind=transport_error, regardless of --stop-on-fail."""

    def _make_pack_json(self, tmp_path: Path, tests: list[str]) -> Path:
        pack = tmp_path / "test_pack.json"
        pack.write_text(
            json.dumps({"name": "test_pack", "board": "stm32h750vbt6", "tests": tests}),
            encoding="utf-8",
        )
        return pack

    def _make_result_json(self, tmp_path: Path, name: str, ok: bool, failure_kind: str = "") -> Path:
        run_dir = tmp_path / "runs" / name
        artifacts_dir = run_dir / "artifacts"
        artifacts_dir.mkdir(parents=True)
        # top-level result.json (what run_paths.result points to)
        (run_dir / "result.json").write_text(
            json.dumps({"ok": ok, "failed_step": "preflight" if not ok else ""}),
            encoding="utf-8",
        )
        # artifacts/result.json (where runner.py writes failure_kind)
        result_file = artifacts_dir / "result.json"
        result_file.write_text(
            json.dumps({"ok": ok, "failure_kind": failure_kind, "failed_step": "preflight" if not ok else ""}),
            encoding="utf-8",
        )
        return run_dir / "result.json"

    def test_pack_aborts_after_first_transport_error(self, tmp_path):
        """When the first test fails with transport_error, remaining tests must not run."""
        import ael.__main__ as main_mod

        tests = [
            "tests/plans/test_a.json",
            "tests/plans/test_b.json",
            "tests/plans/test_c.json",
        ]
        pack_path = self._make_pack_json(tmp_path, tests)

        run_counter = {"n": 0}

        class _FakePaths:
            def __init__(self, result_path):
                self.result = result_path
                self.root = result_path.parent
                self.artifacts_dir = result_path.parent / "artifacts"

        def _fake_run_pipeline(**kwargs):
            idx = run_counter["n"]
            run_counter["n"] += 1
            result_file = self._make_result_json(
                tmp_path,
                f"run_{idx}",
                ok=False,
                failure_kind="transport_error",
            )
            return 1, _FakePaths(result_file)

        with (
            patch.object(main_mod, "run_pipeline", side_effect=_fake_run_pipeline),
            patch.object(main_mod, "resolve_control_instrument_config", return_value="fake_probe.yaml"),
        ):
            exit_code = main_mod.run_pack(str(pack_path))

        assert run_counter["n"] == 1, "pack must stop after the first transport_error"
        assert exit_code != 0

    def test_pack_continues_on_non_transport_failure(self, tmp_path):
        """A normal test failure (not transport_error) must not abort the pack."""
        import ael.__main__ as main_mod

        tests = [
            "tests/plans/test_a.json",
            "tests/plans/test_b.json",
            "tests/plans/test_c.json",
        ]
        pack_path = self._make_pack_json(tmp_path, tests)

        run_counter = {"n": 0}

        class _FakePaths:
            def __init__(self, result_path):
                self.result = result_path
                self.root = result_path.parent
                self.artifacts_dir = result_path.parent / "artifacts"

        def _fake_run_pipeline(**kwargs):
            idx = run_counter["n"]
            run_counter["n"] += 1
            # first test fails with a normal (non-transport) failure
            fk = "" if idx > 0 else "verification_mismatch"
            ok = idx > 0
            result_file = self._make_result_json(tmp_path, f"run_{idx}", ok=ok, failure_kind=fk)
            return (0 if ok else 1), _FakePaths(result_file)

        with (
            patch.object(main_mod, "run_pipeline", side_effect=_fake_run_pipeline),
            patch.object(main_mod, "resolve_control_instrument_config", return_value="fake_probe.yaml"),
        ):
            exit_code = main_mod.run_pack(str(pack_path))

        assert run_counter["n"] == 3, "pack must run all tests when failure is not transport_error"

    def test_stop_on_fail_still_works_independently(self, tmp_path):
        """--stop-on-fail must still abort on any failure, independent of transport_error logic."""
        import ael.__main__ as main_mod

        tests = ["tests/plans/test_a.json", "tests/plans/test_b.json"]
        pack_path = self._make_pack_json(tmp_path, tests)

        run_counter = {"n": 0}

        class _FakePaths:
            def __init__(self, result_path):
                self.result = result_path
                self.root = result_path.parent
                self.artifacts_dir = result_path.parent / "artifacts"

        def _fake_run_pipeline(**kwargs):
            idx = run_counter["n"]
            run_counter["n"] += 1
            result_file = self._make_result_json(tmp_path, f"run_{idx}", ok=False, failure_kind="verification_mismatch")
            return 1, _FakePaths(result_file)

        with (
            patch.object(main_mod, "run_pipeline", side_effect=_fake_run_pipeline),
            patch.object(main_mod, "resolve_control_instrument_config", return_value="fake_probe.yaml"),
        ):
            exit_code = main_mod.run_pack(str(pack_path), stop_on_fail=True)

        assert run_counter["n"] == 1
