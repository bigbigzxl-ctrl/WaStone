"""
AEL Zephyr flash adapter.

Called by _LoadAdapter when flash.method == "zephyr_west".
Delegates to ZephyrBackend.flash() (which handles port release before
calling west flash --runner openocd).

flash_cfg fields consumed:
  runner          — OpenOCD runner name, default "openocd"
  build_dir       — explicit build directory (absolute); if absent, ZephyrBackend
                    defaults to ~/zephyrproject/zephyr/build which is correct when
                    build_zephyr.py wrote into a custom build_dir stored in context.
  openocd_config  — (optional) absolute or repo-relative path to a custom OpenOCD
                    config file. Use when the board's default openocd.cfg hardcodes
                    a different interface (e.g. stlink) but you have a CMSIS-DAP
                    probe. Passed to west flash as: --config <path>

Note: firmware_path is ignored — west flash finds the artefact itself
from the build directory's CMakeCache.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent


def run(flash_cfg: Dict[str, Any]) -> bool:
    from ael.backends.zephyr_backend import ZephyrBackend

    runner        = str(flash_cfg.get("runner") or "openocd").strip()
    build_dir_raw = str(flash_cfg.get("build_dir") or "").strip()
    build_dir     = Path(build_dir_raw) if build_dir_raw else None

    # Optional: override the board's default openocd.cfg (e.g. stlink→cmsis-dap)
    openocd_cfg_raw = str(flash_cfg.get("openocd_config") or "").strip()
    openocd_config: Path | None = None
    if openocd_cfg_raw:
        p = Path(openocd_cfg_raw)
        openocd_config = p if p.is_absolute() else _REPO_ROOT / p

    backend = ZephyrBackend()
    backend.flash(runner=runner, build_dir=build_dir, openocd_config=openocd_config)
    return True
