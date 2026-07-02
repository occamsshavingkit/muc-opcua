---
description: "Task list for feature 028 — conformance-test hardening (per-profile evidence)"
---

# Tasks: Conformance-Test Hardening

**Input**: Design documents from `/specs/028-conformance-test-hardening/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: This feature IS testing. Test-first per Constitution IV. Each task is a
single indivisible job: a failing test and the code that satisfies it are separate
tasks; a per-profile assertion and the doc it reconciles are separate tasks;
"register claim-map rows" is its own task and the checker passing is the phase
checkpoint's acceptance, not bundled into other work.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1 (per-profile keystone) / US2 (fix defects+claims) / US3 (real-crypto security) / US4 (forcing tests) / US5 (encoding)

## Path Conventions

build/CI `CMakeLists.txt`, `Makefile`, `.github/workflows/ci.yml` · claim map `tests/conformance/` · tests `tests/{unit,integration,fuzz,fixtures}/` · code `src/…` · docs `docs/{conformance,traceability}/`

## Reconciliation decisions (resolved from research Decision 5 — no undecided branches remain)

- Nano Base-Information & RegisterNodes: **doc-correct** (narrow the Nano claim to what the profile builds; pulling `BASE_NODES`/RegisterNodes into Nano would break the Nano size anchor).
- Micro ServerProfileArray URI: **doc-correct** (Micro has `BASE_NODES` OFF; remove/curate the `MicroEmbeddedDevice2017` claim).
- `Bad_NodeIdExists`: **implement** on AddNodes duplicate (cheap, genuine). The other four never-emitted codes (`Bad_NodeIdRejected`, `Bad_SessionClosed`, `Bad_TcpServerTooBusy`, `Bad_Timeout`): **remove** the claims (not required by targeted facets).
- Guid / DiagnosticInfo built-ins: **scope the claim** to UADP where actually encoded/tested.

---

## Phase 1: Setup

- [x] T001 [P] Record the size baseline: run `scripts/measure_size.sh all` and save the per-profile `.text`/`data`/`bss` figures in quickstart.md.
- [x] T002 [P] Record the full-profile test baseline: a green `ctest` run of the current `full` build, noted as the pre-change reference.
- [x] T003 [P] Enumerate the current per-profile test set: for nano/micro/embedded/full, record which `tests/unit` + `tests/integration` targets the `RUN_TEST` gates currently include (baseline for US1 gate-liveness).

---

## Phase 3: User Story 1 — Per-profile conformance evidence (Priority: P1) 🎯 keystone

**Goal**: tests build+run per profile; gates are live; a claimed unit without a
profile-runnable test fails. **Checkpoint acceptance**: all four profile suites +
ASAN green; the checker enforces the map.

- [x] T004 [US1] Scope the "force full" rule in `CMakeLists.txt:16-20` so an explicit `-DMUC_OPCUA_PROFILE=<p>` with `-DMUC_OPCUA_BUILD_TESTS=ON` is honored (default with no explicit profile still resolves to `full`). Uses `MUC_OPCUA_PROFILE_EXPLICIT`. Research Decision 1.
- [x] T005 [US1] Add `Makefile` targets `test-nano`, `test-micro`, `test-embedded`, `test-full` that configure+build+`ctest` the respective profile (host).
- [x] T006 [US1] Add per-profile build+ctest jobs (nano/micro/embedded/full) to `.github/workflows/ci.yml`; a profile that fails to build fails the job (no skip).
- [x] T007 [US1] Verify gate-liveness: confirm a SUBSCRIPTIONS-gated test runs in micro/embedded/full and is absent in nano; record the differing test counts in quickstart.md. (Any gate found mis-wired is filed as its own follow-up task, not fixed inline.)
- [x] T008 [US1] Create the claim→test map artifact under `tests/conformance/` (checked manifest or ctest labels) per `contracts/claim-to-test-map.md`: one row per claimed conformance unit → backing test(s) + profiles + kind, seeded from `docs/conformance/*` + `docs/traceability/conformance-claims.md`. Grounding: OPC-10000-7 §4.2 (Conformance Units and Conformance Groups).
- [x] T009 [US1] Implement the checker in `tests/conformance/` that, for the profile it is compiled in, fails when a claimed unit for this profile has no registered+passing backing test (and when a doc claim has no map row). Per contract semantics. Grounding: OPC-10000-7 §4.2, §4.3 (profile/conformance-unit membership).
- [x] T010 [US1] Wire the checker into every profile's `ctest` run (T005/T006).
- [x] T011 [US1] Enforcement proof: remove a backing test (or its gate) for a mapped claim, confirm the claimed profile's build fails naming the unit, then restore. (SC-002 proof.)
- [x] T012 [US1] Run all four profile suites + ASAN + `scripts/measure_size.sh all`; confirm green within gates.

**Checkpoint**: US1 shippable — per-profile evidence + enforced claim→test map. **THIS IS THE MVP.**

---

## Phase 4: User Story 2 — Fix audited defects & reconcile false claims (Priority: P1)

**Goal**: each audited defect fixed or its claim corrected honestly, pinned by a
per-profile test.

- [X] T013 [P] [US2] Add a failing test in `tests/unit/test_browse_limits.c`: Browse with `>` node cap expects `Bad_TooManyOperations` (RED against current `Bad_InternalError`). OPC-10000-4 §5.9.2/§7.38.2.
- [X] T014 [US2] Fix `src/services/browse.c:103` to return `Bad_TooManyOperations` on node overflow (satisfies T013). OPC-10000-4 §5.9.2 / §7.38.2. Depends T013.
- [X] T015 [P] [US2] Add a per-profile test asserting what the Nano build actually exposes for the Server object / ServerStatus / ServerProfileArray / NamespaceArray (present or absent under `BASE_NODES` OFF). OPC-10000-5 (Server object / ServerStatus / ServerProfileArray) / OPC-10000-7 Core Server Facet.
- [X] T016 [US2] Correct `docs/conformance/profile-nano.md` Core-Server-Facet / Base-Information claim to match the Nano build (doc-correct per decision). OPC-10000-7 Core Server Facet / OPC-10000-5 Base Information. Depends T015.
- [X] T017 [P] [US2] Add a per-profile test asserting Nano returns the unsupported StatusCode for RegisterNodes/UnregisterNodes (built only in full). OPC-10000-4 §5.9.5 / §5.9.6.
- [X] T018 [US2] Correct the Nano "full View Service Set incl. RegisterNodes" claim in `docs/conformance/profile-nano.md` and `docs/conformance/services.md`. OPC-10000-4 §5.9.5 / §5.9.6; OPC-10000-7 View facet. Depends T017.
- [X] T019 [P] [US2] Remove/curate the Micro `MicroEmbeddedDevice2017` ServerProfileArray URI claim in `docs/conformance/profile-micro.md` (doc-correct; Micro has no base nodes). OPC-10000-7 §4.3 (ServerProfileArray / profile URIs) / OPC-10000-5.
- [X] T020 [P] [US2] Add a failing test: AddNodes with a duplicate NodeId expects `Bad_NodeIdExists` (RED). OPC-10000-4 §5.8 (NodeManagement / AddNodes) / §7.38.2.
- [X] T021 [US2] Implement the duplicate-NodeId → `Bad_NodeIdExists` path in `src/services/node_management.c` (satisfies T020). OPC-10000-4 §5.8 (AddNodes). Depends T020.
- [X] T022 [P] [US2] Remove the four unbacked StatusCode claims (`Bad_NodeIdRejected`, `Bad_SessionClosed`, `Bad_TcpServerTooBusy`, `Bad_Timeout`) from `docs/traceability/statuscodes.md`. OPC-10000-4 §7.38.2 (StatusCode set) — no emission site exists for these.
- [X] T023 [P] [US2] Scope the Guid / DiagnosticInfo built-in-type rows in the type traceability to UADP (where each is actually encoded/tested), not the binary service codec. OPC-10000-6 §5.2.2.6 (Guid) / §5.2.2.11 (DiagnosticInfo); OPC-10000-14 (UADP).
- [X] T024 [US2] Register the US2 claims (Browse limit, Nano base-info, RegisterNodes, Micro URI, NodeIdExists, removed codes, Guid/DiagnosticInfo scope) as rows in the claim→test map (T008).

**Checkpoint**: US2 shippable — no known false claims; audited defect fixed; checker green for all profiles.

---

## Phase 5: User Story 3 — Real-crypto security acceptance/reject (Priority: P1)

**Goal**: each security accept+reject path exercised on a real crypto backend.

- [X] T025 [P] [US3] Add cert fixtures/generator in `tests/fixtures/`: a real expired cert, a not-yet-valid cert, and an out-of-key-size cert; document generation. Feeds T029/T030.
- [X] T026 [US3] Real-crypto X509 (i=327) user-token ActivateSession test: correctly-signed → accepted, bad signature → rejected (host adapter), replacing the mock-only path. OPC-10000-4 §5.7.3 (ActivateSession / UserIdentityToken signature).
- [X] T027 [US3] UserName/password over a secured channel: valid password → Good, wrong password → `Bad_IdentityTokenRejected` (real backend). OPC-10000-4 §5.7.3 / §7.37 (UserNameIdentityToken).
- [X] T028 [US3] Encrypted-password + ServerNonce anti-replay: correct nonce → accepted, wrong/stale nonce → rejected. OPC-10000-4 §5.6.3.2 (ServerNonce anti-replay) / §7.37.
- [X] T029 [P] [US3] Certificate validity rejection with real certs: expired and not-yet-valid → `Bad_CertificateTimeInvalid` (uses T025 fixtures). OPC-10000-4 §5.5 (certificate validation) / OPC-10000-6 §6.1.3.
- [X] T030 [P] [US3] Out-of-bounds RSA key size → `Bad_SecurityChecksFailed` (uses T025 fixture). OPC-10000-7 Basic256Sha256 asymmetric key-size bounds.
- [X] T031 [US3] End-to-end fail-closed trust: `allow_untrusted_clients=false` + client cert absent from trust list → OPN rejected. OPC-10000-4 §5.5 / §6.1.3 (application authentication / trust).
- [X] T032 [US3] Make all US3 tests `TEST_IGNORE` (skip visibly) when no crypto backend is compiled, and register them to run under embedded/full.
- [X] T033 [US3] Register the US3 claims as rows in the claim→test map (T008).

**Checkpoint**: US3 shippable — security acceptance proven with real crypto; checker green.

---

## Phase 6: User Story 4 — Force implemented-but-untested StatusCodes / negative paths (Priority: P2)

**Goal**: every implemented-but-untested code/path from the audit has a forcing test.

- [ ] T034 [P] [US4] Force `Bad_TooManySessions`: drive CreateSession beyond `MU_MAX_SESSIONS` and assert it. OPC-10000-4 §5.7.2 (CreateSession) / §7.38.2.
- [ ] T035 [P] [US4] Force `Bad_RequestTooLarge`: drive an oversized request/OPN and assert it (`asym_chunk.c` emission). OPC-10000-6 §7.1.5 (UA-TCP error) / §6.7.2.
- [X] T036 [P] [US4] Force Read beyond `MU_DISPATCH_MAX_READ_NODES` → `Bad_TooManyOperations` in `tests/unit/test_read_service.c`. OPC-10000-4 §5.11.2 (Read) / §7.38.2.
- [X] T037 [P] [US4] Force `Bad_HistoryOperationUnsupported` (history.c emission). OPC-10000-4 §5.11.5 (HistoryUpdate) / OPC-10000-11.
- [ ] T038 [P] [US4] Force `Bad_MessageNotAvailable` (subscription.c republish emission). OPC-10000-4 §5.14.6.3 (Republish).
- [ ] T039 [P] [US4] Force `Bad_NotFound` (node_management.c DeleteReferences emission). OPC-10000-4 §5.8 (DeleteReferences).
- [ ] T040 [US4] Drive a CloseSecureChannel (CLO) message through `mu_server_poll` and assert the channel is observably closed. OPC-10000-4 §5.6.3 (CloseSecureChannel) / OPC-10000-6 §6.7.3.
- [ ] T041 [P] [US4] SetMonitoringMode invalid MonitoredItemId → per-item invalid-id StatusCode. OPC-10000-4 §5.13.4 / §7.38.2.
- [ ] T042 [P] [US4] SetPublishingMode invalid SubscriptionId → per-item invalid-id StatusCode. OPC-10000-4 §5.14.4 / §7.38.2.
- [ ] T043 [US4] Register the US4 claims as rows in the claim→test map (T008).

**Checkpoint**: US4 shippable — implemented behaviors verified; checker green.

---

## Phase 7: User Story 5 — Encoding/transport coverage (Priority: P2)

**Goal**: every wire-reachable decoder has malformed-input coverage; missing
primitive round-trips added.

- [ ] T044 [P] [US5] ExpandedNodeId malformed-decode unit test (bad NamespaceUri/ServerIndex flag + truncation → `Bad_DecodingError`). OPC-10000-6 §5.2.2.10 (ExpandedNodeId).
- [ ] T045 [P] [US5] Add a `tests/fuzz/fuzz_expanded_nodeid.c` fuzz target for ExpandedNodeId decode. OPC-10000-6 §5.2.2.10.
- [ ] T046 [P] [US5] HELLO EndpointUrl over-length rejection (`Bad_TcpEndpointUrlInvalid`) in `tests/unit/test_tcp_connection.c`. OPC-10000-6 §7.1.2.3 (Hello Message).
- [ ] T047 [P] [US5] NodeId Guid (0x04) / Opaque (0x05) identifier types handled-or-explicitly-rejected in `tests/unit/test_binary_nodeid_errors.c`. OPC-10000-6 §5.2.2.9 (NodeId).
- [ ] T048 [P] [US5] Round-trip/boundary tests for SByte, Int16, UInt16, Int64, UInt64, Double, DateTime in `tests/unit/test_binary_primitives.c`. OPC-10000-6 §5.2.2.2 / §5.2.2.3 (built-in numeric); DateTime §5.2.2.5.
- [ ] T049 [P] [US5] DataValue timestamp/picosecond mask-bit round-trip in `tests/unit/test_binary_variant_datavalue.c`. OPC-10000-6 §5.2.2.17 (DataValue).
- [ ] T050 [P] [US5] QualifiedName / LocalizedText malformed-decode tests (`Bad_DecodingError` on truncated fields). OPC-10000-6 §5.2.2.12 (QualifiedName) / §5.2.2.13 (LocalizedText).
- [ ] T051 [P] [US5] Reverse wrong-algorithm: RSA-PSS URI advertised on a PKCS#1.5 policy is rejected (extend `tests/integration/test_secure_handshake_modern.c`). OPC-10000-7 SecurityPolicy (signature algorithm) / OPC-10000-4 §7.37 (SignatureData).
- [ ] T052 [P] [US5] MessageChunk count-limit negative: a message spanning `> max_chunk_count` chunks is rejected (`tests/unit/test_message_chunk_errors.c`). OPC-10000-6 §6.7.2 (MessageChunk) / §7.1.2.4 (Acknowledge limits).
- [ ] T053 [US5] Register the US5 claims as rows in the claim→test map (T008).

**Checkpoint**: US5 shippable — wire surface coverage complete; checker green.

---

## Phase 8: Polish & Cross-Cutting

- [ ] T054 Global gate sweep: all four profile suites + ASAN/UBSan green; `scripts/measure_size.sh all` within gates (`.text` +3%, data+bss +5%, no new heap); `scripts/check_build_matrix.sh`.
- [ ] T055 [P] Verify the anti-over-claim guards still pass and status stays profile-targeting after all reconciliations.
- [ ] T056 [P] Update `docs/traceability/conformance-claims.md` to describe the new claim→test enforcement (replaces the substring-matching description). Grounding: OPC-10000-7 §4.2 (Conformance Units and Conformance Groups).
- [ ] T057 [P] Update project memory (feature 028): per-profile CI + claim→test map enforce "claimed unit ⇒ profile-runnable test"; record which claims were reconciled by doc-correction vs implemented.

---

## Dependencies & Execution Order

- **Setup (T001–T003)** → **US1 (T004–T012)** first: the keystone. The claim→test map (T008/T009) is where US2–US5 register their rows (T024/T033/T043/T053), so US1 precedes them.
- **US2, US3, US4, US5** depend on US1 (map + per-profile runs) but are mutually independent and can proceed in parallel after US1.
- **Test-before-code pairs** (must run in order): T013→T014 (Browse); T015→T016 (Nano base-info doc); T017→T018 (RegisterNodes doc); T020→T021 (NodeIdExists).
- **Fixture-before-test**: T025 feeds T029 and T030.
- Each story's "register claims in the map" task (T024/T033/T043/T053) runs after that story's tests exist; the checker passing is verified at the story checkpoint.
- **Polish (T054–T057)** last.

## Parallel Opportunities

- Setup T001/T002/T003 are independent `[P]`.
- US1: T005/T006 (different build files) can parallel T008 (map authoring); T009→T010→T011 are sequential.
- US2: T013, T015, T017, T019, T020, T022, T023 are `[P]` (different files); each doc-correct/fix follows its assertion test where paired.
- US3: T025 `[P]` feeds T029/T030; T026/T027/T028/T031 are independent tests.
- US4: T034–T039, T041, T042 are `[P]`.
- US5: T044–T052 are almost all `[P]` (distinct files).

## MVP Scope

**MVP = Setup + US1 (T001–T012)** — per-profile execution + enforced claim→test map.
That alone converts the suite into per-profile conformance evidence; every later
tier registers rows in a now-enforcing harness.

## Implementation Strategy

Land US1 first (keystone). Then US2 (correctness/honesty), US3 (real-crypto security,
the weakest evidence area), then US4/US5 breadth in parallel. Every task is a single
job, holds the four-profile green + resource gates, and reconciled claims are
corrected honestly with status staying profile-targeting.
