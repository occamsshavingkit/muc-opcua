"""Derive manifest depends_on / profile_defaults from the OPC profile composition graph.

The composition graph (``profiles/opcua-profile-graph.json``) is the
authoritative source for Kconfig facet/CU dependency structure: every
Conformance Unit's Kconfig ``depends_on`` should be the OR of the
kconfig_symbols of the modelled facets it is a member of, and the
nano/micro/embedded/standard columns of ``profile_defaults`` should reflect
transitive all-mandatory reachability from each build profile's graph root.

This module computes those fields from the graph and rewrites them onto the
manifest in place (see :func:`apply`); ``full``/``custom`` are left
untouched -- they encode "implemented server CU" / "user override", not
graph membership.
"""

import json


def load_graph(path="profiles/opcua-profile-graph.json"):
    return json.load(open(path))


def items(manifest):
    return manifest["items"] if isinstance(manifest, dict) and "items" in manifest else manifest


def build_index(manifest):
    """Map graph ids/CU names to the manifest item that carries the kconfig_symbol."""
    by_profile_id, by_cu_name = {}, {}
    for it in items(manifest):
        if not isinstance(it, dict):
            continue
        ref = it.get("opc_reference") or {}
        sym = it.get("kconfig_symbol")
        pid = ref.get("profile_id")
        if pid and sym:
            by_profile_id[str(pid)] = it
        cu_name = ref.get("cu_name")
        if cu_name:
            # prefer the symbol-carrying item; a satisfied_by alias carries the symbol
            cur = by_cu_name.get(cu_name)
            if cur is None or (sym and not cur.get("kconfig_symbol")):
                by_cu_name[cu_name] = it
    return {"by_profile_id": by_profile_id, "by_cu_name": by_cu_name}


def facet_symbol_for_graph_id(idx, graph_id):
    it = idx["by_profile_id"].get(str(graph_id))
    return it.get("kconfig_symbol") if it else None


def selectable_item_for_cu_name(idx, cu_name):
    return idx["by_cu_name"].get(cu_name)


def derive_depends_on(graph, idx, cu_name):
    """depends_on = OR of the kconfig_symbols of the MODELLED facets that contain cu_name.

    Returns (sorted_symbols, op) where op is 'or' for >1, else None.
    """
    syms = set()
    for gid, node in graph["profiles"].items():
        for c in node.get("child_cus", []):
            if c.get("name") == cu_name:
                sym = facet_symbol_for_graph_id(idx, gid)
                if sym:
                    syms.add(sym)
    ordered = sorted(syms)
    return ordered, ("or" if len(ordered) > 1 else None)
