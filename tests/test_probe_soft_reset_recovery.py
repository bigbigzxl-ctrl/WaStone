"""Tests for probe.soft.reset recovery action.

Covers the three layers introduced to auto-recover from probe_busy_or_stuck:

  1. preflight.run() emits failure_kind="instrument_not_ready" when
     probe_busy_or_stuck is detected.
  2. _PreflightAdapter.execute() attaches a recovery_hint with
     action_type="probe.soft.reset" when failure_kind=instrument_not_ready.
  3. _ProbeSoftResetRecoveryAdapter POSTs /set_credentials and polls the GDB
     port until it comes back up.
"""
from __future__ import annotations

import socket
from unittest.mock import MagicMock, patch, call

import pytest


# ---------------------------------------------------------------------------
# Layer 1 — preflight.run() failure_kind for probe_busy_or_stuck
# ---------------------------------------------------------------------------

class TestPreflightBusyOrStuckFailureKind:

    def _probe_cfg(self):
        return {"ip": "192.168.2.63", "gdb_port": 4242, "gdb_cmd": "arm-none-eabi-gdb"}

    def test_busy_or_stuck_sets_instrument_not_ready(self):
        from ael.adapters import preflight

        with (
            patch.object(preflight, "_ping", return_value=True),
            patch.object(preflight, "_check_tcp", return_value=True),
            patch.object(preflight, "_monitor_targets", return_value=(False, [], "probe_busy_or_stuck")),
            patch.object(preflight, "_la_self_test", return_value=False),
            patch.object(preflight, "_fetch_port_config", return_value={"pbcfg": "0"}),
        ):
            ok, info = preflight.run(self._probe_cfg())

        assert ok is False
        assert info.get("failure_kind") == "instrument_not_ready"

    def test_transport_unhealthy_still_sets_transport_error(self):
        from ael.adapters import preflight

        with (
            patch.object(preflight, "_ping", return_value=False),
            patch.object(preflight, "_check_tcp", return_value=False),
            patch.object(preflight, "_monitor_targets", return_value=(False, [], "probe_transport_unhealthy")),
            patch.object(preflight, "_la_self_test", return_value=False),
            patch.object(preflight, "_fetch_port_config", return_value={}),
        ):
            ok, info = preflight.run(self._probe_cfg())

        assert ok is False
        assert info.get("failure_kind") == "transport_error"

    def test_monitor_failed_no_failure_kind(self):
        """probe_monitor_failed (IP reachable but monitor broken) does not set failure_kind."""
        from ael.adapters import preflight

        with (
            patch.object(preflight, "_ping", return_value=True),
            patch.object(preflight, "_check_tcp", return_value=True),
            patch.object(preflight, "_monitor_targets", return_value=(False, [], "probe_monitor_failed")),
            patch.object(preflight, "_la_self_test", return_value=False),
            patch.object(preflight, "_fetch_port_config", return_value={}),
        ):
            ok, info = preflight.run(self._probe_cfg())

        assert ok is False
        assert "failure_kind" not in info


# ---------------------------------------------------------------------------
# Layer 2 — _PreflightAdapter attaches recovery_hint for instrument_not_ready
# ---------------------------------------------------------------------------

class TestPreflightAdapterRecoveryHint:

    def _make_step(self, probe_cfg=None):
        return {
            "inputs": {
                "probe_cfg": probe_cfg or {"ip": "192.168.2.63", "gdb_port": 4242,
                                            "web_user": "admin", "web_pass": "admin"},
                "bench_setup": {},
                "out_json": None,
                "output_mode": "silent",
                "log_path": None,
            }
        }

    def _busy_payload(self, pbcfg="0"):
        return {
            "status": "error",
            "error": {
                "code": "preflight_failed",
                "message": "busy",
                "details": {
                    "preflight": {
                        "ping_ok": True,
                        "tcp_ok": True,
                        "monitor_ok": False,
                        "la_ok": False,
                        "failure_kind": "instrument_not_ready",
                        "port_config": {"pbcfg": pbcfg},
                    }
                },
            },
        }

    def test_recovery_hint_attached_on_instrument_not_ready(self):
        import ael.instruments.native_api_dispatch as dispatch
        from ael.adapter_registry import _PreflightAdapter

        adapter = _PreflightAdapter()
        step = self._make_step()

        with patch.object(dispatch, "preflight_probe", return_value=self._busy_payload("1")):
            result = adapter.execute(step, {}, {})

        assert result["ok"] is False
        assert result.get("failure_kind") == "instrument_not_ready"
        hint = result.get("recovery_hint")
        assert hint is not None
        assert hint["action_type"] == "probe.soft.reset"
        assert hint["recoverable"] is True
        assert hint["retry"] is True
        assert hint["params"]["ip"] == "192.168.2.63"
        assert hint["params"]["pbcfg"] == "1"

    def test_no_recovery_hint_on_transport_error(self):
        """transport_error (instrument offline) should NOT get a recovery_hint."""
        import ael.instruments.native_api_dispatch as dispatch
        from ael.adapter_registry import _PreflightAdapter

        adapter = _PreflightAdapter()
        step = self._make_step()
        offline_payload = {
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

        with patch.object(dispatch, "preflight_probe", return_value=offline_payload):
            result = adapter.execute(step, {}, {})

        assert result["ok"] is False
        assert result.get("failure_kind") == "transport_error"
        assert "recovery_hint" not in result


# ---------------------------------------------------------------------------
# Layer 3 — _ProbeSoftResetRecoveryAdapter behaviour
# ---------------------------------------------------------------------------

class TestProbeSoftResetRecoveryAdapter:

    def _make_action(self, ip="192.168.2.63", gdb_port=4242, pbcfg="0"):
        return {
            "params": {
                "ip": ip,
                "gdb_port": gdb_port,
                "web_scheme": "https",
                "web_port": 443,
                "web_user": "admin",
                "web_pass": "admin",
                "web_verify_ssl": False,
                "pbcfg": pbcfg,
            }
        }

    def test_returns_ok_when_gdb_port_comes_back(self):
        from ael.adapter_registry import _ProbeSoftResetRecoveryAdapter
        import requests as req_mod

        adapter = _ProbeSoftResetRecoveryAdapter()

        mock_resp = MagicMock()
        mock_resp.status_code = 200

        # Simulate: POST succeeds, then port opens on first poll
        conn_mock = MagicMock()
        conn_mock.__enter__ = MagicMock(return_value=conn_mock)
        conn_mock.__exit__ = MagicMock(return_value=False)

        with (
            patch("requests.post", return_value=mock_resp),
            patch("socket.create_connection", return_value=conn_mock),
        ):
            result = adapter.execute(self._make_action(), {}, {})

        assert result["ok"] is True

    def test_returns_fail_when_port_never_comes_back(self):
        from ael.adapter_registry import _ProbeSoftResetRecoveryAdapter
        import time as time_mod

        adapter = _ProbeSoftResetRecoveryAdapter()
        adapter._MAX_WAIT_S = 2   # shorten timeout for test
        adapter._POLL_INTERVAL_S = 0.1

        mock_resp = MagicMock()

        with (
            patch("requests.post", return_value=mock_resp),
            patch("socket.create_connection", side_effect=OSError("refused")),
        ):
            result = adapter.execute(self._make_action(), {}, {})

        assert result["ok"] is False
        assert "did not come back" in result.get("error_summary", "")

    def test_post_exception_still_polls(self):
        """Probe may reboot before responding — POST exception is normal and should not abort."""
        from ael.adapter_registry import _ProbeSoftResetRecoveryAdapter

        adapter = _ProbeSoftResetRecoveryAdapter()

        conn_mock = MagicMock()
        conn_mock.__enter__ = MagicMock(return_value=conn_mock)
        conn_mock.__exit__ = MagicMock(return_value=False)

        with (
            patch("requests.post", side_effect=Exception("connection reset")),
            patch("socket.create_connection", return_value=conn_mock),
        ):
            result = adapter.execute(self._make_action(), {}, {})

        assert result["ok"] is True

    def test_missing_ip_returns_fail(self):
        from ael.adapter_registry import _ProbeSoftResetRecoveryAdapter

        adapter = _ProbeSoftResetRecoveryAdapter()
        result = adapter.execute({"params": {}}, {}, {})

        assert result["ok"] is False
        assert "no IP" in result.get("error_summary", "")

    def test_registered_in_adapter_registry(self):
        from ael.adapter_registry import AdapterRegistry
        from ael import failure_recovery

        reg = AdapterRegistry()
        adapter = reg.recovery(failure_recovery.RECOVERY_ACTION_PROBE_SOFT_RESET)
        assert adapter.__class__.__name__ == "_ProbeSoftResetRecoveryAdapter"
