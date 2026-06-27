# Tasks: Embedded Profile Completion

**Input**: Design documents from `/specs/005-embedded-profile-completion/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Mandatory for all protocol parsing, serialization, service dispatch, StatusCodes,
address-space behavior, and wire-visible changes (constitution Principle IV). Protocol tests and
fixtures appear before the implementation they validate.

**Target profile**: Embedded 2017 UA Server Profile (OPC-10000-7 §6.6.69) = Micro 2017 +
Basic256Sha256 + Standard DataChange Subscription 2017 Server Facet (§6.6.17) + `Base Info Type
System`. Authoritative membership: `research.md`.

## Execution ownership (project labor division)

- **Codex writes implementation only** — `src/**`, `include/**`, `Makefile`/`CMakeLists.txt`
  wiring. One task at a time, given the task line text + a strict file allowlist; Codex never
  builds or runs anything.
- **Claude does everything else** — writes all test code and fixtures (`tests/**`), runs every
  build / sanitizer / fuzz / cross-compile, measures flash/RAM, validates StatusCodes and
  citations, and authors the citation-heavy traceability / conformance / size docs.

Tasks below are tagged `{Codex}` or `{Claude}` accordingly.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1–US4 (maps to spec.md user stories)
- Exact file paths + OPC UA part/section references included

## Path Conventions

- Core protocol: `src/core/`, `src/encoding/`, `src/services/`
- Address space: `src/address_space/`
- Tests: `tests/unit/`, `tests/fixtures/`, `tests/fuzz/`
- Docs: `docs/conformance/`, `docs/traceability/`, `docs/size/`

---

## Phase 1: Setup (Shared Infrastructure)

- [X] T001 [P] {Claude} Create traceability skeleton in `docs/traceability/005-embedded-profile-completion.md` (sections per US1–US4, empty file/test maps to fill in).
- [X] T002 [P] {Claude} Create fixture area `tests/fixtures/embedded/` with a `README.md` noting each fixture's OPC-10000 provenance.

---

## Phase 2: Foundational (Blocking Prerequisites)

**CRITICAL**: Complete before any user-story implementation.

- [X] T003 {Claude} Record the authoritative target profile and the CU→OPC-section map (from `research.md`) in `docs/traceability/005-embedded-profile-completion.md` — the conformance claim basis for all stories.
- [X] T004 {Codex} Declare new capacity/config macros (`MU_MONITORED_QUEUE_DEPTH` default 2, `MU_MAX_TRIGGER_LINKS` default per design) and document the `embedded` raised maxima (`MU_MAX_MONITORED_ITEMS`≥100, `MU_MAX_SUBSCRIPTIONS`≥2, `MU_MAX_PUBLISH_REQUESTS`≥5) as comments, **declarations/defaults only, no logic**, in `src/services/subscription.h` (guarded by `MICRO_OPCUA_SUBSCRIPTIONS`).

**Checkpoint**: Foundation ready — user stories can begin.

---

## Phase 3: User Story 1 - Standard DataChange Subscription facet delta (Priority: P1) [MVP]

**Goal**: Absolute-deadband filter, queue≥2/discardOldest/overflow, SetTriggering, and the
Standard-facet capacities, over the existing no-heap subscription engine.

**Independent Test**: Drive the embedded build with a deadband filter, a deep queue, a
SetTriggering link, and counts up to the facet minimums; assert OPC-10000-4 responses; `micro`
unchanged.

### Tests for User Story 1 (write first, confirm failing) — {Claude}

- [X] T005 [P] [US1] {Claude} Fixture: CreateMonitoredItems carrying a DataChangeFilter with DeadbandType=Absolute (OPC-10000-4 §7.22.2), bytes in `tests/fixtures/embedded/`.
- [X] T006 [P] [US1] {Claude} Test: absolute-deadband sampling — sub-threshold change → no notification, at/above → one notification; status change always reports (§7.22.2) in `tests/unit/test_subscriptions_deadband.c`.
- [X] T007 [P] [US1] {Claude} Test: monitored-item queue≥2 + discardOldest (and discard-newest) with overflow InfoBit (§5.13.2, §7.20.1) in `tests/unit/test_subscriptions_queue.c`.
- [ ] T008 [P] [US1] {Claude} Test: SetTriggering add/remove links + triggered (Sampling-mode) item reports on triggering event; per-link StatusCodes (§5.13.5) in `tests/unit/test_subscriptions_triggering.c`.
- [ ] T009 [P] [US1] {Claude} Test: capacity enforcement — accept ≥100 items / ≥2 subs / ≥5 parallel Publish, reject beyond with `Bad_TooManyMonitoredItems`/`Bad_TooManySubscriptions`/`Bad_TooManyPublishRequests` (§5.13.2, §5.14.2, §5.14.5) in `tests/unit/test_subscriptions_capacity.c`.
- [ ] T010 [P] [US1] {Claude} Test: malformed DataChangeFilter / SetTriggering decode → cited StatusCodes, no OOB (ASan) in `tests/unit/test_subscriptions_errors.c`.
- [ ] T011 [P] [US1] {Claude} Fuzz: SetTriggering and DataChangeFilter decoders in `tests/fuzz/fuzz_subscription_filters.c`.

### Implementation for User Story 1 — {Codex} (one task at a time)

- [X] T012 [US1] {Codex} Extend `mu_monitored_item_t` with deadband fields (`deadband_type`, `deadband_value`), a fixed queue ring (`MU_MONITORED_QUEUE_DEPTH`) + `discard_oldest`/`queue_overflow`, and trigger-link storage, in `src/services/subscription.h`.
- [X] T013 [US1] {Codex} Implement absolute-deadband evaluation in the sampling path (numeric integer + soft-float compare; status changes bypass deadband) (§7.22.2) in `src/services/subscription.c`.
- [X] T014 [US1] {Codex} Implement the notification queue (depth≥2), discardOldest/discard-newest, and overflow InfoBit signaling in the publish path (§5.13.2, §7.20.1) in `src/services/subscription.c`.
- [ ] T015 [US1] {Codex} Implement SetTriggering link storage and triggered-item reporting on triggering events (§5.13.5, §5.13.1.6) in `src/services/subscription.c`.
- [ ] T016 [US1] {Codex} Enforce the raised capacities and return cited `Bad_TooMany*` StatusCodes (§5.13.2, §5.14.2, §5.14.5) in `src/services/subscription.c`.
- [X] T017 [US1] {Codex} Decode the DataChangeFilter deadband fields in the CreateMonitoredItems/ModifyMonitoredItems handlers, rejecting non-numeric/percent with cited StatusCodes (§7.22.2) in `src/core/service_dispatch.c`.
- [ ] T018 [US1] {Codex} Add the SetTriggering request decode, dispatch handler, and response encode (§5.13.5) in `src/core/service_dispatch.c`.
- [ ] T019 [US1] {Claude} Run T006–T011 + full suite, ASan, and record results; iterate with Codex on failures.
- [ ] T020 [US1] {Claude} Measure US1 flash/RAM delta and update `docs/traceability/005-embedded-profile-completion.md` (file/test maps + size).

**Checkpoint**: US1 independently testable; `micro` regression-clean; size known.

---

## Phase 4: User Story 2 - Base Info Type System exposure (Priority: P2)

**Goal**: Expose the namespace-0 type system the server uses (`Base Info Type System` CU) with
HasSubtype/HasTypeDefinition and a ServerProfileArray advertising the profile.

**Independent Test**: Browse the Types hierarchy and resolve an instance's HasTypeDefinition;
read ServerProfileArray.

### Tests for User Story 2 (write first, confirm failing) — {Claude}

- [ ] T021 [P] [US2] {Claude} Test: browse namespace-0 ReferenceType/DataType/ObjectType/VariableType nodes present with correct NodeClass/BrowseName/HasSubtype (OPC-10000-5, OPC-10000-3) in `tests/unit/test_type_system.c`.
- [ ] T022 [P] [US2] {Claude} Test: an exposed instance's HasTypeDefinition resolves to its type node (OPC-10000-3 §7.7) in `tests/unit/test_type_system.c`.
- [ ] T023 [P] [US2] {Claude} Test: read `Server.ServerProfileArray` advertises the Embedded 2017 profile URI (OPC-10000-5) in `tests/unit/test_type_system.c`.
- [ ] T024 [P] [US2] {Claude} Regression: `nano`/`micro` golden browse/read vectors unchanged in `tests/unit/test_view_services.c` (and any golden-vector test) — confirm the added type nodes are profile-gated and do not alter lower-profile output.

### Implementation for User Story 2 — {Codex} (one task at a time)

- [ ] T025 [US2] {Codex} Add the standard ReferenceType nodes the server uses (References, HierarchicalReferences, HasChild, HasComponent, HasProperty, Organizes, HasTypeDefinition, HasSubtype, …) with IsAbstract/Symmetric/InverseName and HasSubtype links (OPC-10000-5) in `src/address_space/base_nodes.c`.
- [ ] T026 [US2] {Codex} Add the standard DataType nodes underpinning exposed values (BaseDataType subtree used) with HasSubtype (OPC-10000-5) in `src/address_space/base_nodes.c`.
- [ ] T027 [US2] {Codex} Add the standard ObjectType/VariableType nodes used (BaseObjectType, BaseVariableType, BaseDataVariableType, PropertyType, ServerType, …) with HasSubtype (OPC-10000-5) in `src/address_space/base_nodes.c`.
- [ ] T028 [US2] {Codex} Add HasTypeDefinition references from exposed instance nodes to their type nodes (OPC-10000-3 §7.7) in `src/address_space/base_nodes.c`.
- [ ] T029 [US2] {Codex} Populate `ServerProfileArray` with the Embedded 2017 profile URI and align ServerCapabilities with actual limits (OPC-10000-5) in `src/address_space/base_nodes.c`.
- [ ] T030 [US2] {Claude} Run T021–T024 + full suite; confirm nano/micro unchanged; record results and update traceability + size.

**Checkpoint**: US1 and US2 both independently functional.

---

## Phase 5: User Story 3 - Method Call: GetMonitoredItems & ResendData (Priority: P3)

**Goal**: A minimal Call service implementing GetMonitoredItems and ResendData; all other
methods rejected. Depends on US1 (engine) and US2 (method-node address-space pattern).

**Independent Test**: Call GetMonitoredItems/ResendData and an unknown method; verify outputs
and StatusCodes.

### Tests for User Story 3 (write first, confirm failing) — {Claude}

- [ ] T031 [P] [US3] {Claude} Fixture: CallRequest bytes for GetMonitoredItems and ResendData (OPC-10000-4 §5.11) in `tests/fixtures/embedded/`.
- [ ] T032 [P] [US3] {Claude} Test: GetMonitoredItems returns matching serverHandles[]/clientHandles[] for a subscription (§5.11; OPC-10000-5) in `tests/unit/test_method_call.c`.
- [ ] T033 [P] [US3] {Claude} Test: ResendData causes the next Publish to re-report all monitored items' current values (§5.11; OPC-10000-5) in `tests/unit/test_method_call.c`.
- [ ] T034 [P] [US3] {Claude} Test: unknown method / wrong object / bad argument count-or-type → cited StatusCodes (`Bad_MethodInvalid`/`Bad_NodeIdInvalid`/`Bad_InvalidArgument`, §5.11) in `tests/unit/test_method_call_errors.c`.
- [ ] T035 [P] [US3] {Claude} Fuzz: Call request decoder in `tests/fuzz/fuzz_call.c`.

### Implementation for User Story 3 — {Codex} (one task at a time)

- [ ] T036 [US3] {Codex} Add GetMonitoredItems and ResendData method nodes on the Server object (HasComponent) with Input/OutputArguments Properties (OPC-10000-5 — pin the exact section/NodeIds via the OPC MCP) in `src/address_space/base_nodes.c`.
- [ ] T037 [US3] {Codex} Add the `resend_data_pending` flag to the subscription and drain it (re-report current values) in `mu_subscriptions_tick` in `src/services/subscription.c`.
- [ ] T038 [US3] {Codex} Implement GetMonitoredItems handle enumeration (serverHandles/clientHandles for a subscription) in `src/services/subscription.c`.
- [ ] T039 [US3] {Codex} Add the Call request decode, method routing (the two methods; reject others with cited StatusCodes), and CallResponse encode (§5.11) in `src/core/service_dispatch.c`.
- [ ] T040 [US3] {Claude} Run T032–T035 + full suite; record results; update traceability + size.

**Checkpoint**: US1–US3 independently functional.

---

## Phase 6: User Story 4 - Conformance declaration, wiring & docs (Priority: P4)

**Goal**: Wire the embedded build to the facet capacities, advertise the profile, and document
conformance at `profile-targeting`.

**Independent Test**: Build `embedded`; confirm advertised surface; review conformance docs;
confirm nano/micro unchanged.

- [ ] T041 [US4] {Codex} Add the `embedded` capacity `-D` overrides (`MU_MAX_MONITORED_ITEMS=100 MU_MAX_SUBSCRIPTIONS=2 MU_MAX_PUBLISH_REQUESTS=5 MU_MONITORED_QUEUE_DEPTH=2`) to the `embedded` target in `Makefile` and the corresponding CMake wiring, leaving `nano`/`micro` unchanged.
- [ ] T042 [US4] {Claude} Create `docs/conformance/profile-embedded.md` enumerating each implemented Embedded conformance unit with exact OPC-10000 citations at status `profile-targeting`, **including a row verifying the `Security Default ApplicationInstance Certificate` CU is satisfied by the existing certificate handling** (research Decision 5).
- [ ] T043 [US4] {Claude} Update `docs/conformance/status.md` — `embedded` now targets the full Embedded 2017 UA Server Profile (subscriptions Standard facet + Type System), still CTT-pending.
- [ ] T044 [US4] {Claude} Update `docs/conformance/profile-micro.md` "Out of scope (Embedded tier)" note for the research corrections (TransferSubscriptions = Client Redundancy Facet; percent deadband = Data Access Facet; SetTriggering/absolute-deadband/Methods are the Standard-facet delta).
- [ ] T045 [US4] {Claude} Refresh `docs/size/feature-size-ledger.md` with measured `embedded` flash/RAM (raised capacities + type nodes + Call), confirming `.bss`=0 and heap=0.

**Checkpoint**: Embedded profile claim is documented and CTT-targeting.

---

## Phase 7: Compliance, Size, and Polish — {Claude}

- [ ] T046 {Claude} Run host unit + integration suite (`make test`); all green.
- [ ] T047 {Claude} Run ASan/UBSan build and suite; clean.
- [ ] T048 {Claude} Run fuzz targets (or document unavailable tooling).
- [ ] T049 {Claude} Run clang-format + static analysis (cppcheck/clang-tidy); clean.
- [ ] T050 {Claude} Run the Pico cross-compile (`MICRO_OPCUA_PLATFORM=pico`); builds.
- [ ] T051 {Claude} Measure flash/RAM vs the plan budget; confirm `.bss`=0/heap=0; record in the size ledger.
- [ ] T052 {Claude} Verify `nano`/`micro` are unchanged in surface and footprint (golden regression + size).
- [ ] T053 {Claude} Verify every unsupported/over-capacity case returns the cited OPC UA StatusCode.
- [ ] T054 {Claude} Validate `quickstart.md` on host; verify `docs/traceability/005-embedded-profile-completion.md` maps each impl/test file to OPC sections.

---

## Dependencies & Execution Order

- **Setup (P1)** → **Foundational (P2)** → **US1 (P3)** → **US2 (P4)** → **US3 (P5)** → **US4 (P6)** → **Polish (P7)**.
- **US1** depends only on Foundational. **US2** depends only on Foundational (independent of US1).
- **US3** depends on **US1** (subscription engine: ResendData/GetMonitoredItems) and **US2** (method-node address-space pattern).
- **US4** depends on US1–US3. **Polish** depends on all.

### PR strategy (one PR per user story; merge when CI green)

- PR-A: Setup + Foundational + **US1** (the MVP) — includes the spec-kit planning docs.
- PR-B: **US2**. PR-C: **US3**. PR-D: **US4** + Polish.
  Each PR: Claude writes/runs tests → dispatches Codex per implementation task → Claude
  builds/measures/validates → squash-merge when CI green, then continue.

### Within each user story

- Fixtures + tests (Claude) before implementation (Codex).
- Header/struct extension before the logic that uses it (T012 before T013–T016).
- Decoder/handler before dispatch wiring; StatusCode mapping before integration.
- Traceability + size records before the checkpoint.

### Parallel opportunities

- All [P] test tasks within a story (different files) can be written together by Claude.
- US1 and US2 implementation could proceed in parallel (different files: `subscription.c` vs
  `base_nodes.c`) but the PR-per-story strategy sequences them for clean review/merge.

---

## Notes

- `[P]` = different files, no dependencies. `[Story]` maps to spec.md.
- Protocol tasks cite OPC UA part/section; tests fail before implementation.
- Codex receives one task's line text + a file allowlist; Codex never builds/runs.
- Claude runs every build/test/measure/validate and writes all tests + citation-heavy docs.
- Same-file Codex tasks (e.g. T013–T016 in `subscription.c`) run sequentially in ID order.
- Commit after each task or logical group; stop at checkpoints to validate behavior,
  conformance traceability, and size.
- No hidden heap, no mutable globals, no uncited protocol behavior; `nano`/`micro` must stay
  byte-identical in surface and footprint.
