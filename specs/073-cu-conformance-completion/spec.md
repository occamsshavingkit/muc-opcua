<!-- markdownlint-disable MD013 -->

# Feature Specification: CU-Based Conformance Completion Reporting

**Feature Branch**: `073-cu-conformance-completion`  
**Created**: 2026-07-15  
**Status**: Draft  
**Input**: Maintainer request for `docs/conformance` to report per-profile and per-facet CU completion as counts — e.g. "Profile Nano 2025: 7/7 required and 8/13 optional CUs implemented" — instead of the current prose/roadmap format.

## Context and Grounding

`docs/conformance/` today is a mix of per-feature prose files (`data-access.md`, `method-server.md`, …), per-profile prose (`profile-nano.md`, …), and a generated `opc-profile-roadmap.md` that lists every manifest item in a flat table. None of these answer the question a maintainer or integrator actually asks: **for a given profile, how many of its required and optional conformance units are implemented?**

Producing that report accurately is blocked by a data-model problem in `profiles/opcua-profile-manifest.yaml`. The manifest carries CUs in **two overlapping representations**:

- **114 canonical OPC CUs** with ids `opc_cu_<digits>`, each carrying `source_metadata`, an `opc_reference.cu_id`, and a per-CU `cu_optional` flag (70 optional / 44 required). These are the OPC-grounded conformance units.
- **43 internal aliases** (`service_read`, `opc_cu_subscription_basic`, `opc_cu_session_base`, `opc_monitor_items_500`, …) that carry the actual `kconfig_symbol`, `implementation_state`, and `backing_tests` for what the codebase implements.

The two representations are only partially cross-linked, and inconsistently:

- Some aliases point at their canonical CU via `opc_reference.cu_id` (`opc_cu_session_base` → 3175, `opc_cu_server_capabilities_2` → 3912).
- Some point at a **different-vintage** canonical than the profile membership uses (`service_read` → 1673, while the nano/2025 profile membership references `opc_cu_3072`).
- Some carry no link at all (`service_register_nodes` → none).
- No two entries share a `kconfig_symbol`, so the symbol cannot be used to auto-join them.

The consequence: a naive completion count over canonical CUs reports nano as "6/15 required" because canonical `opc_cu_3072` (Attribute Read), `opc_cu_3073` (RegisterNodes), `opc_cu_3175` (Session Base), and `opc_cu_3912` (Server Capabilities 2) are all marked `unimplemented` even though the codebase implements every one of them under a separate alias. The number is an artifact of split tracking, not reality.

**Therefore the core of this feature is a manifest reconciliation** — establishing, for each canonical OPC CU, a single honest implementation status (resolved through the implementing alias/symbol where one exists) — after which the completion matrices become a straightforward generation step.

### Confirmed grounding decisions (2026-07-15)

- **Denominator = OPC transitive closure.** A profile's CU scope is the OPC-defined transitive CU closure from `profiles/opcua-profile-normalized-snapshot.json` (`relationships.transitive_cu_closure`), not our hand-picked `profile_defaults` subset. We are implementing the OPC standard, so completion is measured against what the standard defines for each profile. This is larger and more honest: nano's closure is 51 CUs (of which 50 are in the manifest), versus the 15 CUs `profile_defaults` currently targets. Closure CUs missing from the manifest (1 for nano `[3721]`, up to 6 for standard) MUST be added/grounded.
- **Optionality** is sourced from the manifest's per-CU `cu_optional` (present on all 114 canonical CUs); closure-only CUs not yet in the manifest MUST have their `cu_optional` filled and grounded when added.
- **`opc_cu_2600` "SecurityPolicy Support" is satisfied by SecurityPolicy None** — grounded in the CU text ("Support at least one Security Policy … Support of SecurityPolicy None is recommended") and nano's plaintext transport. This is consistent with spec 072.
- **N/A status.** A CU whose profile constraints make it moot MUST be scorable as *not-applicable* with a grounded reason, and excluded from that profile's required denominator — e.g. `opc_cu_3080` "Security Default ApplicationInstance Certificate" under nano's None+Anonymous posture. Documentation CUs (e.g. `opc_cu_3808` "Documentation - Core Capacities") are scored by documentation presence, not code. N/A must be explicit and grounded, never a silent exclusion.

### Extended scope: full Server surface (2026-07-15)

The full OPC catalog has **1182** conformance units (all profile types). The **Server**-relevant surface is **525** distinct CUs across **90** Server profiles/facets (179 required, 346 optional). The report MUST cover this full Server surface, not only the 4 base profiles.

- **Catalog vs build manifest.** The full 525-CU Server surface is captured in a dedicated, API-sourced catalog (`profiles/opcua-server-conformance.json`, produced by `scripts/profile_manifest/fetch_server_catalog.py`) — **separate from the build manifest** so cataloguing the standard does not add hundreds of NOT-IMPLEMENTED entries to Kconfig. The report joins the catalog (membership + `isOptional`) with the build manifest (implementation status, capability model).
- **Capability model (maintainer decision).** A CU counts implemented (`[x]`) when the **codebase can build it** (any profile); per-profile inclusion is Kconfig's responsibility, not the completion metric.
- **Reconciliation is the large remaining work.** Our implementations are tracked as **coarse feature-level CUs** (e.g. one `opc_cu_events`) that do not map 1:1 to OPC's **granular** CUs. Only ~41 of 525 are currently reconciled. Linking the optional Server facets (PubSub, Alarms, History, Methods, Aggregates, Redundancy, …) to their individual OPC CU ids is a per-CU grounding effort that MUST be done incrementally and honestly, not auto-derived; until a CU is reconciled it counts as not-implemented.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Per-profile completion at a glance (Priority: P1)

As a maintainer or integrator, I need each profile's conformance page to state how many of its required and optional CUs are implemented, so that I can see completion and gaps without cross-referencing a flat roadmap table.

**Why this priority**: This is the requested deliverable and the primary value of the feature.

**Independent Test**: Regenerate `docs/conformance` and confirm each profile page shows a completion line (`R/Rtotal required, O/Ototal optional`) plus an itemized list of that profile's CUs with required/optional and implemented/not-implemented status, and that the counts are internally consistent with the itemized list.

**Acceptance Scenarios**:

1. **Given** the regenerated conformance docs, **When** the nano profile page is read, **Then** it shows a headline completion line of the form "Nano 2025: R/Rtotal required and O/Ototal optional CUs implemented" whose numbers equal the counts in its itemized CU list.
2. **Given** any profile page, **When** a CU is required for that profile but not implemented, **Then** it appears in the itemized list flagged as a required gap.
3. **Given** the same manifest, **When** the docs are regenerated twice, **Then** the output is byte-identical (deterministic generation).
4. **Given** a CU implemented under an alias/different symbol than its canonical id, **When** completion is computed, **Then** the canonical CU counts as implemented (status resolved through the reconciliation link).

---

### User Story 2 - Facet completion the same way (Priority: P2)

As a maintainer, I need each facet to report the same required/optional CU completion, so that facet-level conformance is as legible as profile-level.

**Why this priority**: The request explicitly asks for "the same things for Facets and other profiles."

**Independent Test**: Regenerate docs and confirm each facet has a completion line and itemized CU list consistent with its counts.

**Acceptance Scenarios**:

1. **Given** the regenerated docs, **When** a facet page/section is read, **Then** it shows required/optional CU completion counts and an itemized list.
2. **Given** a facet whose CUs are a subset of a profile's, **When** both are reported, **Then** their shared CUs show the same implemented status.

---

### User Story 3 - Honest, reconciled manifest (Priority: P1)

As a maintainer, I need every canonical OPC CU to have exactly one resolved implementation status, so that completion counts reflect what the codebase actually does and cannot silently drift.

**Why this priority**: Without reconciliation the counts are meaningless (US1/US2 depend on it), and a split model invites future false claims.

**Independent Test**: Run validation and confirm every canonical CU resolves to exactly one status, no canonical CU is both "unimplemented" and satisfied-by-an-implemented-alias, and every reconciliation link is grounded.

**Acceptance Scenarios**:

1. **Given** the reconciled manifest, **When** validation runs, **Then** every canonical CU that is implemented under an alias is linked to that alias (or promoted directly), and no canonical CU is left falsely `unimplemented` while its behavior ships.
2. **Given** a canonical CU with no implementing entry, **When** completion is computed, **Then** it counts as not-implemented and is grounded as a genuine gap (not a tracking artifact).
3. **Given** a reconciliation link, **When** validation runs, **Then** the link's OPC grounding (the canonical `cu_id` and the implementing symbol/tests) is present and consistent.

## Requirements *(mandatory)*

- **FR-001**: The manifest MUST express, for each canonical OPC CU, a single resolved implementation status. Where a canonical CU is implemented under an alias or a differently-named symbol, an explicit, grounded cross-reference (working name `satisfied_by` / `implemented_by`) MUST link the canonical CU to the implementing entry so status resolves to one value.
- **FR-002**: Reconciliation links MUST be grounded per CU (canonical `cu_id` ↔ implementing `kconfig_symbol`/`backing_tests`), not inferred by fuzzy name or symbol matching; validation MUST reject a link whose grounding is absent or inconsistent.
- **FR-003**: The system MUST compute, per profile, the count of required CUs implemented over required CUs in scope, and optional CUs implemented over optional CUs in scope, where **in-scope = the OPC transitive CU closure for that profile**, the required/optional split comes from `cu_optional`, and implemented/not comes from the reconciled status. CUs scored **N/A** (profile-mooted or documentation, grounded) are excluded from the denominator and reported separately.
- **FR-004**: The system MUST compute the same required/optional completion per facet.
- **FR-005**: `scripts/profile_manifest/generate.py` MUST emit CU-based completion output into `docs/conformance` — a headline completion line plus an itemized per-CU list (required/optional, implemented/not, implementing symbol, OPC reference) for each profile and facet — replacing or augmenting the current roadmap format, and generation MUST be deterministic.
- **FR-006**: `scripts/profile_manifest/validate.py` (via `--all` / `--check-generated`) MUST verify the committed conformance docs match generator output and that the reconciled manifest is internally consistent (no canonical CU counted both ways; every link grounded).
- **FR-007**: A profile's CU scope MUST be the OPC transitive CU closure from `profiles/opcua-profile-normalized-snapshot.json`. Any closure CU missing from the manifest MUST be added with grounded `cu_optional` and `opc_reference`, and validation MUST fail if a profile's closure contains a CU the manifest does not represent.
- **FR-011**: The manifest MUST support a **not-applicable** CU status per profile with a required grounded reason, excluded from that profile's completion denominator, so a profile is not penalized for a CU its own constraints make moot (e.g. an ApplicationInstance certificate under a None+Anonymous profile).
- **FR-008**: The completion report MUST distinguish "required gap" (a required CU not implemented) from "optional not-yet-implemented", and MUST make required gaps prominent per profile.
- **FR-009**: Existing per-feature prose conformance files MAY be retained, but the generated completion matrices MUST be the authoritative, machine-checked completion source; any counts stated in prose MUST not contradict the generated matrices.
- **FR-010**: Test coverage MUST include a fixture-based unit test computing completion from a small known manifest (required/optional split, alias resolution, per-profile and per-facet counts) so the counting logic is verified independently of the full manifest.

### Key Entities

- **Canonical OPC CU** — an `opc_cu_<digits>` entry with OPC grounding and a `cu_optional` flag; the unit of completion counting.
- **Implementation alias** — an internal entry carrying the real `kconfig_symbol`/`implementation_state`/`backing_tests`.
- **Reconciliation link** — the grounded cross-reference resolving a canonical CU's status through its implementing alias.
- **Completion matrix** — the generated per-profile / per-facet required/optional implemented counts plus itemized CU list.

## Success Criteria *(mandatory)*

- **SC-001**: Each profile and facet page reports required/optional CU completion counts that exactly match its itemized CU list.
- **SC-002**: Every canonical OPC CU resolves to exactly one implementation status; no CU that ships is reported as an unimplemented artifact.
- **SC-003**: `generate.py` output is deterministic and `validate.py --all` passes, including the new consistency and generated-docs checks.
- **SC-004**: A fixture unit test verifies the completion counting (required/optional, alias resolution, per-profile/per-facet) independently of the full manifest.
- **SC-005**: The nano completion line reflects reality (its previously-split CUs — Attribute Read, RegisterNodes, Session Base, Server Capabilities 2 — count as implemented), and any remaining nano gaps (e.g. SecurityPolicy-None posture per spec 072) are shown as genuine, grounded items.

## Scope

- **In scope**: Reconciling canonical CUs with their implementing aliases in the manifest; per-profile and per-facet required/optional completion computation; generator emission of completion matrices into `docs/conformance`; validation of consistency and generated-doc drift; fixture unit test.
- **Out of scope**: Re-scraping the live OPC profile DB (optionality is taken from existing `cu_optional`); changing any build gating or runtime behavior; the nano crypto gating itself (spec 072, though this report will make that change's nano security posture visible).

## Dependencies and Assumptions

- Assumes `cu_optional` on the 114 canonical CUs is authoritative for required/optional (maintainer decision 2026-07-15); the reconciliation fills the canonical→implementation linkage, not optionality.
- Assumes each canonical CU implemented by the codebase can be grounded to exactly one implementing symbol/test set; genuinely ambiguous cases are surfaced during planning rather than guessed.
- Complements spec 072: once nano's crypto is gated, this report should show nano claiming SecurityPolicy None only, with the message-crypto CUs as honest not-implemented items rather than hidden linked code.
