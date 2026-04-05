"""
AEL Zephyr flash adapter.

Called by _LoadAdapter when flash.method == "zephyr_west".
Delegates to ZephyrBackend.flash() (which handles port release before
calling west flash --runner openocd).

flash_cfg fields consumed:
  runner    — OpenOCD runner name, default "openocd"
  build_dir — explicit build directory (absolute); if absent, ZephyrBackend
              defaults to ~/zephyrproject/zephyr/build which is correct when
              build_zephyr.py wrote into a custom build_dir stored in context.

Note: firmware_path is ignored — west flash finds the artefact itself
from the build directory's CMakeCache.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict


def run(flash_cfg: Dict[str, Any]) -> bool:
    from ael.backends.zephyr_backend import ZephyrBackend

    runner    = str(flash_cfg.get("runner") or "openocd").strip()
    build_dir_raw = str(flash_cfg.get("build_dir") or "").strip()
    build_dir = Path(build_dir_raw) if build_dir_raw else None

    backend = ZephyrBackend()
    backend.flash(runner=runner, build_dir=build_dir)
    return True
