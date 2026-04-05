"""
AEL Zephyr build adapter.

Called by _BuildAdapter when build.type == "zephyr".
Delegates to ZephyrBackend.build() and returns the ELF path string,
matching the contract of the other build_* adapters.

board_cfg["build"] fields consumed:
  zephyr_board       — Zephyr board name (e.g. "stm32f4_disco")
  project_dir        — path to the Zephyr app directory (absolute or repo-relative)
  build_dir          — output directory (absolute or repo-relative)
  config_args        — list of extra CMake args  (e.g. ["-DCONFIG_UART_CONSOLE=y"])
  pristine           — bool, default True (always clean build)
  extra_conf_file    — extra .conf file (absolute or repo-relative); passed as --extra-conf
  extra_overlay_file — extra .overlay file (absolute or repo-relative); passed as --extra-dtc-overlay
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any, Dict

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent


def run(board_cfg: Dict[str, Any]) -> str | None:
    from ael.backends.zephyr_backend import ZephyrBackend

    build_cfg  = board_cfg.get("build", {}) if isinstance(board_cfg, dict) else {}
    zephyr_board = str(build_cfg.get("zephyr_board") or "stm32f4_disco").strip()

    # project_dir: absolute path wins; relative is resolved against repo root
    project_dir_raw = str(build_cfg.get("project_dir") or "").strip()
    if not project_dir_raw:
        return None
    project_dir = Path(project_dir_raw)
    if not project_dir.is_absolute():
        project_dir = _REPO_ROOT / project_dir

    # build_dir: same resolution rules
    build_dir_raw = str(build_cfg.get("build_dir") or "").strip()
    if build_dir_raw:
        build_dir = Path(build_dir_raw)
        if not build_dir.is_absolute():
            build_dir = _REPO_ROOT / build_dir
    else:
        target = str(board_cfg.get("target") or "").strip()
        build_dir = _REPO_ROOT / "artifacts" / (f"build_{target}" if target else "build_zephyr")

    config_args = list(build_cfg.get("config_args") or [])
    pristine    = bool(build_cfg.get("pristine", True))

    def _resolve_optional(key: str) -> "Path | None":
        raw = str(build_cfg.get(key) or "").strip()
        if not raw:
            return None
        p = Path(raw)
        return p if p.is_absolute() else _REPO_ROOT / p

    extra_conf        = _resolve_optional("extra_conf_file")
    extra_dtc_overlay = _resolve_optional("extra_overlay_file")

    # Inject extra_overlay_file as -DDTC_OVERLAY_FILE=<abs> in config_args.
    # --extra-dtc-overlay is unreliable with west 1.5.0 for upstream samples
    # (the flag is silently ignored when source_dir is an absolute path outside
    # the workspace).  Passing via CMake -D is always honoured.
    if extra_dtc_overlay:
        config_args = list(config_args) + [f"-DDTC_OVERLAY_FILE={extra_dtc_overlay}"]

    backend = ZephyrBackend()

    # build() takes a sample_dir relative to the workspace; for external apps
    # we pass the absolute project_dir directly (west accepts absolute paths)
    elf = backend.build(
        board=zephyr_board,
        sample_dir=str(project_dir),   # west accepts absolute paths here
        build_dir=build_dir,
        config_args=config_args if config_args else None,
        pristine=pristine,
        extra_conf=extra_conf,
    )

    return str(elf) if elf and elf.exists() else None
