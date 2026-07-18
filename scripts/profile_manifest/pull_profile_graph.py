#!/usr/bin/env python3
"""Pull the OPC UA server-profile composition graph from the profiles API.

Fetches the complete composition graph (pg=171) from
profiles.opcfoundation.org: every profile/facet's child profiles/facets
(includedprofiles) and child CUs (includedconformanceunits), each edge
flagged isOptional. Authoritative source for building Kconfig dependency
structure.
"""
import json, time, urllib.request, sys

BASE = "https://profiles.opcfoundation.org/api"
OUT = "profiles/opcua-profile-graph.json"

def get(path):
    for attempt in range(3):
        try:
            req = urllib.request.Request(BASE + path, headers={"Accept": "application/json"})
            # path is always one of this module's own fixed literal endpoint
            # strings below (never attacker-controlled); BASE is a hardcoded
            # https constant. Maintainer-only fetch tool.
            with urllib.request.urlopen(req, timeout=30) as r:  # nosec B310  # nosemgrep: dynamic-urllib-use-detected, python_urlopen_rule-urllib-urlopen
                if "application/json" not in r.headers.get("Content-Type", ""):
                    return None
                return json.load(r)
        except Exception as e:
            if attempt == 2:
                print(f"  FAIL {path}: {e}", file=sys.stderr)
                return None
            time.sleep(1.0)
    return None

def result(d):
    return d.get("result", []) if isinstance(d, dict) else []


# 1. master lists
profiles = result(get("/profile/?pg=171&all=1"))          # profiles + facets
cus_master = result(get("/conformanceunit/?pg=171&all=1"))  # all CUs
print(f"profiles/facets: {len(profiles)}  cus: {len(cus_master)}")

graph = {"profiles": {}, "cu_master": {}}
for cu in cus_master:
    graph["cu_master"][cu["id"]] = {"name": cu.get("name"), "guid": cu.get("guid")}

# 2. per profile/facet: child profiles/facets + child CUs
for i, p in enumerate(profiles):
    pid = p["id"]
    inc_p = result(get(f"/profile/includedprofiles/{pid}"))
    time.sleep(0.12)
    inc_c = result(get(f"/profile/includedconformanceunits/{pid}"))
    time.sleep(0.12)
    graph["profiles"][pid] = {
        "name": p.get("name"),
        "profileUri": p.get("profileUri"),
        "child_profiles": [{"id": c["id"], "name": c.get("name"), "isOptional": c.get("isOptional")} for c in inc_p],
        "child_cus": [{"id": c["id"], "name": c.get("name"), "isOptional": c.get("isOptional")} for c in inc_c],
    }
    if (i + 1) % 25 == 0:
        print(f"  {i+1}/{len(profiles)} ...")
        json.dump(graph, open(OUT, "w"), indent=1)  # incremental save

json.dump(graph, open(OUT, "w"), indent=1)
print(f"DONE -> {OUT}  ({len(graph['profiles'])} profiles/facets, {len(graph['cu_master'])} cus)")
