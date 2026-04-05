"""
AEL Zephyr Backend

Wraps the `west` toolchain (build / flash / debugserver) so AEL's upper
layer can drive a Zephyr project the same way it drives IDF or bare-metal.

Validated on:
  board : stm32f4_disco (STM32F407VGT6)
  runner: openocd  (ST-Link V2-A onboard)
  host  : Ubuntu 22.04, west 1.5.0, Zephyr 4.4.0-rc2
  date  : 2026-04-05

Pilot steps completed:
  Step 1 — blinky build + flash                    PASS
  Step 2 — hello_world UART observe (PA2, 115200)  PASS
  Step 3 — debugserver + GDB attach                PASS
"""

import os
import subprocess
import time
from pathlib import Path

from .base import AELBackend

# ---------------------------------------------------------------------------
# Default environment variables required by west / Zephyr CMake
# ---------------------------------------------------------------------------
_DEFAULT_TOOLCHAIN_VARIANT = "gnuarmemb"
_DEFAULT_GNUARMEMB_PATH    = "/nvme1t/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi"
_DEFAULT_WEST_PYTHON       = str(Path.home() / "zephyr-venv/bin/python3")
_DEFAULT_WEST_BIN          = str(Path.home() / "zephyr-venv/bin/west")
_DEFAULT_WORKSPACE         = str(Path.home() / "zephyrproject/zephyr")


class ZephyrBackend(AELBackend):
    """
    Minimal Zephyr backend: detect → build → flash → debugserver.

    observe() and verify() deliberately delegate to AEL's existing layers
    (observe_uart + verify) — no duplication needed.
    """

    def __init__(
        self,
        workspace:         str = _DEFAULT_WORKSPACE,
        west_bin:          str = _DEFAULT_WEST_BIN,
        west_python:       str = _DEFAULT_WEST_PYTHON,
        toolchain_variant: str = _DEFAULT_TOOLCHAIN_VARIANT,
        gnuarmemb_path:    str = _DEFAULT_GNUARMEMB_PATH,
    ):
        self.workspace         = Path(workspace).expanduser()
        self.west_bin          = west_bin
        self.west_python       = west_python
        self.toolchain_variant = toolchain_variant
        self.gnuarmemb_path    = gnuarmemb_path

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _env(self) -> dict:
        """Build environment dict for all west / CMake calls."""
        env = os.environ.copy()
        env["ZEPHYR_TOOLCHAIN_VARIANT"] = self.toolchain_variant
        env["GNUARMEMB_TOOLCHAIN_PATH"] = self.gnuarmemb_path
        env["WEST_PYTHON"]              = self.west_python
        return env

    def _run(self, cmd: list, **kwargs) -> subprocess.CompletedProcess:
        return subprocess.run(
            cmd,
            env=self._env(),
            cwd=str(self.workspace),
            check=True,
            **kwargs,
        )

    def _release_port(self, port: int) -> None:
        """
        Kill any process listening on *port* before we try to bind it.

        In an AEL bench session, st-util, pyocd, or a stale OpenOCD may hold
        :3333 or :4242.  We discovered this during the STM32F407 pilot:
          - AEL st-util (port 4242) blocked OpenOCD from claiming the ST-Link
          - pyocd gdbserver (port 3333) blocked west debugserver from binding
        This helper removes both races before flash/debugserver calls.
        """
        try:
            out = subprocess.check_output(
                ["lsof", "-ti", f"TCP:{port}"], text=True
            ).strip()
        except subprocess.CalledProcessError:
            return  # nothing on that port
        for pid_str in out.splitlines():
            try:
                os.kill(int(pid_str), 15)   # SIGTERM
            except ProcessLookupError:
                pass
        time.sleep(0.5)

    # ------------------------------------------------------------------
    # AELBackend interface
    # ------------------------------------------------------------------

    def detect_project_type(self, project_dir: Path) -> bool:
        """
        A directory is a Zephyr project when it contains both
        CMakeLists.txt and prj.conf.
        """
        project_dir = Path(project_dir)
        return (
            (project_dir / "CMakeLists.txt").exists()
            and (project_dir / "prj.conf").exists()
        )

    def build(
        self,
        board:       str,
        sample_dir:  str,
        build_dir:   Path = None,
        config_args: list = None,
        pristine:    bool = True,
    ) -> Path:
        """
        Run: west build [-p always] -b <board> <sample_dir> [-- <config_args>]

        Returns the path to the generated zephyr.elf.
        """
        build_dir = Path(build_dir) if build_dir else self.workspace / "build"
        cmd = [self.west_bin, "build"]
        if pristine:
            cmd += ["-p", "always"]
        cmd += ["-b", board, str(self.workspace / sample_dir),
                "--build-dir", str(build_dir)]
        if config_args:
            cmd += ["--"] + config_args

        self._run(cmd)

        elf = build_dir / "zephyr" / "zephyr.elf"
        if not elf.exists():
            raise FileNotFoundError(f"Expected ELF not found: {elf}")
        return elf

    def flash(
        self,
        artifact:  Path  = None,
        runner:    str   = "openocd",
        build_dir: Path  = None,
    ) -> None:
        """
        Run: west flash --runner <runner> [--build-dir <build_dir>]

        artifact is unused (west locates the hex from the build dir),
        kept for interface compatibility with AELBackend.
        """
        build_dir = Path(build_dir) if build_dir else self.workspace / "build"
        # Release the ST-Link from any AEL-managed gdbserver before OpenOCD takes it
        self._release_port(3333)
        self._release_port(4242)
        cmd = [self.west_bin, "flash", "--runner", runner,
               "--build-dir", str(build_dir)]
        self._run(cmd)

    def start_debugserver(
        self,
        runner:    str  = "openocd",
        build_dir: Path = None,
        port:      int  = 3333,
        timeout_s: int  = 15,
    ) -> subprocess.Popen:
        """
        Run: west debugserver --runner <runner>

        Waits until OpenOCD reports 'Listening on port <port>', then
        returns the Popen handle.  Caller must call proc.terminate() when done.
        """
        build_dir = Path(build_dir) if build_dir else self.workspace / "build"
        self._release_port(port)
        cmd = [self.west_bin, "debugserver", "--runner", runner,
               "--build-dir", str(build_dir)]

        proc = subprocess.Popen(
            cmd,
            env=self._env(),
            cwd=str(self.workspace),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

        ready_marker = f"Listening on port {port}"
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            line = proc.stdout.readline().decode("utf-8", errors="replace")
            if not line and proc.poll() is not None:
                raise RuntimeError(
                    f"west debugserver exited unexpectedly (rc={proc.returncode})"
                )
            if ready_marker in line:
                return proc
        proc.terminate()
        raise TimeoutError(
            f"west debugserver did not become ready within {timeout_s}s"
        )

    # observe() and verify() intentionally not overridden:
    # AEL's existing observe_uart + verify layers handle all serial capture
    # and pattern matching without modification.
