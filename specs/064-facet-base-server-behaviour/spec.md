# Spec 064: Base Server Behaviour Facet (B5)

Project-B facet B5 (roadmap `docs/superpowers/specs/2026-07-09-...-roadmap.md`). Scope
stance (per B1–B4, locked with user): **follow the spec** — implement exactly what the
advertised profile mandates, ground every requirement, don't gold-plate optional CUs.

## Grounding OUTCOME (grounding done first — it re-scoped this facet)

There is **no standalone "Base Server Behaviour Facet"** in the 2017 profile line. Base
server behaviour is defined by the **Core 2017 Server Facet (profile-DB id 1673)** — the
mandatory base of every profile we ship (Nano 1657 → Micro 1659 → Embedded 1661 → Standard
1663). Its base-behaviour conformance units, grounded from the live OPC profile DB
([[opc-profile-reporting-api]]):

| CU | Mandatory? | Requirement |
| --- | --- | --- |
| **Session General Service Behaviour** | **YES** (`isOptional=false`) | "checking the authentication token · returning the requestHandle in responses · respecting a timeoutHint" |
| Base Services Diagnostics | no (`isOptional=true`) | "returns available diagnostic information as requested with the `returnDiagnostics` parameter" |

**Key finding — the mandatory CU is already implemented; the diagnostics the roadmap
imagined are OPTIONAL and mandated by no profile we advertise.** This is the mirror image
of spec 063 (which found a mandatory gap). Verified in code:

- **Auth-token check** — `dispatch_core.c:147-170`: every session-scoped service reads the
  auth token and rejects `token==0`/unknown → `Bad_SessionIdInvalid`, closed →
  `Bad_SessionClosed`, wrong secure channel → security fail, non-activated →
  `Bad_SessionNotActivated`.
- **requestHandle echo** — `service_header.c:51` writes `header->request_handle` into every
  ResponseHeader.
- **timeoutHint** — parsed (`service_header.c:31`), stored on parked Publish requests, and
  expired Publish requests are dropped with `Bad_Timeout` (`publish_due.c:41-45`).

The **ServerDiagnostics object** (`Server.ServerDiagnostics` i=2274,
`.ServerDiagnosticsSummary` i=2275, `.EnabledFlag` i=2294;
`ServerDiagnosticsSummaryDataType` i=859, `_Encoding_DefaultBinary` i=861) is governed by no
mandatory CU. The existing `MUC_OPCUA_SERVER_DIAGNOSTICS` is a **dead shell**: a
`mu_diagnostics_summary_t` counter struct + 6 setter functions **called only by unit
tests** (zero production call sites), 5 of its 11 counters have no writer at all, and it is
exposed nowhere in the address space. It is compiled into `standard`/`full` yet does nothing
observable.

## Requirements

**FR-1 — Formalize the mandatory Session General Service Behaviour CU (spec-required).**
No new runtime code (the behaviour ships); add explicit conformance tests that pin each of
the three sub-requirements so a regression is caught:
- auth-token rejection matrix on a session-scoped service: `token==0` →
  `Bad_SessionIdInvalid`, unknown token → `Bad_SessionIdInvalid`, closed session →
  `Bad_SessionClosed`, not-activated → `Bad_SessionNotActivated`;
- ResponseHeader echoes RequestHeader `requestHandle`;
- an expired `timeoutHint` on a parked Publish is dropped with `Bad_Timeout`.

**FR-2 — Make the OPTIONAL ServerDiagnostics core observable real (remove the dead shell).**
Because the feature is compiled but inert, either delete it or make it work; we make it
work (the roadmap's intent, correctly labelled optional). Wire the counters to real events
and expose the object:
- Align `mu_diagnostics_summary_t` to the grounded 12-field
  `ServerDiagnosticsSummaryDataType` (§12.9) order; add the missing `serverViewCount`.
- Wire counters at real call sites: session created/closed/timeout/rejected (session
  service), subscription created/closed (subscription service), rejected/security-rejected
  requests (dispatch reject path). No counter ships write-less.
- Serve `Server.ServerDiagnostics` (2274, Object), `.ServerDiagnosticsSummary` (2275,
  Variable — a live `ServerDiagnosticsSummaryDataType` built from `server->diag`),
  `.EnabledFlag` (2294, Variable, Boolean) in `base_nodes.c` (kept NodeId-sorted; the base
  array is binary-searched — see [[profile-modernization-roadmap-progress]] note on 057).
- Encode `ServerDiagnosticsSummaryDataType` as an ExtensionObject (12 × UInt32, TypeId ns0
  i=861) via a new writer, wired into the Read value path.
- Gated on `MUC_OPCUA_SERVER_DIAGNOSTICS` (standard/full today); nano/micro/embedded
  byte-identical.

**FR-3 — Conformance doc + status.** `docs/conformance/base-server-behaviour.md` (the
mandatory Session General Service Behaviour CU + evidence; the optional ServerDiagnostics
object + its grounded node ids + that it is optional). Add rows to
`docs/conformance/status.md`; add a `claim_test_map.md` row for the mandatory CU (all
session profiles) and the diagnostics object (standard,full).

**FR-4 — Size + matrix.** Measure `.text` delta (the encoder + nodes add a little to
standard/full; nano/micro/embedded unchanged); `full` stays under the 128 KiB stopper;
record in the size ledger; all-profile matrix + sanitizers green.

## Verification (test-first)

- **Mandatory behaviour** — new `test_base_server_behaviour` (or extend an existing
  session/dispatch test) asserting the auth-token rejection matrix, requestHandle echo, and
  timeoutHint expiry.
- **Diagnostics observable** — an integration test that drives a real session (+ optionally
  a subscription), then Reads `i=2275`, decodes the `ServerDiagnosticsSummaryDataType`
  ExtensionObject, and asserts `currentSessionCount`/`cumulatedSessionCount` reflect the
  traffic (proves the counters are wired *and* exposed, not the current inert state).
- `EnabledFlag` (2294) reads Boolean `true`; base address space stays sorted
  (`test_base_address_space_is_sorted`).
- Docs/traceability green; full matrix green; nano/micro/embedded unchanged.

## Out of scope

- **Per-session `SessionDiagnosticsArray` / per-subscription `SubscriptionDiagnosticsArray`**
  (OPC-10000-5 §6.3.5/§6.3.7). Large fixed per-session/per-subscription structures; heavy
  RAM in the no-heap model and mandated by no profile we advertise. Deferred.
- **`returnDiagnostics` service-diagnostics population** (the optional Base Services
  Diagnostics CU). The header path already parses `returnDiagnostics` and emits an empty
  `serviceDiagnostics`; populating per-service `DiagnosticInfo` is a separate optional CU,
  deferred.
- **Certificate/config management (GDS push, `ServerConfiguration`)** — a distinct facet
  (Global Certificate Management, 2025 line), not base server behaviour.

## On approval

Speckit plan/tasks, execute test-first, PR against `main` with a merge commit. B5 of
Project B; remeasure `full` `.text` and continue to B6 (reverse-connect) while under the
stopper.
