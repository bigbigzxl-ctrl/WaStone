"""
GDB Server Registry — process-safe (ip, port) resource manager.

Prevents port conflicts when multiple AEL suites run in parallel.
Uses fcntl file locking + JSON state for cross-process safety.

Key rules:
  • (ip, port) is the unique allocation key.
  • Remote instruments (non-localhost) occupy a fixed (ip, port); if another
    instrument already holds it, an error is raised.
  • Local instruments (127.0.0.1) will be auto-assigned the next free port in
    _LOCAL_PORT_RANGE if their preferred port is already taken by a different
    instrument.
  • Stale entries (PID dead) are purged automatically on every load.
  • Persistent servers (pyocd --persist) are NOT killed on release; the entry
    is kept alive in the registry until the PID dies or the port is reclaimed.
"""

import json
import os
import time
from pathlib import Path
from typing import Optional

_REGISTRY_PATH = Path("/tmp/ael_gdb_registry.json")
_LOCK_PATH     = Path("/tmp/ael_gdb_registry.lock")
_LOCAL_HOSTS   = {"127.0.0.1", "localhost", "::1"}
_LOCAL_PORT_RANGE = range(3333, 3400)   # 67 slots — plenty for any bench


# ── file lock ────────────────────────────────────────────────────────────────

class _FileLock:
    def __init__(self, path: Path, timeout_s: float = 5.0):
        self._path    = path
        self._timeout = timeout_s
        self._fd      = None

    def __enter__(self):
        import fcntl
        self._fd = open(self._path, "w")
        deadline = time.monotonic() + self._timeout
        while True:
            try:
                fcntl.flock(self._fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return self
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    self._fd.close()
                    raise TimeoutError(f"GDB registry: could not acquire lock {self._path}")
                time.sleep(0.05)

    def __exit__(self, *_):
        import fcntl
        if self._fd:
            fcntl.flock(self._fd, fcntl.LOCK_UN)
            self._fd.close()
            self._fd = None


# ── internal helpers ─────────────────────────────────────────────────────────

def _pid_alive(pid: int) -> bool:
    try:
        os.kill(int(pid), 0)
        return True
    except (ProcessLookupError, OSError):
        return False


def _load() -> dict:
    try:
        return json.loads(_REGISTRY_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {"allocations": {}}


def _save(state: dict) -> None:
    _REGISTRY_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")


def _purge_stale(state: dict) -> None:
    """Remove entries whose server process has died."""
    allocs = state.get("allocations", {})
    dead = [
        key for key, entry in allocs.items()
        if int(entry.get("pid") or 0) > 0 and not _pid_alive(int(entry["pid"]))
    ]
    for key in dead:
        allocs.pop(key, None)


# ── public API ────────────────────────────────────────────────────────────────

def allocate(
    ip: str,
    preferred_port: int,
    server_type: str,
    instrument_id: str,
) -> int:
    """
    Request a (ip, port) slot for a GDB server.

    Returns the assigned port, which may differ from *preferred_port* if a
    conflict is detected for local instruments.

    Raises RuntimeError if a remote port is already held by a different
    instrument.
    """
    with _FileLock(_LOCK_PATH):
        state = _load()
        _purge_stale(state)
        allocs = state.setdefault("allocations", {})

        key      = f"{ip}:{preferred_port}"
        existing = allocs.get(key)

        # Check if this instrument already holds any port — return that allocation.
        for existing_key, existing_entry in allocs.items():
            if existing_entry.get("instrument_id") == instrument_id and existing_key.startswith(f"{ip}:"):
                already_port = int(existing_key.split(":")[-1])
                existing_entry["server_type"] = server_type
                _save(state)
                return already_port

        if existing is None:
            allocs[key] = {
                "instrument_id": instrument_id,
                "server_type":   server_type,
                "pid":           0,
            }
            _save(state)
            return preferred_port

        # Different instrument holds the preferred slot.
        is_local = ip in _LOCAL_HOSTS
        if not is_local:
            raise RuntimeError(
                f"GDB registry: {ip}:{preferred_port} already allocated to "
                f"'{existing.get('instrument_id')}' — cannot share a remote port."
            )

        # Local conflict → find next free port.
        for port in _LOCAL_PORT_RANGE:
            candidate = f"{ip}:{port}"
            if candidate not in allocs:
                allocs[candidate] = {
                    "instrument_id": instrument_id,
                    "server_type":   server_type,
                    "pid":           0,
                }
                _save(state)
                return port

        raise RuntimeError(
            f"GDB registry: no free ports in {_LOCAL_PORT_RANGE.start}–{_LOCAL_PORT_RANGE.stop - 1}"
        )


def update_pid(ip: str, port: int, pid: int) -> None:
    """Record the PID of a successfully started GDB server."""
    with _FileLock(_LOCK_PATH):
        state = _load()
        key = f"{ip}:{port}"
        if key in state.get("allocations", {}):
            state["allocations"][key]["pid"] = pid
            _save(state)


def release(ip: str, port: int) -> None:
    """
    Release a (ip, port) slot.

    Does NOT kill the server process — persistent servers (e.g. pyocd
    --persist) stay running and can be reused by the next test.
    The entry is removed from the registry; stale-detection will purge it
    naturally once the process exits.
    """
    with _FileLock(_LOCK_PATH):
        state = _load()
        state.get("allocations", {}).pop(f"{ip}:{port}", None)
        _save(state)


def get_entry(ip: str, port: int) -> Optional[dict]:
    """Return the registry entry for (ip, port), or None if not allocated."""
    with _FileLock(_LOCK_PATH):
        state = _load()
        _purge_stale(state)
        _save(state)
        return state.get("allocations", {}).get(f"{ip}:{port}")
