# OPC Latest Active Profile Migration Design

## Goal

Migrate the canonical OPC UA profile manifest from the current 2017-era seed data to the latest active OPC Foundation server profile surface from the OPC UA 1.05 profile API. The canonical named profiles must track active 2025 server profiles where available, while superseded 2022 profiles remain historical references and must not seed default selections.

## Context

The current Kconfig/profile redesign treats profile selection as a defaults preset. Users can still globally enable or disable any implemented Profile, Facet, Conformance Unit, or capacity. That behavior remains unchanged.

Local API evidence in `profiles/opcua-1.05-api/` shows the profile catalog for profile group `171` (`UACore 1.05`) contains active 2025 server profiles and superseded 2022 server profiles:

- `2266`: Nano Embedded Device 2025 Server Profile, `releaseStatus=3`
- `2267`: Micro Embedded Device 2025 Server Profile, `releaseStatus=3`
- `2268`: Embedded 2025 UA Server Profile, `releaseStatus=3`
- `2269`: Standard 2025 UA Server Profile, `releaseStatus=3`
- `1330`: Nano Embedded Device 2022 Server Profile, `releaseStatus=4`
- `2255`: Micro Embedded Device 2022 Server Profile, `releaseStatus=4`
- `1332`: Embedded 2022 UA Server Profile, `releaseStatus=4`
- `1333`: Standard 2022 UA Server Profile, `releaseStatus=4`

The API catalog files are evidence snapshots, not checked-in source-of-truth inputs. The committed source-of-truth must be a normalized, deterministic snapshot and manifest that preserve only the data the generator needs.

## Scope

In scope:

- Normalize active Server Profile, Server Facet, and Server Conformance Unit data from the OPC UA 1.05 API.
- Use active 2025 profiles as the canonical named `nano`, `micro`, `embedded`, and `standard` defaults.
- Preserve superseded 2022 profile records as historical metadata, not selectable defaults.
- Preserve current project claim state: existing tested items remain claimed; newly imported API items are visible but unimplemented/deferred unless explicitly mapped to existing tested code.
- Regenerate Kconfig, defconfigs, capacities, claim map, roadmap, and build docs from the migrated manifest.
- Validate that selected 2025 profile/facet/CU/capacity state reaches `.config`, `muc_opcua_config.cmake`, and `autoconf.h`.

Out of scope:

- Claiming support for newly imported OPC items without backing implementation and tests.
- Removing the project-internal `full` profile.
- Reintroducing profile gates that hide unrelated Profile/Facet/CU menus.
- Committing raw downloaded API dumps or scrape playbooks.

## Data Model

Add or normalize these manifest/snapshot fields:

- `opc_id`: OPC Foundation profile DB id for profiles/facets/CUs when available.
- `opc_profile_uri`: profile/facet URI when available.
- `release_status`: raw OPC API release status integer.
- `release_status_name`: normalized label, at minimum `active` for `3` and `superseded` for `4`.
- `supersedes` / `superseded_by`: optional ids or URIs when discoverable from API descriptions or explicit API relationships.
- `included_profiles`: normalized profile/facet inclusion edges.
- `included_conformance_units`: normalized profile/facet to CU edges.
- `source_metadata`: API endpoint, profile group id, retrieval time, and source record id.

The manifest remains the generator's source of truth. Raw local API JSON remains ignored.

## Import Flow

1. Fetch or read an OPC API snapshot for profile group `171`.
2. Capture relationship endpoints for every relevant Server profile/facet:
   - `profile/includedprofiles/<id>/?pg=171&all=1`
   - `profile/includedconformanceunits/<id>/?pg=171&all=1`
3. Classify records:
   - Active server profile/facet: `profileUri` contains `/Server/` and `releaseStatus=3`.
   - Superseded server profile/facet: `profileUri` contains `/Server/` and `releaseStatus=4`.
   - Client-only records are ignored for Kconfig defaults.
4. Select canonical named profiles:
   - `nano` -> active Nano Embedded Device 2025 Server Profile.
   - `micro` -> active Micro Embedded Device 2025 Server Profile.
   - `embedded` -> active Embedded 2025 UA Server Profile.
   - `standard` -> active Standard 2025 UA Server Profile.
5. Traverse inclusion edges recursively to derive Facet and CU coverage for each canonical named profile.
6. Compute the minimum canonical profile tier for each Facet/CU: `nano < micro < embedded < standard < full`.
7. Merge into `profiles/opcua-profile-manifest.yaml` without promoting unimplemented items.
8. Regenerate generated artifacts.

## Kconfig Behavior

The visible profile selector becomes:

- Nano Embedded Device 2025 Server Profile
- Micro Embedded Device 2025 Server Profile
- Embedded 2025 UA Server Profile
- Standard 2025 UA Server Profile
- Full (project-internal)
- Custom

Internal cascade symbols continue to seed defaults only. Selecting `standard` activates the standard internal marker and all lower internal markers. Selecting `full` activates standard plus implemented extras. Selecting `custom` starts from the hand-selected baseline.

Superseded 2022 profiles may appear in docs, source metadata, or non-selectable Kconfig comments, but they must not appear as selectable profile-choice entries and canonical defconfigs must not select them.

## Claim Honesty

The migration does not create new OPC conformance claims. Existing claimed items keep their claim state only when their implementation and tests still match the migrated OPC item. Newly imported Facets and CUs default to `unimplemented` or `deferred` and are rendered as visible, unselectable comments unless mapped to existing tested functionality.

Claimed/implemented CUs must preserve exact OPC references. If the API provides a CU without a part/section reference, it can still be imported as visible metadata, but it cannot be claimed until the manifest has a grounded `opc_reference.spec` and `opc_reference.section`.

## Validation

The migration is accepted only if all checks pass:

- `python3 scripts/profile_manifest/validate.py --all`
- `bash scripts/test_profile_gating.sh`
- Standard profile build and CTest pass.
- Full profile build and CTest pass.
- `git diff --check` has no output.
- Canonical defconfigs select 2025 profile symbols, not superseded 2022 symbols.
- Generated `.config`, `muc_opcua_config.cmake`, and `autoconf.h` expose selected 2025 profile/facet/CU/capacity symbols.
- Imported active-but-unimplemented OPC items are visible but not selectable.

## Risks

- The current local catalog does not include containment edges inline; relationship endpoints must be fetched or normalized before defaults change.
- API descriptions may say one profile supersedes another even when the profile URI is reused, as seen with the Standard 2025 name using the `StandardUA2022` URI. The manifest must preserve both display name and URI rather than deriving year solely from URI.
- Kconfig can define a symbol in only one location. A CU included by multiple Facets needs one canonical owner plus help text listing additional provenance.
- Broad profile imports can greatly enlarge Kconfig. The first implementation must migrate canonical active server defaults and import only enough extra active server records to make unimplemented/superseded visibility honest.

## Implementation Boundary

Implementation must be a follow-up Spec Kit task, separate from the graph-derived Kconfig redo. The previous task intentionally did not change canonical manifest defaults to 2022/2025 API data; this task is the controlled migration that does.
