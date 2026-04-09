#!/usr/bin/env python3
"""
PA35T StarLite — AEL Semantic Debug Full Verification
======================================================

Runs the complete AEL Semantic Debug test suite against a PA StarLite A7-35T
running the LiteX VexRiscv SoC with AEL debug CSRs.

Requires litex_server running:
    cd /nvme1t/work/codex/riscv-semantic-dbg
    tail -f /dev/null | litex_server --jtag --jtag-config=openocd_xc7_ft2232.cfg &

Test stages:
    Stage 1 — SPI loopback (TC-01..05): hardware loopback D19→E19
    Stage 2 — RTOS task state (TC-11..14): task switches, BLOCKED, SPI event
    Stage 3 — CPU fault capture (TC-21): ecall → EV_CPU_FAULT via mtvec

Usage:
    python3 experiments/pa35t_starlite/semantic_debug_verify.py [--host localhost] [--port 1234]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Repo root on path
_REPO = Path(__file__).resolve().parents[2]
_RISCV_REPO = Path("/nvme1t/work/codex/riscv-semantic-dbg")
sys.path.insert(0, str(_REPO))
sys.path.insert(0, '/home/aes/.local/lib/python3.10/site-packages')

from litex.tools.litex_client import RemoteClient


def _import_test(name: str):
    """Import a test module from riscv-semantic-dbg and return its run() fn."""
    import importlib.util
    path = _RISCV_REPO / "ael_integration" / "tests" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    mod  = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.run


def main() -> int:
    parser = argparse.ArgumentParser(description="PA35T StarLite AEL semantic debug verification")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=1234)
    parser.add_argument("--stage", choices=["spi", "rtos", "all"], default="all",
                        help="Which test stage to run (default: all)")
    args = parser.parse_args()

    wb = RemoteClient(host=args.host, port=args.port)
    wb.open()

    results: dict[str, bool] = {}

    print("\n" + "=" * 62)
    print("  PA35T StarLite — AEL Semantic Debug Full Verification")
    print("  SoC: LiteX VexRiscv  |  loopback: D19→E19 (hw wire)")
    print("=" * 62)

    # -----------------------------------------------------------------------
    # Stage 1 — SPI loopback
    # -----------------------------------------------------------------------
    if args.stage in ("spi", "all"):
        print("\n[Stage 1] SPI loopback (TC-01..05) — hardware path D19→E19")
        try:
            run_spi = _import_test("test_semantic_spi")
            ok = run_spi(wb=wb)
            results["stage1_spi"] = ok
            print(f"  Stage 1: {'PASS' if ok else 'FAIL'}")
        except Exception as exc:
            print(f"  Stage 1: ERROR — {exc}")
            results["stage1_spi"] = False

    # -----------------------------------------------------------------------
    # Stage 2+3 — RTOS task state + CPU fault
    # -----------------------------------------------------------------------
    if args.stage in ("rtos", "all"):
        print("\n[Stage 2+3] RTOS task state (TC-11..14) + CPU fault (TC-21)")
        try:
            run_rtos = _import_test("test_rtos_tasks")
            ok = run_rtos(wb=wb)
            results["stage2_rtos"] = ok
            print(f"  Stage 2+3: {'PASS' if ok else 'FAIL'}")
        except Exception as exc:
            print(f"  Stage 2+3: ERROR — {exc}")
            results["stage2_rtos"] = False

    wb.close()

    # -----------------------------------------------------------------------
    # Summary
    # -----------------------------------------------------------------------
    print("\n" + "=" * 62)
    overall = all(results.values())
    print(f"  OVERALL: {'PASS' if overall else 'FAIL'}")
    for stage, ok in results.items():
        print(f"    {stage:25s} {'PASS' if ok else 'FAIL'}")
    print("=" * 62 + "\n")

    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
