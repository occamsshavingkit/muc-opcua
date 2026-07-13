# OPC-complete Kconfig manifest design

Status: approved design, not yet implemented.

## Problem

The current Kconfig migration makes feature gating declarative, browseable, and
dependency-aware. It still only models the project's current feature symbols. That
is too narrow for an OPC UA stack: users need to see the OPC UA profile, facet,
and Conformance Unit universe, including items the project does not yet claim to
implement.

The next step is to make the build/configuration surface reflect the OPC standard
itself. Kconfig should show unimplemented OPC items as visible but unavailable,
so the configuration tree doubles as a conformance roadmap. As the project claim
state changes, generated Kconfig and claim documentation should change from the
same source of truth.

## Goals

- Import and track OPC UA Server Profiles, Facets, and Conformance Units from OPC
  Foundation sources.
- Pre-populate the repository's configuration model with implemented and
  unimplemented OPC items.
- Generate Kconfig so implemented/claimed items are selectable and unimplemented
  items are shown but grayed out/unselectable.
- Generate claim/test documentation only from items the project claims to
  implement.
- Move capacity defaults into the generated configuration model while preserving
  existing public override contracts.
- Reduce drift between OPC facts, build symbols, conformance claims, tests, and
  documentation.

## Non-goals

- Do not claim implementation of new OPC CUs merely because they appear in the
  manifest.
- Do not auto-infer claims from code without human/project approval.
- Do not remove legacy `-DMUC_OPCUA_*` or `-DMU_MAX_*` override support.
- Do not make optional OPC facets implemented by default unless the manifest
  explicitly marks them as claimed for a profile.

## Source of truth

Add a single checked-in manifest, for example
`profiles/opcua-profile-manifest.yaml`, as the canonical declaration for the OPC
configuration and conformance model.

The manifest contains all OPC UA Server Profiles, Facets, and Conformance Units
in scope for this project, including unimplemented items. Each item records both
OPC facts and project-specific state.

Required OPC fields include:

- stable profile, facet, or CU identifier when available
- OPC profile URI or profile DB identifier when available
- name and display name
- mandatory/optional relationship to containing profiles or facets
- source reference, such as OPC Foundation profile DB entry, OPC specification
  part/section, or pinned exported snapshot

Required project fields include:

- implementation state: `unimplemented`, `deferred`, `implemented`, or `claimed`
- owning Kconfig symbol, when the item maps to build-time gating
- dependencies or containment edges
- source files or implementation notes when useful
- backing tests for claimed items
- notes explaining intentional gaps or deferred work

The manifest is the edit point. Generated files should be marked as generated
where practical.

## Kconfig generation

Generate `/Kconfig` from the manifest.

Kconfig should show the OPC hierarchy rather than only the current project feature
set:

```text
Server Profiles
  StandardUA2017
    [*] Core 2017 Server Facet
      [*] Attribute Read
      [*] View Basic
      ...
    [ ] Historical Access Server Facet        (not implemented, unavailable)
    [ ] Complex Type Server Facet             (not implemented, unavailable)
```

Rules:

- `claimed` and `implemented` items become selectable if their dependencies are
  satisfied.
- `unimplemented` and `deferred` items are visible but unselectable.
- Unimplemented items may be blocked with generated internal symbols or
  generated dependency expressions that are always false for unavailable project
  state.
- Help text includes OPC grounding and project implementation state.
- Profile defaults come from the manifest, not hand-maintained Kconfig blocks.
- Existing `-DMUC_OPCUA_<FLAG>=ON/OFF` overrides continue to translate to Kconfig
  override fragments.

The generated Kconfig remains dependency-aware. OPC containment and project build
dependencies must be represented declaratively rather than duplicated in CMake.

## Capacity model

Move capacity defaults into the manifest and generate Kconfig `int` symbols for
capacity settings.

Each capacity entry records:

- public override name, such as `MU_MAX_SESSIONS`
- generated build/config symbol name
- current internal consumer macro, if one exists, such as
  `MU_INTERN_MAX_SESSIONS`
- profile-specific defaults
- valid range when known
- whether the value is profile-varying, invariant, or derived
- OPC minima and source references where relevant

Existing `-DMU_MAX_*=n` overrides remain supported by translating them into the
same Kconfig override mechanism used for feature symbols.

`MU_INTERN_*` is not automatically preserved as permanent API. It must earn its
keep as an abstraction layer.

Decision rule:

- Keep `MU_INTERN_*` where it represents a derived value, a normalized value, or
  a meaningful internal abstraction distinct from the public/user-facing knob.
- Retire `MU_INTERN_*` where it is only a redundant rename of one resolved
  generated value.
- Classify each capacity in the manifest and generate code from that
  classification.
- Any retirement must be mechanical, tested, and limited to capacities classified
  as redundant.

Storage calculations in `include/muc_opcua/config.h` continue to consume resolved
generated capacity values, whether those are retained `MU_INTERN_*` names or
direct generated capacity macros.

## Generated artifacts

The manifest should generate at least:

- `/Kconfig`
- `configs/*.defconfig`
- capacity header output replacing or reducing hand-maintained
  `include/muc_opcua/capacities.h`
- `tests/conformance/claim_test_map.md`
- `docs/build-and-gating.md` feature and capacity tables
- a full OPC conformance roadmap or matrix that includes unimplemented items

Generated claim/test documentation must include only claimed items. The full OPC
roadmap/matrix must include implemented and unimplemented items.

## Import and update flow

1. Import or refresh OPC facts from OPC Foundation sources or pinned exported
   snapshots.
2. Preserve project claim state across imports.
3. Update implementation state and backing tests in the manifest as work lands.
4. Regenerate Kconfig, defconfigs, capacity headers, and documentation.
5. Run validation gates to prove generated files and claim state are consistent.

The import step must never silently promote an OPC item to a project claim. Claim
state is project-owned.

## Verification gates

Generation is a build contract, not a documentation convenience.

Required checks:

- OPC import integrity: every imported profile, facet, and CU has stable source
  metadata from OPC Foundation data or a pinned exported snapshot.
- Grounding check: every claimed item has an OPC source reference.
- Kconfig consistency: generated Kconfig validates with `kconfiglib`.
- Availability check: unimplemented items are visible but cannot be selected.
- Claim/test check: every claimed CU has at least one backing test runnable in
  each claimed profile.
- Capacity compatibility: generated capacity defaults match current profile
  behavior unless a manifest change explicitly records an intentional behavior
  change.
- Legacy override check: representative `-DMUC_OPCUA_*` and `-DMU_MAX_*`
  overrides still resolve through CMake.
- Drift check: regeneration produces no diff in generated files during CI.
- Profile gating regression: `scripts/test_profile_gating.sh` continues to cover
  feature overrides, dependency cascades, capacity overrides, and unavailable OPC
  items.

At minimum, local verification for implementation work should run the profile
gating script and a representative CTest profile. GitHub Actions remains the merge
gate.

## Acceptance criteria

- The manifest contains OPC profiles, facets, and CUs in scope, including
  unimplemented ones.
- Generated Kconfig visibly includes unimplemented OPC items and prevents
  selecting them.
- Current claimed feature defaults remain behavior-compatible unless the manifest
  explicitly changes them.
- Existing `-DMUC_OPCUA_*` and `-DMU_MAX_*` override contracts still work.
- Claim/test docs are generated from the manifest and include only claimed items.
- Full roadmap/matrix docs include unimplemented OPC items.
- Validation fails on stale generated outputs, missing OPC grounding, missing
  claim tests, or selectable unimplemented items.

## Open implementation decisions

- Exact manifest file format: YAML is preferred for reviewability, but JSON is
  acceptable if generation tooling benefits from stricter parsing.
- Exact OPC import mechanism: direct profile DB/API fetch, pinned exported
  snapshot, or both.
- Exact generated file split: a single generated Kconfig file may be sufficient at
  first; later work can split generated includes by subsystem if the tree becomes
  too large.
- Whether generated capacity headers keep the current file path or move generated
  content under the build directory with a committed template.

These decisions should be settled in the implementation plan before code changes.
