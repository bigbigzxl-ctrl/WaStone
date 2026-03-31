from __future__ import annotations

import json
import os
import re
import threading
from datetime import datetime
from pathlib import Path
from typing import Any, Iterator

from ael import paths as ael_paths


SCHEMA_VERSION = "ael.workflow_archive.event.v0.2"
_APPEND_LOCK = threading.Lock()
_DAILY_PAT = re.compile(r"^\d{4}-\d{2}-\d{2}\.jsonl$")


def archive_root() -> Path:
    env = str(os.getenv("AEL_WORKFLOW_ARCHIVE_ROOT", "")).strip()
    root = Path(env).expanduser() if env else (ael_paths.repo_root() / "workflow_archive")
    root.mkdir(parents=True, exist_ok=True)
    return root


def global_events_path_for_date(date: str) -> Path:
    """Return the daily archive file path for *date* (``YYYY-MM-DD``)."""
    return archive_root() / f"{date}.jsonl"


def list_daily_files(root: Path | None = None) -> list[Path]:
    """Return a sorted list of daily ``YYYY-MM-DD.jsonl`` files in *root*."""
    r = root if root is not None else archive_root()
    if not r.exists():
        return []
    return sorted(p for p in r.iterdir() if p.is_file() and _DAILY_PAT.match(p.name))


def run_events_path(run_root: str | Path) -> Path:
    return Path(run_root) / "workflow_events.jsonl"


def _is_json_scalar(value: Any) -> bool:
    return value is None or isinstance(value, (str, int, float, bool))


def _normalize(value: Any) -> Any:
    if _is_json_scalar(value):
        return value
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, dict):
        out = {}
        for key, item in value.items():
            norm = _normalize(item)
            if norm is not None:
                out[str(key)] = norm
        return out
    if isinstance(value, (list, tuple)):
        out = []
        for item in value:
            norm = _normalize(item)
            if norm is not None:
                out.append(norm)
        return out
    return str(value)


def _append_jsonl(path: Path, record: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as f:
        f.write(json.dumps(record, sort_keys=True) + "\n")


def append_event(event: dict, run_root: str | Path | None = None) -> dict:
    record = {"schema": SCHEMA_VERSION, **_normalize(event)}
    if "timestamp" not in record:
        record["timestamp"] = datetime.now().isoformat()
    date = record["timestamp"][:10]
    with _APPEND_LOCK:
        _append_jsonl(global_events_path_for_date(date), record)
        if run_root:
            _append_jsonl(run_events_path(run_root), record)
    return record


def _iter_jsonl(path: Path, run_id: str | None = None) -> Iterator[dict]:
    """Yield parsed records from *path*, optionally filtered by *run_id*."""
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except Exception:
                continue
            if run_id and str(record.get("run_id") or "") != run_id:
                continue
            yield record


def env_conversation_context() -> dict:
    context = {
        "session_id": str(os.getenv("AEL_SESSION_ID", "")).strip() or None,
        "task_id": str(os.getenv("AEL_TASK_ID", "")).strip() or None,
        "user_request": str(os.getenv("AEL_USER_REQUEST", "")).strip() or None,
        "ai_response": str(os.getenv("AEL_AI_RESPONSE", "")).strip() or None,
        "user_confirmation": str(os.getenv("AEL_USER_CONFIRMATION", "")).strip() or None,
        "user_correction": str(os.getenv("AEL_USER_CORRECTION", "")).strip() or None,
        "ai_next_action": str(os.getenv("AEL_AI_NEXT_ACTION", "")).strip() or None,
    }
    return {k: v for k, v in context.items() if v is not None}


def workflow_event(
    *,
    actor: str,
    action: str,
    text: str | None = None,
    status: str | None = None,
    stage: str | None = None,
    extra: dict | None = None,
) -> dict:
    event = {
        "category": "workflow",
        "actor": actor,
        "action": action,
    }
    if status is not None:
        event["status"] = status
    if stage is not None:
        event["stage"] = stage
    if text is not None:
        event["message"] = {"text": text}
    if isinstance(extra, dict):
        event.update(extra)
    return event


def runtime_event(
    *,
    action: str,
    status: str | None = None,
    stage: str | None = None,
    extra: dict | None = None,
) -> dict:
    event = {
        "category": "runtime",
        "actor": "ael",
        "action": action,
    }
    if status is not None:
        event["status"] = status
    if stage is not None:
        event["stage"] = stage
    if isinstance(extra, dict):
        event.update(extra)
    return event


def read_events(
    limit: int = 20,
    run_id: str | None = None,
    source: str = "global",
    date_from: str | None = None,
    date_to: str | None = None,
) -> list[dict]:
    """Return events from the archive.

    Parameters
    ----------
    limit:
        Maximum number of records to return (0 = all).  When positive, the
        *last* ``limit`` records are returned (most-recent-last order).
    run_id:
        Filter to records matching this run id.
    source:
        ``"global"`` to read all daily archive files, or a path to a specific
        JSONL file (e.g. a per-run ``workflow_events.jsonl``).
    date_from / date_to:
        ISO date strings (``YYYY-MM-DD``) to narrow the scan when
        ``source == "global"``.  Files outside the range are skipped entirely.
    """
    if source != "global":
        path = Path(source)
        if not path.exists():
            return []
        records = list(_iter_jsonl(path, run_id))
        if limit > 0:
            records = records[-limit:]
        return records

    root = archive_root()
    files = list_daily_files(root)
    if date_from:
        files = [f for f in files if f.stem >= date_from]
    if date_to:
        files = [f for f in files if f.stem <= date_to]

    records: list[dict] = []

    # Backward compat: include the legacy monolithic file if it still exists
    # (i.e. migration has not yet been run).  Once migrated it is renamed to
    # events.jsonl.bak and will no longer appear here.
    legacy = root / "events.jsonl"
    if legacy.exists():
        records.extend(_iter_jsonl(legacy, run_id))

    for f in files:
        records.extend(_iter_jsonl(f, run_id))

    if limit > 0:
        records = records[-limit:]
    return records
