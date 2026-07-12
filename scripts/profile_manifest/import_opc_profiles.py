#!/usr/bin/env python3
"""OPC profile import/snapshot tool.

Reads a pinned OPC profile/facet/CU snapshot and merges OPC facts into the
manifest. Preserves existing project claim state: imported items that do not
already exist in the manifest are added as ``unimplemented``; items that
already exist are never promoted to ``claimed`` or ``implemented``.

Usage:
    python3 scripts/profile_manifest/import_opc_profiles.py \
        --snapshot profiles/opcua-profile-snapshot.json \
        --manifest profiles/opcua-profile-manifest.yaml \
        --dry-run

    python3 scripts/profile_manifest/import_opc_profiles.py \
        --snapshot profiles/opcua-profile-snapshot.json \
        --manifest profiles/opcua-profile-manifest.yaml \
        --write

Standard-library only.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.request
from typing import Any

_HERE = os.path.dirname(os.path.abspath(__file__))
_PKG_PARENT = os.path.dirname(_HERE)
if _PKG_PARENT not in sys.path:
    sys.path.insert(0, _PKG_PARENT)

from profile_manifest.model import load_manifest, validate_manifest  # noqa: E402

_DEFAULT_PROFILES = ("nano", "micro", "embedded", "standard", "full", "custom")


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Import OPC profile/facet/CU snapshot into the manifest.",
    )
    parser.add_argument(
        "--snapshot",
        help="Path to the pinned OPC snapshot JSON file for manifest merge modes.",
    )
    parser.add_argument(
        "--manifest",
        help="Path to the manifest .yaml/.json file for manifest merge modes.",
    )
    parser.add_argument(
        "--profile-catalog",
        help="Raw OPC API profile catalog JSON to normalize.",
    )
    parser.add_argument(
        "--cu-catalog",
        help="Raw OPC API conformance-unit catalog JSON to normalize.",
    )
    parser.add_argument(
        "--relationship-dir",
        help="Directory containing includedprofiles-<id>.json and includedconformanceunits-<id>.json files.",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--dry-run",
        action="store_true",
        help="Read the snapshot and print a summary without modifying the manifest.",
    )
    mode.add_argument(
        "--write",
        action="store_true",
        help="Merge snapshot OPC facts into the manifest file in place.",
    )
    mode.add_argument(
        "--write-normalized-snapshot",
        help="Path to write deterministic normalized snapshot JSON.",
    )
    mode.add_argument(
        "--fetch-relationships",
        action="store_true",
        help="Fetch included profile/CU relationship JSON for server profiles into --relationship-dir.",
    )
    return parser.parse_args(argv)


def _load_snapshot(path: str) -> dict:
    if not os.path.isfile(path):
        raise FileNotFoundError(f"snapshot not found: {path}")
    with open(path, "r", encoding="utf-8") as fh:
        data = fh.read()
    try:
        snapshot = json.loads(data)
    except json.JSONDecodeError as exc:
        raise ValueError(f"snapshot {path} is not valid JSON: {exc}") from exc
    if not isinstance(snapshot, dict):
        raise ValueError(f"snapshot {path} must decode to a JSON object at top level")
    return snapshot


def _load_relationship_dir(path: str | None) -> tuple[dict[str, list[dict]], dict[str, list[dict]]]:
    if not path:
        return {}, {}
    included_profiles: dict[str, list[dict]] = {}
    included_cus: dict[str, list[dict]] = {}
    for name in sorted(os.listdir(path)):
        full_path = os.path.join(path, name)
        if not os.path.isfile(full_path):
            continue
        if name.startswith("includedprofiles-") and name.endswith(".json"):
            payload = _load_snapshot(full_path)
            rows = payload.get("result") or []
            included_profiles[name[len("includedprofiles-"):-len(".json")]] = rows
        elif name.startswith("includedconformanceunits-") and name.endswith(".json"):
            payload = _load_snapshot(full_path)
            rows = payload.get("result") or []
            included_cus[name[len("includedconformanceunits-"):-len(".json")]] = rows
    return included_profiles, included_cus


def fetch_relationships(profile_ids: list[int], output_dir: str) -> None:
    os.makedirs(output_dir, exist_ok=True)
    base = "https://profiles.opcfoundation.org/api/profile"
    endpoints = (
        ("includedprofiles", "includedprofiles"),
        ("includedconformanceunits", "includedconformanceunits"),
    )
    for profile_id in sorted(set(profile_ids)):
        for endpoint, prefix in endpoints:
            url = f"{base}/{endpoint}/{profile_id}/?pg=171&all=1"
            target = os.path.join(output_dir, f"{prefix}-{profile_id}.json")
            with urllib.request.urlopen(url, timeout=30) as response:
                payload = response.read().decode("utf-8")
            parsed = json.loads(payload)
            with open(target, "w", encoding="utf-8") as fh:
                json.dump(parsed, fh, indent=2, sort_keys=True)
                fh.write("\n")


def _snapshot_id(entry: dict) -> str | None:
    sid = entry.get("snapshot_id")
    if isinstance(sid, str) and sid:
        return sid
    return None


def _release_status_name(status: int | None) -> str:
    if status == 3:
        return "active"
    if status == 4:
        return "superseded"
    if status is None:
        return "unknown"
    return "unknown_" + str(status)


def _is_server_record(record: dict) -> bool:
    uri = record.get("profileUri") or record.get("opc_uri") or ""
    return isinstance(uri, str) and "/Server/" in uri


def _select_canonical_server_profiles(records: list[dict]) -> dict[str, dict]:
    patterns = {
        "nano": (2266, "Nano Embedded Device 2025 Server Profile"),
        "micro": (2267, "Micro Embedded Device 2025 Server Profile"),
        "embedded": (2268, "Embedded 2025 UA Server Profile"),
        "standard": (2269, "Standard 2025 UA Server Profile"),
    }
    selected: dict[str, dict] = {}
    active_server_records = [
        r for r in records
        if _is_server_record(r) and r.get("releaseStatus") == 3
    ]
    for key, (expected_id, name) in patterns.items():
        match = next(
            (
                r for r in active_server_records
                if r.get("id") == expected_id and r.get("name") == name
            ),
            None,
        )
        if match is None:
            raise ValueError(
                "missing active canonical server profile: "
                + name
                + " (id "
                + str(expected_id)
                + ")"
            )
        selected[key] = match
    return selected


def _normalize_profile_row(row: dict) -> dict:
    return {
        "opc_id": row.get("id"),
        "name": row.get("name"),
        "display_name": row.get("name"),
        "opc_profile_uri": row.get("profileUri"),
        "release_status": row.get("releaseStatus"),
        "release_status_name": _release_status_name(row.get("releaseStatus")),
        "description": row.get("description") or "",
        "last_update_time": row.get("lastUpdateTime"),
        "source_metadata": {
            "profile_group_id": row.get("profileGroupId"),
            "source_endpoint": "profile/?pg=171&all=1",
        },
    }


def _normalize_cu_row(row: dict) -> dict:
    return {
        "opc_id": row.get("id"),
        "name": row.get("name"),
        "release_status": row.get("releaseStatus"),
        "release_status_name": _release_status_name(row.get("releaseStatus")),
        "description": row.get("description") or "",
        "source_metadata": {
            "profile_group_id": row.get("profileGroupId"),
            "source_endpoint": "conformanceunit/?pg=171&all=1",
        },
    }


def _ids(rows: list[dict]) -> list[int]:
    values = [row.get("id") for row in rows]
    return sorted(v for v in values if isinstance(v, int))


def _populate_relationship_closures(snapshot: dict) -> None:
    included_profiles = snapshot["relationships"]["included_profiles"]
    included_cus = snapshot["relationships"]["included_conformance_units"]

    def profile_closure(profile_id: int, seen: set[int] | None = None) -> list[int]:
        if seen is None:
            seen = set()
        if profile_id in seen:
            return []
        seen.add(profile_id)
        result: list[int] = []
        for child in included_profiles.get(str(profile_id), []):
            result.append(child)
            result.extend(profile_closure(child, seen))
        return sorted(set(result))

    def cu_closure(profile_id: int) -> list[int]:
        profile_ids = [profile_id] + profile_closure(profile_id)
        result: set[int] = set()
        for pid in profile_ids:
            result.update(included_cus.get(str(pid), []))
        return sorted(result)

    canonical = snapshot.get("canonical_profiles") or {}
    for profile in canonical.values():
        opc_id = profile.get("opc_id")
        if not isinstance(opc_id, int):
            continue
        snapshot["relationships"]["transitive_profile_closure"][str(opc_id)] = profile_closure(opc_id)
        snapshot["relationships"]["transitive_cu_closure"][str(opc_id)] = cu_closure(opc_id)

    order = ("nano", "micro", "embedded", "standard")
    for key in order:
        profile = canonical.get(key) or {}
        opc_id = profile.get("opc_id")
        if not isinstance(opc_id, int):
            continue
        for cu_id in snapshot["relationships"]["transitive_cu_closure"].get(str(opc_id), []):
            snapshot["minimum_profile_defaults"].setdefault(str(cu_id), key)


def _normalize_api_snapshot(
    profile_rows: list[dict],
    cu_rows: list[dict],
    included_profiles: dict[str, list[dict]],
    included_cus: dict[str, list[dict]],
) -> dict:
    canonical = _select_canonical_server_profiles(profile_rows)
    server_records = [row for row in profile_rows if _is_server_record(row)]
    facet_records = [
        row for row in server_records
        if isinstance(row.get("profileUri"), str) and "Facet" in row.get("profileUri")
    ]
    profile_records = [row for row in server_records if row not in facet_records]
    normalized = {
        "snapshot_version": "2",
        "metadata": {
            "profile_group_id": 171,
            "source": "https://profiles.opcfoundation.org/api/",
            "note": "Normalized subset of OPC UA 1.05 API data for deterministic Kconfig generation.",
        },
        "canonical_profiles": {
            key: _normalize_profile_row(row)
            for key, row in sorted(canonical.items())
        },
        "profiles": [
            _normalize_profile_row(row)
            for row in sorted(profile_records, key=lambda r: (r.get("id") or 0))
            if row.get("releaseStatus") == 3
        ],
        "facets": [
            _normalize_profile_row(row)
            for row in sorted(facet_records, key=lambda r: (r.get("id") or 0))
            if row.get("releaseStatus") == 3
        ],
        "superseded_profiles": [
            _normalize_profile_row(row)
            for row in sorted(profile_records, key=lambda r: (r.get("id") or 0))
            if row.get("releaseStatus") == 4
        ],
        "conformance_units": [_normalize_cu_row(row) for row in sorted(cu_rows, key=lambda r: (r.get("id") or 0))],
        "relationships": {
            "included_profiles": {
                str(key): _ids(rows)
                for key, rows in sorted(included_profiles.items())
            },
            "included_conformance_units": {
                str(key): _ids(rows)
                for key, rows in sorted(included_cus.items())
            },
            "transitive_profile_closure": {},
            "transitive_cu_closure": {},
        },
        "minimum_profile_defaults": {},
    }
    _populate_relationship_closures(normalized)
    return normalized


def _match_manifest_item(
    snapshot_entry: dict,
    manifest_item: dict,
) -> bool:
    """Return True if *snapshot_entry* and *manifest_item* refer to the same OPC item.

    Matching is by (in priority order):
      1. OPC profile DB id (opc_id / cu_id / cu_ids)
      2. OPC profile URI
      3. Normalized facet/CU name
    """
    snap_id = snapshot_entry.get("opc_id")
    snap_uri = snapshot_entry.get("opc_uri")
    snap_name = (snapshot_entry.get("name") or "").strip().lower()

    opc_ref = manifest_item.get("opc_reference")
    if not isinstance(opc_ref, dict):
        opc_ref = {}

    # Match by OPC DB id.
    if isinstance(snap_id, str) and snap_id:
        cu_id = opc_ref.get("cu_id")
        if isinstance(cu_id, str) and cu_id == snap_id:
            return True
        cu_ids = opc_ref.get("cu_ids")
        if isinstance(cu_ids, list) and snap_id in cu_ids:
            return True

    # Match by OPC profile URI.
    if isinstance(snap_uri, str) and snap_uri:
        item_uri = opc_ref.get("opc_profile_uri")
        if isinstance(item_uri, str) and item_uri == snap_uri:
            return True

    # Match by normalized facet name.
    if snap_name:
        facet = opc_ref.get("facet")
        if isinstance(facet, str) and facet.strip().lower() == snap_name:
            return True
        cu_name = opc_ref.get("cu_name")
        if isinstance(cu_name, str) and cu_name.strip().lower() == snap_name:
            return True

    return False


def _build_unimplemented_item(
    snapshot_entry: dict,
    kind: str,
) -> dict:
    """Build a new unimplemented manifest item from a snapshot entry."""
    source = snapshot_entry.get("source") or {}
    spec = source.get("spec") if isinstance(source, dict) else None
    section = source.get("section") if isinstance(source, dict) else None
    doc_path = source.get("doc_path") if isinstance(source, dict) else None
    profile_db_url = source.get("profile_db_url") if isinstance(source, dict) else None
    name = snapshot_entry.get("name") or snapshot_entry.get("snapshot_id") or "imported_opc_item"
    item_id = "opc_" + name.strip().lower().replace(" ", "_").replace("/", "_")

    opc_reference: dict[str, Any] = {
        "spec": spec,
        "section": section,
        "facet": name,
        "cu_optional": bool(snapshot_entry.get("is_optional", True)),
        "source_doc": doc_path,
        "source_url": profile_db_url,
        "snapshot_id": _snapshot_id(snapshot_entry),
    }

    return {
        "id": item_id,
        "kind": kind,
        "kconfig_symbol": None,
        "implementation_state": "unimplemented",
        "depends_on": [],
        "profile_defaults": {pk: False for pk in _DEFAULT_PROFILES},
        "backing_tests": [],
        "opc_reference": opc_reference,
        "source_metadata": {
            "imported_from": "profiles/opcua-profile-snapshot.json",
            "snapshot_id": _snapshot_id(snapshot_entry),
            "source_doc": doc_path,
            "source_url": profile_db_url,
        },
        "notes": f"Imported from OPC snapshot. {snapshot_entry.get('description', '')}".strip(),
    }


def _merge_snapshot_into_manifest(
    snapshot: dict,
    manifest: dict,
) -> tuple[dict, dict[str, int]]:
    """Merge snapshot OPC facts into *manifest*.

    Returns ``(new_manifest, counts)`` where counts summarizes the merge.
    Existing items are never promoted; new items are added as ``unimplemented``.
    """
    items = list(manifest.get("items") or [])
    counts = {
        "profiles_in_snapshot": 0,
        "facets_in_snapshot": 0,
        "conformance_units_in_snapshot": 0,
        "profiles_matched": 0,
        "facets_matched": 0,
        "conformance_units_matched": 0,
        "facets_added": 0,
        "conformance_units_added": 0,
        "claimed_items_preserved": 0,
        "implemented_items_preserved": 0,
        "unimplemented_items_imported": 0,
    }

    # Count existing claim states.
    for item in items:
        state = item.get("implementation_state")
        if state == "claimed":
            counts["claimed_items_preserved"] += 1
        elif state == "implemented":
            counts["implemented_items_preserved"] += 1

    # Track which manifest items were matched so we can report unmatched counts.
    matched_item_indices: set[int] = set()

    # Merge facets.
    snapshot_facets = snapshot.get("facets") or []
    counts["facets_in_snapshot"] = len(snapshot_facets)
    for facet_entry in snapshot_facets:
        matched = False
        for idx, item in enumerate(items):
            if _match_manifest_item(facet_entry, item):
                matched = True
                matched_item_indices.add(idx)
                counts["facets_matched"] += 1
                break
        if not matched:
            new_item = _build_unimplemented_item(facet_entry, "facet")
            items.append(new_item)
            counts["facets_added"] += 1
            counts["unimplemented_items_imported"] += 1

    # Merge conformance units.
    snapshot_cus = snapshot.get("conformance_units") or []
    counts["conformance_units_in_snapshot"] = len(snapshot_cus)
    for cu_entry in snapshot_cus:
        matched = False
        for idx, item in enumerate(items):
            if _match_manifest_item(cu_entry, item):
                matched = True
                matched_item_indices.add(idx)
                counts["conformance_units_matched"] += 1
                break
        if not matched:
            new_item = _build_unimplemented_item(cu_entry, "conformance_unit")
            items.append(new_item)
            counts["conformance_units_added"] += 1
            counts["unimplemented_items_imported"] += 1

    # Track profiles (informational — profiles live in manifest.profiles, not items).
    snapshot_profiles = snapshot.get("profiles") or []
    counts["profiles_in_snapshot"] = len(snapshot_profiles)
    manifest_profiles = manifest.get("profiles") or {}
    manifest_uris = set()
    for pdata in manifest_profiles.values():
        if isinstance(pdata, dict):
            uri = pdata.get("opc_profile_uri")
            if isinstance(uri, str) and uri:
                manifest_uris.add(uri)
    for prof in snapshot_profiles:
        uri = prof.get("opc_uri")
        if isinstance(uri, str) and uri in manifest_uris:
            counts["profiles_matched"] += 1

    new_manifest = dict(manifest)
    new_manifest["items"] = items
    return new_manifest, counts


def _merge_normalized_snapshot_into_manifest(
    snapshot: dict,
    manifest: dict,
) -> tuple[dict, dict[str, int]]:
    counts = {
        "profiles_updated": 0,
        "facets_added": 0,
        "conformance_units_added": 0,
        "claimed_items_preserved": 0,
        "implemented_items_preserved": 0,
        "unimplemented_items_imported": 0,
    }
    new_manifest = json.loads(json.dumps(manifest))

    metadata = new_manifest.setdefault("metadata", {})
    metadata["superseded_profiles"] = snapshot.get("superseded_profiles", [])

    profiles = new_manifest.setdefault("profiles", {})
    for key, profile_data in (snapshot.get("canonical_profiles") or {}).items():
        if key not in profiles:
            continue
        target = profiles[key]
        target["opc_display_name"] = profile_data.get("display_name") or profile_data.get("name")
        target["opc_profile_uri"] = profile_data.get("opc_profile_uri")
        target["opc_id"] = profile_data.get("opc_id")
        target["release_status"] = profile_data.get("release_status")
        target["release_status_name"] = profile_data.get("release_status_name")
        counts["profiles_updated"] += 1

    items = new_manifest.setdefault("items", [])
    facet_containment = new_manifest.setdefault("facet_containment", {})
    existing_cu_ids = set()
    existing_item_ids = {item.get("id") for item in items if isinstance(item.get("id"), str)}
    for item in items:
        if item.get("implementation_state") == "claimed":
            counts["claimed_items_preserved"] += 1
        if item.get("implementation_state") == "implemented":
            counts["implemented_items_preserved"] += 1
        opc_ref = item.get("opc_reference") if isinstance(item.get("opc_reference"), dict) else {}
        cu_id = opc_ref.get("cu_id")
        if cu_id is not None:
            existing_cu_ids.add(str(cu_id))

    minimum_defaults = snapshot.get("minimum_profile_defaults") or {}
    profile_order = ("nano", "micro", "embedded", "standard", "full")
    relationships = snapshot.get("relationships") or {}
    included_cus = relationships.get("included_conformance_units") or {}
    transitive_cu_closure = relationships.get("transitive_cu_closure") or {}
    reachable_cu_ids = {
        str(cu_id)
        for cu_ids in transitive_cu_closure.values()
        for cu_id in cu_ids
    }
    facet_ids = {
        str(facet.get("opc_id"))
        for facet in snapshot.get("facets") or []
        if facet.get("opc_id") is not None
    }
    # Exclude canonical and superseded profile IDs from facet containment parents
    canonical_ids = {
        str(row.get("opc_id"))
        for row in (snapshot.get("canonical_profiles") or {}).values()
        if row.get("opc_id") is not None
    }
    superseded_ids = {
        str(row.get("opc_id"))
        for row in snapshot.get("superseded_profiles") or []
        if row.get("opc_id") is not None
    }
    profile_ids_to_skip = canonical_ids | superseded_ids
    cu_to_facets: dict[str, list[str]] = {}
    for facet_id, cu_ids in sorted(included_cus.items()):
        if str(facet_id) in profile_ids_to_skip:
            continue
        for cu_id in cu_ids:
            cu_to_facets.setdefault(str(cu_id), [])
            if str(facet_id) not in cu_to_facets[str(cu_id)]:
                cu_to_facets[str(cu_id)].append(str(facet_id))
    for facet_list in cu_to_facets.values():
        facet_list.sort()

    def defaults_for_cu(cu_id: int) -> dict[str, bool]:
        minimum = minimum_defaults.get(str(cu_id))
        defaults = {pk: False for pk in _DEFAULT_PROFILES}
        if minimum in profile_order:
            start = profile_order.index(minimum)
            for pk in profile_order[start:]:
                defaults[pk] = True
        return defaults

    for facet in snapshot.get("facets") or []:
        opc_id = facet.get("opc_id")
        if opc_id is None:
            continue
        facet_id = "opc_facet_" + str(opc_id)
        if facet_id not in existing_item_ids:
            items.append({
                "id": facet_id,
                "kind": "facet",
                "opc_display_name": facet.get("name"),
                "kconfig_symbol": None,
                "implementation_state": "unimplemented",
                "depends_on": [],
                "profile_defaults": {pk: False for pk in _DEFAULT_PROFILES},
                "backing_tests": [],
                "opc_reference": {
                    "spec": "OPC-10000-7",
                    "section": "4.2",
                    "profile_id": str(opc_id),
                    "profile_uri": None,
                    "source_url": "https://profiles.opcfoundation.org",
                },
                "source_metadata": facet.get("source_metadata", {}),
                "notes": facet.get("description") or "Imported from OPC API as unimplemented.",
            })
            existing_item_ids.add(facet_id)
            counts["facets_added"] += 1

    for cu in snapshot.get("conformance_units") or []:
        opc_id = cu.get("opc_id")
        if opc_id is None or str(opc_id) in existing_cu_ids:
            continue
        if str(opc_id) not in reachable_cu_ids:
            continue
        assigned_facets = cu_to_facets.get(str(opc_id), [])
        if len(assigned_facets) != 1:
            continue
        item_id = "opc_cu_" + str(opc_id)
        if any(item.get("id") == item_id for item in items):
            continue
        items.append({
            "id": item_id,
            "kind": "conformance_unit",
            "opc_display_name": cu.get("name"),
            "kconfig_symbol": None,
            "implementation_state": "unimplemented",
            "depends_on": [],
            "profile_defaults": defaults_for_cu(opc_id),
            "backing_tests": [],
            "opc_reference": {
                "spec": None,
                "section": None,
                "cu_id": str(opc_id),
                "cu_name": cu.get("name"),
                "source_url": "https://profiles.opcfoundation.org",
            },
            "source_metadata": cu.get("source_metadata", {}),
            "notes": cu.get("description") or "Imported from OPC API as unimplemented.",
        })
        counts["unimplemented_items_imported"] += 1
        existing_item_ids.add(item_id)
        counts["conformance_units_added"] += 1
        facet_item_id = "opc_facet_" + assigned_facets[0]
        contained = facet_containment.setdefault(facet_item_id, [])
        if item_id not in contained:
            contained.append(item_id)
        contained.sort()

    return new_manifest, counts


def _print_summary(counts: dict[str, int]) -> None:
    print("OPC snapshot import summary")
    print(f"  profiles in snapshot:          {counts.get('profiles_in_snapshot', 0)}")
    print(f"  profiles matched in manifest:   {counts.get('profiles_matched', 0)}")
    print(f"  profiles updated:               {counts.get('profiles_updated', 0)}")
    print(f"  facets in snapshot:             {counts.get('facets_in_snapshot', 0)}")
    print(f"  facets matched (preserved):     {counts.get('facets_matched', 0)}")
    print(f"  facets added (unimplemented):   {counts.get('facets_added', 0)}")
    print(f"  CUs in snapshot:                {counts.get('conformance_units_in_snapshot', 0)}")
    print(f"  CUs matched (preserved):        {counts.get('conformance_units_matched', 0)}")
    print(f"  CUs added (unimplemented):      {counts.get('conformance_units_added', 0)}")
    print(f"  claimed items preserved:        {counts.get('claimed_items_preserved', 0)}")
    print(f"  implemented items preserved:    {counts.get('implemented_items_preserved', 0)}")
    print(f"  unimplemented items imported:   {counts.get('unimplemented_items_imported', 0)}")


def _write_manifest(path: str, manifest: dict) -> None:
    """Write the manifest as indented JSON (JSON-in-YAML convention)."""
    text = json.dumps(manifest, indent=2, ensure_ascii=False)
    # Ensure trailing newline for clean diffs.
    if not text.endswith("\n"):
        text += "\n"
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _require_args(args: argparse.Namespace, names: tuple[str, ...], mode: str) -> bool:
    missing = [name for name in names if not getattr(args, name)]
    if missing:
        print(mode + ": FAIL")
        print("  missing required option(s): " + ", ".join("--" + name.replace("_", "-") for name in missing))
        return False
    return True


def _write_normalized_snapshot(args: argparse.Namespace) -> int:
    if not _require_args(
        args,
        ("profile_catalog", "cu_catalog", "relationship_dir", "write_normalized_snapshot"),
        "normalized snapshot",
    ):
        return 1
    profile_catalog = _load_snapshot(args.profile_catalog)
    cu_catalog = _load_snapshot(args.cu_catalog)
    profile_rows = profile_catalog.get("result") or []
    cu_rows = cu_catalog.get("result") or []
    included_profiles, included_cus = _load_relationship_dir(args.relationship_dir)
    normalized = _normalize_api_snapshot(profile_rows, cu_rows, included_profiles, included_cus)
    with open(args.write_normalized_snapshot, "w", encoding="utf-8") as fh:
        json.dump(normalized, fh, indent=2, sort_keys=True)
        fh.write("\n")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)

    if args.fetch_relationships:
        if not _require_args(args, ("profile_catalog", "relationship_dir"), "relationship fetch"):
            return 1
        profile_catalog = _load_snapshot(args.profile_catalog)
        profile_rows = profile_catalog.get("result") or []
        server_ids = [row.get("id") for row in profile_rows if _is_server_record(row)]
        fetch_relationships([v for v in server_ids if isinstance(v, int)], args.relationship_dir)
        return 0

    if args.write_normalized_snapshot:
        return _write_normalized_snapshot(args)

    if not _require_args(args, ("snapshot", "manifest"), "manifest merge"):
        return 1

    try:
        snapshot = _load_snapshot(args.snapshot)
    except (FileNotFoundError, ValueError) as exc:
        print(f"snapshot: FAIL")
        print(f"  load error: {exc}")
        return 1

    try:
        manifest = load_manifest(args.manifest)
    except (FileNotFoundError, ValueError) as exc:
        print(f"manifest: FAIL")
        print(f"  load error: {exc}")
        return 1

    if snapshot.get("snapshot_version") == "2":
        new_manifest, counts = _merge_normalized_snapshot_into_manifest(snapshot, manifest)
    else:
        new_manifest, counts = _merge_snapshot_into_manifest(snapshot, manifest)

    if args.dry_run:
        _print_summary(counts)
        # Validate the merged result in memory to prove it would be clean.
        errors = validate_manifest(new_manifest)
        if errors:
            print(f"  WARNING: merged manifest would have {len(errors)} validation error(s):")
            for err in errors:
                print(f"    - {err}")
            return 1
        print("  dry-run validation: OK")
        return 0

    # --write mode
    errors = validate_manifest(new_manifest)
    if errors:
        print(f"manifest: FAIL (merged result has validation errors)")
        for err in errors:
            print(f"  - {err}")
        return 1

    _write_manifest(args.manifest, new_manifest)
    _print_summary(counts)
    print(f"  manifest written: {args.manifest}")
    print(f"  manifest validation: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
