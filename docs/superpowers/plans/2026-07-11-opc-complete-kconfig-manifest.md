# OPC-Complete Kconfig Manifest Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a manifest-driven OPC UA profile/facet/CU configuration system that pre-populates Kconfig with implemented and unimplemented OPC items, moves capacities into generated configuration, and generates claim/test documentation from the same source.

**Architecture:** A committed manifest is the source of truth. Small Python tools load the manifest, validate it, and generate Kconfig, profile defconfigs, capacity headers, and conformance docs. Existing CMake continues to resolve Kconfig with `kconfiglib`, but legacy `-DMUC_OPCUA_*` and `-DMU_MAX_*` overrides are translated into generated Kconfig override fragments.

**Tech Stack:** CMake 3.20, Python 3 standard library, vendored `scripts/kconfig/kconfiglib.py`, YAML-compatible manifest stored as JSON-compatible YAML or JSON, C99 headers, shell profile-gating script.

## Global Constraints

- All coding and code modification tasks are delegated to `software-engineer-zai`.
- Main session owns architecture decisions, planning, validation, and test execution.
- Ground OPC profile/facet/CU data from OPC Foundation sources: `profiles.opcfoundation.org/api`, `opc-ua-reference` MCP, NodeIds.csv, or a pinned exported snapshot.
- Do not claim implementation of new OPC items just because they appear in the manifest.
- Unimplemented OPC items must appear in generated Kconfig but must be unselectable.
- Existing `-DMUC_OPCUA_*` feature overrides must keep working.
- Existing `-DMU_MAX_*` capacity overrides must keep working.
- Current profile defaults and capacity defaults must remain behavior-compatible unless a manifest entry explicitly records an intentional change.
- Generated claim/test docs include only claimed items; full roadmap docs include implemented and unimplemented items.
- Generated-file drift must be detectable by a local validation command.
- Do not commit unless explicitly requested.

---

## File Structure

- Create `profiles/opcua-profile-manifest.yaml`: canonical manifest with OPC facts, project claim state, Kconfig mapping, and capacity definitions.
- Create `profiles/opcua-profile-snapshot.json`: pinned imported OPC profile/facet/CU source data used for deterministic regeneration.
- Create `scripts/profile_manifest/__init__.py`: package marker for manifest tooling.
- Create `scripts/profile_manifest/model.py`: typed loader and validation helpers using standard-library data structures.
- Create `scripts/profile_manifest/import_opc_profiles.py`: importer/discovery tool that fetches or refreshes pinned OPC snapshot data.
- Create `scripts/profile_manifest/generate.py`: generator for Kconfig, defconfigs, capacity header, claim map, and roadmap docs.
- Create `scripts/profile_manifest/validate.py`: validation command for manifest integrity, generated-file drift, Kconfig consistency, unselectable unimplemented items, capacity defaults, and claim/test coverage.
- Modify `Kconfig`: make it generated from the manifest.
- Modify `configs/*.defconfig`: make profile defaults generated from the manifest.
- Modify `include/muc_opcua/capacities.h`: make capacity definitions generated from the manifest, preserving or retiring `MU_INTERN_*` per classification.
- Modify `CMakeLists.txt`: include generated capacity symbols in Kconfig override fragment translation and include generated Kconfig feature/capacity symbol lists.
- Modify `scripts/test_profile_gating.sh`: add assertions for capacity overrides and unavailable OPC items.
- Modify `docs/build-and-gating.md`: include generated feature/capacity/unavailable-item tables.
- Modify `tests/conformance/claim_test_map.md`: make it generated from claimed manifest items.
- Create `docs/conformance/opc-profile-roadmap.md`: generated full OPC item matrix including unimplemented items.

## Task 1: Manifest Schema And Seed From Current State

**Files:**
- Create: `profiles/opcua-profile-manifest.yaml`
- Create: `scripts/profile_manifest/__init__.py`
- Create: `scripts/profile_manifest/model.py`
- Create: `scripts/profile_manifest/validate.py`
- Test: `scripts/profile_manifest/validate.py`

**Interfaces:**
- Produces: `load_manifest(path: str) -> dict`
- Produces: `validate_manifest(manifest: dict) -> list[str]`
- Produces CLI: `python3 scripts/profile_manifest/validate.py --manifest profiles/opcua-profile-manifest.yaml --manifest-only`

- [ ] **Step 1: Ask `software-engineer-zai` to create schema loader and seed manifest**

Delegate one coding task to `software-engineer-zai` with this exact brief:

```text
Create the initial manifest schema and validator for the OPC-complete Kconfig manifest.

Files to create:
- profiles/opcua-profile-manifest.yaml
- scripts/profile_manifest/__init__.py
- scripts/profile_manifest/model.py
- scripts/profile_manifest/validate.py

Requirements:
- Use Python standard library only. Do not add dependencies.
- Because PyYAML is not guaranteed, store the manifest in JSON-compatible YAML syntax that `json.load()` can parse, or use `.yaml` extension with JSON content.
- Seed the manifest from the current repository state only. Do not invent new OPC claims.
- Include profiles: nano, micro, embedded, standard, full, custom.
- Include current Kconfig boolean symbols from /Kconfig with current dependencies and profile defaults.
- Include current capacities from include/muc_opcua/capacities.h with current defaults, public override names, current internal macro names, and initial classification.
- Include implementation states: current build symbols are `claimed` or `implemented`; unavailable OPC placeholder examples are `unimplemented`.
- Add at least one unimplemented OPC item to prove the schema supports unavailable Kconfig entries, but mark it clearly unimplemented and not claimed.
- `model.py` exposes `load_manifest(path: str) -> dict` and `validate_manifest(manifest: dict) -> list[str]`.
- `validate.py --manifest <path> --manifest-only` prints `manifest: OK` and exits 0 if valid; otherwise prints errors and exits 1.
- Validation errors must cover missing profile ids, missing symbol names for build-gated claimed items, missing implementation state, missing capacity defaults, and claimed items without tests.

Do not modify Kconfig, CMakeLists.txt, capacities.h, docs, or tests in this task.
```

- [ ] **Step 2: Run manifest-only validation**

Run:

```bash
python3 scripts/profile_manifest/validate.py --manifest profiles/opcua-profile-manifest.yaml --manifest-only
```

Expected:

```text
manifest: OK
```

- [ ] **Step 3: Inspect seed manifest for accidental new claims**

Run:

```bash
git diff -- profiles/opcua-profile-manifest.yaml scripts/profile_manifest/model.py scripts/profile_manifest/validate.py
```

Expected:

```text
The diff shows current Kconfig symbols and capacities only. Any OPC item not currently implemented is marked unimplemented and has no backing test claim.
```

## Task 2: OPC Import Snapshot

**Files:**
- Create: `profiles/opcua-profile-snapshot.json`
- Create: `scripts/profile_manifest/import_opc_profiles.py`
- Modify: `profiles/opcua-profile-manifest.yaml`
- Modify: `scripts/profile_manifest/validate.py`

**Interfaces:**
- Consumes: `load_manifest(path: str) -> dict`
- Produces CLI: `python3 scripts/profile_manifest/import_opc_profiles.py --snapshot profiles/opcua-profile-snapshot.json --manifest profiles/opcua-profile-manifest.yaml --dry-run`
- Produces CLI: `python3 scripts/profile_manifest/import_opc_profiles.py --snapshot profiles/opcua-profile-snapshot.json --manifest profiles/opcua-profile-manifest.yaml --write`

- [ ] **Step 1: Ask `software-engineer-zai` to implement OPC snapshot importer**

Delegate one coding task to `software-engineer-zai` with this exact brief:

```text
Implement the OPC profile import/snapshot tool.

Files to create or modify:
- profiles/opcua-profile-snapshot.json
- scripts/profile_manifest/import_opc_profiles.py
- profiles/opcua-profile-manifest.yaml
- scripts/profile_manifest/validate.py

Requirements:
- Use Python standard library only.
- The importer must support two modes:
  1. `--dry-run`: read the snapshot and print a summary of profiles/facets/CUs that would be merged.
  2. `--write`: merge snapshot OPC facts into the manifest while preserving existing project claim state.
- If live API discovery is implemented, it must write a deterministic snapshot file and record source URL and retrieval timestamp in the snapshot. If live API discovery is not possible from the API root, seed the snapshot from pinned, explicitly sourced data already present in repository docs/specs and include the source paths in the snapshot metadata.
- Do not promote imported items to `claimed` or `implemented` unless they already exist as claimed in the manifest.
- Every imported item must have source metadata. Use `profiles.opcfoundation.org/api` or existing grounded repository docs/specs as the source metadata for this task.
- Add validation that every OPC item has source metadata.
- Add enough server profile/facet/CU items to demonstrate the full model: at minimum NanoEmbeddedDevice2017, MicroEmbeddedDevice2017, EmbeddedUA2017, StandardUA2017, Core 2017 Server Facet, Standard DataChange Subscription 2017, Enhanced DataChange Subscription 2017, and at least five unimplemented optional facets/CUs from existing grounded docs/specs.
```

- [ ] **Step 2: Run importer dry-run**

Run:

```bash
python3 scripts/profile_manifest/import_opc_profiles.py --snapshot profiles/opcua-profile-snapshot.json --manifest profiles/opcua-profile-manifest.yaml --dry-run
```

Expected:

```text
The command exits 0 and prints counts for profiles, facets, conformance units, claimed items preserved, and unimplemented items imported.
```

- [ ] **Step 3: Run manifest validation**

Run:

```bash
python3 scripts/profile_manifest/validate.py --manifest profiles/opcua-profile-manifest.yaml --manifest-only
```

Expected:

```text
manifest: OK
```

## Task 3: Generate Kconfig And Defconfigs

**Files:**
- Create: `scripts/profile_manifest/generate.py`
- Modify: `Kconfig`
- Modify: `configs/nano.defconfig`
- Modify: `configs/micro.defconfig`
- Modify: `configs/embedded.defconfig`
- Modify: `configs/standard.defconfig`
- Modify: `configs/full.defconfig`
- Modify: `configs/custom.defconfig`
- Modify: `scripts/profile_manifest/validate.py`

**Interfaces:**
- Consumes: `profiles/opcua-profile-manifest.yaml`
- Produces CLI: `python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfigs`
- Produces validation: `python3 scripts/profile_manifest/validate.py --manifest profiles/opcua-profile-manifest.yaml --check-generated`

- [ ] **Step 1: Ask `software-engineer-zai` to implement Kconfig generation**

Delegate one coding task to `software-engineer-zai` with this exact brief:

```text
Generate Kconfig and profile defconfigs from the manifest.

Files to create or modify:
- scripts/profile_manifest/generate.py
- Kconfig
- configs/*.defconfig
- scripts/profile_manifest/validate.py

Requirements:
- Use Python standard library only.
- `generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfigs` writes Kconfig and configs/*.defconfig.
- Generated Kconfig keeps `CONFIG_=MUC_OPCUA_` compatibility: symbol names stay bare in Kconfig and generate MUC_OPCUA_* outputs.
- Generated Kconfig preserves current selectable symbols, dependencies, prompts, and profile defaults.
- Generated Kconfig includes unimplemented OPC items visibly but unselectable. Use generated helper symbols or false dependency expressions. The menu text must identify them as not implemented.
- Generated Kconfig help text includes OPC source metadata and implementation state.
- Generated defconfigs select the same profile choices as today.
- Add `validate.py --check-generated` drift detection for Kconfig and defconfigs by regenerating to temporary files and comparing bytes.
- Do not modify CMakeLists.txt in this task.
```

- [ ] **Step 2: Validate Kconfig parses**

Run:

```bash
python3 -c "import os, sys; os.environ['CONFIG_']='MUC_OPCUA_'; sys.path.insert(0, 'scripts/kconfig'); import kconfiglib; kconfiglib.Kconfig('Kconfig'); print('kconfig: OK')"
```

Expected:

```text
kconfig: OK
```

- [ ] **Step 3: Validate generated-file drift check**

Run:

```bash
python3 scripts/profile_manifest/validate.py --manifest profiles/opcua-profile-manifest.yaml --check-generated
```

Expected:

```text
generated: OK
```

- [ ] **Step 4: Run existing profile gating smoke test**

Run:

```bash
bash scripts/test_profile_gating.sh
```

Expected:

```text
21 passed, 0 failed
```

## Task 4: Generate Capacity Configuration

**Files:**
- Modify: `profiles/opcua-profile-manifest.yaml`
- Modify: `scripts/profile_manifest/generate.py`
- Modify: `include/muc_opcua/capacities.h`
- Modify: `CMakeLists.txt`
- Modify: `scripts/profile_manifest/validate.py`
- Modify: `scripts/test_profile_gating.sh`

**Interfaces:**
- Consumes: manifest capacity entries
- Produces: generated `include/muc_opcua/capacities.h`
- Preserves: `-DMU_MAX_*=n` CMake overrides

- [ ] **Step 1: Ask `software-engineer-zai` to implement capacity generation and override translation**

Delegate one coding task to `software-engineer-zai` with this exact brief:

```text
Move capacities into manifest-driven generated Kconfig and generated capacities.h.

Files to modify:
- profiles/opcua-profile-manifest.yaml
- scripts/profile_manifest/generate.py
- include/muc_opcua/capacities.h
- CMakeLists.txt
- scripts/profile_manifest/validate.py
- scripts/test_profile_gating.sh

Requirements:
- Add Kconfig int symbols for capacities from the manifest.
- Preserve current capacity defaults for nano, micro, embedded, standard, full, and custom.
- Preserve existing public CMake overrides such as -DMU_MAX_SESSIONS=32 by translating them into the Kconfig override fragment.
- Generate capacities.h from manifest data.
- Classify each current MU_INTERN_* capacity as `keep_internal` or `retire_internal` in the manifest.
- For this task, keep existing MU_INTERN_* names unless a capacity is trivially one-to-one and all consumers can be updated mechanically in the same diff. Do not perform broad renames unless tests prove behavior is unchanged.
- Add validation that capacity defaults generated by Kconfig match current expected values.
- Extend scripts/test_profile_gating.sh with at least two capacity checks: one profile-default capacity and one -DMU_MAX_* override.
```

- [ ] **Step 2: Run profile gating with capacity checks**

Run:

```bash
bash scripts/test_profile_gating.sh
```

Expected:

```text
The script reports all previous checks plus new capacity checks passing, with 0 failed.
```

- [ ] **Step 3: Confirm legacy capacity override reaches compile definitions/header**

Run:

```bash
cmake -S . -B build/capacity-override-check -DMUC_OPCUA_PROFILE=standard -DMU_MAX_SESSIONS=7 -DMUC_OPCUA_BUILD_TESTS=OFF
```

Expected:

```text
Configure succeeds. Generated config/header state resolves sessions to 7.
```

- [ ] **Step 4: Build representative target**

Run:

```bash
cmake --build build/capacity-override-check --target muc_opcua
```

Expected:

```text
Build succeeds.
```

## Task 5: Generate Claim Map And Roadmap Docs

**Files:**
- Modify: `scripts/profile_manifest/generate.py`
- Modify: `tests/conformance/claim_test_map.md`
- Modify: `docs/build-and-gating.md`
- Create: `docs/conformance/opc-profile-roadmap.md`
- Modify: `scripts/profile_manifest/validate.py`

**Interfaces:**
- Produces generated docs from manifest.
- Preserves claim/test semantics enforced by existing `test_claim_map` CTest.

- [ ] **Step 1: Ask `software-engineer-zai` to implement doc generation**

Delegate one coding task to `software-engineer-zai` with this exact brief:

```text
Generate conformance claim and roadmap documentation from the manifest.

Files to modify/create:
- scripts/profile_manifest/generate.py
- tests/conformance/claim_test_map.md
- docs/build-and-gating.md
- docs/conformance/opc-profile-roadmap.md
- scripts/profile_manifest/validate.py

Requirements:
- `tests/conformance/claim_test_map.md` is generated only from manifest items marked claimed or implemented with backing tests.
- `docs/conformance/opc-profile-roadmap.md` includes implemented, claimed, deferred, and unimplemented OPC items.
- `docs/build-and-gating.md` includes generated feature/capacity tables and explicitly explains unavailable OPC items in Kconfig.
- Validation fails if a claimed item has no backing tests for its claimed profiles.
- Preserve existing claim map row semantics so existing CTest claim-map checks continue to pass.
- Do not add profile-compliant, CTT-certified, or equivalent unsupported compliance wording.
```

- [ ] **Step 2: Run manifest validation**

Run:

```bash
python3 scripts/profile_manifest/validate.py --manifest profiles/opcua-profile-manifest.yaml --check-generated --check-claims
```

Expected:

```text
manifest: OK
```

- [ ] **Step 3: Run claim-map tests through CMake/CTest**

Run:

```bash
cmake -S . -B build/claim-map-check -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/claim-map-check --target test_claim_map
ctest --test-dir build/claim-map-check -R '^test_claim_map$' --output-on-failure
```

Expected:

```text
test_claim_map passes.
```

## Task 6: Full Validation Integration

**Files:**
- Modify: `scripts/profile_manifest/validate.py`
- Modify: `scripts/test_profile_gating.sh`
- Modify: `docs/build-and-gating.md`
- Optional modify: `.github/workflows/*.yml` only if a suitable existing validation job already owns generated-file checks.

**Interfaces:**
- Produces full validation CLI: `python3 scripts/profile_manifest/validate.py --all`
- Main session runs validation; coding modifications still delegated.

- [ ] **Step 1: Ask `software-engineer-zai` to add final validation integration**

Delegate one coding task to `software-engineer-zai` with this exact brief:

```text
Add final manifest/generator validation integration.

Files to modify:
- scripts/profile_manifest/validate.py
- scripts/test_profile_gating.sh
- docs/build-and-gating.md
- optionally .github/workflows/*.yml if there is an existing generated-file or docs validation job that clearly fits

Requirements:
- Add `python3 scripts/profile_manifest/validate.py --all` that runs manifest validation, generated drift check, Kconfig parse check, unimplemented-item availability check, capacity compatibility check, and claim/test map validation.
- Extend profile gating to assert at least one unimplemented OPC item appears in Kconfig output but cannot be selected.
- Document the regeneration and validation commands in docs/build-and-gating.md.
- If CI is modified, keep it minimal: run the validation command in an existing build/test workflow without changing merge policy.
```

- [ ] **Step 2: Run full manifest validation**

Run:

```bash
python3 scripts/profile_manifest/validate.py --all
```

Expected:

```text
manifest: OK
```

- [ ] **Step 3: Run profile gating regression**

Run:

```bash
bash scripts/test_profile_gating.sh
```

Expected:

```text
All checks pass, 0 failed.
```

- [ ] **Step 4: Run representative build and CTest profile**

Run:

```bash
cmake -S . -B build/full-manifest-check -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/full-manifest-check
ctest --test-dir build/full-manifest-check --output-on-failure
```

Expected:

```text
Configure, build, and CTest complete successfully.
```

## Self-Review Notes

- Spec coverage: Tasks cover the manifest, OPC import/snapshot, generated Kconfig/defconfigs, capacity generation and override compatibility, claim/test docs, full roadmap docs, and validation gates.
- Placeholder scan: The plan intentionally leaves implementation decisions to code tasks only where the approved spec also made them implementation-plan decisions; every task has concrete files, commands, and expected outcomes.
- Type consistency: The manifest tooling interface is stable across tasks: `load_manifest`, `validate_manifest`, `generate.py`, `import_opc_profiles.py`, and `validate.py` are introduced before use.

## Execution Mode

The user has specified the execution mode: coding and code modification go to `software-engineer-zai`; planning, architecture, validation, and testing remain in the main session.
