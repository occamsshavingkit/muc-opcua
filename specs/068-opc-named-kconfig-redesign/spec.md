# Feature Specification: OPC-Named Kconfig Redesign

**Feature Branch**: `068-opc-named-kconfig-redesign`  
**Created**: 2026-07-11  
**Status**: Draft  
**Input**: User description: "implement the spec at docs/superpowers/specs/2026-07-11-opc-named-kconfig-redesign.md"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Select an OPC UA Profile by Standard Name (Priority: P1)

A build-system integrator wants to configure an OPC UA Server build against a standard OPC UA profile without knowing any project-internal feature names. They invoke `cmake -S . -B build -DMUC_OPCUA_PROFILE=standard`, then inspect the generated Kconfig surface. They see the top-level menu titled by the OPC UA standard profile name and verified OPC profile URI in the help text.

**Why this priority**: The profile selector is the primary entry point. Without OPC-named profiles, the entire Kconfig surface remains project-centric and inaccessible to OPC UA domain users.

**Independent Test**: Configure a build with `-DMUC_OPCUA_PROFILE=standard`, run `menuconfig`, and verify the top-level choice lists "Standard 2017 UA Server Profile" with its OPC URI in help text. Verify the generated defconfig selects `MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER=y`.

**Acceptance Scenarios**:

1. **Given** a clean build directory, **When** the user configures with `-DMUC_OPCUA_PROFILE=standard`, **Then** the generated `.config` selects `MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER=y` and the Kconfig help text references the OPC UA Standard profile URI.
2. **Given** a build configured for standard profile, **When** the user opens `menuconfig`, **Then** the profile choice menu displays "Standard 2017 UA Server Profile" as the selected option.
3. **Given** any build, **When** the user selects a named profile in Kconfig, **Then** all Facets and CUs are preset to that profile's defined defaults from the manifest.

---

### User Story 2 - Drill into OPC UA Facets and Conformance Units (Priority: P1)

A developer wants to see which OPC UA Facets and Conformance Units are enabled for their selected profile, using OPC standard names. They open the Kconfig tree and see a Facets menu where each Facet is labeled `Facet: <OPC UA standard name>` and each Conformance Unit inside is labeled `CU: <OPC UA standard name>`.

**Why this priority**: The Facet/CU drilldown is the core value-add of the redesign. Without it, users cannot navigate OPC conformance structure in Kconfig.

**Independent Test**: Configure a standard-profile build, run `menuconfig`, and verify the Facets menu contains entries like "Facet: Core 2017 Server" with "CU: Attribute Read" inside.

**Acceptance Scenarios**:

1. **Given** a standard profile build, **When** the user opens the Kconfig Facets menu, **Then** each menu entry is labeled `Facet: <OPC UA standard name>` matching the OPC UA specification.
2. **Given** an enabled Facet, **When** the user drills into it, **Then** each Conformance Unit is labeled `CU: <OPC UA standard name>` with its implementation state and OPC source in help text.
3. **Given** an unimplemented Facet or CU, **When** viewed in Kconfig, **Then** it is visible but unselectable (NOT IMPLEMENTED comment).

---

### User Story 3 - Toggle Facets as Groups and Individual CUs (Priority: P2)

A developer wants to enable or disable an entire OPC UA Facet as a group, then fine-tune individual CUs within it. They toggle the Facet group switch and see all its CUs follow. They then disable a specific CU within the enabled Facet.

**Why this priority**: Facet group toggles provide efficient OPC-level configuration while CU-level toggles offer precision. This is the second-most-important interaction after profile selection.

**Independent Test**: Configure a custom build, enable "Subscription Server Facet" via its group toggle, verify all its CUs are enabled, then disable "CU: Monitored Items Deadband Filter" individually.

**Acceptance Scenarios**:

1. **Given** a custom profile, **When** the user enables a Facet group toggle, **Then** all implemented CUs in that Facet are enabled.
2. **Given** a custom profile with a Facet enabled, **When** the user disables a specific CU within the Facet, **Then** only that CU is disabled and the Facet remains otherwise enabled.
3. **Given** a Facet whose CUs have been individually customized, **When** the user toggles the Facet group off and back on, **Then** all CUs reset to the group default (enabled).

---

### User Story 4 - Profile Switches to Custom on Deviation (Priority: P2)

A developer selects "Standard 2017 UA Server Profile" then manually adds a Facet not normally in that profile. The configuration system recognizes this deviation and switches the effective profile to "Custom", preserving the non-standard selection while clearly indicating the configuration no longer matches a named OPC profile.

**Why this priority**: Profile integrity is essential for conformance claims. Users must know when their build diverges from a standard profile.

**Independent Test**: Configure standard profile, enable an out-of-profile Facet, verify the effective profile reports as custom.

**Acceptance Scenarios**:

1. **Given** a standard profile build, **When** the user changes any Facet from the standard default, **Then** the effective profile becomes custom.
2. **Given** a standard profile build, **When** the user changes any CU from the standard default, **Then** the effective profile becomes custom.
3. **Given** a custom build, **When** the user returns all Facets and CUs to match a named profile's exact set, **Then** tooling may report the configuration matches that profile again.

---

### User Story 5 - Capacities Remain Separate and Support Overrides (Priority: P3)

A developer adjusts capacity limits (max sessions, max subscriptions) independently of the OPC profile/facet tree. They set `-DMU_MAX_SESSIONS=42` at the command line and verify the override reaches the compiled binary.

**Why this priority**: Capacities work correctly today. This story ensures the redesign preserves existing behavior without regressions.

**Independent Test**: Configure a build with `-DMU_MAX_SESSIONS=42`, verify `MU_MAX_SESSIONS=42` appears in the compile command via the capacity override chain.

**Acceptance Scenarios**:

1. **Given** any profile build, **When** the user passes `-DMU_MAX_SESSIONS=42` via CMake, **Then** the compiled binary uses 42 for max sessions.
2. **Given** any profile build, **When** the user opens `menuconfig`, **Then** capacities appear in a "Capacities" menu separate from OPC Facets/CUs.
3. **Given** a profile-dependent capacity, **When** a different profile is selected, **Then** the capacity default changes to match the new profile.

---

### Edge Cases

- What happens when an OPC CU belongs to multiple Facets? The manifest must pick one canonical Kconfig owner and document cross-references in help text (Kconfig can only define a symbol at one menu location).
- What happens when a user enables a CU whose dependency Facet is disabled? Kconfig `depends on` enforces dependency ordering; enabling the CU forces its dependency on.
- What happens when an unimplemented item is referenced as a dependency? Unimplemented items have no Kconfig symbol, so they cannot appear in `depends on` chains.
- What happens when a Facet name collides after uppercasing? The naming convention's deterministic algorithm guarantees unique names because OPC standard names are unique.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST generate Kconfig with a top-level profile choice using OPC UA standard profile names as display labels (e.g., "Standard 2017 UA Server Profile").
- **FR-002**: System MUST generate Kconfig profile symbols following the naming convention `MUC_OPCUA_PROFILE_<NAME>` where `<NAME>` is the OPC standard name uppercased with non-alphanumeric chars replaced by underscores, without a redundant trailing `_PROFILE`.
- **FR-003**: System MUST generate Facet menus labeled `Facet: <OPC UA standard name>` with symbols named `MUC_OPCUA_FACET_<NAME>` without a redundant trailing `_FACET`.
- **FR-004**: System MUST generate CU entries labeled `CU: <OPC UA standard name>` inside their containing Facet menus, with symbols named `MUC_OPCUA_CU_<NAME>`.
- **FR-005**: System MUST set Facet and CU defaults to the manifest-defined state for the selected profile.
- **FR-006**: System MUST detect when the user deviates from a named profile's Facet/CU set and report the effective profile as custom.
- **FR-007**: System MUST generate a Facet group toggle that enables/disables all implemented CUs within that Facet.
- **FR-008**: System MUST allow individual CU toggles within an enabled Facet.
- **FR-009**: System MUST keep capacity controls in a separate Capacities menu, distinct from the OPC Facet/CU tree.
- **FR-010**: System MUST preserve existing `-DMU_MAX_*=N` capacity override support.
- **FR-011**: System MUST show unimplemented OPC items as visible but unselectable Kconfig comments.
- **FR-012**: System MUST place non-OPC implementation options under a "Project options" menu, not in the OPC profile/facet/CU tree.
- **FR-013**: System MUST NOT preserve old project-centric `MUC_OPCUA_*` feature symbol aliases (no legacy aliases for the new OPC-named symbols).
- **FR-014**: The manifest MUST declare Facet containment (which CUs belong to which Facet) for OPC structure generation.
- **FR-015**: Validation MUST check that generated Kconfig symbols follow the OPC naming convention and do not redundantly repeat kind suffixes.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Profile definitions reference OPC UA Server Facet URIs per OPC-10000-7, derived from pinned `profiles/opcua-profile-snapshot.json`.
- **OPC-002**: Facet and CU names match OPC Foundation standard names as recorded in the manifest `opc_reference.facet` and `opc_reference.cu_name` fields, grounded in OPC-10000-4/5/6/7/8/9/11/13/14/20.
- **OPC-003**: The Standard 2017 UA Server Profile symbol `MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER` corresponds to the defined OPC UA Standard profile. It is not an implementation detail.
- **OPC-004**: Conformance claim maps and roadmaps continue to reference OPC specification part/section citations per the existing manifest.

### Scope Boundaries *(mandatory)*

- **In Scope**: Renaming all Profile/Facet/CU Kconfig symbols to OPC-name-derived names. Restructuring Kconfig menus into Profile selector, Facet drilldown, CU entries. Facet group toggles. Profile→custom transition. Isolating project options. Removing legacy feature aliases.
- **Out of Scope**: Target-system selection menus. Binary-size estimation in Kconfig. Live binary-size calculation during menuconfig. Adding new OPC conformance claims. Changing capacity override semantics.
- **Compatibility Claim**: Existing public `-DMU_MAX_*=N` capacity override contracts remain supported. Non-feature legacy interfaces are preserved. Full test suite (132 tests) must still pass.
- **Application Headroom Goal**: No regression in binary size or RAM usage for any profile configuration. The redesign is a naming/menu restructuring only.

### Key Entities

- **Profile**: An OPC UA Server Profile (e.g., NanoEmbeddedDevice2017, StandardUA2017). Has a display name, OPC profile URI, a default Facet/CU set defined in the manifest, and a Kconfig symbol derived from the OPC standard name.
- **Facet**: An OPC UA Facet containing Conformance Units. Has a group toggle symbol controlling all its CUs. Appears as a drilldown Kconfig menu labeled with its OPC standard name.
- **Conformance Unit**: An OPC UA Conformance Unit. Has an implementation state (claimed/implemented/unimplemented/deferred), optional Kconfig symbol, backing tests, and OPC source reference.
- **Capacity**: A numeric configuration value (e.g., max sessions). Has profile-varying defaults, a public override name (`MU_MAX_*`), and appears in a separate Capacities Kconfig menu.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can select an OPC UA named profile using only OPC standard names, without encountering any project-internal feature names in the profile/Facet/CU Kconfig surface.
- **SC-002**: All generated Kconfig Facet symbols use the format `MUC_OPCUA_FACET_<NAME>` with zero occurrences of redundant `_FACET` after the name body.
- **SC-003**: All generated Kconfig Profile symbols use the format `MUC_OPCUA_PROFILE_<NAME>` with zero occurrences of redundant `_PROFILE` after the name body.
- **SC-004**: Running `python3 scripts/profile_manifest/validate.py --all` passes with exit code 0 after regeneration.
- **SC-005**: Running `bash scripts/test_profile_gating.sh` passes all checks using the new OPC-named symbols.
- **SC-006**: Running `ctest --test-dir build/<profile>-check --output-on-failure` passes all 132 tests for at least the standard and full profiles.
- **SC-007**: No old project-centric `MUC_OPCUA_*` feature symbol aliases (e.g., `MUC_OPCUA_SECURITY`, `MUC_OPCUA_SUBSCRIPTIONS`) remain as the canonical Kconfig surface for OPC features.
- **SC-008**: `-DMU_MAX_SESSIONS=42` override still reaches the compiled binary via the existing capacity resolution chain.

## Assumptions

- The `docs/superpowers/specs/2026-07-11-opc-named-kconfig-redesign.md` design document is the authority for naming convention, menu structure, and profile/facet/CU behavior.
- The manifest (`profiles/opcua-profile-manifest.yaml`) is the single source of truth and will be restructured to add Facet containment and OPC-name mappings.
- The generator (`scripts/profile_manifest/generate.py`) will be the sole code path for producing Kconfig from the manifest.
- Kconfig's limitation of one symbol definition per menu location is accepted; CUs belonging to multiple OPC groupings will have one canonical owner with cross-references in help text.
- The existing capacity override chain (default < profile < user) remains correct and needs no semantic changes.
- Existing test names and test registration in CMake are unchanged; only the Kconfig symbol names they are gated on change.
