#!/usr/bin/env python3
"""Fetch the full OPC UA Server conformance surface from the profile REST API.

Writes ``profiles/opcua-server-conformance.json`` — the authoritative catalog of
every Server profile/facet and its conformance-unit membership (with the OPC
``isOptional`` flag). This is the *denominator* source for the conformance
completion report (spec 073); it is deliberately separate from the build
manifest so cataloguing the full standard does not bloat Kconfig with hundreds
of NOT-IMPLEMENTED entries.

The report joins this catalog (membership + optionality) with the build manifest
(implementation status). Re-run this script to refresh the catalog; the
committed JSON is what the generator and drift check consume.

Usage:
    python3 scripts/profile_manifest/fetch_server_catalog.py            # fetch + write
    python3 scripts/profile_manifest/fetch_server_catalog.py --check    # fail if stale
"""

from __future__ import annotations

import argparse
import json
import os
import time
import urllib.request

_API = "https://profiles.opcfoundation.org/api/profile"
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(os.path.dirname(_HERE))
_OUT = os.path.join(_REPO, "profiles", "opcua-server-conformance.json")


def _get(url: str, retries: int = 3) -> dict:
    # Only ever fetch the fixed https REST endpoint; reject any other scheme so a
    # crafted value can never reach urllib's file:// handler (Bandit B310).
    if not url.startswith("https://"):
        raise ValueError(f"refusing non-https catalog URL: {url}")
    last: Exception | None = None
    for _ in range(retries):
        try:
            # nosec B310 -- scheme restricted to https above; dev-only catalog fetch.
            with urllib.request.urlopen(url, timeout=60) as resp:  # nosemgrep: dynamic-urllib-use-detected
                return json.loads(resp.read().decode("utf-8"))
        except Exception as exc:  # noqa: BLE001 - network best-effort
            last = exc
            time.sleep(0.5)
    raise RuntimeError(f"GET failed after {retries} tries: {url}: {last}")


def build_catalog() -> dict:
    profiles = _get(f"{_API}/?pg=171&all=1")["result"]
    server = [p for p in profiles if "/Server/" in (p.get("profileUri") or "")]

    profile_rows = []
    optionality: dict[str, bool] = {}
    for p in sorted(server, key=lambda x: x.get("id") or 0):
        pid = p.get("id")
        members = _get(f"{_API}/includedconformanceunits/{pid}/?pg=171&all=1").get("result", [])
        cu_ids = []
        for c in members:
            cid = str(c["id"])
            cu_ids.append(cid)
            # OPC optionality is per-membership; the catalog is deterministic so
            # required (False) wins if any membership marks the CU required.
            if cid not in optionality or c.get("isOptional") is False:
                optionality[cid] = bool(c.get("isOptional"))
        profile_rows.append({
            "opc_id": str(pid),
            "name": p.get("name"),
            "uri": p.get("profileUri"),
            "conformance_units": sorted(set(cu_ids), key=int),
        })

    # CU id -> display name (from the flat catalog).
    names = {str(c["id"]): c.get("name") for c in _get(f"{_API}/../conformanceunit/?pg=171&all=1")["result"]}

    return {
        "source": "https://profiles.opcfoundation.org/api/ (pg=171)",
        "note": "Server profiles/facets and CU membership with OPC isOptional. "
                "Regenerate with scripts/profile_manifest/fetch_server_catalog.py.",
        "server_profile_count": len(profile_rows),
        "conformance_unit_optional": optionality,
        "conformance_unit_names": {k: names.get(k) for k in sorted(optionality, key=int)},
        "profiles": profile_rows,
    }


def _serialize(catalog: dict) -> str:
    return json.dumps(catalog, indent=1, sort_keys=True) + "\n"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="Fetch and compare against the committed catalog; fail on drift.")
    args = ap.parse_args(argv)

    catalog = build_catalog()
    payload = _serialize(catalog)
    if args.check:
        try:
            current = open(_OUT, encoding="utf-8").read()
        except FileNotFoundError:
            print("server catalog missing:", _OUT)
            return 1
        if current != payload:
            print("server catalog drift: re-run fetch_server_catalog.py")
            return 1
        print("server catalog: up to date")
        return 0
    with open(_OUT, "w", encoding="utf-8") as fh:
        fh.write(payload)
    print(f"wrote {_OUT}: {catalog['server_profile_count']} Server profiles, "
          f"{len(catalog['conformance_unit_optional'])} distinct CUs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
