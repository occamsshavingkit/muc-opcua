# ADR 0005: The OPC profile composition graph is the authoritative source for structural Kconfig dependencies

**Status:** Accepted (feature 089-graph-driven-kconfig-deps); amended 2026-08-01
**Date:** 2026-07-18

## Context

The Kconfig feature tree (`Kconfig`, defconfigs, capacities) is generated from
`profiles/opcua-profile-manifest.yaml` by `scripts/profile_manifest/generate.py`.
Two properties of each conformance unit (CU) drove the generated dependency
structure:

- `depends_on` / `depends_on_op` — which facet symbol(s) a CU requires.
- `profile_defaults` — whether the CU defaults *on* for nano / micro /
  embedded / standard / full.

These were **hand-authored**. Hand-authoring drifted from the specification in
two ways: some CUs carried fabricated inter-CU dependency chains (e.g.
`SERVERTYPE → BASE_TYPES → DATATYPES`) that model *implementation* coupling
rather than the spec's *facet membership*; and several CUs that the OPC profiles
mark **optional** were defaulted *on* for embedded/standard, silently inflating
those builds beyond what the profile requires.

The authoritative structure — which facet each CU belongs to, whether that
membership is mandatory or optional, and how facets compose into the
Nano ⊂ Micro ⊂ Embedded ⊂ Standard profile chain — is published as data by the
OPC Foundation profile REST API. We already pull it. The decision recorded here:
**make that graph the single, live source of OPC structural composition facts,
and stop hand-authoring structural dependencies — while preserving explicit
manifest fields for project-controlled policy.**

## Decisions

### 1. The composition graph is committed as a snapshot and is authoritative

`profiles/opcua-profile-graph.json` is a committed snapshot of the full server
profile graph (266 profiles/facets, 1182 CUs), pulled by
`scripts/profile_manifest/pull_profile_graph.py`. Every parent→child edge (facet
member CU, or sub-profile) carries its `isOptional` flag. This file is the source
of truth for dependency structure; to change the structure you re-pull (or edit)
the graph and regenerate — there is no hand-authored dependency layer to keep in
sync.

### 2. `resolve_into` applies a layered dependency/default model

`scripts/profile_manifest/graph_deps.py` is a pure resolver.
`resolve_into(manifest, graph)` mutates each **graph-mapped** CU (kind
`conformance_unit`, with an `opc_reference.cu_name` that appears as a child
CU in the graph) in memory by applying these layers in order:

1. **Structural `depends_on`** — derived from the graph: the OR of the Kconfig
   symbols of the modelled facets that contain the CU; `depends_on_op` = `"or"`
   iff there is more than one. Membership, not fabricated inter-CU chains.
2. **Named-profile defaults** (nano/micro/embedded/standard) — derived from the
   graph: transitive **all-mandatory reachability** from each profile root
   (nano 2266 ⊂ micro 2267 ⊂ embedded 2268 ⊂ standard 2269). A CU defaults on
   for a profile iff it is a mandatory member of a facet the profile pulls in
   through an all-mandatory path.
3. **`full`** — derived from `implementation_state ∈ {implemented, claimed,
   documented}`. **Ours, not an OPC profile.**
4. **`custom`** — defaulted to `false` when absent.
5. **Validated `project_profile_defaults`** — an additive final layer.
   When a CU carries `project_profile_defaults: {<profile>: true}` entries
   (validated as booleans by `model.py`), those profiles are set `true` in
   `profile_defaults` regardless of graph reachability. This is the mechanism
   for deliberate policy defaults that the graph does not mandate.

Graph-**absent** items (no `cu_name`, or a `cu_name` the graph doesn't model)
are left untouched — their hand-authored values are the only data we have.

**`semantic_depends_on`** is a manifest-owned field (validated by `model.py`
as containing only known Kconfig symbols). It encodes hard implementation or
semantic prerequisites that the graph does not express — e.g. a CU that builds
on another CU's code regardless of facet co-membership. `generate.py`
concatenates `semantic_depends_on` with the structural `depends_on` as a
combined AND requirement in the emitted Kconfig `depends on` expression
(no deduplication). `generate_build_docs_section` separately deduplicates the
union of both lists via `dict.fromkeys` for display in the build docs table.

`resolve_into` is called immediately after manifest load in `generate.py` and
`validate.py`. The join is **in-memory only**; the manifest is never written
back with derived values. `completion.py` reads only `implementation_state` /
`satisfied_by`, not the derived fields.

Implementation files: `scripts/profile_manifest/graph_deps.py`,
`model.py` (validation of `project_profile_defaults` and `semantic_depends_on`),
`generate.py` (Kconfig emission combining structural + semantic deps).

### 3. Stored `depends_on` / `profile_defaults` on graph-mapped items are non-authoritative

Graph-mapped items may carry stored `depends_on` / `depends_on_op` /
`profile_defaults` entries in the manifest (e.g. placeholder values or
pre-graph snapshots). These stored fields are non-authoritative: `resolve_into`
overwrites them in memory on every run. Safety was proven by a **byte-identical
Kconfig regeneration** before and after stripping these fields from the
manifest: the generated `Kconfig`, defconfigs and capacities were unchanged
(md5 identical), confirming they had no effect at generation time.

In contrast, **`semantic_depends_on`** and **`project_profile_defaults`**
remain stored authoritative project inputs — they are explicit manifest fields
that `resolve_into` never mutates. `semantic_depends_on` flows into Kconfig
via `generate.py`'s AND concatenation; `project_profile_defaults` is applied
as an additive final layer by `resolve_into`. These fields carry project policy
that the graph alone cannot express.

### 4. The `_emit_default` facet shortcut was removed

`generate.py`'s `_emit_default` previously emitted `default y if <facet>` whenever
a CU had `depends_on_op == "or"`, leaking a facet-based default that ignored
`profile_defaults`. That made optional CUs default on whenever any facet they
belong to was enabled. It was deleted; defaults now derive solely from the
graph-resolved `profile_defaults`.

## Consequences

- **Layered dependency/default model.** The graph is authoritative for OPC
  composition facts, but not absolute: (a) structural `depends_on` and
  named-profile defaults come from graph reachability; (b) `semantic_depends_on`
  records manifest-owned implementation/semantic prerequisites that the graph
  does not encode; (c) `project_profile_defaults` enables deliberate policy
  defaults beyond graph reachability; (d) `full` is derived from
  `implementation_state`; (e) `custom` defaults to false. `generate.py`
  concatenates structural `depends_on` and `semantic_depends_on` as a combined
  AND `depends on` expression (no dedup); the build docs table deduplicates
  the union for display.

- **Intentional exception: Enhanced DataChange Subscription CUs in standard.**
  The graph root standard=2269 has no mandatory path to facet 1627 (Enhanced
  DataChange Subscription 2017 Server Facet) in the committed snapshot. Facet
  1627 contains four mandatory CUs — Monitor Items 500 (cu_id 5242), Monitor
  MinQueueSize_05 (5250), Subscription Minimum 05 (5248), Subscription Publish
  Min 10 (5249) — and child facet 1324 (Standard DataChange Subscription 2022).
  Those four CUs are claimed with `semantic_depends_on:
  [MUC_OPCUA_CU_SUBSCRIPTION_STANDARD]` and `project_profile_defaults:
  {standard: true}`. Graph resolution would leave their `standard` default
  false (no mandatory reachability); `project_profile_defaults` sets it true.
  Their `full` default is derived true from `implementation_state: claimed`.
  This models the project's deliberate "standard ≥ Facet 1324 + Enhanced
  DataChange capacity" policy without claiming graph authority for it.

- **Smaller conformant binaries.** Because optional-in-facet CUs no longer
  compile into the smaller profiles (only mandatory-reachable CUs default on),
  `.text` shrank: micro −4597 B, embedded −3661 B, standard −4478 B (measured
  on `examples/minimal_server`); nano and full net-unchanged; `.bss` stays 0 on
  all five. The four Enhanced DataChange CUs were not part of the 20-CU drop
  set because their `project_profile_defaults` keeps them on for standard.
- **Oracles realigned to graph truth:**
  `tests/conformance/check_claim_map.py` (`IN_SCOPE_CU_PROFILE_DEFAULTS`),
  `tests/conformance/test_check_claim_map_profile_defaults.py`, and
  `tests/unit/test_service_header.c` (CU 3983 diagnostics now `full`-only) were
  updated to the graph-resolved defaults.
- **Change-graph → regenerate is the workflow.** No re-bake step, no
  hand-authored structural dependency layer. Edit or re-pull the graph, run
  `generate.py`, and Kconfig follows. Graph changes do not replace reviewing
  explicit manifest policy fields (`semantic_depends_on`,
  `project_profile_defaults`, `implementation_state`), which remain the
  project's deliberate overrides and must be validated after any graph update.
- All five profiles build clean and pass their suites (nano 102, micro 122,
  embedded 123, standard 123, full 138); `validate.py --all` reports
  `manifest: OK`.

## Known follow-ups (not addressed here)

- **Dead nano-surface assertion (pre-existing).**
  `tests/unit/test_profile_surface.c` gates
  `test_nano_default_service_surface_does_not_claim_optional_cus` on
  `MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER`, a macro that is never
  emitted as a compiler define — only the short `MUC_OPCUA_PROFILE_<UPPER>`
  markers are. The test always short-circuits to a pass and never asserts. It
  predates this feature; it should be fixed so the assertion actually runs.
- **`MUC_OPCUA_CU_SESSION_TIMEOUT` custom-build default changed.** This item is
  graph-absent but hand-authored with `depends_on_op: or`. Removing the global
  `_emit_default` OR-shortcut changed its `custom`-build default: it no longer
  auto-enables when a user manually turns on one of its enablers
  (`MULTIPLE_CONNECTIONS` / `MULTI_CHUNK`). For all named profiles the output is
  unchanged. If the old custom-build convenience is wanted, model it explicitly
  in `profile_defaults` rather than via a facet shortcut.
- **Dropped implementation prerequisites in custom builds — RESOLVED.**
  `semantic_depends_on` (see Decision 2) provides the implementation-prerequisite
  layer that this follow-up requested. The four Enhanced DataChange CUs (5242,
  5250, 5248, 5249) carry `semantic_depends_on:
  [MUC_OPCUA_CU_SUBSCRIPTION_STANDARD]`, which `generate.py` ANDs into their
  generated `depends on`. The field is manifest-owned, validated by `model.py`
  against known Kconfig symbols. See `test_manifest_integrity.py` and
  `test_generate_kconfig.py` for validation coverage.
- **Multi-facet CUs are emitted under a single facet's menu.** For a CU that
  belongs to more than one facet, `resolve_into` yields `depends_on_op == "or"`,
  but `generate.py` still nests the CU inside one canonical facet's `if <facet>`
  block. So a custom config enabling only the *other* facet won't surface the CU
  even though its `depends on` lists both. Named profiles bundle the facets, so
  this only affects hand-rolled custom configs; it is pre-existing menu-nesting
  behavior, not introduced by the graph join, but the OR now makes the mismatch
  visible.
- **`isOptional: null` treated as mandatory** (`graph_deps.py`). The committed
  graph contains no nulls, so this is defensive only; consider asserting no-nulls
  at load, or treating null as optional (the safer direction — it turns CUs off,
  not on).
- **Known empty `depends_on` case: Enhanced DataChange CUs under orphaned facet 1627.**
  Facet 1627 has no parent in the committed graph and carries no Kconfig symbol
  in the manifest, so its four mandatory CUs resolve to an empty structural
  `depends_on` (unconditionally selectable). The original "zero occurrences
  today" observation has changed to one deliberate case. These CUs are gated
  by `semantic_depends_on: [MUC_OPCUA_CU_SUBSCRIPTION_STANDARD]` and
  enabled on `standard` via `project_profile_defaults`. Consider adding a
  warning when a graph-mapped CU has no modelled parent facets. Test files:
  `scripts/profile_manifest/test_graph_deps.py`,
  `scripts/test_profile_gating.sh`.
- **Redundant recomputation.** `derive_profile_defaults` recomputes the four
  root mandatory-sets per CU per call; memoizing them once per `resolve_into`
  would remove the 135×4 repeat traversal. Correctness is unaffected.

See ADR 0003 (profile-tier system) and
[[cmake-profile-gating-mechanism]] for the surrounding gating design.
