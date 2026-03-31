#!/usr/bin/env python3
"""Migrate workflow_archive/events.jsonl to per-day JSONL files.

Old layout:  workflow_archive/events.jsonl          (one growing file)
New layout:  workflow_archive/YYYY-MM-DD.jsonl      (one file per day)

Usage
-----
    # Dry run – count only, nothing written
    python3 tools/migrate_workflow_archive.py --dry-run

    # Real migration (interactive confirmation if daily files already exist)
    python3 tools/migrate_workflow_archive.py

    # Custom archive root
    python3 tools/migrate_workflow_archive.py --archive-root /path/to/workflow_archive

After a successful migration the original file is renamed to
``events.jsonl.bak``.  It is never deleted automatically.

Exit codes
----------
0  Success (or nothing to do)
1  Aborted by user or integrity check failed
2  Source file not found
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from collections import defaultdict
from pathlib import Path

# Allow running directly from the repo root without installing the package.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))


def _auto_archive_root() -> Path:
    env = os.getenv("AEL_WORKFLOW_ARCHIVE_ROOT", "").strip()
    if env:
        return Path(env).expanduser()
    repo_root = Path(__file__).resolve().parent.parent
    return repo_root / "workflow_archive"


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--archive-root",
        default=None,
        metavar="PATH",
        help="Path to workflow_archive directory (default: auto-detect)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and report only — do not write any files",
    )
    args = parser.parse_args()

    archive_root = Path(args.archive_root) if args.archive_root else _auto_archive_root()
    monolith = archive_root / "events.jsonl"

    if not monolith.exists():
        print(f"Source file not found: {monolith}")
        print("Nothing to migrate.")
        return 0

    print(f"Source:       {monolith}  ({monolith.stat().st_size // 1024} KB)")
    print(f"Archive root: {archive_root}")
    print()

    # ------------------------------------------------------------------ parse
    by_date: dict[str, list[str]] = defaultdict(list)
    skipped = 0
    total = 0

    with open(monolith, "r", encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            stripped = raw.strip()
            if not stripped:
                continue
            total += 1
            try:
                record = json.loads(stripped)
            except json.JSONDecodeError as exc:
                print(f"  WARNING line {lineno}: JSON parse error – {exc}")
                skipped += 1
                continue
            ts = record.get("timestamp", "")
            if ts and len(ts) >= 10:
                date = ts[:10]
            else:
                date = "unknown"
                print(f"  WARNING line {lineno}: missing/short timestamp, routing to 'unknown.jsonl'")
            by_date[date].append(stripped)

    # ------------------------------------------------------------------ report
    to_write = total - skipped
    print(f"Lines read:          {total}")
    print(f"Lines skipped:       {skipped}")
    print(f"Lines to migrate:    {to_write}")
    print()
    print("Per-day breakdown:")
    for date in sorted(by_date):
        print(f"  {date}: {len(by_date[date]):>6} events")
    print()
    print(f"Total to write:      {sum(len(v) for v in by_date.values())}")

    if args.dry_run:
        print("\nDry run — no files written.")
        return 0

    if to_write == 0:
        print("Nothing to write.")
        return 0

    # --------------------------------------------------- check for conflicts
    conflicts = [archive_root / f"{d}.jsonl" for d in by_date if (archive_root / f"{d}.jsonl").exists()]
    if conflicts:
        print("\nWARNING: The following daily files already exist.")
        print("Events will be APPENDED (no duplicates check performed):")
        for p in conflicts:
            print(f"  {p}")
        try:
            ans = input("Continue? [y/N] ").strip().lower()
        except EOFError:
            ans = ""
        if ans != "y":
            print("Aborted.")
            return 1

    # ------------------------------------------------------------------ write
    written = 0
    for date in sorted(by_date):
        daily = archive_root / f"{date}.jsonl"
        with open(daily, "a", encoding="utf-8") as fh:
            for line in by_date[date]:
                fh.write(line + "\n")
                written += 1
        print(f"  Wrote {len(by_date[date]):>6} events → {daily.name}")

    print()
    if written != to_write:
        print(f"ERROR: integrity check failed — wrote {written}, expected {to_write}")
        return 1

    print(f"Integrity check OK: {written} events written across {len(by_date)} daily file(s).")

    # ------------------------------------------------------- rename original
    bak = archive_root / "events.jsonl.bak"
    monolith.rename(bak)
    print(f"\nOriginal renamed to: {bak.name}  (kept, not deleted)")
    print("\nMigration complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
