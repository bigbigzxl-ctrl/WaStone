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
        board:            str,
        sample_dir:       str,
        build_dir:        Path = None,
        config_args:      list = None,
        pristine:         bool = True,
        extra_conf:       Path = None,
        extra_dtc_overlay: Path = None,
    ) -> Path:
        """
        Run: west build [-p always] -b <board> <sample_dir> [-- <config_args>]

        Returns the path to the generated zephyr.elf.
        """
        build_dir = Path(build_dir) if build_dir else self.workspace / "build"
        cmd = [self.west_bin, "build"]
        if pristine:
            cmd += ["-p", "always"]
        cmd += ["-b", board, "--build-dir", str(build_dir)]
        if extra_conf:
            cmd += ["--extra-conf", str(extra_conf)]
        if extra_dtc_overlay:
            cmd += ["--extra-dtc-overlay", str(extra_dtc_overlay)]
        cmd += [str(self.workspace / sample_dir)]
        if config_args:
            cmd += ["--"] + config_args

        self._run(cmd)

        elf = build_dir / "zephyr" / "zephyr.elf"
        if not elf.exists():
            raise FileNotFoundError(f"Expected ELF not found: {elf}")
        return elf

    def flash(
        self,
        artifact:        Path  = None,
        runner:          str   = "openocd",
        build_dir:       Path  = None,
        openocd_config:  Path  = None,
        openocd_exe:     Path  = None,
        pyocd_target:    str   = None,
        pyocd_uid:       str   = None,
    ) -> None:
        """
        Run: west flash --runner <runner> [--build-dir <build_dir>]
                        [--openocd <exe>] [--config <cfg>]

        Special runner "pyocd_direct":
          Uses pyocd directly (not via west flash) to program the ELF from
          <build_dir>/zephyr/zephyr.elf.  Required for boards where
          west's openocd runner cannot halt the MCU without a hardware RESET
          line (e.g. STM32F103RCT6 + DAPLink with no nRESET wired).
          pyocd_target: pyOCD target name (e.g. "stm32f103rc")
          pyocd_uid:    probe unique ID (optional, selects a specific probe)

        artifact is unused (west locates the hex from the build dir),
        kept for interface compatibility with AELBackend.

        openocd_config: optional path to a custom OpenOCD config file.
        Use when the board's default openocd.cfg hardcodes a different interface
        (e.g. stlink) but the bench uses a CMSIS-DAP probe (DAPLink).
        Passed to the openocd runner as: west flash --runner openocd --config <path>

        openocd_exe: optional path to a specific openocd binary.
        OpenOCD 0.11 misidentifies the CMSIS-DAPv2 interface on DAPLink
        (c251:f001) and fails with "Resource busy".  Pass the esp32 OpenOCD 0.12+
        binary to work around this bug.
        Passed as: west flash --runner openocd --openocd <path>
        """
        build_dir = Path(build_dir) if build_dir else self.workspace / "build"

        if runner == "pyocd_direct":
            # Use pyocd directly — required when the openocd runner can't halt
            # the MCU without a hardware RESET line (e.g. DAPLink + STM32F1
            # with no nRESET wired).  pyocd halts via DHCSR (software debug
            # halt) which works regardless of RESET wiring.
            elf = build_dir / "zephyr" / "zephyr.elf"
            if not elf.exists():
                raise FileNotFoundError(f"pyocd_direct: ELF not found: {elf}")
            cmd = ["pyocd", "flash"]
            if pyocd_target:
                cmd += ["--target", pyocd_target]
            if pyocd_uid:
                cmd += ["--uid", pyocd_uid]
            cmd.append(str(elf))
            result = subprocess.run(
                cmd, capture_output=True, text=True
            )
            # pyocd exits non-zero on "WAIT ACK" during uninit (MCU resets and
            # disconnects — expected and harmless).  Check output instead.
            output = (result.stdout or "") + (result.stderr or "")
            if result.returncode != 0 and "programmed" not in output.lower():
                raise subprocess.CalledProcessError(result.returncode, cmd, result.stdout, result.stderr)
            return

        # Release the ST-Link from any AEL-managed gdbserver before OpenOCD takes it
        self._release_port(3333)
        self._release_port(4242)
        cmd = [self.west_bin, "flash", "--runner", runner,
               "--build-dir", str(build_dir)]
        if openocd_exe:
            cmd += ["--openocd", str(openocd_exe)]
        if openocd_config:
            cmd += ["--config", str(openocd_config)]
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

    def observe(
        self,
        port:            str,
        baud:            int   = 115200,
        duration_s:      float = 6.0,
        expect_patterns: list  = None,
        forbid_patterns: list  = None,
        profile:         str   = "stm32",
        raw_log_path:    str   = None,
    ) -> dict:
        """
        Capture serial output from a Zephyr board and check expected patterns.

        Delegates entirely to AEL's existing observe_uart_log layer — no
        custom transport code needed.  The 'stm32' profile is the right
        default for Zephyr on STM32 targets (no ESP32-style boot ROM noise).

        Returns the raw observe result dict from observe_uart_log.run().
        The dict includes an extra '_cfg' key so that verify() can reconstruct
        the observation context without the caller having to pass it again.

        Typical usage:
            obs = backend.observe(port='/dev/ttyUSB0', baud=115200,
                                  duration_s=6, expect_patterns=['Hello World'])
            verdict = backend.verify(obs)
        """
        import os
        import tempfile
        from ael.adapters import observe_log

        cfg = {
            "enabled":         True,
            "port":            port,
            "baud":            baud,
            "duration_s":      duration_s,
            "profile":         profile,
            "expect_patterns": expect_patterns or [],
            "forbid_patterns": forbid_patterns or [],
            "reset_strategy":  "none",
        }

        # Provide a writable log path; create a temp file if caller didn't supply one
        if raw_log_path is None:
            fd, raw_log_path = tempfile.mkstemp(
                suffix=".log", prefix="zephyr_uart_"
            )
            os.close(fd)

        result = observe_log.run_serial_log(cfg, raw_log_path=raw_log_path)

        # Stash cfg inside the result so verify() has full context
        result["_cfg"] = cfg
        result["_raw_log_path"] = raw_log_path
        return result

    def verify(self, observation: dict, expectations: dict = None) -> dict:
        """
        Verify a serial observation against expected patterns.

        Delegates to AEL's check_eval.evaluate_uart_facts().
        'observation' should be the dict returned by observe().

        Returns:
            {
                "ok":            bool,
                "failure_kind":  str,   # empty string when ok
                "failure_class": str,
                "error_summary": str,
                "recovery_hint": dict | None,
                "verify_substage": "uart.verify",
            }
        """
        from ael import check_eval

        cfg = observation.get("_cfg", {})
        if expectations:
            # Allow caller to override/supplement cfg fields
            cfg = {**cfg, **expectations}

        # evaluate_uart_facts needs firmware_ready_seen derived from missing_expect
        facts = {
            **observation,
            "firmware_ready_seen": not bool(observation.get("missing_expect", [])),
        }

        return check_eval.evaluate_uart_facts(facts, cfg)
