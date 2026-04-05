"""
AEL Backend Contract — abstract base class for all execution substrates.

Substrates: IDF, Zephyr, Linux, FPGA, native bare-metal.
Each backend implements this interface; the AEL upper layer stays substrate-agnostic.
"""

from abc import ABC, abstractmethod
from pathlib import Path


class AELBackend(ABC):

    @abstractmethod
    def detect_project_type(self, project_dir: Path) -> bool:
        """Return True if project_dir belongs to this backend."""

    @abstractmethod
    def build(self, **kwargs) -> Path:
        """Build the project. Returns path to the output ELF artifact."""

    @abstractmethod
    def flash(self, artifact: Path = None, **kwargs) -> None:
        """Flash the artifact to the target board."""

    @abstractmethod
    def start_debugserver(self, **kwargs):
        """
        Start a debug server (OpenOCD, JLink, pyOCD, …).
        Returns a subprocess.Popen handle; caller is responsible for cleanup.
        """

    def observe(self, port: str, baud: int,
                duration_s: float, expect_patterns: list) -> dict:
        """
        Collect serial/log output and check against expected patterns.
        Default implementation re-uses AEL's existing observe_uart layer.
        Backends may override for custom transports (RTT, semihosting, …).
        """
        raise NotImplementedError(
            "observe() not implemented — wire into AEL observe_uart layer"
        )

    def verify(self, observation: dict, expectations: dict) -> dict:
        """
        Verify an observation dict against expectations.
        Default re-uses AEL's existing verify layer.
        """
        raise NotImplementedError(
            "verify() not implemented — wire into AEL verify layer"
        )
