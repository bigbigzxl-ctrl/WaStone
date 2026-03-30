from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Tuple

from ael import assets
from ael.dut.model import DUTConfig
from ael.dut.registry import load_dut_from_file
from ael.connection_model import (
    build_connection_digest,
    build_connection_rows,
    build_connection_setup,
    diff_connection_setups,
    normalize_connection_context,
    render_connection_setup_text,
)
from ael.instrument_metadata import capability_names, validate_capability_surfaces, validate_communication
from ael.instruments.registry import InstrumentRegistry
from ael.instrument_view import build_resolved_instrument_inventory, render_resolved_instrument_inventory_text
from ael.pack_loader import load_pack
from ael.pipeline import _simple_yaml_load
from ael.probe_binding import load_probe_binding
from ael.strategy_resolver import resolve_control_instrument_override
from ael.test_plan_schema import extract_plan_metadata
from ael.verification_model import summarize_resource_keys


REPO_ROOT = Path(__file__).resolve().parents[1]
_PREFERRED_SUITE_LABELS = {"golden", "candidate", "testing", "pre_release", "legacy"}


def _normalize_suite_label(value: Any) -> str | None:
    raw = str(value or "").strip().lower()
    if not raw:
        return None
    aliases = {
        "golden": "golden",
        "candidate": "candidate",
        "draft": "candidate",
        "testing": "testing",
        "experimental": "testing",
        "runnable": "testing",
        "validated": "pre_release",
        "pre_release": "pre_release",
        "pre-release": "pre_release",
        "prerelease": "pre_release",
        "merged_to_main": "pre_release",
        "legacy": "legacy",
    }
    return aliases.get(raw)


def _normalize_vendor(value: Any) -> str | None:
    raw = str(value or "").strip().lower()
    if not raw:
        return None
    aliases = {
        "stmicroelectronics": "st",
        "st": "st",
        "generic": "generic",
        "weact studio": "weact_studio",
        "weact": "weact_studio",
        "espressif": "espressif",
        "raspberry pi foundation": "raspberry_pi",
        "raspberry pi": "raspberry_pi",
        "yd": "yd",
    }
    return aliases.get(raw, raw.replace(" ", "_"))


def _line_from_part_number(value: Any) -> str | None:
    raw = str(value or "").strip().lower()
    if not raw:
        return None
    if raw.startswith("stm32"):
        suffix = raw[5:]
        if not suffix:
            return None
        line = "stm32"
        for char in suffix:
            if char.isdigit():
                line += char
            else:
                break
        if len(line) > len("stm32"):
            return line
    return raw


def _metadata_explanation(metadata: Dict[str, Any]) -> Dict[str, str | None]:
    test_kind = str(metadata.get("test_kind") or "").strip()
    requires = metadata.get("requires") if isinstance(metadata.get("requires"), dict) else {}
    if test_kind == "instrument_specific":
        return {
            "verification_mode_summary": "instrument-side measurement path",
            "requires_summary": "requires instrument-side measurement and no mailbox dependency",
        }
    if test_kind == "baremetal_mailbox":
        mailbox_required = requires.get("mailbox") is True
        return {
            "verification_mode_summary": "bare-metal mailbox verification",
            "requires_summary": "requires mailbox-backed DUT result path" if mailbox_required else "mailbox optional",
        }
    return {
        "verification_mode_summary": None,
        "requires_summary": None,
    }


def _supported_instrument_advisory(
    supported_instruments: List[str] | None,
    *,
    selected_instrument_id: str | None,
    selected_instrument_type: str | None,
) -> Dict[str, Any] | None:
    declared = [item for item in (supported_instruments or []) if isinstance(item, str) and item.strip()]
    if not declared:
        return None
    if not selected_instrument_type:
        return {
            "status": "selection_unresolved",
            "selected_instrument_id": selected_instrument_id,
            "selected_instrument_type": None,
            "declared_supported_instruments": declared,
            "summary": "supported instruments declared, but no selected instrument type was resolved",
        }
    status = "declared_supported" if selected_instrument_type in declared else "declared_unsupported"
    summary = (
        f"selected instrument type {selected_instrument_type} is declared supported"
        if status == "declared_supported"
        else f"selected instrument type {selected_instrument_type} is not in declared support set"
    )
    return {
        "status": status,
        "selected_instrument_id": selected_instrument_id,
        "selected_instrument_type": selected_instrument_type,
        "declared_supported_instruments": declared,
        "summary": summary,
    }


def _load_json(path: Path) -> Dict[str, Any]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _load_board_cfg(repo_root: Path, board_id: str) -> DUTConfig:
    """Load board config. Returns DUTConfig (was: raw dict)."""
    try:
        return load_dut_from_file(repo_root, board_id)
    except FileNotFoundError:
        return DUTConfig(board_id=board_id, name=board_id, processors=[])


def _load_instrument_instance_type(repo_root: Path, instrument_id: str | None) -> str | None:
    instance_id = str(instrument_id or "").strip()
    if not instance_id:
        return None
    path = repo_root / "configs" / "instrument_instances" / f"{instance_id}.yaml"
    if not path.exists():
        return None
    raw = _simple_yaml_load(str(path))
    instance = raw.get("instance", {}) if isinstance(raw, dict) else {}
    if not isinstance(instance, dict):
        return None
    type_id = str(instance.get("type") or raw.get("type") or "").strip()
    return type_id or None


def _load_plan_index(repo_root: Path) -> Tuple[Dict[str, Dict[str, Any]], List[Dict[str, Any]]]:
    plans_dir = repo_root / "tests" / "plans"
    plans_by_path: Dict[str, Dict[str, Any]] = {}
    generic_plans: List[Dict[str, Any]] = []
    if not plans_dir.exists():
        return plans_by_path, generic_plans

    for path in sorted(plans_dir.glob("*.json")):
        payload = _load_json(path)
        rel = path.relative_to(repo_root).as_posix()
        metadata = extract_plan_metadata(payload)
        entry = {
            "name": payload.get("name") or path.stem,
            "path": rel,
            "board": payload.get("board"),
            "dut": payload.get("dut"),
            "validation_style": _infer_validation_style(payload),
            "schema_version": metadata.get("schema_version"),
            "declared_schema_version": metadata.get("declared_schema_version"),
            "test_kind": metadata.get("test_kind"),
            "supported_instruments": metadata.get("supported_instruments"),
            "requires": metadata.get("requires"),
            "labels": metadata.get("labels"),
            "covers": metadata.get("covers"),
            "validation_errors": list(metadata.get("validation_errors") or []),
        }
        plans_by_path[rel] = entry
        if not entry["board"] and not entry["dut"]:
            generic_plans.append(entry)
    return plans_by_path, generic_plans


def _load_pack_index(repo_root: Path) -> List[Dict[str, Any]]:
    packs: List[Dict[str, Any]] = []
    pack_dirs = [repo_root / "packs"]
    for root in (repo_root / "assets_golden" / "duts", repo_root / "assets_user" / "duts"):
        if root.exists():
            for path in root.glob("*/packs"):
                pack_dirs.append(path)

    seen = set()
    for root in (repo_root / "assets_branch" / "duts",):
        if root.exists():
            for path in root.glob("*/packs"):
                pack_dirs.append(path)

    for pack_dir in pack_dirs:
        if not pack_dir.exists():
            continue
        for path in sorted(pack_dir.glob("*.json")):
            rel = path.relative_to(repo_root).as_posix()
            if rel in seen:
                continue
            seen.add(rel)
            try:
                payload = load_pack(path)
            except Exception:
                payload = {}
            packs.append(
                {
                    "name": payload.get("name") or path.stem,
                    "path": rel,
                    "board": payload.get("board"),
                    "bench_profile": payload.get("bench_profile"),
                    "status": payload.get("status"),
                    "description": payload.get("description") or payload.get("notes"),
                    "stages": payload.get("stages") if isinstance(payload.get("stages"), dict) else {},
                    # "programs" is the preferred key; "tests" is the legacy alias
                    "tests": [str(t) for t in (payload.get("programs") or payload.get("tests") or [])],
                }
            )
    return packs


def _infer_validation_style(payload: Dict[str, Any]) -> str:
    if isinstance(payload.get("instrument"), dict):
        return "meter"
    if payload.get("selftest"):
        return "instrument_selftest"
    if payload.get("observe_uart"):
        return "uart_or_signal"
    if isinstance(payload.get("signal_checks"), list) and payload.get("signal_checks"):
        return "signal"
    if payload.get("pin"):
        return "signal"
    return "generic"


def _resolve_probe_or_instrument(root: Path, board_id: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    if isinstance(payload.get("instrument"), dict):
        inst = payload["instrument"]
        tcp = inst.get("tcp", {}) if isinstance(inst.get("tcp"), dict) else {}
        manifest = InstrumentRegistry().get(str(inst.get("id") or "")) or {}
        communication = manifest.get("communication", {}) if isinstance(manifest.get("communication"), dict) else {}
        return {
            "kind": "instrument",
            "id": inst.get("id"),
            "type": _load_instrument_instance_type(root, str(inst.get("id") or "")) or manifest.get("type"),
            "endpoint": {
                "host": tcp.get("host"),
                "port": tcp.get("port"),
            } if tcp else None,
            "communication": communication,
            "capability_surfaces": manifest.get("capability_surfaces", {}) if isinstance(manifest.get("capability_surfaces"), dict) else {},
            "metadata_validation_errors": (
                validate_communication(communication)
                + validate_capability_surfaces(
                    manifest.get("capability_surfaces"),
                    capabilities=capability_names(manifest),
                    communication=communication,
                )
            ),
        }
    override_instance_id, override_probe_rel = resolve_control_instrument_override(root, payload)
    board_cfg = _load_board_cfg(root, board_id)
    extra = board_cfg.extra
    instance_id = str(
        override_instance_id
        or extra.get("control_instrument_instance")
        or extra.get("instrument_instance")
        or ""
    ).strip() or None
    probe_path = str(
        override_probe_rel
        or extra.get("control_instrument_config")
        or extra.get("probe_config")
        or ""
    ).strip() or None
    binding = load_probe_binding(root, probe_path=probe_path, instance_id=instance_id)
    return {
        "kind": "controller",
        "legacy_kind": "control_instrument",
        "compatibility_kind": "probe",
        "id": binding.instance_id or binding.raw.get("probe", {}).get("name") or "ESP32JTAG",
        "type": binding.type_id,
        "instrument_role": "control",
        "endpoint": {
            "host": binding.endpoint_host,
            "port": binding.endpoint_port,
        } if (binding.endpoint_host or binding.endpoint_port is not None) else None,
        "communication": binding.communication,
        "capability_surfaces": binding.capability_surfaces,
        "metadata_validation_errors": list(binding.metadata_validation_errors),
    }


def _primary_selected_instrument(poi: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(poi, dict):
        return {}
    kind = str(poi.get("kind") or "").strip()
    if kind in {"control_instrument", "controller"}:
        return {
            "kind": "controller",
            "legacy_kind": poi.get("legacy_kind") or "control_instrument",
            "compatibility_kind": poi.get("compatibility_kind") or "probe",
            "id": poi.get("id"),
            "type": poi.get("type"),
            "instrument_role": poi.get("instrument_role") or "control",
            "endpoint": poi.get("endpoint"),
            "communication": dict(poi.get("communication") or {}),
            "capability_surfaces": dict(poi.get("capability_surfaces") or {}),
            "metadata_validation_errors": list(poi.get("metadata_validation_errors") or []),
        }
    return dict(poi)


def _selected_dut_payload(root: Path, board_id: str, board_cfg: DUTConfig) -> Dict[str, Any]:
    dut = assets.load_dut_prefer_user(board_id)
    manifest = dut.get("manifest") if isinstance(dut, dict) and isinstance(dut.get("manifest"), dict) else {}
    return {
        "id": board_id,
        "name": manifest.get("description") or board_cfg.name,
        "target": board_cfg.target,
        "mcu": manifest.get("mcu") or board_cfg.mcu,
        "family": manifest.get("family"),
        "source": "user" if isinstance(dut, dict) and "/assets_user/" in str(dut.get("path") or "") else ("golden" if dut else None),
        "runtime_binding": "board_profile_driven",
    }


def _selected_board_profile_payload(root: Path, board_id: str, board_cfg: DUTConfig) -> Dict[str, Any]:
    board_path = root / "configs" / "boards" / f"{board_id}.yaml"
    return {
        "id": board_id,
        "name": board_cfg.name,
        "target": board_cfg.mcu,
        "config": board_path.relative_to(root).as_posix() if board_path.exists() else None,
        "role": "runtime_policy",
    }


def _bench_selection_digest(payload: Dict[str, Any]) -> List[str]:
    items: List[str] = []
    instrument = payload.get("selected_instrument")
    if isinstance(instrument, dict):
        kind = str(instrument.get("kind") or "").strip()
        instrument_id = instrument.get("id")
        if instrument_id:
            if kind in {"controller", "control_instrument"}:
                items.append(f"controller_id:{instrument_id}")
                items.append(f"control_instrument_id:{instrument_id}")
            else:
                items.append(f"{kind}_id:{instrument_id}")
        endpoint = instrument.get("endpoint")
        if isinstance(endpoint, dict) and endpoint.get("host") and endpoint.get("port") is not None:
            if kind in {"controller", "control_instrument"}:
                endpoint_text = f"{endpoint.get('host')}:{endpoint.get('port')}"
                items.append(f"controller_endpoint:{endpoint_text}")
                items.append(f"control_instrument_endpoint:{endpoint_text}")
            else:
                items.append(f"{kind}_endpoint:{endpoint.get('host')}:{endpoint.get('port')}")
    return items


def _selected_bench_resources_payload(selected_instrument: Dict[str, Any], connection_setup: Dict[str, Any]) -> Dict[str, Any]:
    resource_keys = []
    kind = str(selected_instrument.get("kind") or "").strip()
    endpoint = selected_instrument.get("endpoint") if isinstance(selected_instrument.get("endpoint"), dict) else {}
    if kind in {"control_instrument", "controller"}:
        if endpoint.get("host") and endpoint.get("port") is not None:
            endpoint_text = f"{endpoint.get('host')}:{endpoint.get('port')}"
            resource_keys.append(f"probe:{endpoint_text}")
            resource_keys.append(f"controller:{endpoint_text}")
        elif selected_instrument.get("config"):
            config_path = selected_instrument.get('config')
            resource_keys.append(f"probe_path:{config_path}")
            resource_keys.append(f"controller_path:{config_path}")
    elif kind == "instrument":
        inst_id = selected_instrument.get("id")
        if inst_id and endpoint.get("host") and endpoint.get("port") is not None:
            resource_keys.append(f"instrument:{inst_id}:{endpoint.get('host')}:{endpoint.get('port')}")
    payload = {
        "contract_version": 1,
        "selected_instrument": dict(selected_instrument or {}),
        "resource_keys": resource_keys,
        "resource_summary": summarize_resource_keys(resource_keys),
        "connection_setup": dict(connection_setup or {}),
        "connection_digest": build_connection_digest(connection_setup),
    }
    payload["selection_digest"] = _bench_selection_digest(payload)
    return payload


def _build_expected_checks(payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    checks: List[Dict[str, Any]] = []
    signal_checks = payload.get("signal_checks")
    if isinstance(signal_checks, list) and signal_checks:
        for item in signal_checks:
            if not isinstance(item, dict):
                continue
            checks.append(
                {
                    "type": "signal",
                    "name": item.get("name"),
                    "pin": item.get("pin"),
                    "min_freq_hz": item.get("min_freq_hz"),
                    "max_freq_hz": item.get("max_freq_hz"),
                    "duty_min": item.get("duty_min"),
                    "duty_max": item.get("duty_max"),
                    "duration_s": item.get("duration_s", payload.get("duration_s")),
                    "min_edges": item.get("min_edges", payload.get("min_edges")),
                    "max_edges": item.get("max_edges", payload.get("max_edges")),
                }
            )
        for relation in payload.get("signal_relations", []) if isinstance(payload.get("signal_relations"), list) else []:
            if isinstance(relation, dict):
                checks.append({"type": "signal_relation", **relation})
    elif payload.get("pin"):
        checks.append(
            {
                "type": "signal",
                "pin": payload.get("pin"),
                "min_freq_hz": payload.get("min_freq_hz"),
                "max_freq_hz": payload.get("max_freq_hz"),
                "duty_min": payload.get("duty_min"),
                "duty_max": payload.get("duty_max"),
                "duration_s": payload.get("duration_s"),
                "min_edges": payload.get("min_edges"),
                "max_edges": payload.get("max_edges"),
            }
        )
    uart = payload.get("observe_uart", {}) if isinstance(payload.get("observe_uart"), dict) else {}
    if uart.get("enabled"):
        checks.append(
            {
                "type": "uart",
                "baud": uart.get("baud"),
                "duration_s": uart.get("duration_s"),
                "expect_patterns": uart.get("expect_patterns"),
            }
        )
    if isinstance(payload.get("uart_expect"), dict):
        checks.append({"type": "uart_expect", **payload.get("uart_expect")})
    if isinstance(payload.get("instrument"), dict):
        measure = payload.get("instrument", {}).get("measure", {})
        if isinstance(measure, dict):
            checks.append({"type": "instrument_measure", **measure})
    led_observe = payload.get("observe_led", {}) if isinstance(payload.get("observe_led"), dict) else {}
    if led_observe.get("enabled"):
        checks.append(
            {
                "type": "led",
                "pin": led_observe.get("pin") or "led",
                "duration_s": led_observe.get("duration_s"),
                "min_edges": led_observe.get("min_edges"),
                "max_edges": led_observe.get("max_edges"),
                "expected_hz": led_observe.get("expected_hz"),
            }
        )
    return checks

def _merge_tests(items: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    merged: Dict[Tuple[str, str], Dict[str, Any]] = {}
    for item in items:
        key = (str(item.get("path") or ""), str(item.get("name") or ""))
        current = merged.get(key)
        source = {"via": item.get("via")}
        if item.get("pack"):
            source["pack"] = item.get("pack")
        if item.get("missing"):
            source["missing"] = True
        if current is None:
            current = {
                "name": item.get("name"),
                "path": item.get("path"),
                "validation_style": item.get("validation_style"),
                "schema_version": item.get("schema_version"),
                "test_kind": item.get("test_kind"),
                "supported_instruments": item.get("supported_instruments"),
                "requires": item.get("requires"),
                "labels": item.get("labels"),
                "covers": item.get("covers"),
                "missing": bool(item.get("missing")),
                "validation_errors": list(item.get("validation_errors") or []),
                "sources": [source],
            }
            merged[key] = current
        else:
            if item.get("missing"):
                current["missing"] = True
            for err in item.get("validation_errors") or []:
                if err not in current["validation_errors"]:
                    current["validation_errors"].append(err)
            if source not in current["sources"]:
                current["sources"].append(source)
    return list(merged.values())


def _is_legacy_manifest(manifest: Dict[str, Any]) -> bool:
    tags = {str(item).strip().lower() for item in (manifest.get("tags") or []) if str(item).strip()}
    description = str(manifest.get("description") or "").lower()
    notes = str(((manifest.get("notes") or {}).get("status")) if isinstance(manifest.get("notes"), dict) else "").lower()
    return "legacy_path" in tags or "legacy" in description or "legacy" in notes


def _select_canonical_pack(manifest: Dict[str, Any], packs_for_dut: List[Dict[str, Any]]) -> Dict[str, Any] | None:
    if not packs_for_dut:
        return None

    def _pack_sort_key(item: Dict[str, Any]) -> Tuple[int, str]:
        path = str(item.get("path") or "")
        preferred_root = 0 if path.startswith("packs/") else 1
        return (preferred_root, path)

    by_path = {str(item.get("path")): item for item in packs_for_dut if item.get("path")}
    verified = manifest.get("verified") if isinstance(manifest.get("verified"), dict) else {}
    preferred_paths: List[str] = []
    for candidate in (
        verified.get("golden_pack") if isinstance(verified, dict) else None,
        manifest.get("golden_pack"),
    ):
        path = str(candidate or "").strip()
        if path:
            preferred_paths.append(path)
    for path in preferred_paths:
        if path in by_path:
            return by_path[path]

    golden_status_packs = [item for item in packs_for_dut if _normalize_suite_label(item.get("status")) == "golden"]
    if golden_status_packs:
        return sorted(golden_status_packs, key=_pack_sort_key)[0]

    golden_name_packs = [
        item for item in packs_for_dut if str(item.get("name") or "").endswith("_golden") or str(item.get("path") or "").endswith("_golden.json")
    ]
    if golden_name_packs:
        return sorted(golden_name_packs, key=_pack_sort_key)[0]

    default_packs = manifest.get("default_packs") if isinstance(manifest.get("default_packs"), list) else []
    for candidate in default_packs:
        path = str(candidate or "").strip()
        if path in by_path:
            return by_path[path]

    full_packs = [
        item
        for item in packs_for_dut
        if "full" in str(item.get("name") or "").lower() or "full" in str(item.get("path") or "").lower()
    ]
    if full_packs:
        return sorted(full_packs, key=_pack_sort_key)[0]
    return sorted(packs_for_dut, key=_pack_sort_key)[0]


def _suite_label_for_dut(manifest: Dict[str, Any], canonical_pack: Dict[str, Any] | None) -> Dict[str, str | None]:
    explicit = _normalize_suite_label((canonical_pack or {}).get("status"))
    if explicit:
        return {"label": explicit, "source": "pack.status"}

    if _is_legacy_manifest(manifest):
        return {"label": "legacy", "source": "manifest.tags_or_description"}

    lifecycle = _normalize_suite_label(manifest.get("lifecycle_stage"))
    verified_status = bool((manifest.get("verified") or {}).get("status")) if isinstance(manifest.get("verified"), dict) else False
    if lifecycle == "golden" and verified_status:
        return {"label": "golden", "source": "manifest.lifecycle_stage+verified.status"}
    if lifecycle == "pre_release" and verified_status and canonical_pack:
        return {"label": "golden", "source": "manifest.lifecycle_stage+verified.status+canonical_pack"}
    if lifecycle == "pre_release":
        return {"label": "pre_release", "source": "manifest.lifecycle_stage"}
    if lifecycle == "testing":
        return {"label": "testing", "source": "manifest.lifecycle_stage"}
    if lifecycle == "candidate":
        return {"label": "candidate", "source": "manifest.lifecycle_stage"}
    if verified_status:
        return {"label": "pre_release", "source": "manifest.verified.status"}
    return {"label": "candidate", "source": "default_inference"}


def _load_bench_profile(repo_root: Path, board_id: str, bench_profile_id: str | None) -> Dict[str, Any]:
    profile_id = str(bench_profile_id or "").strip()
    if not profile_id:
        return {}
    path = repo_root / "configs" / "bench_profiles" / f"{profile_id}.yaml"
    if not path.exists():
        return {}
    raw = _simple_yaml_load(str(path))
    profile = raw.get("bench_profile", {}) if isinstance(raw, dict) else {}
    if not isinstance(profile, dict):
        return {}
    return {
        "id": profile.get("id") or profile_id,
        "path": path.relative_to(repo_root).as_posix(),
        "board_id": profile.get("board_id") or board_id,
        "description": profile.get("description"),
        "connections": list(profile.get("bench_connections") or []),
        "observe_map": dict(profile.get("observe_map") or {}),
        "verification_views": dict(profile.get("verification_views") or {}),
        "default_wiring": dict(profile.get("default_wiring") or {}),
        "safe_pins": list(profile.get("safe_pins") or []),
    }


def _tests_by_stage(pack: Dict[str, Any], plans_by_path: Dict[str, Dict[str, Any]]) -> List[Dict[str, Any]]:
    stages = pack.get("stages") if isinstance(pack.get("stages"), dict) else {}
    if not stages:
        return [
            {
                "stage": "all",
                "tests": [
                    {
                        "name": plans_by_path.get(path, {}).get("name") or Path(path).stem,
                        "path": path,
                    }
                    for path in (pack.get("tests") or [])
                ],
            }
        ]
    items: List[Dict[str, Any]] = []
    for stage_key in sorted(stages.keys(), key=lambda value: int(value) if str(value).isdigit() else str(value)):
        tests = []
        for path in stages.get(stage_key) or []:
            plan = plans_by_path.get(str(path), {})
            tests.append({"name": plan.get("name") or Path(str(path)).stem, "path": str(path)})
        items.append({"stage": str(stage_key), "tests": tests})
    return items


def _classification_for_manifest(manifest: Dict[str, Any]) -> Dict[str, Any]:
    classification = manifest.get("classification") if isinstance(manifest.get("classification"), dict) else {}
    if classification:
        return {
            "platform_class": str(classification.get("platform_class") or "").strip() or None,
            "vendor": _normalize_vendor(classification.get("vendor")),
            "family": str(classification.get("family") or "").strip().lower() or None,
            "series": str(classification.get("series") or "").strip().lower() or None,
            "line": str(classification.get("line") or "").strip().lower() or None,
            "part_number": str(classification.get("part_number") or "").strip().lower() or None,
            "source": "manifest.classification",
        }

    board = manifest.get("board") if isinstance(manifest.get("board"), dict) else {}
    family_raw = str(manifest.get("family") or "").strip().lower()
    mcu = str(manifest.get("mcu") or "").strip().lower()
    family = family_raw or None
    series = family_raw or None
    line = _line_from_part_number(mcu)
    part_number = mcu or None
    if family_raw.startswith("stm32"):
        family = "stm32"
        series = family_raw
        line = _line_from_part_number(mcu) or line
    elif family_raw.startswith("esp32"):
        family = "esp32"
        series = mcu or family_raw
        line = mcu or family_raw
    elif family_raw.startswith("rp"):
        family = family_raw
        series = family_raw
        line = family_raw
    return {
        "platform_class": "mcu" if family or mcu else None,
        "vendor": _normalize_vendor(board.get("vendor")),
        "family": family,
        "series": series,
        "line": line,
        "part_number": part_number,
        "source": "inferred_from_manifest",
    }


def build_instrument_instance_inventory(repo_root: Path | None = None) -> Dict[str, Any]:
    return build_resolved_instrument_inventory(repo_root or REPO_ROOT)


def build_inventory(repo_root: Path | None = None) -> Dict[str, Any]:
    root = Path(repo_root or REPO_ROOT)
    plans_by_path, generic_plans = _load_plan_index(root)
    packs = _load_pack_index(root)

    dut_entries: List[Dict[str, Any]] = []
    all_duts = []
    for source_name, source_root in (
        ("golden", root / "assets_golden" / "duts"),
        ("user", root / "assets_user" / "duts"),
        ("branch", root / "assets_branch" / "duts"),
    ):
        if not source_root.exists():
            continue
        for entry in assets.list_duts(source_root):
            manifest = entry.get("manifest") or {}
            dut_id = str(manifest.get("id") or entry.get("id") or Path(entry["path"]).name)
            packs_for_dut = [item for item in packs if item.get("board") == dut_id]
            canonical_pack = _select_canonical_pack(manifest, packs_for_dut)
            suite_label = _suite_label_for_dut(manifest, canonical_pack)
            classification = _classification_for_manifest(manifest)
            tests: List[Dict[str, Any]] = []
            for plan in plans_by_path.values():
                if plan.get("board") == dut_id or plan.get("dut") == dut_id:
                    tests.append(
                        {
                            "name": plan["name"],
                            "path": plan["path"],
                            "via": "direct_plan",
                            "validation_style": plan["validation_style"],
                            "schema_version": plan.get("schema_version"),
                            "test_kind": plan.get("test_kind"),
                            "supported_instruments": plan.get("supported_instruments"),
                            "requires": plan.get("requires"),
                            "labels": plan.get("labels"),
                            "covers": plan.get("covers"),
                            "validation_errors": plan.get("validation_errors"),
                        }
                    )
            for pack in packs:
                if pack.get("board") != dut_id:
                    continue
                for test_path in pack.get("tests") or []:
                    rel = str(test_path)
                    plan = plans_by_path.get(rel)
                    if plan:
                        tests.append(
                            {
                                "name": plan["name"],
                                "path": rel,
                                "via": "pack",
                                "pack": pack["name"],
                                "validation_style": plan["validation_style"],
                                "schema_version": plan.get("schema_version"),
                                "test_kind": plan.get("test_kind"),
                                "supported_instruments": plan.get("supported_instruments"),
                                "requires": plan.get("requires"),
                                "labels": plan.get("labels"),
                                "covers": plan.get("covers"),
                                "validation_errors": plan.get("validation_errors"),
                            }
                        )
                    else:
                        tests.append(
                            {
                                "name": Path(rel).stem,
                                "path": rel,
                                "via": "pack",
                                "pack": pack["name"],
                                "validation_style": "unknown",
                                "missing": True,
                            }
                        )
            tests = _merge_tests(tests)
            board_config = root / "configs" / "boards" / f"{dut_id}.yaml"
            dut_entries.append(
                {
                    "dut_id": dut_id,
                    "mcu": manifest.get("mcu"),
                    "family": manifest.get("family"),
                    "description": manifest.get("description"),
                    "source": source_name,
                    "lifecycle_stage": str(manifest.get("lifecycle_stage") or "").strip() or None,
                    "board_config": board_config.relative_to(root).as_posix() if board_config.exists() else None,
                    "verified_status": bool((manifest.get("verified") or {}).get("status")) if isinstance(manifest.get("verified"), dict) else None,
                    "classification": classification,
                    "suite_label": suite_label.get("label"),
                    "suite_label_source": suite_label.get("source"),
                    "canonical_pack": {
                        "name": canonical_pack.get("name"),
                        "path": canonical_pack.get("path"),
                        "status": canonical_pack.get("status"),
                        "bench_profile": canonical_pack.get("bench_profile"),
                    } if canonical_pack else None,
                    "tests": tests,
                }
            )
            all_duts.append(dut_id)

    mcus_with_tests = sorted({str(item.get("mcu")) for item in dut_entries if item.get("mcu") and item.get("tests")})
    duts_with_tests = sorted([item["dut_id"] for item in dut_entries if item.get("tests")])
    all_test_names = sorted({test["name"] for item in dut_entries for test in item.get("tests", []) if test.get("name")})
    inventory = {
        "ok": True,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(root),
        "summary": {
            "dut_count": len(dut_entries),
            "duts_with_tests": duts_with_tests,
            "mcus_with_tests": mcus_with_tests,
            "test_names": all_test_names,
        },
        "duts": sorted(dut_entries, key=lambda item: item["dut_id"]),
        "generic_tests": generic_plans,
    }
    return inventory


def describe_test(board_id: str, test_path: str, repo_root: Path | None = None) -> Dict[str, Any]:
    root = Path(repo_root or REPO_ROOT)
    path = Path(test_path)
    if not path.is_absolute():
        path = root / path
    if not path.exists():
        # Try resolving as a bare test name → tests/plans/<name>.json
        candidate = root / "tests" / "plans" / (Path(test_path).stem + ".json")
        if candidate.exists():
            path = candidate
        else:
            return {"ok": False, "error": f"test not found: {test_path}"}

    payload = _load_json(path)
    metadata = extract_plan_metadata(payload)
    explanation = _metadata_explanation(metadata)
    board_cfg = _load_board_cfg(root, board_id)
    connection_ctx = normalize_connection_context(
        board_cfg,
        payload,
        required_wiring=["swd", "reset", "verify"],
    )
    selected_instrument = _primary_selected_instrument(_resolve_probe_or_instrument(root, board_id, payload))
    supported_instrument_advisory = _supported_instrument_advisory(
        metadata.get("supported_instruments"),
        selected_instrument_id=selected_instrument.get("id"),
        selected_instrument_type=selected_instrument.get("type"),
    )
    connection_setup = build_connection_setup(connection_ctx)
    clock_hz = board_cfg.primary_processor.clock_hz if board_cfg.processors else None
    result = {
        "ok": True,
        "board": board_id,
        "test": {
            "name": payload.get("name") or path.stem,
            "path": path.relative_to(root).as_posix(),
            "validation_style": _infer_validation_style(payload),
            "schema_version": metadata.get("schema_version"),
            "declared_schema_version": metadata.get("declared_schema_version"),
            "test_kind": metadata.get("test_kind"),
            "supported_instruments": metadata.get("supported_instruments"),
            "requires": metadata.get("requires"),
            "labels": metadata.get("labels"),
            "covers": metadata.get("covers"),
            "verification_mode_summary": explanation.get("verification_mode_summary"),
            "requires_summary": explanation.get("requires_summary"),
            "supported_instrument_advisory": supported_instrument_advisory,
            "validation_errors": list(metadata.get("validation_errors") or []),
        },
        "selected_dut": _selected_dut_payload(root, board_id, board_cfg),
        "selected_board_profile": _selected_board_profile_payload(root, board_id, board_cfg),
        "selected_instrument": selected_instrument,
        "selected_bench_resources": _selected_bench_resources_payload(selected_instrument, connection_setup),
        "connections": build_connection_rows(connection_ctx, payload),
        "expected_checks": _build_expected_checks(payload),
        "board_context": {
            "target": board_cfg.mcu,
            "clock_hz": clock_hz,
            "observe_map": dict(connection_ctx.observe_map),
            "verification_views": dict(connection_ctx.verification_views),
            "default_wiring": dict(connection_ctx.default_wiring),
        },
        "connection_setup": connection_setup,
        "setup_readiness": connection_ctx.setup_readiness.to_dict() if connection_ctx.setup_readiness is not None else None,
        "notes": payload.get("notes"),
        "warnings": [f"warning: {item}" for item in connection_ctx.warnings],
    }
    legacy = _resolve_probe_or_instrument(root, board_id, payload)
    if legacy.get("legacy_kind"):
        result["compatibility"] = {"probe_or_instrument": legacy}
    return result


def describe_connection(board_id: str, test_path: str, repo_root: Path | None = None) -> Dict[str, Any]:
    payload = describe_test(board_id=board_id, test_path=test_path, repo_root=repo_root)
    if not payload.get("ok"):
        return payload
    return {
        "ok": True,
        "board": payload.get("board"),
        "test": payload.get("test"),
        "connection_setup": payload.get("connection_setup"),
        "setup_readiness": payload.get("setup_readiness"),
        "connections": payload.get("connections"),
        "warnings": payload.get("warnings"),
        "validation_errors": (
            payload.get("connection_setup", {}).get("validation_errors")
            if isinstance(payload.get("connection_setup"), dict)
            else []
        ),
        "source_summary": (
            payload.get("connection_setup", {}).get("source_summary")
            if isinstance(payload.get("connection_setup"), dict)
            else {}
        ),
    }


def describe_dut(board_id: str, repo_root: Path | None = None) -> Dict[str, Any]:
    root = Path(repo_root or REPO_ROOT)
    plans_by_path, _ = _load_plan_index(root)
    packs = _load_pack_index(root)
    dut = assets.load_dut_prefer_user(board_id)
    if not isinstance(dut, dict):
        return {"ok": False, "error": f"dut not found: {board_id}"}
    manifest = dut.get("manifest") if isinstance(dut.get("manifest"), dict) else {}
    board_cfg = _load_board_cfg(root, board_id)
    packs_for_dut = [item for item in packs if item.get("board") == board_id]
    canonical_pack = _select_canonical_pack(manifest, packs_for_dut)
    suite_label = _suite_label_for_dut(manifest, canonical_pack)
    bench_profile_id = (
        str((canonical_pack or {}).get("bench_profile") or "").strip()
        or str(board_cfg.extra.get("default_bench_profile") or "").strip()
        or None
    )
    bench_profile = _load_bench_profile(root, board_id, bench_profile_id)
    representative_test = None
    for path in (canonical_pack or {}).get("tests") or []:
        if str(path).strip():
            representative_test = str(path)
            break
    representative_payload = (
        describe_test(board_id=board_id, test_path=representative_test, repo_root=root)
        if representative_test
        else None
    )
    return {
        "ok": True,
        "dut": {
            "id": board_id,
            "name": manifest.get("name") or board_cfg.name or board_id,
            "mcu": manifest.get("mcu") or board_cfg.mcu,
            "family": manifest.get("family"),
            "description": manifest.get("description"),
            "source": "user" if "/assets_user/" in str(dut.get("path") or "") else "golden",
            "lifecycle_stage": str(manifest.get("lifecycle_stage") or "").strip() or None,
            "verified_status": bool((manifest.get("verified") or {}).get("status")) if isinstance(manifest.get("verified"), dict) else None,
            "classification": _classification_for_manifest(manifest),
        },
        "suite": {
            "label": suite_label.get("label"),
            "label_source": suite_label.get("source"),
            "canonical_pack": {
                "name": (canonical_pack or {}).get("name"),
                "path": (canonical_pack or {}).get("path"),
                "status": (canonical_pack or {}).get("status"),
                "description": (canonical_pack or {}).get("description"),
                "bench_profile": bench_profile_id,
                "test_count": len((canonical_pack or {}).get("tests") or []),
                "stage_count": len((canonical_pack or {}).get("stages") or {}),
                "stages": _tests_by_stage(canonical_pack or {}, plans_by_path) if canonical_pack else [],
            } if canonical_pack else None,
        },
        "selected_board_profile": _selected_board_profile_payload(root, board_id, board_cfg),
        "selected_instrument": (
            representative_payload.get("selected_instrument")
            if isinstance(representative_payload, dict) and representative_payload.get("ok")
            else None
        ),
        "bench_profile": bench_profile,
        "connection_setup": (
            representative_payload.get("connection_setup")
            if isinstance(representative_payload, dict) and representative_payload.get("ok")
            else None
        ),
        "connections": list(bench_profile.get("connections") or []),
        "tests": sorted(
            [
                {
                    "name": plan.get("name") or Path(plan["path"]).stem,
                    "path": plan["path"],
                }
                for plan in plans_by_path.values()
                if plan.get("board") == board_id or plan.get("dut") == board_id
            ],
            key=lambda item: item["path"],
        ),
    }


def list_suites(
    *,
    repo_root: Path | None = None,
    platform_class: str | None = None,
    vendor: str | None = None,
    family: str | None = None,
    series: str | None = None,
    line: str | None = None,
    part_number: str | None = None,
    label: str | None = None,
    group_by: str | None = None,
    canonical_only: bool = False,
) -> Dict[str, Any]:
    payload = build_inventory(repo_root)
    filters = {
        "platform_class": str(platform_class or "").strip().lower() or None,
        "vendor": _normalize_vendor(vendor),
        "family": str(family or "").strip().lower() or None,
        "series": str(series or "").strip().lower() or None,
        "line": str(line or "").strip().lower() or None,
        "part_number": str(part_number or "").strip().lower() or None,
        "label": _normalize_suite_label(label),
        "group_by": str(group_by or "").strip().lower() or "none",
        "canonical_only": bool(canonical_only),
    }

    suites: List[Dict[str, Any]] = []
    for dut in payload.get("duts") or []:
        classification = dut.get("classification") if isinstance(dut.get("classification"), dict) else {}
        if filters["canonical_only"]:
            if dut.get("source") != "golden":
                continue
            if dut.get("suite_label") == "legacy":
                continue
        if filters["platform_class"] and classification.get("platform_class") != filters["platform_class"]:
            continue
        if filters["vendor"] and classification.get("vendor") != filters["vendor"]:
            continue
        if filters["family"] and classification.get("family") != filters["family"]:
            continue
        if filters["series"] and classification.get("series") != filters["series"]:
            continue
        if filters["line"] and classification.get("line") != filters["line"]:
            continue
        if filters["part_number"] and classification.get("part_number") != filters["part_number"]:
            continue
        if filters["label"] and dut.get("suite_label") != filters["label"]:
            continue
        suites.append(
            {
                "dut_id": dut.get("dut_id"),
                "mcu": dut.get("mcu"),
                "family": dut.get("family"),
                "source": dut.get("source"),
                "classification": classification,
                "suite_label": dut.get("suite_label"),
                "suite_label_source": dut.get("suite_label_source"),
                "canonical_pack": dut.get("canonical_pack"),
                "lifecycle_stage": dut.get("lifecycle_stage"),
                "verified_status": dut.get("verified_status"),
                "test_count": len(dut.get("tests") or []),
            }
        )
    grouped: List[Dict[str, Any]] = []
    if filters["group_by"] == "taxonomy":
        buckets: Dict[Tuple[str, str, str, str], List[Dict[str, Any]]] = {}
        for item in suites:
            classification = item.get("classification") if isinstance(item.get("classification"), dict) else {}
            key = (
                str(classification.get("platform_class") or "unknown"),
                str(classification.get("vendor") or "unknown"),
                str(classification.get("family") or "unknown"),
                str(classification.get("series") or "unknown"),
            )
            buckets.setdefault(key, []).append(item)
        for key in sorted(buckets.keys()):
            grouped.append(
                {
                    "platform_class": key[0],
                    "vendor": key[1],
                    "family": key[2],
                    "series": key[3],
                    "suite_count": len(buckets[key]),
                    "suites": sorted(buckets[key], key=lambda item: str(item.get("dut_id") or "")),
                }
            )

    return {
        "ok": True,
        "generated_at": payload.get("generated_at"),
        "filters": filters,
        "summary": {"suite_count": len(suites)},
        "suites": sorted(suites, key=lambda item: str(item.get("dut_id") or "")),
        "groups": grouped,
    }


def diff_connection(
    *,
    board_id: str,
    test_path: str,
    against_board: str,
    against_test: str,
    repo_root: Path | None = None,
) -> Dict[str, Any]:
    left = describe_connection(board_id=board_id, test_path=test_path, repo_root=repo_root)
    if not left.get("ok"):
        return left
    right = describe_connection(board_id=against_board, test_path=against_test, repo_root=repo_root)
    if not right.get("ok"):
        return right
    diff = diff_connection_setups(
        left.get("connection_setup"),
        right.get("connection_setup"),
        left_label=f"{board_id}:{test_path}",
        right_label=f"{against_board}:{against_test}",
    )
    return {
        "ok": True,
        "left": {"board": left.get("board"), "test": left.get("test")},
        "right": {"board": right.get("board"), "test": right.get("test")},
        "diff": diff,
    }


def render_describe_text(payload: Dict[str, Any]) -> str:
    if not payload.get("ok"):
        return f"error: {payload.get('error')}\n"
    lines: List[str] = []
    lines.append(f"board: {payload.get('board')}")
    test = payload.get("test", {})
    lines.append(f"test: {test.get('name')} ({test.get('path')})")
    plan_schema_kind = "structured" if test.get("schema_version") and test.get("schema_version") != "legacy" else "legacy"
    lines.append(f"plan_schema_kind: {plan_schema_kind}")
    if test.get("schema_version"):
        lines.append(f"schema_version: {test.get('schema_version')}")
    if test.get("test_kind"):
        lines.append(f"test_kind: {test.get('test_kind')}")
    if test.get("supported_instruments"):
        lines.append("supported_instruments: " + ", ".join(test.get("supported_instruments") or []))
    if isinstance(test.get("requires"), dict) and test.get("requires"):
        lines.append("requires: " + json.dumps(test.get("requires"), sort_keys=True))
    if test.get("labels"):
        lines.append("labels: " + ", ".join(test.get("labels") or []))
    if test.get("covers"):
        lines.append("covers: " + ", ".join(test.get("covers") or []))
    if test.get("verification_mode_summary"):
        lines.append(f"verification_mode_summary: {test.get('verification_mode_summary')}")
    if test.get("requires_summary"):
        lines.append(f"requires_summary: {test.get('requires_summary')}")
    if isinstance(test.get("supported_instrument_advisory"), dict):
        lines.append(f"supported_instrument_advisory: {test.get('supported_instrument_advisory', {}).get('summary')}")
    if test.get("validation_errors"):
        lines.append("test_validation_errors:")
        for item in test.get("validation_errors") or []:
            lines.append(f"  - {item}")
    dut = payload.get("selected_dut", {})
    if isinstance(dut, dict) and dut:
        lines.append(f"selected_dut: {dut.get('id')}")
        if dut.get("name"):
            lines.append(f"dut_name: {dut.get('name')}")
        if dut.get("target"):
            lines.append(f"dut_target: {dut.get('target')}")
        if dut.get("runtime_binding"):
            lines.append(f"dut_runtime_binding: {dut.get('runtime_binding')}")
    board_profile = payload.get("selected_board_profile", {})
    if isinstance(board_profile, dict) and board_profile:
        lines.append(f"selected_board_profile: {board_profile.get('id')}")
        if board_profile.get("config"):
            lines.append(f"board_profile_config: {board_profile.get('config')}")
        if board_profile.get("role"):
            lines.append(f"board_profile_role: {board_profile.get('role')}")
    poi = payload.get("selected_instrument", {})
    lines.append(f"{poi.get('kind')}: {poi.get('id')}")
    if poi.get("legacy_kind") and poi.get("legacy_kind") != poi.get("kind"):
        lines.append(f"legacy_kind: {poi.get('legacy_kind')}")
    if poi.get("compatibility_kind"):
        lines.append(f"compatibility_kind: {poi.get('compatibility_kind')}")
    if poi.get("type"):
        lines.append(f"type: {poi.get('type')}")
    endpoint = poi.get("endpoint")
    if isinstance(endpoint, dict) and (endpoint.get("host") or endpoint.get("port")):
        lines.append(f"endpoint: {endpoint.get('host')}:{endpoint.get('port')}")
    if isinstance(poi.get("communication"), dict) and poi.get("communication"):
        lines.append("communication:")
        for key, value in (poi.get("communication") or {}).items():
            lines.append(f"  - {key}: {value}")
    if isinstance(poi.get("capability_surfaces"), dict) and poi.get("capability_surfaces"):
        lines.append("capability_surfaces:")
        for key, value in (poi.get("capability_surfaces") or {}).items():
            lines.append(f"  - {key}: {value}")
    if poi.get("metadata_validation_errors"):
        lines.append("metadata_validation_errors:")
        for item in poi.get("metadata_validation_errors") or []:
            lines.append(f"  - {item}")
    conn_setup = payload.get("connection_setup", {})
    if isinstance(conn_setup, dict) and conn_setup:
        lines.append("connection_setup:")
        lines.extend(render_connection_setup_text(conn_setup, indent="  "))
    bench = payload.get("selected_bench_resources", {})
    if isinstance(bench, dict) and bench:
        if bench.get("contract_version") is not None:
            lines.append(f"bench_resource_contract_version: {bench.get('contract_version')}")
        if bench.get("selection_digest"):
            lines.append(f"bench_resource_selection_digest: {'; '.join(bench.get('selection_digest', []))}")
        if bench.get("connection_digest"):
            lines.append(f"connection_digest: {'; '.join(bench.get('connection_digest', []))}")
    lines.append("connections:")
    for conn in payload.get("connections", []):
        extra = []
        if conn.get("kind"):
            extra.append(str(conn.get("kind")))
        if conn.get("expect"):
            extra.append(str(conn.get("expect")))
        if conn.get("freq_hz") is not None:
            extra.append(f"{conn.get('freq_hz')}Hz")
        if conn.get("baud") is not None:
            extra.append(f"baud={conn.get('baud')}")
        if conn.get("expect_v_min") is not None and conn.get("expect_v_max") is not None:
            extra.append(f"{conn.get('expect_v_min')}..{conn.get('expect_v_max')}V")
        if conn.get("status"):
            extra.append(f"status={conn.get('status')}")
        if conn.get("direction"):
            extra.append(f"direction={conn.get('direction')}")
        if conn.get("notes"):
            extra.append(f"notes={conn.get('notes')}")
        suffix = f" ({', '.join(extra)})" if extra else ""
        lines.append(f"  - {conn.get('from')} -> {conn.get('to')}{suffix}")
    lines.append("expected_checks:")
    for check in payload.get("expected_checks", []):
        if check.get("type") == "frequency_ratio":
            lines.append(
                "  - frequency_ratio: "
                f"{check.get('numerator')}/{check.get('denominator')} "
                f"min={check.get('min_ratio')} max={check.get('max_ratio')}"
            )
            continue
        lines.append(f"  - {check.get('type')}: {json.dumps(check, sort_keys=True)}")
    for warning in payload.get("warnings", []):
        lines.append(warning)
    if payload.get("notes"):
        lines.append(f"notes: {payload.get('notes')}")
    return "\n".join(lines).rstrip() + "\n"


def render_describe_dut_text(payload: Dict[str, Any]) -> str:
    if not payload.get("ok"):
        return f"error: {payload.get('error')}\n"
    lines: List[str] = []
    dut = payload.get("dut", {}) if isinstance(payload.get("dut"), dict) else {}
    suite = payload.get("suite", {}) if isinstance(payload.get("suite"), dict) else {}
    canonical_pack = suite.get("canonical_pack", {}) if isinstance(suite.get("canonical_pack"), dict) else {}
    lines.append(f"dut: {dut.get('id')}")
    if dut.get("name"):
        lines.append(f"name: {dut.get('name')}")
    if dut.get("mcu"):
        lines.append(f"mcu: {dut.get('mcu')}")
    if dut.get("family"):
        lines.append(f"family: {dut.get('family')}")
    if dut.get("lifecycle_stage"):
        lines.append(f"lifecycle_stage: {dut.get('lifecycle_stage')}")
    if suite.get("label"):
        lines.append(f"suite_label: {suite.get('label')}")
    if suite.get("label_source"):
        lines.append(f"suite_label_source: {suite.get('label_source')}")
    if canonical_pack:
        lines.append(f"canonical_pack: {canonical_pack.get('path')}")
        if canonical_pack.get("name"):
            lines.append(f"canonical_pack_name: {canonical_pack.get('name')}")
        if canonical_pack.get("status"):
            lines.append(f"canonical_pack_status: {canonical_pack.get('status')}")
        if canonical_pack.get("bench_profile"):
            lines.append(f"bench_profile: {canonical_pack.get('bench_profile')}")
        lines.append(f"stage_count: {canonical_pack.get('stage_count', 0)}")
        lines.append(f"test_count: {canonical_pack.get('test_count', 0)}")
    selected_instrument = payload.get("selected_instrument", {}) if isinstance(payload.get("selected_instrument"), dict) else {}
    if selected_instrument:
        lines.append(f"selected_instrument: {selected_instrument.get('id')}")
        if selected_instrument.get("type"):
            lines.append(f"selected_instrument_type: {selected_instrument.get('type')}")
        endpoint = selected_instrument.get("endpoint") if isinstance(selected_instrument.get("endpoint"), dict) else {}
        if endpoint.get("host") and endpoint.get("port") is not None:
            lines.append(f"selected_instrument_endpoint: {endpoint.get('host')}:{endpoint.get('port')}")
    bench_profile = payload.get("bench_profile", {}) if isinstance(payload.get("bench_profile"), dict) else {}
    if bench_profile:
        lines.append("connections:")
        for conn in bench_profile.get("connections") or []:
            lines.append(f"  - {conn.get('from')} -> {conn.get('to')}")
    if canonical_pack.get("stages"):
        lines.append("stages:")
        for stage in canonical_pack.get("stages") or []:
            lines.append(f"  - stage {stage.get('stage')}:")
            for test in stage.get("tests") or []:
                lines.append(f"    {test.get('name')} ({test.get('path')})")
    return "\n".join(lines).rstrip() + "\n"


def render_suite_list_text(payload: Dict[str, Any]) -> str:
    if not payload.get("ok"):
        return f"error: {payload.get('error')}\n"
    lines: List[str] = []
    filters = payload.get("filters", {}) if isinstance(payload.get("filters"), dict) else {}
    active_filters = [f"{key}={value}" for key, value in filters.items() if value]
    lines.append("Suite inventory")
    if active_filters:
        lines.append("filters: " + ", ".join(active_filters))
    lines.append(f"suite_count: {((payload.get('summary') or {}).get('suite_count', 0))}")
    lines.append("")
    groups = payload.get("groups") if isinstance(payload.get("groups"), list) else []
    if groups:
        for group in groups:
            lines.append(
                "group: "
                f"{group.get('platform_class')} / {group.get('vendor')} / {group.get('family')} / {group.get('series')}"
            )
            lines.append(f"  suite_count: {group.get('suite_count', 0)}")
            for item in group.get("suites") or []:
                classification = item.get("classification") if isinstance(item.get("classification"), dict) else {}
                pack = item.get("canonical_pack") if isinstance(item.get("canonical_pack"), dict) else {}
                parts = [
                    f"  - {item.get('dut_id')}",
                    f"label={item.get('suite_label')}",
                ]
                if classification.get("line"):
                    parts.append(f"line={classification.get('line')}")
                if pack.get("name"):
                    parts.append(f"pack={pack.get('name')}")
                lines.append("  ".join(parts))
            lines.append("")
        return "\n".join(lines).rstrip() + "\n"
    for item in payload.get("suites") or []:
        classification = item.get("classification") if isinstance(item.get("classification"), dict) else {}
        pack = item.get("canonical_pack") if isinstance(item.get("canonical_pack"), dict) else {}
        parts = [
            str(item.get("dut_id") or ""),
            f"label={item.get('suite_label')}",
        ]
        if classification.get("family"):
            parts.append(f"family={classification.get('family')}")
        if classification.get("series"):
            parts.append(f"series={classification.get('series')}")
        if classification.get("line"):
            parts.append(f"line={classification.get('line')}")
        if pack.get("name"):
            parts.append(f"pack={pack.get('name')}")
        lines.append("  ".join(parts))
    return "\n".join(lines).rstrip() + "\n"


def render_connection_text(payload: Dict[str, Any]) -> str:
    if not payload.get("ok"):
        return f"error: {payload.get('error')}\n"
    lines: List[str] = []
    lines.append(f"board: {payload.get('board')}")
    test = payload.get("test", {})
    lines.append(f"test: {test.get('name')} ({test.get('path')})")
    lines.append("connection_setup:")
    lines.extend(render_connection_setup_text(payload.get("connection_setup"), indent="  "))
    lines.append("connections:")
    for conn in payload.get("connections", []):
        extra = []
        if conn.get("kind"):
            extra.append(str(conn.get("kind")))
        if conn.get("expect"):
            extra.append(str(conn.get("expect")))
        if conn.get("freq_hz") is not None:
            extra.append(f"{conn.get('freq_hz')}Hz")
        if conn.get("baud") is not None:
            extra.append(f"baud={conn.get('baud')}")
        if conn.get("expect_v_min") is not None and conn.get("expect_v_max") is not None:
            extra.append(f"{conn.get('expect_v_min')}..{conn.get('expect_v_max')}V")
        if conn.get("required") is True:
            extra.append("required")
        if conn.get("status"):
            extra.append(f"status={conn.get('status')}")
        if conn.get("direction"):
            extra.append(f"direction={conn.get('direction')}")
        if conn.get("notes"):
            extra.append(f"notes={conn.get('notes')}")
        suffix = f" ({', '.join(extra)})" if extra else ""
        lines.append(f"  - {conn.get('from')} -> {conn.get('to')}{suffix}")
    return "\n".join(lines).rstrip() + "\n"


def render_connection_diff_text(payload: Dict[str, Any]) -> str:
    if not payload.get("ok"):
        return f"error: {payload.get('error')}\n"
    diff = payload.get("diff", {}) if isinstance(payload.get("diff"), dict) else {}
    left = payload.get("left", {}) if isinstance(payload.get("left"), dict) else {}
    right = payload.get("right", {}) if isinstance(payload.get("right"), dict) else {}
    lines: List[str] = []
    lines.append(f"left: {left.get('board')} ({(left.get('test') or {}).get('path')})")
    lines.append(f"right: {right.get('board')} ({(right.get('test') or {}).get('path')})")
    lines.append(f"same: {diff.get('same')}")
    lines.append("left_only:")
    for item in diff.get("left_only", []) or []:
        lines.append(f"  - {item}")
    lines.append("right_only:")
    for item in diff.get("right_only", []) or []:
        lines.append(f"  - {item}")
    return "\n".join(lines).rstrip() + "\n"


def render_text(inventory: Dict[str, Any]) -> str:
    lines: List[str] = []
    summary = inventory.get("summary") or {}
    lines.append("DUT inventory")
    lines.append(f"dut_count: {summary.get('dut_count', 0)}")
    lines.append("mcus_with_programs: " + ", ".join(summary.get("mcus_with_tests") or []))
    lines.append("")
    for dut in inventory.get("duts") or []:
        source = dut.get("source") or "golden"
        lifecycle = dut.get("lifecycle_stage")
        suite_label = dut.get("suite_label")
        canonical_pack = (dut.get("canonical_pack") or {}).get("name") if isinstance(dut.get("canonical_pack"), dict) else None
        source_tag = f" [{source}]" if source != "golden" else ""
        lifecycle_tag = f" stage={lifecycle}" if lifecycle else ""
        suite_tag = f" suite={suite_label}" if suite_label else ""
        pack_tag = f" pack={canonical_pack}" if canonical_pack else ""
        lines.append(f"{dut['dut_id']} ({dut.get('mcu')}){source_tag}{lifecycle_tag}{suite_tag}{pack_tag}")
        tests = dut.get("tests") or []
        if not tests:
            lines.append("  programs: none")
            continue
        for test in tests:
            extras = []
            schema_tag = test.get("schema_version")
            if schema_tag and schema_tag != "legacy":
                extras.append(f"schema={schema_tag}")
            if test.get("test_kind"):
                extras.append(f"kind={test.get('test_kind')}")
            if test.get("validation_errors"):
                extras.append(f"errors={len(test.get('validation_errors') or [])}")
            for source in test.get("sources") or []:
                via = source.get("via")
                if source.get("pack"):
                    via = f"{via}, pack={source['pack']}"
                if source.get("missing"):
                    via = f"{via}, missing"
                extras.append(via)
            lines.append(f"  - {test.get('name')} [{' ; '.join([e for e in extras if e])}]")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def render_instance_text(payload: Dict[str, Any]) -> str:
    return render_resolved_instrument_inventory_text(payload)
