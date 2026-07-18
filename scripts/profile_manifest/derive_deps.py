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


_ROOTS = {"nano": "2266", "micro": "2267", "embedded": "2268", "standard": "2269"}


def _mandatory_cu_names(graph, root_id):
    """CU names reachable from root via all-mandatory profile edges AND a mandatory CU edge."""
    seen_profiles, mandatory_cus = set(), set()
    stack = [str(root_id)]
    while stack:
        pid = stack.pop()
        if pid in seen_profiles:
            continue
        seen_profiles.add(pid)
        node = graph["profiles"].get(pid)
        if not node:
            continue
        for c in node.get("child_cus", []):
            if not c.get("isOptional"):
                mandatory_cus.add(c.get("name"))
        for p in node.get("child_profiles", []):
            if not p.get("isOptional"):
                stack.append(str(p.get("id")))
    return mandatory_cus


def derive_profile_defaults(graph, cu_name):
    return {prof: (cu_name in _mandatory_cu_names(graph, rid)) for prof, rid in _ROOTS.items()}


def apply(manifest, graph):
    idx = build_index(manifest)
    graph_cu_names = {
        c["name"] for node in graph["profiles"].values() for c in node.get("child_cus", [])
    }
    for it in items(manifest):
        if not isinstance(it, dict) or it.get("kind") != "conformance_unit":
            continue
        cu_name = (it.get("opc_reference") or {}).get("cu_name")
        # graph-absent -> we have no authoritative data; preserve hand-authored values
        if not cu_name or cu_name not in graph_cu_names:
            continue
        # only rewrite the symbol-carrying (selectable) item for this CU
        if idx["by_cu_name"].get(cu_name) is not it:
            continue
        deps, op = derive_depends_on(graph, idx, cu_name)
        it["depends_on"] = deps
        if op:
            it["depends_on_op"] = op
        elif "depends_on_op" in it:
            del it["depends_on_op"]
        pd = it.setdefault("profile_defaults", {})
        pd.update(derive_profile_defaults(graph, cu_name))  # nano/micro/embedded/standard
        pd.setdefault("full", False)
        pd.setdefault("custom", False)
    return manifest


if __name__ == "__main__":
    MAN = "profiles/opcua-profile-manifest.yaml"
    manifest = json.load(open(MAN))
    apply(manifest, load_graph())
    # The manifest's existing convention is ensure_ascii=True (en-dashes,
    # curly quotes, etc. are all \uXXXX-escaped) with exactly one outlier:
    # three pre-existing literal section-sign (§, U+00A7) characters in
    # unrelated notes fields. Using ensure_ascii=False globally would match
    # those three but flip ~26 other already-escaped characters elsewhere
    # in the file to literal unicode -- unrelated byte-churn outside our
    # field scope. Instead, dump with the default (matching the dominant
    # convention) and undo just the one escape that regresses a literal
    # the file already had.
    text = json.dumps(manifest, indent=2)
    text = text.replace("\\u00a7", "§")
    with open(MAN, "w") as fh:
        fh.write(text)
        fh.write("\n")
    print("derive_deps: applied")
