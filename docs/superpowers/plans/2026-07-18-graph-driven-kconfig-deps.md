# Graph-Driven Kconfig Dependencies — Implementation Plan (v2: live join)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use `- [ ]` checkboxes.

**Goal:** Make the committed OPC composition graph the *live* source of the Kconfig dependency structure: `generate.py` (and `validate.py`/`completion.py`) compute each item's `depends_on` and nano/micro/embedded/standard `profile_defaults` from the graph **at generation time**, joined with the manifest's "us" data (kconfig_symbol, implementation_state, capacities, tests). Change the graph → re-run `generate.py` → new Kconfig. No derived dependency fields stored in the manifest.

**Architecture:** The graph (`profiles/opcua-profile-graph.json`) owns spec *structure* (facet membership, mandatory/optional, profile composition). The manifest owns the *us* side (symbols, implementation_state, capacities, backing_tests, and hand-authored values ONLY for CUs absent from the graph, e.g. our merged `service_*` pseudo-CUs). A shared pure module `graph_deps.py` resolves, for any symbol-carrying item: `depends_on` = OR of member-facet symbols; nano/micro/embedded/standard = transitive all-mandatory reachability; `full` = `implementation_state`∈implemented/claimed/documented; graph-absent items fall back to their manifest-stored values. `generate.py`/`validate.py`/`completion.py` call this resolver in-memory; nothing is written back to the manifest.

**Tech Stack:** Python 3 stdlib; existing `scripts/profile_manifest/` toolchain; CMake+Kconfig; the committed graph snapshot.

## Reworking the in-progress 089 branch
Branch `089-graph-driven-kconfig-deps` currently has: T1 graph snapshot (`07fc8c1`, **keep**), `derive_deps.py` + tests + a manifest **rewrite** (`8cea68f/7d4b3ae/63651f7/b277f06`, **rework**). Under v2 the derivation logic is kept but must NOT write to the manifest; the manifest bake is reverted. Task 0 handles that reset.

## File Structure
- `profiles/opcua-profile-graph.json` — committed graph (kept from T1).
- `scripts/profile_manifest/graph_deps.py` — **renamed from derive_deps.py**: pure resolver functions (`load_graph`, `build_index`, `derive_depends_on`, `derive_profile_defaults`, and a new `resolve_into(manifest, graph)` that returns/attaches resolved values IN MEMORY). No `__main__` manifest-writer.
- `scripts/profile_manifest/test_graph_deps.py` — the existing derive tests (renamed) + a resolver test.
- `scripts/profile_manifest/generate.py` — call `graph_deps.resolve_into` after load; fix `_emit_default`.
- `scripts/profile_manifest/validate.py` — resolve before validating depends_on/profile_defaults.
- `scripts/profile_manifest/completion.py` — resolve before reading profile_defaults (if it does).
- `profiles/opcua-profile-manifest.yaml` — **derived fields removed** from graph-mapped items (single source of truth); graph-absent items keep hand-authored depends_on/profile_defaults.

---

## Task 0: Reset the manifest bake; rename module

**Files:** `profiles/opcua-profile-manifest.yaml`, `derive_deps.py`→`graph_deps.py`, tests.

- [ ] **Step 1** — Restore the manifest to its pre-bake state: `git checkout main -- profiles/opcua-profile-manifest.yaml` (removes the T5 derived-field rewrite; brings back hand-authored values as the pre-graph baseline).
- [ ] **Step 2** — `git mv scripts/profile_manifest/derive_deps.py scripts/profile_manifest/graph_deps.py` and `git mv scripts/profile_manifest/test_derive_deps.py scripts/profile_manifest/test_graph_deps.py`; update the `import derive_deps as d` in the test to `import graph_deps as d`; delete the `__main__` manifest-writer block from graph_deps.py (the module is now pure/importable only).
- [ ] **Step 3** — `cd scripts/profile_manifest && python3 -m pytest test_graph_deps.py -q` → the 8 existing tests still pass (they test the pure functions, unaffected by the rename).
- [ ] **Step 4** — Commit `refactor(manifest): graph_deps as a pure resolver module (no manifest write); reset bake`.

## Task 1: `resolve_into` — the in-memory join (TDD)

**Files:** `scripts/profile_manifest/graph_deps.py`, `test_graph_deps.py`

- [ ] **Step 1: Failing test** — `resolve_into(manifest, graph)` mutates each graph-mapped conformance_unit's in-memory `depends_on`/`depends_on_op`/`profile_defaults` (nano/micro/embedded/standard from graph; `full` from implementation_state∈{implemented,claimed,documented}; `custom` preserved/false) and LEAVES graph-absent items untouched. Returns the manifest.
```python
def test_resolve_into_maps_graph_cu_and_leaves_absent():
    graph, manifest = _full_fixture()   # ServerType mapped + a graph-absent 'service_x'
    graph_deps.resolve_into(manifest, graph)
    st = _item(manifest, "opc_cu_3189")
    assert st["depends_on"] == ["MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER"]
    assert st["profile_defaults"]["embedded"] is True and st["profile_defaults"]["nano"] is False
    assert st["profile_defaults"]["full"] is True     # implementation_state=claimed
    sx = _item(manifest, "service_x")                 # cu_name absent from graph
    assert sx["depends_on"] == ["HAND_AUTHORED"] and sx["profile_defaults"]["nano"] is True
```
- [ ] **Step 2: Run, fail.**
- [ ] **Step 3: Implement**
```python
_IMPLEMENTED = {"implemented", "claimed", "documented"}

def resolve_into(manifest, graph):
    idx = build_index(manifest)
    graph_cu_names = {c["name"] for n in graph["profiles"].values() for c in n.get("child_cus", [])}
    for it in items(manifest):
        if not isinstance(it, dict) or it.get("kind") != "conformance_unit":
            continue
        cu_name = (it.get("opc_reference") or {}).get("cu_name")
        if not cu_name or cu_name not in graph_cu_names or idx["by_cu_name"].get(cu_name) is not it:
            continue  # graph-absent or non-selectable duplicate -> leave manifest values
        deps, op = derive_depends_on(graph, idx, cu_name)
        it["depends_on"] = deps
        if op: it["depends_on_op"] = op
        elif "depends_on_op" in it: del it["depends_on_op"]
        pd = it.setdefault("profile_defaults", {})
        pd.update(derive_profile_defaults(graph, cu_name))          # nano/micro/embedded/standard
        pd["full"] = it.get("implementation_state") in _IMPLEMENTED  # us-side
        pd.setdefault("custom", False)
    return manifest
```
- [ ] **Step 4: Run, pass.** Commit `feat(manifest): graph_deps.resolve_into live join (in-memory)`.

## Task 2: Wire into generate.py + fix `_emit_default`

**Files:** `scripts/profile_manifest/generate.py`

- [ ] **Step 1** — In `generate.py`, import `graph_deps`; immediately after the manifest is loaded in the entry path (find where `load_manifest`/`json.load` yields the manifest for `generate_kconfig` and the other outputs), call `graph_deps.resolve_into(manifest, graph_deps.load_graph())` so every downstream reader sees graph-resolved `depends_on`/`profile_defaults`. (In-memory only — never re-serialize the manifest.)
- [ ] **Step 2** — Fix `_emit_default`: **delete** the `if op == "or" and depends_on: default y if <deps>; return` shortcut so the default ALWAYS comes from `profile_defaults` (the `_lowest_default_profile` + custom path). Multi-facet CUs now default per their graph-resolved profile membership, not "on if any facet on".
- [ ] **Step 3** — `python3 scripts/profile_manifest/generate.py` runs clean; then `grep -A4 'config MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES' Kconfig` — its `default` line must now be `default n` (optional everywhere) or a profile-cascade condition matching its (empty) mandatory membership, NOT `default y if <facet>`.
- [ ] **Step 4** — Commit `feat(manifest): generate.py joins graph live; defaults from profile_defaults only`.

## Task 3: Wire into validate.py + completion.py

**Files:** `scripts/profile_manifest/validate.py`, `completion.py`

- [ ] **Step 1** — In `validate.py`, before the depends_on/profile_defaults checks, call `graph_deps.resolve_into(manifest, graph_deps.load_graph())` so it validates the RESOLVED values (the computed facet-symbol depends_on must reference declared symbols — they will, since facets carry symbols). Confirm `--check-generated` still diffs against a `generate.py` run that also resolves (consistent).
- [ ] **Step 2** — In `completion.py`, if it reads `profile_defaults` for per-profile counts, resolve first (same call) so completion reflects the graph truth. If it only reads implementation_state, no change.
- [ ] **Step 3** — `python3 scripts/profile_manifest/validate.py --all` → OK. Commit `feat(manifest): validate/completion consume graph-resolved deps`.

## Task 4: Strip derived fields from graph-mapped manifest items

**Files:** `profiles/opcua-profile-manifest.yaml`, a one-shot `scripts/profile_manifest/strip_derived.py` (throwaway, not committed, or a documented step)

- [ ] **Step 1** — For every graph-mapped conformance_unit (cu_name in the graph, symbol-carrier), REMOVE `depends_on`, `depends_on_op`, and the nano/micro/embedded/standard keys of `profile_defaults` from the on-disk manifest (keep `full`/`custom` — `full` is us-side but computed; keep it as a hint or drop it too and let resolve compute — DECISION: drop nano/micro/embedded/standard + depends_on; KEEP `custom`, drop `full` since resolve_into computes it). Graph-absent items keep everything. Write the manifest back with the same `indent`/`ensure_ascii` convention (match Task-from-v1 finding: default ensure_ascii + restore literal `§`).
- [ ] **Step 2** — Confirm single-source-of-truth: `python3 scripts/profile_manifest/validate.py --all` → OK (validate resolves in-memory, so stripped fields are fine); `git diff` shows only field REMOVALS on mapped items.
- [ ] **Step 3** — Commit `refactor(manifest): drop graph-derived fields; graph is the single source`.

## Task 5: Regenerate + prove all-profile parity + update oracles

**Files:** generated artifacts; `tests/conformance/check_claim_map.py`; `tests/unit/test_service_header.c`

- [ ] **Step 1** — `python3 scripts/profile_manifest/generate.py && python3 scripts/profile_manifest/validate.py --all` → OK.
- [ ] **Step 2** — Per-profile enabled-CU-set: for each profile, the enabled IMPLEMENTED-CU set must equal the graph mandatory set for that profile (optionals off; `full` = all-implemented). Compute before (main) vs after via the muc_opcua_autoconf.h diff (see v1 Task 7 method). Expected drops: the optional CUs (Diagnostics, Attribute-Write, Currency, LocalTime, …) leave micro/embedded/standard; nano loses Interfaces/CoreViewsFolder/NamespaceMetadata (all optional). **This is the intended pure-graph result** — confirm it's uniform (no optional left defaulted-on anywhere).
- [ ] **Step 3** — Update the two oracles to the graph-authoritative values (they encoded the superseded "ship optionals at micro+" intent):
  - `tests/conformance/check_claim_map.py`: change `IN_SCOPE_CU_PROFILE_DEFAULTS` so the 6 affected CUs (2389/2400/2936/3192/3983 + 3147) are NOT expected default-on at micro/embedded/standard (match the graph: optional → off). Add a comment citing the pure-graph decision + this plan.
  - `tests/unit/test_service_header.c`: `test_base_services_diagnostics_symbol_matches_profile_surface` — update to reflect CU-3983 is optional (not in the mandatory micro/embedded/standard surface).
- [ ] **Step 4** — Build+test all 5 profiles: `for P in nano micro embedded standard full; do cmake -S . -B build_$P -DMUC_OPCUA_PROFILE=$P -DMUC_OPCUA_BUILD_TESTS=ON && cmake --build build_$P -j && ctest --test-dir build_$P --output-on-failure; done` → all green (incl. the updated oracles). `.bss`=0. Report `.text` deltas (micro/embedded/standard shrink; nano net; full unchanged).
- [ ] **Step 5** — Commit `chore(manifest): regenerate Kconfig from live graph join; align profile oracles`.

## Task 6: Docs + PR

- [ ] Reconciliation-log: the graph is now the live source; `generate.py`/`validate.py`/`completion.py` join it in-memory; no derived fields in the manifest; pure-graph profile model (optionals off in the four profiles, on in `full`); oracles aligned. Push; PR (merge-commit; trailers). Do NOT merge without user go-ahead.

## Self-Review
- Single source of truth: graph owns structure, manifest owns us-side; derived fields removed from mapped items; resolver is the one join, shared by generate/validate/completion. ✅
- The `_emit_default` op=="or" facet shortcut (the bug that leaked facet-based defaults) is removed. ✅
- Pure-graph decision honored: optionals default-off in nano/micro/embedded/standard, on in full; oracles updated to match. ✅
- Change-graph→regenerate works: no re-bake step; `resolve_into` runs live in generate.py. ✅
