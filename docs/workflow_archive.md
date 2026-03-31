# Workflow Archive

## Purpose

This is the minimal archival foundation for AEL workflow history.

It captures append-only workflow events now so retrieval, summaries, and broader workflow intelligence can be built later without changing the core run flow again.

Current scope is intentionally small:

- archive now
- analyze later

## Storage

### Daily archive files (current)

Events are written to per-day JSONL files inside `workflow_archive/`:

```
workflow_archive/
  2026-03-07.jsonl
  2026-03-08.jsonl
  ...
  YYYY-MM-DD.jsonl   ← one file per calendar day (event timestamp, not wall clock)
```

Each file contains one JSON object per line, sorted by insertion time within the day.

The target daily file is chosen from the event's own `timestamp` field, so
replayed or backfilled events always land in the correct file regardless of
when they are written.

### Per-run archive (unchanged)

Each run also keeps a self-contained copy of its events:

```
runs/<run_id>/workflow_events.jsonl
```

### Legacy monolithic file (pre-migration)

Before daily rotation was introduced, all events were written to:

```
workflow_archive/events.jsonl
```

This file is no longer written to.  If it still exists on disk, `read_events()`
will include it transparently alongside the new daily files (backward compat).
Run the migration tool to split it and rename it to `events.jsonl.bak`.

## Migration

To migrate an existing `workflow_archive/events.jsonl` to daily files:

```bash
# Dry run first — shows per-day counts, writes nothing
python3 tools/migrate_workflow_archive.py --dry-run

# Real migration
python3 tools/migrate_workflow_archive.py
```

The tool:
1. Parses every line from `events.jsonl`
2. Extracts the `timestamp` field and routes each event to `YYYY-MM-DD.jsonl`
3. Verifies written line count equals parsed line count
4. Renames `events.jsonl` to `events.jsonl.bak` on success (never deletes)
5. Prints a per-day breakdown

Custom archive root:

```bash
python3 tools/migrate_workflow_archive.py --archive-root /path/to/workflow_archive
```

## Record Shape

Each line is one JSON object with schema:

- `schema`: current record schema id
- `timestamp`: ISO timestamp
- `category`: `workflow` or `runtime`
- `actor`: `user`, `assistant`, or `ael`
- `action`: interaction or runtime action such as `request`, `response`, `confirmation`, `correction`, `next_action`, `run_started`, `run_finished`
- `run_id`: AEL run id when available
- `session_id`: optional external session id
- `task_id`: optional external task id
- `selected_dut`: canonical DUT identity
- `selected_board_profile`: canonical board-policy identity
- `selected_bench_resources`: canonical bound bench-resource identity
- `test`: optional test metadata
- `control_instrument`: canonical control-instrument metadata when applicable
- `instrument`: optional instrument metadata
- `stage`: current workflow stage or requested stage boundary
- `status`: event status
- `stage_execution`: executed/deferred stage summary when available
- `selected`: selected config file references when available
- `compatibility`: explicit legacy aliases when older readers still need them
- `artifacts`: references to generated files when available
- `message`: optional interaction payload
- `result`: optional run result summary

Unknown fields are omitted or null instead of guessed.

Current control-instrument / instrument metadata may include:

- resolved communication metadata
- capability-to-surface metadata
- compact ConnA connection digests for easier setup diffing across runs

These are archived as run-context facts only. They are not yet interpreted as runtime routing policy by the archive layer.

Canonical archive readers should prefer:

- `selected_dut`
- `selected_board_profile`
- `selected_bench_resources`
- `control_instrument`

Legacy readers may still encounter:

- `compatibility.board`
- `compatibility.probe`
- `selected.compatibility.board_config`
- `selected.compatibility.probe_config`

These legacy fields should be read only as compatibility material.
New archive consumers should not treat them as the primary contract.

## Current Capture Points

The run pipeline currently appends:

- runtime:
  - `run_started`
  - `run_finished`
- workflow interaction when provided by an outer wrapper:
  - user `request`
  - assistant `response`
  - user `confirmation`
  - user `correction`
  - assistant `next_action`

This keeps interaction history first-class while still preserving useful execution context.

## Optional Conversation Context

Conversation-relevant fields can be supplied by an external AI wrapper or orchestration layer through environment variables:

- `AEL_SESSION_ID`
- `AEL_TASK_ID`
- `AEL_USER_REQUEST`
- `AEL_AI_RESPONSE`
- `AEL_USER_CONFIRMATION`
- `AEL_USER_CORRECTION`
- `AEL_AI_NEXT_ACTION`

This is the current bridge for capturing user/AI interaction. It is intentionally simple and not presented as the final long-term integration interface.

If these are not present, the archive still records runtime workflow progress normally.

## Inspect Helper

```bash
# Show last 20 events across all daily files
python3 -m ael workflow-archive show --limit 20

# Filter by run id
python3 -m ael workflow-archive show --run-id <run_id>

# Filter by date range
python3 -m ael workflow-archive show --date-from 2026-03-01 --date-to 2026-03-31

# Read a specific file (e.g. per-run archive)
python3 -m ael workflow-archive show --source runs/<run_id>/workflow_events.jsonl
```

`--date-from` / `--date-to` narrow the file scan by filename when using the
global source, so they are efficient even with many daily files.

## Git Tracking

Daily archive files and the migration backup are runtime-generated data and
are excluded from git via `.gitignore`:

```
workflow_archive/*.jsonl
workflow_archive/*.jsonl.bak
```

## Why This Is Minimal

This is not a reporting or analytics subsystem.

It does not yet provide:

- search UI
- summaries
- correction mining
- friction analysis
- automatic recommendations

It only preserves structured workflow history so those features can be built later on top of real usage data.

## Likely Evolution Path

Later phases can build on the JSONL archive to add:

- session/task retrieval
- board/test history summaries
- stage transition summaries
- unresolved-item extraction
- repeated-friction analysis
- prompt/template refinement based on real workflow history
