# Phase 0 Research: Conformance-Test Hardening

No `[NEEDS CLARIFICATION]` remained after specify. The open questions are design
decisions about the test/CI mechanism and, per Tier-1 item, whether to reconcile a
false claim by implementing or by correcting the doc.

## Decision 1 — Honor an explicit profile when building tests

**Decision**: Change the `CMakeLists.txt:16-20` "force `full` when tests are built"
rule so it only forces `full` when the profile is the *default* and not explicitly
set; an explicit `-DMUC_OPCUA_PROFILE={nano,micro,embedded}` with
`-DMUC_OPCUA_BUILD_TESTS=ON` is honored. `MUC_OPCUA_PROFILE_EXPLICIT` already exists
to signal an explicit choice.

**Rationale**: This is the single blocker making per-profile test runs impossible.
Keeping the default-forces-full behavior preserves the common `make test` workflow;
only an explicit profile changes.

**Alternatives**: a separate `MUC_OPCUA_TEST_PROFILE` var (rejected — duplicates the
profile concept); leaving it and only cross-compiling (rejected — never runs tests).

## Decision 2 — Per-profile CI jobs + Makefile targets

**Decision**: Add CI jobs that configure+build+`ctest` for each of nano, micro,
embedded, full (host platform), and `make test-nano|test-micro|test-embedded|
test-full` targets. Each runs the subset of tests that compile+run in that profile
(the `RUN_TEST OFF` gates in `tests/unit/CMakeLists.txt` become live).

**Rationale**: Makes the gates real and produces per-profile evidence. Host builds
suffice for behavior; the existing ARM cross-compile size jobs stay as-is.

**Alternatives**: one matrix job (fine, equivalent); per-profile ARM run (heavier,
not needed for behavior evidence).

## Decision 3 — Claim→test map as a checked table, not prose

**Decision**: Represent the map as a machine-readable table (each row = conformance
unit / claim → the test name(s) that verify it + the profile(s) it must run in). A
new checker test (in `tests/conformance/`) parses it, and — using the profile it is
compiled in — asserts every claim required for that profile has a named,
registered, passing test. A claim with no backing test in its profile fails.

**Rationale**: Replaces `test_traceability_docs.c`'s substring matching (which
passes on a doc citing a doc) with an enforced unit⇒test link — the audit's central
missing keystone. Keeping it a table (not code-gen) keeps it auditable and diffable
per the constitution's "compact, auditable tables" rule.

**Alternatives**: keep prose + reviewer discipline (rejected — that is the status
quo that produced the gaps); encode the map in CMake test properties/labels
(viable; the checker can read `ctest` labels — an implementation option for tasks).

**Open item for design**: the exact backing mechanism (ctest labels vs a checked
manifest file) is settled in data-model/contracts; either satisfies FR-002.

## Decision 4 — Real-crypto security tests use generated keys/certs; skip cleanly without a backend

**Decision**: US3 tests use the host/OpenSSL adapter (and mbedTLS/wolfSSL where the
build enables them) to generate real keys and sign real tokens/nonces. Expired and
not-yet-valid certs are produced as fixtures (generated with past/future validity)
or via a backend cert-gen helper. When no crypto backend is compiled in, the tests
`TEST_IGNORE` (skip visibly) — never silently pass.

**Rationale**: The mock-only coverage is the weak point (it hid the 027 bug). Real
keys are the only honest evidence. Visible skips prevent "green = covered" illusions
on a no-crypto build.

**Alternatives**: keep mocks (rejected — the whole point); require a backend always
(rejected — nano/micro builds legitimately have no crypto).

## Decision 5 — Tier-1 reconciliations: implement vs correct-the-doc (per item)

For each audited claim/build mismatch, the honest resolution — implement+test, or
correct the doc — chosen by size/scope:

- **Browse overflow StatusCode**: IMPLEMENT — return `Bad_TooManyOperations`
  (`browse.c:103`) + overflow test. Clear defect, trivial, zero size cost.
- **Nano/Micro Base Information (Server object/ServerStatus/ServerProfileArray/
  NamespaceArray)**: LIKELY CORRECT-THE-DOC for Nano — pulling `BASE_NODES` into
  Nano would grow flash and change the Nano size anchor; narrow the Nano claim to
  what the profile actually builds, and verify with a per-profile test that the
  claim matches the build. Re-evaluate for Micro during implementation (Micro
  already carries more). Final call recorded in the task + doc.
- **RegisterNodes/UnregisterNodes Nano claim**: CORRECT-THE-DOC — they are `full`-
  only; the Nano "full View Service Set incl. RegisterNodes" claim is narrowed (or
  those services are documented as an optional add-on), backed by a per-profile
  test that Nano returns the unsupported StatusCode.
- **Micro ServerProfileArray URI**: DECIDE IN TASK — emitting the
  `MicroEmbeddedDevice2017` URI is cheap IF the Micro profile actually builds the
  base-node that holds it; since Micro has `BASE_NODES` OFF, this couples to the
  Base-Information decision. Default: correct the claim unless base nodes come to
  Micro.
- **Five never-emitted StatusCodes** (`NodeIdExists`, `NodeIdRejected`,
  `SessionClosed`, `TcpServerTooBusy`, `Timeout`): `Bad_NodeIdExists` on AddNodes
  duplicate is a genuine, cheap IMPLEMENT+test (NodeManagement). The rest
  (`SessionClosed`, `TcpServerTooBusy`, `Timeout`, `NodeIdRejected`) are not
  required by the targeted facets → REMOVE from the claimed set unless a concrete
  need exists.
- **Guid / DiagnosticInfo built-ins**: SCOPE-THE-CLAIM — they are used in UADP/
  PubSub encoding but not the binary service codec; narrow the "built-in type"
  claim to where each is actually encoded/tested, rather than adding unused codec
  paths to the size-constrained core.

**Rationale**: The audit's intent is honesty, not maximal features; the constitution
forbids growing the core for unclaimed/unneeded surface. Each item's final decision
is captured in its task and reflected in the docs.

## Decision 6 — Test-first ordering; Tier 0 precedes all

**Decision**: Tier 0 (US1) lands first so every subsequent test is validated in its
profile and the claim→test check is enforcing before the coverage tiers add rows.
Within each later tier, the failing test/fixture precedes the fix (for US2 code
items) or is the deliverable (US3–US5 coverage).

**Rationale**: Sequencing that maximizes evidence value and prevents adding tests
that only ever run in `full`.
