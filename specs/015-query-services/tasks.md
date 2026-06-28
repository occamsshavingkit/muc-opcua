# Tasks: Query Services

**Input**: Design documents from `specs/015-query-services/`
**Prerequisites**: plan.md, spec.md

**Tests**: Tests are mandatory for protocol parsing, serialization, service dispatch, StatusCodes, SecureChannel/session state, address-space behavior, security behavior, and wire-visible changes.

## Phase 1: Setup & Definition

**Purpose**: Core definitions for the Query services and ContentFilter parsing.

- [x] `T001` [US1] Define Query service structures: `mu_query_first_request_t`, `mu_query_first_response_t`, `mu_query_next_request_t`, `mu_query_next_response_t` in `include/micro_opcua/services/query.h`. Include OPC-10000-4 5.9.3 and 5.9.4 references.
- [ ] `T002` [US1] Define `ContentFilter` and `ContentFilterElement` structures in `include/micro_opcua/services/query.h`. Include subset of standard operators (e.g., `Equals`, `Like`) per OPC-10000-4 7.7.
- [ ] `T003` [US1] Register `QueryFirstRequest_Encoding_DefaultBinary` (533), `QueryFirstResponse` (536), `QueryNextRequest` (547), `QueryNextResponse` (550) IDs in `include/micro_opcua/opcua_ids.h`.

## Phase 2: Encoding & Decoding (US1 & US2)

**Purpose**: Binary parsing for queries.

- [ ] `T004` [P] [US1] Implement `mu_content_filter_decode` in `src/encoding/binary_query.c` to parse a `ContentFilter` (array of `ContentFilterElement`).
- [ ] `T005` [US1] Implement `mu_query_first_request_decode` in `src/encoding/binary_query.c`.
- [ ] `T006` [US1] Implement `mu_query_first_response_encode` in `src/encoding/binary_query.c`.
- [ ] `T007` [US2] Implement `mu_query_next_request_decode` and `mu_query_next_response_encode` in `src/encoding/binary_query.c`.
- [ ] `T008` [US1] Write unit tests in `tests/unit/test_query_encoding.c` to verify encoding/decoding of QueryFirst and ContentFilters.

## Phase 3: Service Logic (US1 & US2)

**Purpose**: Implementation of the search capability.

- [ ] `T009` [US1] Implement `mu_evaluate_content_filter` in `src/services/query.c` to evaluate a filter against a specific node.
- [ ] `T010` [US1] Implement `mu_query_first_process` in `src/services/query.c` to scan the static/dynamic address space, apply the filter, and populate the response, generating a `ContinuationPoint` if `MaxQueryResults` is hit.
- [ ] `T011` [US2] Implement `mu_query_next_process` in `src/services/query.c` to resume the scan from a given `ContinuationPoint`, or release it if requested.
- [ ] `T012` [US1] Wire up `mu_query_first_process` and `mu_query_next_process` in `src/core/service_dispatch.c` (`handle_query_first` and `handle_query_next`).
- [ ] `T013` [US1] Add `QueryFirst` / `QueryNext` to the test coverage suite. Create `tests/unit/test_query_service.c` validating end-to-end processing and correct `ContinuationPoint` behavior.

## Phase 4: Integration, Limits, and Docs

**Purpose**: Final compliance, testing, and memory validation.

- [ ] `T014` [US1] Add `MU_MAX_QUERY_CONTINUATION_POINTS` (default 2) to `include/micro_opcua/config.h` and increase `MU_SERVER_STORAGE_BYTES` if query services are enabled via `-DMICRO_OPCUA_SERVICE_QUERY=1`.
- [ ] `T015` [US1] Run `measure_size.sh` to capture the flash/RAM impact.
- [ ] `T016` [US1] Add `binary_query.c`, `query.c`, and `test_query_service.c` to traceability matrix `docs/traceability/files-to-sections.md`.
- [ ] `T017` [US1] Run formatting, cppcheck, and ensure all tests pass (CTEST).
