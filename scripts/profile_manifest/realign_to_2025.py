#!/usr/bin/env python3
"""Realign manifest items to 2025 OPC profile definitions.

- Imports missing facets/CUs from the normalized snapshot
- Computes correct profile_defaults for facets from the profile hierarchy
- Assigns multi-parent CUs to the lowest-tier facet parent
- Builds facet_containment for ALL facets (even empty ones)
- Sets kconfig_symbol and deferred state for facets that need rendering
"""

import json
import os
import sys

_DEFAULT_PROFILES = ("nano", "micro", "embedded", "standard", "full", "custom")
_PROFILE_ORDER = ("nano", "micro", "embedded", "standard", "full")


def load_json(path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def save_json(path, data):
    text = json.dumps(data, indent=2, ensure_ascii=False)
    if not text.endswith("\n"):
        text += "\n"
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


_RAW_CATALOG_PATH = "profiles/opcua-1.05-api/profile-catalog.pg171.json"


def _load_raw_catalog_rows():
    """Return raw OPC API profile catalog rows keyed by id (as str).

    The normalized snapshot only captures Server Facet records (URIs with
    ``Facet`` in them). Transport and security profiles such as
    id 837 (``UA-TCP UA-SC UA-Binary``) and id 1760
    (``Security Time Synchronization``) are required sub-profiles for the
    Nano 2025 profile but are filtered out during normalization because
    their URIs live under ``/Transport/`` and ``/Security/`` rather than
    ``/Server/``. The raw catalog at
    ``profiles/opcua-1.05-api/profile-catalog.pg171.json`` has the full
    profile list (loaded as ``{"result": [...]}``) and is the
    authoritative source for these rows. This helper exposes them in the
    same shape as normalized snapshot rows so the rest of the realign
    pipeline can consume them transparently.
    """
    if not os.path.isfile(_RAW_CATALOG_PATH):
        return {}
    with open(_RAW_CATALOG_PATH, "r", encoding="utf-8") as fh:
        payload = json.load(fh)
    rows = {}
    for entry in payload.get("result", []) or []:
        rid = entry.get("id")
        if rid is None:
            continue
        rows[str(rid)] = {
            "opc_id": rid,
            "name": entry.get("name"),
            "display_name": entry.get("name"),
            "opc_profile_uri": entry.get("profileUri"),
            "release_status": entry.get("releaseStatus"),
            "description": entry.get("description") or "",
            "source_metadata": {
                "profile_group_id": entry.get("profileGroupId"),
                "source_endpoint": "profile/?pg=171&all=1",
                "imported_from_raw_catalog": True,
            },
        }
    return rows


def main():
    snapshot = load_json("profiles/opcua-profile-normalized-snapshot.json")
    manifest = load_json("profiles/opcua-profile-manifest.yaml")

    rel = snapshot.get("relationships") or {}
    min_defaults = snapshot.get("minimum_profile_defaults") or {}
    icu = rel.get("included_conformance_units") or {}
    ip = rel.get("included_profiles") or {}
    tcu = rel.get("transitive_cu_closure") or {}

    canonical_ids = {
        str(snapshot["canonical_profiles"][k]["opc_id"])
        for k in ("nano", "micro", "embedded", "standard")
    }

    # --- Compute facet metadata ---
    # For each facet, determine: which is the lowest profile that includes it
    facet_lowest_profile: dict[str, str] = {}
    for k in ("nano", "micro", "embedded", "standard"):
        pid = str(snapshot["canonical_profiles"][k]["opc_id"])
        for fid in ip.get(pid, []):
            sfid = str(fid)
            if sfid not in canonical_ids:
                if sfid not in facet_lowest_profile:
                    facet_lowest_profile[sfid] = k

    def profile_defaults_from(lowest_key):
        defaults = {pk: False for pk in _DEFAULT_PROFILES}
        if lowest_key in _PROFILE_ORDER:
            start = _PROFILE_ORDER.index(lowest_key)
            for pk in _PROFILE_ORDER[start:]:
                defaults[pk] = True
        return defaults

    def cu_defaults(cid_str):
        minimum = min_defaults.get(cid_str)
        defaults = {pk: False for pk in _DEFAULT_PROFILES}
        if minimum in _PROFILE_ORDER:
            start = _PROFILE_ORDER.index(minimum)
            for pk in _PROFILE_ORDER[start:]:
                defaults[pk] = True
        return defaults

    # Build lookup of all snapshot items by opc_id. The normalized snapshot
    # filters out non-Facet server records, so transport/security sub-profiles
    # required by Nano 2025 (e.g. id 837 "UA-TCP UA-SC UA-Binary" and
    # id 1760 "Security Time Synchronization") are missing. Augment with the
    # raw catalog rows for any id not already covered so the realignment can
    # populate their name/URI/description and import them as facets.
    all_snapshot_items = {}
    for source in ("profiles", "facets", "conformance_units"):
        for row in snapshot.get(source, []):
            oid = row.get("opc_id")
            if oid is not None:
                all_snapshot_items[str(oid)] = row
    for rid, row in _load_raw_catalog_rows().items():
        if rid not in all_snapshot_items:
            all_snapshot_items[rid] = row

    items = list(manifest.get("items", []))
    existing_ids = {item.get("id") for item in items if isinstance(item.get("id"), str)}

    # --- Import missing facets ---
    facets_added = 0
    for fid in sorted(facet_lowest_profile.keys(), key=int):
        facet_id = "opc_facet_" + fid
        if facet_id in existing_ids:
            continue
        if fid not in all_snapshot_items:
            continue
        row = all_snapshot_items[fid]
        items.append({
            "id": facet_id, "kind": "facet",
            "opc_display_name": row.get("name", f"Facet {fid}"),
            "kconfig_symbol": None, "implementation_state": "unimplemented",
            "depends_on": [],
            "profile_defaults": profile_defaults_from(facet_lowest_profile[fid]),
            "backing_tests": [],
            "opc_reference": {
                "spec": "OPC-10000-7", "section": "4.2",
                "profile_id": fid, "profile_uri": row.get("opc_profile_uri"),
                "source_url": "https://profiles.opcfoundation.org",
            },
            "source_metadata": row.get("source_metadata", {}),
            "notes": row.get("description") or f"Imported from OPC API facet {fid}.",
        })
        existing_ids.add(facet_id)
        facets_added += 1

    # --- Update existing facet profile_defaults ---
    for item in items:
        if item.get("kind") != "facet":
            continue
        ref = item.get("opc_reference") or {}
        fid = ref.get("profile_id")
        if fid and str(fid) in facet_lowest_profile:
            new_defaults = profile_defaults_from(facet_lowest_profile[str(fid)])
            if item.get("profile_defaults") != new_defaults:
                item["profile_defaults"] = new_defaults

    # --- Import CUs ---
    # For multi-parent CUs: assign to the lowest-tier parent
    cus_added = 0
    skipped = 0

    # Build CU-to-best-parent mapping
    cu_to_parent: dict[str, str] = {}
    for cid_str in sorted({str(c) for cu_ids in tcu.values() for c in cu_ids}):
        parents = [p for p, clist in icu.items()
                   if int(cid_str) in clist and p in facet_lowest_profile]
        if len(parents) == 1:
            cu_to_parent[cid_str] = parents[0]
        elif len(parents) > 1:
            # Pick the lowest-tier parent (by profile order)
            best = None
            best_idx = len(_PROFILE_ORDER)
            for p in parents:
                p_lowest = facet_lowest_profile.get(p)
                if p_lowest and p_lowest in _PROFILE_ORDER:
                    idx = _PROFILE_ORDER.index(p_lowest)
                    if idx < best_idx:
                        best_idx = idx
                        best = p
            if best:
                cu_to_parent[cid_str] = best
            else:
                skipped += 1
        # else: no parents, skip

    for cid_str, parent_fid in sorted(cu_to_parent.items(), key=lambda x: int(x[0])):
        cu_id = "opc_cu_" + cid_str
        if cu_id in existing_ids:
            continue
        if cid_str not in all_snapshot_items:
            continue
        row = all_snapshot_items[cid_str]
        items.append({
            "id": cu_id, "kind": "conformance_unit",
            "opc_display_name": row.get("name", f"CU {cid_str}"),
            "kconfig_symbol": None, "implementation_state": "unimplemented",
            "depends_on": [], "profile_defaults": cu_defaults(cid_str),
            "backing_tests": [],
            "opc_reference": {
                "spec": None, "section": None,
                "cu_id": cid_str, "cu_name": row.get("name"),
                "source_url": "https://profiles.opcfoundation.org",
            },
            "source_metadata": row.get("source_metadata", {}),
            "notes": row.get("description") or f"Imported from OPC API CU {cid_str}.",
        })
        existing_ids.add(cu_id)
        cus_added += 1

    # --- Rebuild facet_containment for ALL facets ---
    # Preserve project-level friendly-name CU assignments (items whose id is
    # not of the form ``opc_cu_<numeric>``) from the existing manifest --
    # these are hand-curated mappings (e.g. ``service_read``,
    # ``opc_cu_subscription_basic``) that are not derivable from OPC API
    # relationship data. Only the ``opc_cu_<numeric>`` assignments are
    # re-derived from the OPC included_conformance_units relationships.
    existing_facet_containment = manifest.get("facet_containment") or {}
    facet_containment = {}
    for fid in sorted(facet_lowest_profile.keys(), key=int):
        facet_item_id = "opc_facet_" + fid
        if facet_item_id not in existing_ids:
            continue
        cu_list = icu.get(fid, [])
        contained = []
        for cid in cu_list:
            cu_item_id = "opc_cu_" + str(cid)
            if cu_item_id in existing_ids and cu_to_parent.get(str(cid)) == fid:
                if cu_item_id not in contained:
                    contained.append(cu_item_id)
        # Merge in preserved friendly-name CU assignments for this facet.
        # Only non-``opc_cu_<numeric>`` ids are preserved: numeric ids are
        # fully re-derived from OPC relationship data above, so a stale
        # numeric assignment is dropped when the OPC hierarchy changes.
        for cu_item_id in existing_facet_containment.get(facet_item_id, []):
            if cu_item_id.startswith("opc_cu_") and cu_item_id[7:].isdigit():
                continue  # numeric ids are authoritative from OPC data
            if cu_item_id not in contained and cu_item_id in existing_ids:
                contained.append(cu_item_id)
        if contained:
            contained.sort()
        facet_containment[facet_item_id] = contained

    # --- Update profile_defaults for items with 2025 CU mapping ---
    defaults_updated = 0
    for item in items:
        ref = item.get("opc_reference") or {}
        cid = ref.get("cu_id")
        if cid is None:
            continue
        cid_str = str(cid)
        if cid_str not in min_defaults:
            continue
        new_defaults = cu_defaults(cid_str)
        if item.get("profile_defaults") != new_defaults:
            item["profile_defaults"] = new_defaults
            defaults_updated += 1

    manifest["items"] = items
    manifest["facet_containment"] = facet_containment

    save_json("profiles/opcua-profile-manifest.yaml", manifest)

    print(f"Realignment complete:")
    print(f"  Facets added: {facets_added}")
    print(f"  CUs added: {cus_added}")
    print(f"  CUs skipped: {skipped}")
    print(f"  Profile defaults updated: {defaults_updated}")
    print(f"  Facet containment entries: {len(facet_containment)}")
    for k, v in sorted(facet_containment.items()):
        print(f"    {k}: {len(v)} CUs")

    return 0


if __name__ == "__main__":
    sys.exit(main())
