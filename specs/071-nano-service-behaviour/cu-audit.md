<!-- markdownlint-disable MD013 MD060 -->

# CU Audit: Server Nano Core Service Behaviour CUs

**Feature**: `071-nano-service-behaviour`  
**Created**: 2026-07-14  
**Task Evidence**: T001  
**Scope Rule**: Keep nano-default claims limited to manifest entries whose `profile_defaults.nano` is true. Optional write, session change-user, and diagnostics CUs remain off for nano unless a separate approved spec changes the manifest defaults.

## Classification Key

- **Needs symbol**: Service behaviour may exist, but the manifest entry has no dedicated `MUC_OPCUA_CU_*` symbol.
- **Needs tests**: Behaviour requires direct backing tests listed in `profiles/opcua-profile-manifest.yaml` before `implementation_state` can become `implemented`.
- **Needs behaviour**: Repository evidence shows missing or incomplete observable behaviour for the CU.
- **Satisfied pending manifest**: Existing source and tests appear sufficient for the service surface, but the CU still needs manifest/generated artifact promotion and validation.

## In-Scope CU Audit

| CU | Profile Default | Current Manifest State | Evidence | Classification |
|----|-----------------|------------------------|----------|----------------|
| `opc_cu_2317` View TranslateBrowsePath | nano and higher | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Implementation surface exists in `src/cu/core_2022_server/view_basic_translate/translate_paths.c`; integration coverage exists in `tests/integration/test_view_services.c` for TranslateBrowsePathsToNodeIds over valid, no-match, namespace-mismatch, and empty targetName paths. | Needs symbol and needs tests; promote only after dedicated unit/integration evidence is listed for OPC-10000-4 section 5.9.4. |
| `opc_cu_2328` Discovery Get Endpoints | nano and higher | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Implementation surface exists in `src/cu/core_2022_server/discovery/get_endpoints.c`; tests cover endpoint descriptions, profile URI match/mismatch, endpoint URL filtering, locale filtering, malformed arrays, and truncated bodies in `tests/unit/test_discovery_endpoint.c`, `tests/unit/test_discovery_services.c`, and `tests/unit/test_discovery_encode.c`. | Satisfied pending manifest; still needs dedicated symbol, backing-test list, generated artifacts, and profile validation for OPC-10000-4 sections 5.5.1 and 5.5.4. |
| `opc_cu_2352` Discovery Find Servers Self | nano and higher | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Implementation surface exists in `src/cu/core_2022_server/discovery/find_servers.c`; tests cover FindServers request filters, endpoint URL exclusion, localeIds, serverUris, and truncated body handling in `tests/unit/test_discovery_services.c` and no-session discovery behaviour in `tests/unit/test_discovery_endpoint.c`. | Satisfied pending manifest; still needs dedicated symbol, backing-test list, generated artifacts, and profile validation for OPC-10000-4 section 5.5.2. |
| `opc_cu_2389` Attribute Write Values | micro and higher, custom opt-in | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Implementation surface exists in `src/cu/core_2022_server/attribute_write/write.c`; tests cover Write request decoding, WriteResponse encoding, Good results, Bad_TypeMismatch, Bad_NotWritable, Bad_WriteNotSupported, Bad_NodeIdUnknown, and Bad_TooManyOperations in `tests/unit/test_write_service.c`, `tests/unit/test_write_errors.c`, `tests/unit/test_write_response.c`, and `tests/unit/test_write_decoder.c`. | Needs symbol and needs tests; core value-write behaviour appears present, but claim promotion requires explicit CU-gated evidence and nano default false preservation for OPC-10000-4 section 5.11.4. |
| `opc_cu_2400` Session Change User | micro and higher, custom opt-in | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Implementation surface exists in `src/core/service_dispatch/activate_session.c` and `src/services/session.c`; activation code validates identity tokens, updates `user_identity_kind`, copies username/certificate identity data, refreshes server nonce, and tests cover first activation plus invalid token/session paths in `tests/unit/test_session_auth.c` and `tests/unit/test_session.c`. | Needs tests and needs symbol; change-user reactivation on an already activated Session needs direct positive/negative tests before claim promotion for OPC-10000-4 sections 5.7.2 and 5.7.3. |
| `opc_cu_2936` Attribute Write StatusCode & Timestamp | micro and higher, custom opt-in | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Write request decoding includes `DataValue` in `src/cu/core_2022_server/attribute_write/write.c`, but current tests primarily prove values and result StatusCodes, not preservation/rejection semantics for written StatusCode and timestamps. | Needs behaviour and needs tests; add explicit StatusCode/Timestamp semantics before manifest promotion for OPC-10000-4 section 5.11.4. |
| `opc_cu_3147` Attribute Write Index | micro and higher, custom opt-in | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | `src/cu/core_2022_server/attribute_write/write.c` decodes `index_range`, but current grep evidence did not show partial array update logic or tests beyond decode/result errors. | Needs behaviour and needs tests; add IndexRange partial-update and invalid-range semantics before manifest promotion for OPC-10000-4 section 5.11.4. |
| `opc_cu_3192` Base Info Diagnostics | micro and higher, custom opt-in | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Diagnostics counters exist in `src/cu/core_2022_server/diagnostics/diagnostics.c`; address-space exposure exists in `src/address_space/base_nodes.c`; tests cover session counters, rejected/security counters, ServerDiagnosticsSummary node encoding, EnabledFlag, and build flag in `tests/unit/test_diagnostics.c`. | Satisfied pending manifest; still needs dedicated symbol, backing-test list, generated artifacts, and profile validation for OPC-10000-5 sections 6.3.1, 6.3.3, 8.3.2, and 12.9. |
| `opc_cu_3530` View Basic 2 | nano and higher | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | Implementation surface exists in `src/cu/core_2022_server/view_basic_translate/browse.c` and `src/cu/core_2022_server/view_basic_translate/browse_next.c`; tests cover BrowseResponse encoding, invalid BrowseDirection, continuation-point exhaustion, and Bad_TooManyOperations in `tests/unit/test_browse_service.c` and `tests/unit/test_browse_limits.c`. | Satisfied pending manifest; still needs dedicated symbol, backing-test list, generated artifacts, and profile validation for OPC-10000-4 sections 5.9.2 and 5.9.3. |
| `opc_cu_3983` Base Services Diagnostics | micro and higher, custom opt-in | `implementation_state: unimplemented`, `kconfig_symbol: null`, no `backing_tests` | `src/services/service_header.c` reads `return_diagnostics`, and response headers are encoded through `src/core/service_dispatch/response_encode.c`; grep evidence did not show service-level DiagnosticInfo returned according to the request mask. | Needs behaviour and needs tests; add explicit available-or-empty DiagnosticInfo handling before manifest promotion for OPC-10000-4 sections 7.32 and 7.38. |

## Aggregate Findings

- All ten in-scope manifest entries currently require at least symbol and backing-test promotion before any can honestly become `implemented`.
- Nano-default candidates are limited to `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, and `opc_cu_3530`.
- Optional/micro+ candidates are `opc_cu_2389`, `opc_cu_2400`, `opc_cu_2936`, `opc_cu_3147`, `opc_cu_3192`, and `opc_cu_3983`; these must preserve `profile_defaults.nano: false`.
- Behaviour gaps are currently suspected for `opc_cu_2936`, `opc_cu_3147`, and `opc_cu_3983` based on missing direct evidence for StatusCode/Timestamp writes, IndexRange partial writes, and service-level returned diagnostics.
- Direct test gaps remain for `opc_cu_2317` unit-level coverage, `opc_cu_2400` change-user reactivation, and CU-gated profile/default checks.

## T001 Result

T001 is complete. Later tasks must update this ledger with command outputs and revised classifications as each CU receives tests, implementation changes, manifest promotion, generated artifacts, and validation evidence.

## T002 Discovery and View Proof Audit

T002 maps Discovery and View evidence to `SCN-001`, `CASE-001`, `CASE-002`, `CASE-008`, and `CASE-010`.

| CU / Case | Evidence Found | Coverage Status | Follow-up Task |
|-----------|----------------|-----------------|----------------|
| `opc_cu_2328` / `CASE-001` GetEndpoints filters | `tests/unit/test_discovery_endpoint.c` covers no-session dispatch, profile URI match/mismatch, endpoint URL exclusion, and locale filtering. `tests/unit/test_discovery_services.c` covers malformed profileUris arrays and truncated request bodies with `MU_STATUS_BAD_DECODINGERROR`. | Strong unit coverage exists for OPC-10000-4 sections 5.5.1 and 5.5.4; still needs CU symbol/backing-test manifest promotion and profile-gating evidence. | T011, T015, T018, T019, T020 |
| `opc_cu_2352` / `CASE-001` FindServers self | `tests/unit/test_discovery_services.c` covers endpoint URL, localeIds, serverUris, serverTypes filters, malformed array count, and truncated body. `tests/unit/test_discovery_endpoint.c` covers no-session discovery dispatch. | Strong unit coverage exists for OPC-10000-4 section 5.5.2; still needs explicit claim promotion as Find Servers Self, not broad discovery alias. | T010, T014, T018, T019, T020 |
| `opc_cu_3530` / `CASE-002` View Basic 2 Browse/BrowseNext | `tests/unit/test_browse_service.c` covers invalid BrowseDirection and non-null ViewDescription rejection. `tests/unit/test_browse_limits.c` covers no continuation points, includeSubtypes matching, no-subtypes exclusion, and over-capacity `MU_STATUS_BAD_TOOMANYOPERATIONS`. | Unit coverage exists for Browse limits and result StatusCodes, but BrowseNext-specific proof should be confirmed or added before manifest promotion. | T012, T016, T018, T019, T020 |
| `opc_cu_2317` / `CASE-002` TranslateBrowsePathsToNodeIds | `tests/integration/test_view_services.c` covers TranslateBrowsePathsToNodeIds with Good target, Bad_NoMatch for bad name, Bad_NoMatch for namespace mismatch, and Bad_BrowseNameInvalid for empty targetName. | Integration coverage exists for OPC-10000-4 section 5.9.4; planned task T013 remains necessary for focused unit or claim-map-friendly coverage before manifest promotion. | T013, T017, T018, T019, T020 |
| Discovery/View / `CASE-008` boundary counts and path depth | `tests/unit/test_browse_limits.c` covers over-capacity Browse decode as `MU_STATUS_BAD_TOOMANYOPERATIONS` and continuation-point exhaustion as `MU_STATUS_BAD_NOCONTINUATIONPOINTS`. `tests/unit/test_discovery_services.c` covers malformed and truncated Discovery request arrays as `MU_STATUS_BAD_DECODINGERROR`. | Boundary coverage exists for Discovery and Browse; TranslateBrowsePaths excessive depth/count coverage still needs explicit confirmation in T013/T017. | T008, T012, T013, T016, T017 |
| Discovery/View / `CASE-010` nano vs optional defaults | No current direct test evidence was found in the read paths for generated profile-default preservation across all ten in-scope CUs. | Gap: profile-default and claim-map assertions are needed before profile claims can be trusted. | T006, T009, T018, T019, T020, T043 |

T002 is complete. Discovery/GetEndpoints and FindServers have strong existing unit evidence; View Basic 2 has partial unit evidence needing BrowseNext confirmation; TranslateBrowsePaths has integration evidence needing claim-map-friendly focused coverage; profile-default drift remains a test gap.

## T003 Write and Session Proof Audit

T003 maps Write and Session evidence to `SCN-002`, `CASE-003`, `CASE-004`, `CASE-007`, and `CASE-009`.

| CU / Case | Evidence Found | Coverage Status | Follow-up Task |
|-----------|----------------|-----------------|----------------|
| `opc_cu_2389` / `CASE-003` Write Values | `tests/unit/test_write_service.c` covers successful Value writes, callback invocation, response shape, batching, and type mismatch. `tests/unit/test_write_errors.c` covers empty arrays as `MU_STATUS_BAD_NOTHINGTODO`, missing DataValue value as `MU_STATUS_BAD_TYPEMISMATCH`, non-Value attributes as `MU_STATUS_BAD_NOTWRITABLE`, unknown node as `MU_STATUS_BAD_NODEIDUNKNOWN`, object Value writes as `MU_STATUS_BAD_NOTWRITABLE`, and over-capacity requests as `MU_STATUS_BAD_TOOMANYOPERATIONS`. | Existing behaviour and tests are strong for basic Value writes, but they are guarded by `MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE`, not a dedicated `opc_cu_2389` symbol. | T021, T026, T030, T031 |
| `opc_cu_2936` / `CASE-003` Write StatusCode & Timestamp | `tests/unit/test_write_service.c` and `tests/unit/test_write_errors.c` generally encode `DataValue` with `has_status`, `has_source_timestamp`, and `has_server_timestamp` false. No direct proof was found that supported StatusCode/Timestamp fields are preserved or explicitly rejected. | Gap: needs explicit StatusCode/Timestamp tests and behaviour before claim promotion. | T022, T027, T030, T031 |
| `opc_cu_3147` / `CASE-003` Write Index | `src/cu/core_2022_server/attribute_write/write.c` decodes `index_range`. `tests/unit/test_write_errors.c` currently verifies non-empty `indexRange` returns `MU_STATUS_BAD_WRITENOTSUPPORTED`. | Gap for this CU: existing evidence proves explicit unsupported behaviour, not the required partial array update support for `opc_cu_3147`. | T023, T028, T030, T031 |
| Write / `CASE-007` Disabled CU behaviour | Current Write tests are conditionally compiled under `MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE`. No dedicated disabled-CU tests were found for the new `opc_cu_2389`, `opc_cu_2936`, or `opc_cu_3147` symbols because those symbols do not exist yet. | Gap: new profile/default and disabled-CU tests are required before optional CU claims are honest. | T006, T009, T024, T030, T031, T043 |
| Write / `CASE-009` Invalid write targets, attributes, types, and ranges | `tests/unit/test_write_service.c` and `tests/unit/test_write_errors.c` cover `MU_STATUS_BAD_TYPEMISMATCH`, `MU_STATUS_BAD_NOTWRITABLE`, `MU_STATUS_BAD_NODEIDUNKNOWN`, `MU_STATUS_BAD_WRITENOTSUPPORTED`, and `MU_STATUS_BAD_TOOMANYOPERATIONS`. | Strong negative coverage exists for core Write Value behaviour; IndexRange invalid-range and StatusCode/Timestamp invalid-field semantics still need targeted tests. | T021, T022, T023, T026, T027, T028, T031 |
| `opc_cu_2400` / `CASE-004` Session Change User | `src/core/service_dispatch/activate_session.c` validates identity tokens, stores compact identity metadata, updates last activity, and emits a new response server nonce. `tests/unit/test_session.c` covers first activation success plus `MU_STATUS_BAD_IDENTITYTOKENINVALID` and `MU_STATUS_BAD_SESSIONIDINVALID`; `tests/unit/test_session_auth.c` covers anonymous and username first activation. | Gap: existing tests prove initial activation, not change-user reactivation on an already activated Session with user/nonce invariants. | T025, T029, T030, T031 |

T003 is complete. Basic Write Values are close to claim-ready after symbol/gating promotion; StatusCode/Timestamp, IndexRange write support, disabled-CU behaviour, and Session Change User reactivation require additional tests and likely behaviour work.

## T004 Diagnostics Proof Audit

T004 maps diagnostics evidence to `SCN-003`, `CASE-005`, and `CASE-007`.

| CU / Case | Evidence Found | Coverage Status | Follow-up Task |
|-----------|----------------|-----------------|----------------|
| `opc_cu_3192` / `CASE-005` Base Info Diagnostics address-space exposure | `src/cu/core_2022_server/diagnostics/diagnostics.c` maintains bounded caller-owned counters for sessions, requests, security rejections, and subscriptions under `MUC_OPCUA_CU_DIAGNOSTICS`. `src/address_space/base_nodes.c` exposes ServerDiagnostics, ServerDiagnosticsSummary, and EnabledFlag under the Server object when diagnostics are compiled in. `tests/unit/test_diagnostics.c` verifies counter increments/decrements, rejected/security counters, ServerDiagnosticsSummary encoding as ServerDiagnosticsSummaryDataType ExtensionObject, and EnabledFlag Boolean true. | Strong evidence exists for OPC-10000-5 sections 6.3.1, 6.3.3, 8.3.2, and 12.9, but the current guard is the legacy diagnostics symbol rather than dedicated `opc_cu_3192` claim metadata. | T032, T035, T036, T038, T039 |
| `opc_cu_3983` / `CASE-005` Base Services Diagnostics returned diagnostics | `src/services/service_header.c` decodes `RequestHeader.return_diagnostics`; `src/core/service_dispatch/response_encode.c` writes response headers. Current `mu_response_header_encode` always emits an empty DiagnosticInfo encoding mask byte and no string table, with no visible returnDiagnostics mask-dependent behaviour in the read paths. | Gap: available-or-explicit-empty DiagnosticInfo behaviour needs tests and probably implementation alignment before claim promotion for OPC-10000-4 sections 7.32 and 7.38. | T033, T037, T038, T039 |
| Diagnostics / `CASE-007` disabled CU behaviour | `tests/unit/test_diagnostics.c` contains a disabled-build test path when `MUC_OPCUA_SERVER_DIAGNOSTICS` is off, but no direct evidence exists for the new `opc_cu_3192` and `opc_cu_3983` dedicated symbols because they do not exist yet. | Gap: disabled-CU tests must prove no diagnostics CU claim, no fabricated DiagnosticInfo, and no partial conformant diagnostics surface when profile configuration disables diagnostics. | T006, T009, T034, T038, T039, T043 |

T004 is complete. Base Info Diagnostics appears close to claim-ready after dedicated symbol/backing-test promotion; Base Services Diagnostics still needs explicit `returnDiagnostics` tests and implementation alignment.

## Final Validation Evidence

The dedicated-symbol remediation is complete for the claimable CUs. The following
commands passed after T056-T059:

- `python3 scripts/profile_manifest/validate.py --all` -> `manifest: OK`.
- `cmake --build build/pr-check --target format-check` -> pass.
- `cmake --build build/pr-check` -> pass.
- `ctest --test-dir build/pr-check --output-on-failure` -> 136/136 passed.
- `ctest --test-dir build/pr-check --output-on-failure -R 'discovery|browse|write|session|diagnostics'` -> 18/18 passed.
- `scripts/measure_size.sh nano` -> 29,442 bytes of text, 512 bytes of BSS.

The profile-gating script reports 94/95 checks passed. Its only failure is the
pre-existing section 5b expectation that `CU_SUBSCRIPTION_STANDARD` remain on
when `CU_SUBSCRIPTION_BASIC` is disabled; that unrelated profile cascade issue
is outside this feature's Discovery, View, Write, Session, and Diagnostics scope.

`opc_cu_3147` remains intentionally unclaimed under `TODO.md` item `S071-D1`.
Its current `MU_STATUS_BAD_WRITENOTSUPPORTED` result is explicit unsupported
behaviour, not conformance evidence.

## T005 Final M+U Scope Decision

T005 records the final manifest-plus-unit/integration validation scope boundary for each in-scope CU. These decisions preserve `profiles/opcua-profile-manifest.yaml` as the source of truth for defaults and prevent roadmap-title drift from becoming a false nano claim.

| CU | Final Scope Decision | Nano Default Decision | Required Proof Before Claim |
|----|----------------------|-----------------------|-----------------------------|
| `opc_cu_2317` View TranslateBrowsePath | Claim in this feature if T013/T017/T018/T019/T020 pass. | Keep `profile_defaults.nano: true`. | Dedicated CU symbol, focused TranslateBrowsePaths tests, integration evidence, generated artifacts, profile-gating evidence. |
| `opc_cu_2328` Discovery Get Endpoints | Claim in this feature if T011/T015/T018/T019/T020 pass. | Keep `profile_defaults.nano: true`. | Dedicated CU symbol, endpoint/profile/locale filter tests, generated artifacts, profile-gating evidence. |
| `opc_cu_2352` Discovery Find Servers Self | Claim in this feature if T010/T014/T018/T019/T020 pass. | Keep `profile_defaults.nano: true`. | Dedicated CU symbol, self-only FindServers filter tests, generated artifacts, profile-gating evidence. |
| `opc_cu_3530` View Basic 2 | Claim in this feature if T012/T016/T018/T019/T020 pass. | Keep `profile_defaults.nano: true`. | Dedicated CU symbol, Browse/BrowseNext continuation-point tests, generated artifacts, profile-gating evidence. |
| `opc_cu_2389` Attribute Write Values | Claim only for micro and higher or custom opt-in if T021/T026/T030/T031 pass. | Preserve `profile_defaults.nano: false`. | Dedicated CU symbol, Write Value success/error tests, disabled-CU tests, generated artifacts, profile-gating evidence. |
| `opc_cu_2400` Session Change User | Claim only for micro and higher or custom opt-in if T025/T029/T030/T031 pass. | Preserve `profile_defaults.nano: false`. | Dedicated CU symbol, ActivateSession reactivation tests, user/nonce invariants, disabled-CU/profile evidence. |
| `opc_cu_2936` Attribute Write StatusCode & Timestamp | Claim only for micro and higher or custom opt-in if T022/T027/T030/T031 pass. | Preserve `profile_defaults.nano: false`. | Dedicated CU symbol, StatusCode/Timestamp preservation or explicit rejection tests, generated artifacts, profile-gating evidence. |
| `opc_cu_3147` Attribute Write Index | Claim only for micro and higher or custom opt-in if T023/T028/T030/T031 pass. | Preserve `profile_defaults.nano: false`. | Dedicated CU symbol, IndexRange partial-update or explicit invalid-range tests, generated artifacts, profile-gating evidence. |
| `opc_cu_3192` Base Info Diagnostics | Claim only for micro and higher or custom opt-in if T032/T035/T036/T038/T039 pass. | Preserve `profile_defaults.nano: false`. | Dedicated CU symbol, ServerDiagnostics node/counter tests, disabled-CU tests, generated artifacts, profile-gating evidence. |
| `opc_cu_3983` Base Services Diagnostics | Claim only for micro and higher or custom opt-in if T033/T037/T038/T039 pass. | Preserve `profile_defaults.nano: false`. | Dedicated CU symbol, `returnDiagnostics` response-header tests, disabled-CU tests, generated artifacts, profile-gating evidence. |

T005 is complete. The MVP implementation boundary is T001-T020 and claims only `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, and `opc_cu_3530`; optional CUs remain outside nano even if implemented later in this feature.

## T020 US1 Contract Validation Evidence

T020 validates the US1 manifest, generated artifacts, profile gating, and Discovery/View service behaviour for `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, and `opc_cu_3530`.

| Command | Result | Evidence |
|---------|--------|----------|
| `python3 scripts/profile_manifest/validate.py --all` | PASS | Output: `manifest: OK`. Generated `Kconfig`, defconfigs, capacities, claim map, roadmap, build docs, Kconfig parse, claim/test map, naming, OPC help references, and profile resolution are consistent with the manifest. |
| `bash scripts/test_profile_gating.sh` | PASS | Output summary: `95 passed, 0 failed`. Nano, micro/embedded/standard/full cascade defaults, custom profile defaults, facet overrides, capacity overrides, and unimplemented-item non-selectability pass. |
| `ctest --test-dir build/pr-check --output-on-failure -R 'discovery\|browse'` | PASS | After configuring `build/pr-check` and building all selected executables, output summary: `100% tests passed, 0 tests failed out of 7`. Passed tests: `test_browse_limits`, `test_browse_service`, `test_discovery_encode`, `test_discovery_endpoint`, `test_discovery_services`, `test_read_browsename_namespace`, and `test_discovery_endpoint_no_session`. |

T020 is complete. The US1 nano-default Discovery/View CUs have manifest claims, generated artifact rows, profile-gating evidence, and passing Discovery/View service tests. T021-T031 remain needed before optional Write and Session CUs can be claimed.

## Approved T023 Deferral: `opc_cu_3147` Write IndexRange

On 2026-07-14, the user approved deferring T023 and recording the work in `TODO.md` without planning a follow-up workflow.

Evidence for the deferral:

- `src/cu/core_2022_server/attribute_write/write.c` decodes `WriteValue.indexRange` but contains no partial array-update implementation.
- `tests/unit/test_write_errors.c::test_write_service_index_range` verifies the current non-empty range result is `MU_STATUS_BAD_WRITENOTSUPPORTED`.
- `MU_STATUS_BAD_INDEXRANGEINVALID` and `MU_STATUS_BAD_INDEXRANGENODATA` exist in `include/muc_opcua/status.h`, but Write does not currently produce them.

Consequences for spec 071:

- T023 and T028 remain incomplete and explicitly deferred; their checkboxes must not be marked complete.
- `opc_cu_3147` must remain `implementation_state: unimplemented`, with no dedicated claimed/implemented symbol or backing-test claim in T030.
- T030 may promote only claim-ready `opc_cu_2389`, `opc_cu_2400`, and `opc_cu_2936`; it must preserve `opc_cu_3147.profile_defaults.nano: false` and its unimplemented state.
- T031 must validate Write/Session behaviour without claiming `opc_cu_3147`; current `MU_STATUS_BAD_WRITENOTSUPPORTED` remains evidence of explicit unsupported behaviour, not CU conformance.
- T049 must document `opc_cu_3147` as intentionally unclaimed, cite `TODO.md` item `S071-D1`, and state that partial IndexRange writes remain future work.

## T023/T028 Resolution: `opc_cu_3147` Write IndexRange Implemented (2026-07-15)

The 2026-07-14 deferral above is fully superseded: partial array Write IndexRange updates are implemented, and the CU is now claimed with a dedicated Kconfig symbol (see "Promotion" below). All ten in-scope CUs from `specs/071-nano-service-behaviour/cu-audit.md` T005 are now claimed; no deferred CU remains for this feature.

Implementation:

- `write_value_index_range()` and its helpers (`parse_write_index_range()`, `write_index_range_elem_size()`) in `src/core/service_dispatch/attribute_handler.c` parse a strict `start` or `start:end` NumericRange (OPC-10000-4 §7.27: the first integer must be lower than the second), read the Node's current array via `mu_value_source_read`, and merge the WriteValue's array into the selected slice before dispatching the full merged array to the configured write callback.
- Status code mapping, grounded in OPC-10000-4 §5.11.4.2/§5.11.4.4 and §7.27:
  - Malformed syntax, a reversed range, or an in-bounds-syntax range whose end falls outside the current array -> `MU_STATUS_BAD_INDEXRANGEINVALID` (§5.11.4.4 extends this code's Write-specific usage to "the passed IndexRange cannot be written by the Server").
  - `indexRange` provided against a Node whose current value is not an array -> `MU_STATUS_BAD_WRITENOTSUPPORTED` (§5.11.4.2: "writing of indexRange is not possible for the Node").
  - `indexRange` provided but the WriteValue's `Value` is not itself an array -> `MU_STATUS_BAD_TYPEMISMATCH` (§5.11.4.2: "the Value shall be an array even if only one element is being written").
  - Patch array length not equal to the selected range length -> `MU_STATUS_BAD_INDEXRANGEDATAMISMATCH`.
- The merge buffer is a C VLA sized to the Node's own configured array length (fixed at integration time by the address-space owner, not by client-controlled input), consistent with the project's no-heap-by-default embedded memory model; no new heap allocation was introduced.

Test evidence:

- `tests/unit/test_write_service.c::test_write_index_range_updates_array_slice` — range `"1:2"` against `{10, 20, 30, 40}` with patch `{200, 300}` merges to `{10, 200, 300, 40}` and the callback observes the full 4-element array.
- `tests/unit/test_write_service.c::test_write_index_range_rejects_invalid_ranges_without_callback` — `"2:1"` (reversed), `"1:8"` (end out of bounds for a 4-element array), and `"x:y"` (malformed) all return `MU_STATUS_BAD_INDEXRANGEINVALID` with no callback invocation.
- `tests/unit/test_write_errors.c::test_write_service_index_range` — a non-empty range against a scalar (non-array) current value now returns `MU_STATUS_BAD_WRITENOTSUPPORTED`, matching §5.11.4.2 verbatim; this pre-existing test's assertion was already `MU_STATUS_BAD_WRITENOTSUPPORTED` and required no change.

Full validation re-run after the change:

- `ctest --test-dir build/pr-check --output-on-failure -R '^test_write_service$|^test_write_errors$'` -> 2/2 passed.
- `ctest --test-dir build/pr-check --output-on-failure` (full `pr-check` suite) -> 136/136 passed.
- `bash scripts/test_profile_gating.sh` -> 95/95 passed.
- `python3 scripts/profile_manifest/validate.py --all` -> `manifest: OK`.
- `cmake --build build/pr-check --target format-check` -> pass.
- `git diff --check` -> clean.
- `scripts/measure_size.sh nano` -> `29,442` bytes text / `512` bytes BSS, byte-identical to the prior recorded baseline (Write is not compiled into nano at all, so the new code has zero nano footprint).

### Promotion: dedicated symbol and enabled/disabled gating (2026-07-15)

Following the T054-T056 pattern, `opc_cu_3147` is promoted from unclaimed to claimed:

- `profiles/opcua-profile-manifest.yaml`: `opc_cu_3147` now has `kconfig_symbol: "MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE"`, `implementation_state: "claimed"`, `depends_on: ["MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES"]`, `backing_tests: ["test_write_service"]`, and unchanged `profile_defaults` (`nano: false`, `micro`/`embedded`/`standard`/`full`: `true`, `custom`: `false`).
- Regenerated via `python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs`: `Kconfig`, all six `configs/*.defconfig`, `include/muc_opcua/capacities.h`, `tests/conformance/claim_test_map.md`, `docs/conformance/opc-profile-roadmap.md`, and `docs/build-and-gating.md`.
- Wired the new symbol through `CMakeLists.txt` (added to the Kconfig-derived variable import list) and `src/CMakeLists.txt` (`if(MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE) target_compile_definitions(... MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE=1)`), mirroring the `MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP` wiring.
- Gated `write_value_index_range()` and its two static helpers behind `#ifdef MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE` in `src/core/service_dispatch/attribute_handler.c`; the dispatch call site returns `MU_STATUS_BAD_WRITENOTSUPPORTED` (no callback, no mutation) when the symbol is off, mirroring the StatusCode/Timestamp field CU's disabled behaviour.
- Added a RED-verified disabled-CU test, `tests/unit/test_write_service.c::test_write_index_range_fields_off_rejects_without_mutation`: with the symbol undefined, a non-empty `"1:2"` range on a 4-element array returns `MU_STATUS_BAD_WRITENOTSUPPORTED`, the callback is never invoked, and the underlying array is unmodified. The two existing enabled-path tests were wrapped in `#ifdef MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE` / `#else` alongside it, matching the `opc_cu_2936` enabled/disabled test-selection idiom already used in this file.
- RED confirmed before gating the production code (disabled test observed `MU_STATUS_GOOD` instead of the expected `MU_STATUS_BAD_WRITENOTSUPPORTED`, since the merge ran unconditionally); GREEN confirmed after gating, then again after reconfiguring `build/pr-check` (profile `full`, where `opc_cu_3147` defaults on) so the enabled-path tests exercise the real merge behaviour rather than falling back to the disabled branch.

Re-validation after promotion:

- `ctest --test-dir build/pr-check --output-on-failure` -> 136/136 passed (enabled-path IndexRange tests confirmed running via direct executable output, not just the disabled fallback).
- `bash scripts/test_profile_gating.sh` -> 95/95 passed.
- `python3 scripts/profile_manifest/validate.py --all` -> `manifest: OK`.
- `cmake --build build/pr-check --target format-check` -> pass.
- `git diff --check` -> clean.
- `scripts/measure_size.sh nano` -> `29,442` bytes text / `512` bytes BSS, still byte-identical (`opc_cu_3147.profile_defaults.nano` remains `false`, so the promoted CU compiles out of nano exactly as before).

`opc_cu_3147` is now `implementation_state: claimed` in the manifest, matching the other nine in-scope CUs from this feature. No CU from the T005 scope list remains deferred or unclaimed.

Remaining gap: `opc_cu_3147` is still `implementation_state: unimplemented` in `profiles/opcua-profile-manifest.yaml` with `kconfig_symbol: null`. Promoting the claim requires the same dedicated-symbol/gating-test/artifact-regeneration workflow used for `opc_cu_2389`/`opc_cu_2936`/`opc_cu_2400` (T054-T056): add a dedicated `MUC_OPCUA_CU_*` symbol, enabled/disabled gating tests, regenerated profile artifacts, and a manifest promotion. That follow-up is tracked as `TODO.md` item `S071-D1` and is intentionally out of scope for this change.
