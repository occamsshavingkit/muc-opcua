# OPC-named Kconfig profile, facet, and CU redesign

Status: approved design, not yet implemented.

## Problem

The manifest-driven Kconfig work makes OPC UA profiles, facets, conformance
units, unavailable items, and capacities visible from one generated source of
truth. The current Kconfig shape is still too project-centric: it exposes many
symbols and labels through implementation names instead of the OPC UA standard
names users are trying to configure.

Users should not have to know internal feature names to build an OPC UA profile.
The Kconfig surface should read like the OPC UA profile model: choose a Profile,
drill into Facets, and drill into CUs. Project-specific implementation details
must stay separate from standard OPC items.

## Goals

- Make the top-level Kconfig flow start with an OPC UA Server Profile selector.
- Show OPC UA Facets as drilldown menus labeled with OPC UA standard names.
- Show OPC UA Conformance Units inside their containing Facets, labeled with OPC
  UA standard names.
- Let profile selection set the exact profile-defined Facet and CU defaults.
- Let users toggle a Facet as a group and toggle individual CUs below it.
- Switch the selected profile to `custom` when users deviate from a profile's
  Facet or CU set.
- Keep capacity controls as a separate capacities menu with profile defaults.
- Keep project-only implementation options out of the OPC profile/facet/CU tree.
- Use deterministic OPC-name-derived symbol names for Profiles, Facets, and CUs.

## Non-goals

- Do not preserve old project-internal `MUC_OPCUA_*` feature aliases for OPC
  feature symbols.
- Do not add compatibility aliases for previous project-centric OPC feature
  names.
- Do not hide unimplemented OPC items from the Kconfig tree merely because this
  project has not implemented them yet.
- Do not rename or remove existing public capacity override support such as
  `-DMU_MAX_SESSIONS=...` as part of this redesign.
- Do not make target-system selection or binary-size estimation a required part
  of this work.

## Menu model

The generated Kconfig tree should be OPC-first:

```text
OPC UA Server Profile
  ( ) Embedded 2017 UA Server Profile
  (*) Standard 2017 UA Server Profile
  ( ) Full 2017 UA Server Profile
  ( ) Custom

OPC UA Facets and Conformance Units
  Facet: Core 2017 Server
    [*] Enable Core 2017 Server as a group
    [*] CU: Attribute Read
    [*] CU: Attribute Write
    [*] CU: View Basic

  Facet: Subscription Server
    [*] Enable Subscription Server as a group
    [*] CU: Monitor Items 500
    [ ] CU: Monitored Items Deadband Filter

Capacities
  Maximum sessions
  Maximum subscriptions
  ...

Project options
  Project-specific implementation toggles only
```

Rules:

- The top-level profile selector is the primary entry point.
- `STANDARD_PROFILE` is not an implementation detail; it represents the defined
  OPC UA Standard profile.
- Selecting a profile applies exactly that profile's default Facet and CU set.
- Facets appear as drilldown menus labeled `Facet: <OPC UA standard name>`.
- CUs appear inside Facet menus labeled `CU: <OPC UA standard name>`.
- A Facet group toggle enables or disables all available CUs in that Facet.
- Individual CUs remain separately toggleable where implemented and dependency
  constraints allow them.
- If a user selects a named profile and changes any Facet or CU state from that
  profile's defined state, the effective profile becomes `custom`.
- Unimplemented or deferred OPC items remain visible but unavailable.
- Non-OPC implementation details belong under `menu "Project options"`.

## Symbol naming

Profiles, Facets, and CUs use OPC UA standard names as the source for symbol
names. Symbol names must be deterministic and must not carry redundant kind
suffixes after the name body.

Naming convention:

```text
MUC_OPCUA_{PROFILE|FACET|CU}_<NAME>
```

`<NAME>` is derived by:

1. Start from the OPC UA standard Profile, Facet, or CU name.
2. Remove a trailing kind word when it duplicates the symbol kind, such as
   `Profile` from profile symbols or `Facet` from facet symbols.
3. Uppercase the remaining name.
4. Replace each run of non-`A-Z0-9` characters with one underscore.
5. Trim leading and trailing underscores.

Examples:

| OPC UA name | Symbol |
| --- | --- |
| Embedded 2017 UA Server Profile | `MUC_OPCUA_PROFILE_EMBEDDED_2017_UA_SERVER` |
| Standard 2017 UA Server Profile | `MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER` |
| Core 2017 Server Facet | `MUC_OPCUA_FACET_CORE_2017_SERVER` |
| Subscription Server Facet | `MUC_OPCUA_FACET_SUBSCRIPTION_SERVER` |
| Attribute Read | `MUC_OPCUA_CU_ATTRIBUTE_READ` |
| Monitor Items 500 | `MUC_OPCUA_CU_MONITOR_ITEMS_500` |

The `FACET` kind is already present in the prefix, so it is not repeated after
the name body.

## Profile behavior

Profile choice must be explicit and reversible:

- Named profile choices set all profile-controlled Facets and CUs to the manifest
  state for that profile.
- `custom` means the user has intentionally deviated from a named profile.
- Any user change to a profile-controlled Facet or CU after selecting a named
  profile changes the effective profile to `custom`.
- Returning all profile-controlled Facets and CUs to a named profile's exact set
  may allow tooling to report that the configuration matches that profile again.
- Dependency rules still apply; selecting an unavailable CU must not silently
  create a false implementation claim.

The implementation may use generated helper symbols or generated post-processing
scripts where Kconfig cannot directly express exact-profile-versus-custom state.
The observable behavior is the contract.

## Facet and CU behavior

Facet toggles provide group control, not a replacement for CU-level visibility:

- A Facet toggle sets all implemented and dependency-satisfied CUs in that Facet
  on or off as a group.
- A disabled Facet disables its contained CUs.
- A user may re-enable or disable individual CUs where dependencies allow.
- A Facet's displayed state should reflect whether its selectable CUs are fully
  enabled, fully disabled, or effectively customized when Kconfig/tooling can
  represent that distinction.
- CUs shared by multiple OPC structures must have one canonical config symbol;
  generated menus should avoid conflicting duplicate definitions.

Kconfig can define a symbol at only one menu location. If an OPC CU belongs to
multiple logical groupings, the manifest must pick one canonical Kconfig owner
and document cross-references in help text or generated documentation.

## Capacity behavior

The existing capacity menu model is correct and should remain distinct from the
OPC Facet/CU tree.

Rules:

- Capacities get profile defaults from the manifest.
- Changing a capacity above or equal to the selected profile's requirement does
  not make the profile `custom`.
- Changing a capacity below what the selected profile allows or requires makes
  the configuration incompatible with that profile and should be reported as
  `custom` or invalid for that profile.
- Existing public capacity override contracts such as `-DMU_MAX_*=...` remain
  supported.

## Generated artifacts

The manifest and generator should produce or update:

- `/Kconfig` with OPC-named Profile, Facet, and CU symbols.
- `configs/*.defconfig` using the OPC-named symbols.
- CMake profile and override translation logic for the new symbol model.
- Documentation that explains the Profile, Facet, CU, capacity, and Project
  options menu structure.
- Generated conformance roadmap and claim maps using OPC UA standard names.
- Validation output that identifies stale generated files and naming violations.

Generated files should remain visibly generated where practical.

## Migration rules

- Do not add legacy aliases for old project-centric `MUC_OPCUA_*` feature names.
- Update generated defconfigs, generated docs, tests, and CMake translation to
  the new OPC-named symbols in the same change.
- Keep non-feature public overrides, especially `-DMU_MAX_*`, working.
- Remove or rewrite documentation examples that mention obsolete project-centric
  OPC feature symbols.
- Tests should prove the old project-centric feature names are not the canonical
  surface anymore.

## Verification gates

Required checks:

- Kconfig parses successfully with `kconfiglib`.
- Generated Kconfig contains Profile symbols derived from OPC UA profile names.
- Generated Kconfig contains Facet menus labeled `Facet: <OPC UA standard name>`.
- Generated Kconfig contains CU entries labeled `CU: <OPC UA standard name>`.
- No Facet symbol redundantly ends with `_FACET` after the name body.
- No Profile symbol redundantly ends with `_PROFILE` after the name body.
- Selecting each named profile produces the expected manifest-defined Facet and
  CU set.
- Changing a Facet after selecting a named profile results in `custom` profile
  behavior.
- Changing a CU after selecting a named profile results in `custom` profile
  behavior.
- Capacity overrides still flow from CMake into generated config and compile
  definitions.
- Lower-than-profile capacity values are reported as incompatible with the
  selected profile.
- Unimplemented OPC items remain visible and unselectable.
- Existing full test and conformance validation gates still pass after
  regeneration.

Representative local verification should include:

```sh
python3 scripts/profile_manifest/validate.py --all
bash scripts/test_profile_gating.sh
cmake -S . -B build/opc-named-kconfig-check -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/opc-named-kconfig-check
ctest --test-dir build/opc-named-kconfig-check --output-on-failure
```

## Acceptance criteria

- A user can select an OPC UA named profile without seeing project-internal OPC
  feature names as the primary configuration surface.
- Facets and CUs are presented with OPC UA standard names and deterministic
  OPC-name-derived symbols.
- Facet names and symbols do not redundantly repeat `Facet` after the Facet
  prefix.
- Profile names and symbols do not redundantly repeat `Profile` after the Profile
  prefix.
- Profile selection, Facet group toggles, CU toggles, and `custom` behavior match
  this spec.
- Capacity configuration remains separate from the Facet/CU tree and keeps
  existing public capacity overrides working.
- Project-only implementation options are isolated under `Project options`.
- Generated docs and tests describe the new OPC-named surface.

## Optional follow-up

Target-system selection and size reporting are useful follow-up capabilities but
are not required for this redesign.

Potential follow-up scope:

- Add a target-system choice such as Linux x86_64, Linux aarch64, Cortex-M0, or
  another embedded class.
- Use target-system choices to shape default capacities where justified.
- Provide manifest-backed size estimates, or report real binary sizes after build
  with an external script such as `scripts/measure_size.sh`.

Kconfig should not be expected to calculate live binary size during menuconfig.
