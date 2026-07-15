#!/usr/bin/env python3
"""CU-based conformance completion computation and reporting (spec 073).

Computes, per profile (and facet), how many of its OPC-defined conformance
units are implemented, split into required vs optional, against the OPC
transitive CU closure as the denominator.

Status resolution rules (spec 073):
  * A CU counts implemented if its own implementation_state is claimed/implemented,
    OR its `satisfied_by` link points at an entry that is.
  * required/optional split comes from `cu_optional`.
  * A CU with `not_applicable[profile]` set is excluded from that profile's
    denominator and reported separately (grounded reason required).
  * Denominator = relationships.transitive_cu_closure[profile.opc_id].

Reconciliation links and N/A flags are read from the manifest CU entries
(`satisfied_by`, `not_applicable`). See spec 073 FR-001/FR-011.
"""

from __future__ import annotations

import json
import pathlib
from typing import Any

_IMPLEMENTED = {"claimed", "implemented"}


def _index_by_cu_id(items: list[dict]) -> dict[str, dict]:
    """Map OPC numeric cu_id -> the representative manifest CU entry.

    Several cu_ids appear on more than one entry (a canonical `opc_cu_<cu_id>`
    placeholder plus an implementation entry that carries the same cu_id). The
    representative prefers the canonical `opc_cu_<cu_id>` entry, since it holds
    the authoritative `cu_optional`, `not_applicable`, and OPC display name.
    """
    out: dict[str, dict] = {}
    for it in items:
        if it.get("kind") != "conformance_unit":
            continue
        cid = it.get("opc_reference", {}).get("cu_id")
        if cid is None:
            continue
        cid = str(cid)
        canonical = it.get("id") == f"opc_cu_{cid}"
        if cid not in out or canonical:
            out[cid] = it
    return out


def _entries_by_cu_id(items: list[dict]) -> dict[str, list[dict]]:
    """All CU entries sharing each OPC numeric cu_id (for aggregate status)."""
    out: dict[str, list[dict]] = {}
    for it in items:
        if it.get("kind") != "conformance_unit":
            continue
        cid = it.get("opc_reference", {}).get("cu_id")
        if cid is not None:
            out.setdefault(str(cid), []).append(it)
    return out


def _index_by_id(items: list[dict]) -> dict[str, dict]:
    return {it["id"]: it for it in items if it.get("kind") == "conformance_unit"}


def _cu_id_implemented(cid: str, group: list[dict], by_id: dict[str, dict]) -> bool:
    """Report whether a cu_id resolves to an implemented entry.

    True if any entry sharing the cu_id is implemented, or the entry's grounded
    `satisfied_by` link resolves to an implemented entry.
    """
    for entry in group:
        if entry.get("implementation_state") in _IMPLEMENTED:
            return True
        sat = entry.get("satisfied_by")
        if sat:
            target = by_id.get(sat)
            if target and target.get("implementation_state") in _IMPLEMENTED:
                return True
    return False


def compute_profile_completion(manifest: dict, snapshot: dict, profile: dict) -> dict[str, Any]:
    """Return completion detail for one profile/facet given its opc_id.

    `profile` must carry `opc_id`. Denominator is the transitive CU closure.
    """
    items = manifest["items"]
    by_cu_id = _index_by_cu_id(items)
    groups = _entries_by_cu_id(items)
    by_id = _index_by_id(items)
    profile_key = profile.get("key")

    opc_id = str(profile["opc_id"])
    rel = profile.get("rel", "transitive_cu_closure")
    closure = snapshot["relationships"][rel].get(opc_id, [])
    closure = [str(x) for x in closure]

    required_total = required_impl = 0
    optional_total = optional_impl = 0
    required_gaps: list[dict] = []
    optional_gaps: list[dict] = []
    required_items: list[dict] = []
    optional_items: list[dict] = []
    not_applicable: list[dict] = []
    missing: list[str] = []

    for cid in closure:
        entry = by_cu_id.get(cid)
        if entry is None:
            missing.append(cid)
            continue
        na = entry.get("not_applicable") or {}
        if profile_key is not None and profile_key in na:
            not_applicable.append({"id": entry["id"], "reason": na[profile_key],
                                    "name": entry.get("opc_display_name")})
            continue
        implemented = _cu_id_implemented(cid, groups.get(cid, [entry]), by_id)
        row = {"id": entry["id"], "cu_id": cid, "name": entry.get("opc_display_name", ""),
               "implemented": implemented}
        if entry.get("cu_optional") is True:
            optional_total += 1
            optional_items.append(row)
            if implemented:
                optional_impl += 1
            else:
                optional_gaps.append(entry)
        else:
            required_total += 1
            required_items.append(row)
            if implemented:
                required_impl += 1
            else:
                required_gaps.append(entry)

    return {
        "opc_id": opc_id,
        "required_implemented": required_impl,
        "required_total": required_total,
        "optional_implemented": optional_impl,
        "optional_total": optional_total,
        "required_gaps": required_gaps,
        "optional_gaps": optional_gaps,
        "required_items": required_items,
        "optional_items": optional_items,
        "not_applicable": not_applicable,
        "missing_from_manifest": missing,
    }


# --------------------------------------------------------------------------- #
# Report rendering / CLI
# --------------------------------------------------------------------------- #

_REPO = pathlib.Path(__file__).resolve().parents[2]

# Reconciliation (`satisfied_by`, `not_applicable`) and CU status now live on the
# manifest CU entries themselves (spec 073 FR-001); status resolves through
# shared-cu_id aggregation plus explicit `satisfied_by` links — no overlay here.


def _load_yaml(path: pathlib.Path) -> dict:
    import yaml  # local import so the pure-computation API has no hard dep
    return yaml.safe_load(path.read_text())


# --------------------------------------------------------------------------- #
# Full Server surface (spec 073, extended): all 90 Server profiles/facets from
# the OPC REST API catalog (profiles/opcua-server-conformance.json), joined with
# the build manifest for implementation status.
# --------------------------------------------------------------------------- #

def compute_catalog_completion(manifest: dict, cu_ids: list[str], optional_by_cu_id: dict[str, bool],
                               profile_key: str | None = None) -> dict:
    """Completion for a membership list, optionality from the catalog map."""
    by_cu_id = _index_by_cu_id(manifest["items"])
    groups = _entries_by_cu_id(manifest["items"])
    by_id = _index_by_id(manifest["items"])
    req_t = req_i = opt_t = opt_i = na_n = 0
    for cid in cu_ids:
        cid = str(cid)
        entry = by_cu_id.get(cid)
        if entry is not None and profile_key and profile_key in (entry.get("not_applicable") or {}):
            na_n += 1
            continue
        implemented = _cu_id_implemented(cid, groups.get(cid, []), by_id) if entry is not None else False
        if optional_by_cu_id.get(cid) is True:
            opt_t += 1
            opt_i += 1 if implemented else 0
        else:
            req_t += 1
            req_i += 1 if implemented else 0
    return {"required_implemented": req_i, "required_total": req_t,
            "optional_implemented": opt_i, "optional_total": opt_t, "not_applicable": na_n}


def _render_server_surface(manifest: dict, catalog: dict) -> list[str]:
    opt_map = catalog["conformance_unit_optional"]
    profiles = catalog["profiles"]
    # Overall: distinct CUs across all Server profiles.
    all_ids = sorted({c for p in profiles for c in p["conformance_units"]}, key=int)
    overall = compute_catalog_completion(manifest, all_ids, opt_map)
    rows = ["## Full Server Surface (all 90 OPC Server profiles/facets)", "",
            "Denominator is the full OPC Server conformance surface from the REST API",
            "catalog (`profiles/opcua-server-conformance.json`); status is codebase",
            "capability from the build manifest. Coarse feature-level CUs that are not",
            "yet reconciled to individual OPC CU ids count as not-implemented — see the",
            "reconciliation note below.", "",
            f"- Distinct Server CUs: **{len(all_ids)}** (required {overall['required_total']}, optional {overall['optional_total']})",
            f"- Reconciled as implemented: **required {overall['required_implemented']}/{overall['required_total']}**, **optional {overall['optional_implemented']}/{overall['optional_total']}**",
            "",
            "### Per Server profile / facet", "",
            "| OPC id | Profile / Facet | Required | Optional |",
            "| --- | --- | --- | --- |"]
    for p in sorted(profiles, key=lambda x: int(x["opc_id"])):
        r = compute_catalog_completion(manifest, p["conformance_units"], opt_map)
        name = (p.get("name") or p["opc_id"]).strip()
        rows.append(f"| {p['opc_id']} | {name} | {r['required_implemented']}/{r['required_total']} "
                    f"| {r['optional_implemented']}/{r['optional_total']} |")
    rows.append("")
    rows.append("> Reconciliation status: of the full Server surface, only CUs linked to a")
    rows.append("> build-manifest entry (directly or via `satisfied_by`) count as implemented.")
    rows.append("> Our feature-level implementations (PubSub, Alarms, History, Methods,")
    rows.append("> Aggregates, Redundancy, …) are tracked as coarse CUs that do not yet map")
    rows.append("> 1:1 to the granular OPC CU ids; reconciling them is tracked, ongoing work.")
    rows.append("")
    return rows


def render_report(manifest: dict, snapshot: dict, catalog: dict | None = None) -> str:
    lines = ["<!-- markdownlint-disable MD013 -->", "",
             "# Conformance Completion", "",
             "Generated from `profiles/opcua-profile-manifest.yaml`, the OPC transitive",
             "CU closure in `profiles/opcua-profile-normalized-snapshot.json`, and the",
             "full Server catalog in `profiles/opcua-server-conformance.json`.",
             "Do not edit by hand. See spec 073.", ""]
    if catalog is not None:
        lines.extend(_render_server_surface(manifest, catalog))
    lines.append("## Base Server Profiles (detailed)")
    lines.append("")
    profiles = manifest.get("profiles", {})
    order = ["nano", "micro", "embedded", "standard", "full"]
    for key in order:
        p = profiles.get(key)
        if not p or not p.get("opc_id"):
            continue
        prof = {"key": key, "opc_id": p["opc_id"]}
        r = compute_profile_completion(manifest, snapshot, prof)
        disp = (p.get("opc_display_name") or key).strip()
        lines.append(f"## Profile {disp}")
        lines.append("")
        lines.append(f"- Required CUs implemented: **{r['required_implemented']}/{r['required_total']}**")
        lines.append(f"- Optional CUs implemented: **{r['optional_implemented']}/{r['optional_total']}**")
        if r["not_applicable"]:
            lines.append(f"- Not applicable (grounded): {len(r['not_applicable'])}")
        if r["missing_from_manifest"]:
            lines.append(f"- Closure CUs not yet in manifest: {len(r['missing_from_manifest'])} "
                         f"({', '.join(r['missing_from_manifest'])})")
        lines.append("")
        lines.extend(_render_cu_list("Required CUs", r["required_items"]))
        lines.extend(_render_cu_list("Optional CUs", r["optional_items"]))
        if r["not_applicable"]:
            lines.append("Not applicable (grounded):")
            lines.append("")
            for na in r["not_applicable"]:
                lines.append(f"- `{na['id']}` {na.get('name','')} — {na['reason']}")
            lines.append("")

    lines.extend(_render_facets(manifest, snapshot))
    return "\n".join(lines).rstrip() + "\n"


def _render_cu_list(title: str, items: list[dict]) -> list[str]:
    if not items:
        return []
    out = [f"{title}:", ""]
    for c in sorted(items, key=lambda x: (not x["implemented"], x["id"])):
        mark = "x" if c["implemented"] else " "
        out.append(f"- [{mark}] `{c['id']}` {c.get('name','')}")
    out.append("")
    return out


def _facet_opc_id(item: dict) -> str | None:
    """Best-effort OPC numeric id for a facet manifest entry."""
    ref = item.get("opc_reference", {}).get("cu_id")
    if ref is not None:
        return str(ref)
    fid = item.get("id", "")
    tail = fid.rsplit("_", 1)[-1]
    return tail if tail.isdigit() else None


def _render_facets(manifest: dict, snapshot: dict) -> list[str]:
    inc = snapshot["relationships"]["included_conformance_units"]
    facets = [it for it in manifest["items"] if it.get("kind") == "facet"]
    rows: list[str] = ["## Facets", "",
                       "Facet CU membership uses direct `included_conformance_units`",
                       "(sub-facet CUs are counted where the snapshot expands them).", ""]
    any_facet = False
    for f in sorted(facets, key=lambda x: x.get("id", "")):
        oid = _facet_opc_id(f)
        if oid is None or oid not in inc:
            continue
        any_facet = True
        r = compute_profile_completion(manifest, snapshot,
                                       {"key": None, "opc_id": oid, "rel": "included_conformance_units"})
        disp = (f.get("opc_display_name") or f.get("id") or "").strip()
        req = f"{r['required_implemented']}/{r['required_total']}"
        opt = f"{r['optional_implemented']}/{r['optional_total']}"
        extra = ""
        if r["missing_from_manifest"]:
            extra = f" — {len(r['missing_from_manifest'])} closure CU(s) not in manifest"
        rows.append(f"- **{disp}** (`{f.get('id')}`): required {req}, optional {opt}{extra}")
    if not any_facet:
        rows.append("_No facet CU membership available in the snapshot._")
    rows.append("")
    return rows


def load_server_catalog() -> dict | None:
    path = _REPO / "profiles/opcua-server-conformance.json"
    if path.exists():
        return json.loads(path.read_text())
    return None


def main() -> int:
    manifest = _load_yaml(_REPO / "profiles/opcua-profile-manifest.yaml")
    snapshot = json.loads((_REPO / "profiles/opcua-profile-normalized-snapshot.json").read_text())
    catalog = load_server_catalog()
    out = _REPO / "docs/conformance/completion.md"
    out.write_text(render_report(manifest, snapshot, catalog))
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
