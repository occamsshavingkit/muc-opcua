# ADR 0005: The OPC profile composition graph is the live source of Kconfig dependencies

**Status:** Accepted (feature 089-graph-driven-kconfig-deps)
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
**make that graph the single, live source of the dependency structure, and stop
hand-authoring it.**

## Decisions

### 1. The composition graph is committed as a snapshot and is authoritative

`profiles/opcua-profile-graph.json` is a committed snapshot of the full server
profile graph (266 profiles/facets, 1182 CUs), pulled by
`scripts/profile_manifest/pull_profile_graph.py`. Every parent→child edge (facet
member CU, or sub-profile) carries its `isOptional` flag. This file is the source
of truth for dependency structure; to change the structure you re-pull (or edit)
the graph and regenerate — there is no hand-authored dependency layer to keep in
sync.

### 2. `resolve_into` joins the graph into the manifest in-memory, at load time

`scripts/profile_manifest/graph_deps.py` is a pure resolver.
`resolve_into(manifest, graph)` mutates each **graph-mapped** CU (kind
`conformance_unit`, with an `opc_reference.cu_name` that appears as a child
CU in the graph) in memory:

- `depends_on` = the OR of the Kconfig symbols of the facets the CU is a member
  of; `depends_on_op` = `"or"` iff there is more than one. Membership, not
  fabricated inter-CU chains — implementation coupling is satisfied by facet
  co-membership (a facet enables all its mandatory CUs together).
- `profile_defaults` nano/micro/embedded/standard = transitive **all-mandatory
  reachability** from each profile root (nano 2266 ⊂ micro 2267 ⊂
  embedded 2268 ⊂ standard 2269). A CU defaults on for a profile iff it is a
  mandatory member of a facet the profile pulls in through an all-mandatory
  path.
- `full` = `implementation_state ∈ {implemented, claimed, documented}`. **`full`
  is ours, not an OPC profile** — it turns on every server CU we have actually
  implemented, regardless of optional status.

Graph-**absent** items (project constructs, aliases, optimizations, CUs not in the
server graph) are left untouched — their hand-authored values remain real inputs.

`resolve_into` is called immediately after manifest load in `generate.py` and
`validate.py`, so every consumer sees graph-resolved values. `completion.py` needs
no call — it reads only `implementation_state` / `satisfied_by`, not the derived
fields. The join is **in-memory only**; the manifest is never written back with
derived values.

### 3. The derived fields are not stored in the manifest

Because `resolve_into` supplies them on every run, the stored
`depends_on` / `depends_on_op` / `profile_defaults.{nano,micro,embedded,standard,full}`
on graph-mapped items were dead. They were stripped. Safety was proven by a
**byte-identical Kconfig regeneration** before and after the strip: the generated
`Kconfig`, defconfigs and capacities were unchanged (md5 identical), confirming
the stored values had no effect. The manifest now owns only us-side facts
(implementation state, satisfied_by aliasing, notes, capacities); the graph owns
structure.

### 4. The `_emit_default` facet shortcut was removed

`generate.py`'s `_emit_default` previously emitted `default y if <facet>` whenever
a CU had `depends_on_op == "or"`, leaking a facet-based default that ignored
`profile_defaults`. That made optional CUs default on whenever any facet they
belong to was enabled. It was deleted; defaults now derive solely from the
graph-resolved `profile_defaults`.

## Consequences

- **Pure-graph profile model.** Optional CUs default **off** for
  nano/micro/embedded/standard and **on** only for `full`. We build to spec;
  we do not override the OPC profiles. 20 optional-in-facet CUs that had
  been defaulted on for embedded/standard correctly dropped off: 2389, 2400,
  2446, 2447, 2476, 2711, 2936, 2969, 3127, 3147, 3186, 3192, 3198, 3545,
  3560, 3983, 4053, 4237, 5240, 5592 (each `isOptional=true` with no
  all-mandatory path from any named profile root).
- **Smaller conformant binaries.** Because those optionals no longer compile into
  the smaller profiles, `.text` shrank: micro −4597 B, embedded −3661 B, standard
  −4478 B (measured on `examples/minimal_server`); nano and full net-unchanged;
  `.bss` stays 0 on all five. Conformance and footprint improved together.
- **Oracles realigned to graph truth:**
  `tests/conformance/check_claim_map.py` (`IN_SCOPE_CU_PROFILE_DEFAULTS`),
  `tests/conformance/test_check_claim_map_profile_defaults.py`, and
  `tests/unit/test_service_header.c` (CU 3983 diagnostics now `full`-only) were
  updated to the graph-resolved defaults.
- **Change-graph → regenerate is the workflow.** No re-bake step, no
  hand-authored dependency layer. Edit or re-pull the graph, run `generate.py`,
  and Kconfig follows.
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
- **Dropped implementation prerequisites in custom builds.** `resolve_into`
  overwrites a graph-mapped CU's `depends_on` with facet membership, discarding
  any hand-authored inter-CU dependency. Most of those were fabricated *spec*
  chains (SERVERTYPE→BASE_TYPES→DATATYPES) that facet co-membership reconstructs
  — the deliberate cleanup. But a few encoded genuine *code* prerequisites: on
  `main`, `ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP` and
  `ATTRIBUTE_WRITE_INDEX_RANGE`
  declared `depends_on: [ATTRIBUTE_WRITE_VALUES]` (they build on the base
  write-values implementation). After resolution that guard is gone, so a custom
  config could select the sub-feature with `ATTRIBUTE_WRITE_VALUES=n`. Named
  profiles are unaffected (the facet enables them together). If custom-build
  build-safety matters, the fix is a dedicated implementation-prerequisite layer
  (a manifest field generate.py ANDs into `depends on`) kept distinct from the
  graph-derived spec `depends_on` — preserving "graph is the sole source of spec
  structure" while restoring real code guards. Same class as the SESSION_TIMEOUT
  item above, but potentially compile-time rather than default-convenience.
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
- **Silent empty `depends_on`.** A graph-mapped, symbol-carrying CU whose
  containing facets have no modeled symbol resolves to an empty `depends_on`
  (unconditionally selectable) rather than an error. Zero occurrences today; a
  future CU under an unmodeled facet would slip through. Consider a warning.
- **Redundant recomputation.** `derive_profile_defaults` recomputes the four
  root mandatory-sets per CU per call; memoizing them once per `resolve_into`
  would remove the 135×4 repeat traversal. Correctness is unaffected.

See ADR 0003 (profile-tier system) and
[[cmake-profile-gating-mechanism]] for the surrounding gating design.
