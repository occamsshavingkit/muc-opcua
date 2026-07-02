# Phase 1 Data Model: Conformance-Test Hardening

No runtime data structures change (this is a test/CI feature). The "entities" are
the build-time/test artifacts and the small code/doc sites touched.

## 1. Profile build matrix (existing, made testable)

| Profile | Feature macros ON (beyond base) |
|---------|----------------------------------|
| nano | (none — base Nano: TCP/SC/Session/Discovery/Read/Browse) |
| micro | SUBSCRIPTIONS, USER_AUTH, SERVICE_WRITE |
| embedded | + SUBSCRIPTIONS_STANDARD, SECURITY, BASE_NODES, BASE_TYPE_SYSTEM, MULTIPLE_CONNECTIONS, EVENTS, EMBEDDED_PROFILE |
| full | + SERVICE_REGISTER_NODES, SERVICE_HISTORY, SERVICE_QUERY, SERVICE_NODEMANAGEMENT, DYNAMIC_NODES, PUBSUB, CUSTOM_METHODS |

- **Invariant after US1**: each profile is buildable+testable with
  `-DMUC_OPCUA_PROFILE=<p> -DMUC_OPCUA_BUILD_TESTS=ON`, and `ctest` runs the tests
  that survive that profile's `RUN_TEST` gates.
- Confirms audit facts: nano/micro have `BASE_NODES` OFF (no Server object);
  `SERVICE_REGISTER_NODES` is full-only; `SECURITY` starts at embedded.

## 2. Claim→test map (NEW, checked artifact)

One row per claimed conformance unit / behavior:

| Field | Meaning |
|-------|---------|
| claim id / unit | the conformance unit or claimed behavior (matches the conformance docs) |
| cited OPC UA § | the normative citation |
| profiles | which profiles must verify it (e.g. nano+ , embedded+) |
| backing test(s) | the registered test name(s) that verify it |
| kind | happy / negative / round-trip |

- **Validation rule (FR-002)**: for the profile the checker is compiled in, every
  row whose `profiles` includes this profile MUST have at least one `backing test`
  that is registered and passes in this build. A row with no such test → FAIL,
  naming the unit.
- **Anti-false-positive rule**: a row covered only in a superset profile is NOT
  counted as covering its own profile — the test must actually run in the claimed
  profile's build.
- Mechanism (settled in contracts): either a checked manifest file parsed by a
  `tests/conformance/` checker, or ctest labels the checker reads. Either satisfies
  FR-002; auditable table form preferred (Constitution VI).

## 3. Code sites touched (small, per US2 Decision 5)

- `src/services/browse.c:103` — overflow return `Bad_InternalError` → `Bad_TooManyOperations`. Behavior fix.
- `src/services/node_management.c` (AddNodes) — add duplicate-NodeId → `Bad_NodeIdExists` IF that claim is implemented (else the claim is removed).
- `src/address_space/base_nodes.c` — Micro ServerProfileArray URI IF that claim is implemented (else removed); coupled to the Base-Information decision.
- No other production code changes; everything else is tests/docs.

## 4. Doc reconciliations (per US2 Decision 5)

- `docs/conformance/profile-nano.md` — narrow Core Server Facet / RegisterNodes
  claims to what the Nano build actually contains.
- `docs/conformance/profile-micro.md` — reconcile ServerProfileArray URI + base
  info claims.
- `docs/traceability/statuscodes.md` — remove `Bad_SessionClosed`,
  `Bad_TcpServerTooBusy`, `Bad_Timeout`, `Bad_NodeIdRejected` (and `Bad_NodeIdExists`
  if not implemented) from the claimed set, or link them to a new forcing test.
- Type-traceability rows for Guid/DiagnosticInfo — scope to where each is actually
  encoded/tested.
- Invariant: after reconciliation the anti-over-claim guards
  (`test_conformance_docs.c`) still pass and status stays profile-targeting.

## 5. New/expanded test artifacts (US3–US5)

- Integration: real-crypto X509 (327) accept+reject; UserName accept+reject over a
  secured channel; encrypted-password nonce accept + stale-nonce reject; fail-closed
  trust reject; CLO close-through-poll.
- Unit: real expired / not-yet-valid / bad-key-size cert rejection; Read overflow;
  Bad_TooManySessions/RequestTooLarge/HistoryOperationUnsupported/MessageNotAvailable/
  NotFound; SetMonitoringMode/SetPublishingMode invalid-id; ExpandedNodeId malformed;
  HELLO EndpointUrl over-length; NodeId 0x04/0x05; primitive round-trips (SByte,
  Int16, UInt16, Int64, UInt64, Double, DateTime); DataValue mask; QN/LT malformed;
  reverse wrong-algorithm; chunk-count-limit.
- Fixtures: expired/not-yet-valid/bad-key-size certs; malformed-encoding byte blobs.
- Fuzz: ExpandedNodeId target.
