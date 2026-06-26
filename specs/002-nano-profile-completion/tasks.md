# Tasks: Nano Embedded Device 2017 Server Profile Completion

**Input**: [spec.md](spec.md)
**Target Profile**: Nano Embedded Device 2017 Server Profile (≡ Core 2017 Server Facet).

**Ownership tags**: `[Claude-test]` = Claude writes the failing test first;
`[Codex-impl]` = Codex writes production code to pass it (no test/spec/doc edits);
`[Claude]` = Claude-only (design contract, docs, validation).

**OPC Reference Rule**: every protocol task cites the exact OPC UA section.
**Task Size Rule**: each `[Codex-impl]` task is one reviewable PR-sized diff.

Existing convention: View Service Set is OPC 10000-4 §5.9; service handlers live in
`src/core/service_dispatch.c`, gated tables in the same file; address space in
`src/address_space/`; service IDs in `include/micro_opcua/opcua_ids.h`.

---

## US1 — Complete the View Service Set (P1)

- [X] T001 [Claude-test] Integration test driving `mu_server_poll` through a
  RegisterNodes request (echo nodes back) and an UnregisterNodes request (Good), in
  `tests/integration/test_view_services.c`. (OPC refs: OPC 10000-4 §5.9.5, §5.9.6)
- [X] T002 [Codex-impl] Implement `RegisterNodes` (identity-map `nodesToRegister` →
  `registeredNodeIds`) and `UnregisterNodes` (Good, ResponseHeader only) handlers,
  add them to `g_supported_services[]` (requires_session=true) and the dispatch
  `switch`, and add any missing request/response IDs to `opcua_ids.h`, in
  `src/core/service_dispatch.c`. (OPC refs: OPC 10000-4 §5.9.5, §5.9.6)

- [X] T003 [Claude-test] Integration test: BrowseNext with `releaseContinuationPoints
  = true` → Good/empty; BrowseNext with an arbitrary continuation point →
  operation-level `Bad_ContinuationPointInvalid`. (OPC refs: OPC 10000-4 §5.9.3, §7.9)
- [X] T004 [Codex-impl] Implement `BrowseNext` handler + dispatch/table wiring in
  `src/core/service_dispatch.c` (gated under `MICRO_OPCUA_SERVICE_BROWSE`). This
  server issues no ContinuationPoints, so: release→empty Good; otherwise each point
  → `Bad_ContinuationPointInvalid`. (OPC refs: OPC 10000-4 §5.9.3, §7.9)

- [X] T005 [Claude-test] Test for browse-path resolution: a `BrowsePath` from
  Objects(i=85) via the `Organizes`/HierarchicalReferences path to a target node
  resolves to its NodeId; an unknown name → `Bad_NoMatch`. In
  `tests/integration/test_view_services.c` (+ a unit test of the resolver if Claude
  extracts one). (OPC refs: OPC 10000-4 §5.9.4)
- [X] T006 [Codex-impl] Implement `TranslateBrowsePathsToNodeIds`: decode
  `browsePaths[]`, walk each `RelativePath` element over the static address space by
  BrowseName + reference type, emit `BrowsePathResult` with target NodeIds or
  `Bad_NoMatch`; handler + dispatch/table wiring in `src/core/service_dispatch.c`
  (and a path-walk helper in `src/address_space/` if Claude's test expects one).
  (OPC refs: OPC 10000-4 §5.9.4)

## US2 — Standard Base Information address space (P1)

- [X] T007 [Claude] Design the value-source contract for dynamic Server attributes
  (`ServerStatus.CurrentTime/State/StartTime`) — a callback signature bound to the
  server's time adapter + running state — and document it in the base-nodes header
  `src/address_space/base_nodes.h`. (OPC refs: OPC 10000-5 §6.3.1, §6.3.3)
- [X] T008 [Claude-test] Integration tests: Browse from `Server`(i=2253) lists
  ServerStatus/ServerCapabilities/NamespaceArray/ServerArray; Read of
  `ServerStatus`(i=2256), `ServerStatus.State`(i=2259), `ServerCapabilities`(i=2268),
  `ServiceLevel`(i=2267), `NamespaceArray`(i=2255), `ServerArray`(i=2254) return Good
  with correct types. (OPC refs: OPC 10000-5 §6.3, §7; OPC 10000-6 Annex A NodeIds)
- [X] T009 [Codex-impl] Implement the library default Base Information node set as a
  `static const` node table in `src/address_space/base_nodes.c` per the T007 header:
  Root(84)/Objects(85)/Types(86)/Views(87)/Server(2253) hierarchy with references,
  ServerStatus(2256)+State(2259)+BuildInfo(2260)+CurrentTime+StartTime,
  ServiceLevel(2267), ServerCapabilities(2268) incl. ServerProfileArray,
  LocaleIdArray, MaxBrowseContinuationPoints=0 and OperationLimits
  MaxNodesPerRead(11705)/MaxNodesPerBrowse(11710), NamespaceArray(2255),
  ServerArray(2254), with the value-source callbacks the header declares.
  (OPC refs: OPC 10000-5 §6.3, §7; OPC 10000-6 Annex A)
- [X] T010 [Codex-impl] Wire the base node set into address-space resolution as a
  fallback consulted after the integrator's `address_space` (so Read/Browse of the
  standard nodes succeed even when the integrator supplies none), in
  `src/address_space/address_space.c`. (OPC refs: OPC 10000-5 §6.3)

## US3 — Conformance traceability (P1, Claude)

- [ ] T011 [Claude] Fill `docs/conformance/{status,profile-nano,services}.md` with
  the Nano conformance-unit → service/feature → test mapping, and add the new files
  to `docs/traceability/files-to-sections.md`. Keep `profile-targeting` (no CTT yet).
  (OPC refs: OPC 10000-7 Nano 2017 profile, §4.2)

## Validation (Claude, after each Codex task)
Build host+tests, run the full ctest suite, ASan/UBSan the new tests, and re-run the
asyncua + .NET `dotnet-interop` jobs in CI; only then mark the task `[X]`.
