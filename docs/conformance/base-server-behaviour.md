# Conformance: Base Server Behaviour Facet (spec 064)

There is no standalone "Base Server Behaviour Facet" in the 2017 profile line. Base server
behaviour is defined by the **Core 2017 Server Facet** (OPC profile-DB id 1673) — the
mandatory base of every profile this server ships (Nano → Micro → Embedded → Standard). This
document covers its base-behaviour conformance units and the (optional) ServerDiagnostics
object this server also exposes.

## Mandatory: Session General Service Behaviour (Core 2017, `isOptional=false`)

> "Implement basic Service behaviour. This includes in particular: checking the
> authentication token; returning the requestHandle in responses; respecting a timeoutHint."

Fully implemented; evidence:

| Sub-requirement | Where | Evidence |
|---|---|---|
| Check the authentication token | `dispatch_core.c` — every session-scoped service reads the token and rejects `token==0`/unknown → `Bad_SessionIdInvalid`, closed → `Bad_SessionClosed`, wrong SecureChannel → security fail, non-activated → `Bad_SessionNotActivated` | `test_service_state_errors` (`test_read_before_activate_session`, `test_read_with_unknown_session_token_returns_bad_sessionidinvalid`, `test_read_with_known_inactive_session_returns_bad_sessionnotactivated`), `test_dispatch_services` (`Bad_SessionClosed`) |
| Return the requestHandle in responses | `service_header.c:51` writes `request_handle` into every ResponseHeader | `test_write_response` (echo asserted) |
| Respect a timeoutHint | parsed (`service_header.c`), stored on parked Publish requests, expired requests dropped with `Bad_Timeout` (`publish_due.c`) | `test_base_server_behaviour` (`test_publish_within/past/zero_timeout_hint_*`) |

## Optional: ServerDiagnostics object (this server exposes it)

The **Base Services Diagnostics** CU (returning per-service `DiagnosticInfo` via the
`returnDiagnostics` request parameter) is `isOptional=true` and is **out of scope** here (the
header parses `returnDiagnostics` but emits an empty `serviceDiagnostics`). Separately, the
**ServerDiagnostics address-space object** (OPC-10000-5 §6.3) — mandated by no profile we
advertise — is exposed when `MUC_OPCUA_SERVER_DIAGNOSTICS` is compiled (standard/full):

| Node | NodeId | Notes |
|---|---|---|
| `Server.ServerDiagnostics` | `i=2274` (Object) | ServerDiagnosticsType (`i=2020`) |
| `Server.ServerDiagnostics.ServerDiagnosticsSummary` | `i=2275` (Variable) | live `ServerDiagnosticsSummaryDataType` (`i=859`) ExtensionObject, Encoding DefaultBinary `i=861`, 12 × UInt32 (§12.9) built from the caller-owned counter struct |
| `Server.ServerDiagnostics.EnabledFlag` | `i=2294` (Variable) | Boolean `true` |

Read-addressable by NodeId (the primary diagnostic-client access pattern). The nodes live in
the runtime-bound dynamic address space (`mu_base_runtime_init`), alongside
`CurrentTime`/`StartTime`; Browse-linkage from the `Server` object is a documented refinement.

### Counters (§12.9 field order) and how they are driven

| # | Field | Driven by |
|---|---|---|
| 1 | serverViewCount | structurally 0 (no View management on this server) |
| 2 | currentSessionCount | CreateSession success ↑ / CloseSession · idle-timeout ↓ |
| 3 | cumulatedSessionCount | CreateSession success ↑ |
| 4 | securityRejectedSessionCount | `mu_diagnostics_session_security_rejected` |
| 5 | rejectedSessionCount | CreateSession failure |
| 6 | sessionTimeoutCount | idle-timeout close (`poll.c`) |
| 7 | sessionAbortCount | structurally 0 (sessions close/timeout, no distinct abort) |
| 8 | currentSubscriptionCount | CreateSubscription ↑ / DeleteSubscription · session close ↓ |
| 9 | cumulatedSubscriptionCount | CreateSubscription ↑ |
| 10 | publishingIntervalCount | structurally 0 (not distinctly tracked) |
| 11 | securityRejectedRequestsCount | dispatch security rejection (wrong SecureChannel) |
| 12 | rejectedRequestsCount | dispatch session-validation rejection |

The structurally-zero counters are honest zeros for this server's behaviour, not unwired
placeholders; the remaining counters are driven from real session/subscription/dispatch
events (previously the whole struct was inert — no production call sites).

## Evidence

- `test_base_server_behaviour` — the mandatory timeoutHint sub-requirement.
- `test_diagnostics` — counter wiring (incl. security-rejected / request-rejected) and the
  end-to-end proof that `ServerDiagnosticsSummary` (2275) encodes the live counters as a
  decodable ExtensionObject, plus `EnabledFlag` (2294) reads Boolean true.
- Mandatory auth-token / requestHandle behaviour: mapped to existing suites above.
