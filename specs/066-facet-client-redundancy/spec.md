# Spec 066: Client Redundancy Facet (B7)

Project-B facet B7 (final). Scope stance (per B1–B6, locked with user): **follow the spec —
ground the requirement, fix the correctness gap, don't gold-plate.** Optional facet
(`MUC_OPCUA_REDUNDANCY`), `full`-only after the spec-067 strict-profile grounding.

## Grounding (OPC-10000-4 §5.14.7 TransferSubscriptions, §5.14.1, §6.6.3)

§6.6.3: *"Client Redundancy is supported in OPC UA by the TransferSubscriptions Service and
by exposing Client information in the Server diagnostic information. TransferSubscriptions
requires the same user on all redundant Clients to succeed."*

The server-side surface is the **TransferSubscriptions** service — moving a Subscription (and
its MonitoredItems) from the Session that owns it to the Session that calls the service.
State machine (§5.14.1.2/§5.14.1.4):

- `SessionChanged()` = the calling Session differs from the Session currently owning the
  Subscription.
- `ClientValidated()` = the caller operates on behalf of the **same user** (and same
  Profiles) as the owning Session.
- Row 23 (SessionChanged && ClientValidated): `SetSession()` — re-home the Subscription to
  the caller — `ResetLifetimeCounter()`, `ReturnResponse()`, **`IssueStatusChangeNotification()`
  to the OLD session** (`Good_SubscriptionTransferred` 0x002D0000).
- Row 22 (¬SessionChanged): negative response.
- Row 24 (SessionChanged && ¬ClientValidated): negative response.

Operation-level result codes (Table 96): `Bad_SubscriptionIdInvalid` (unknown id),
`Bad_NothingToDo` (**already owned by the calling Session**), `Bad_UserAccessDenied`
(**different user**), `Bad_TooManySubscriptions` (new Session at its subscription cap).
Service-level: `Bad_NothingToDo` (empty `subscriptionIds`). The response's per-subscription
`TransferResult` carries `statusCode` + `availableSequenceNumbers[]` (the sequence numbers
still available for Republish from the retransmit buffer). Request `sendInitialValues`: if
TRUE the first Publish after transfer re-sends current values for all data MonitoredItems.

`RedundancySupport` (OPC-10000-5 §12.5, enum DataType i=**851**): the
`Server.ServerRedundancy.RedundancySupport` instance (i=**3709**) advertises the mode;
`None (0)` for a server that supports client redundancy (TransferSubscriptions) without being
itself redundant. `Server.ServerRedundancy` object = i=**2296**.

## Gap (STUB — a no-op self-transfer that claims to work)

`handle_transfer_subscriptions` (`transfer_handler.c`) is wired and encodes the correct wire
format but does **not** transfer:

- It finds the subscription keyed on the **caller's** `session_id`
  (`mu_subscription_find(&subs, active_session->session_id, id)`), so a subscription owned by
  a **different** session is never found — a genuine cross-session transfer is impossible.
- On "success" it reassigns the **same** `session_id` back (a no-op), never re-targets the
  publish queue, always writes `availableSequenceNumbers = 0`, and ignores `sendInitialValues`.
- No `SessionChanged`/`ClientValidated` logic → no `Bad_NothingToDo`, no `Bad_UserAccessDenied`.
- `Server.ServerRedundancy` (2296) is served but **`RedundancySupport` (3709) is not** — a
  redundant client can't discover the mode. (`s_str_RedundancySupport` is declared but unused.)
- The existing `test_transfer_subscriptions` encodes the no-op (`availableSequenceNumbers == 0`)
  as the expected contract.

**Security prerequisite:** the mandatory same-user check needs per-session user identity, which
`mu_session_t` does **not** store today (only `auth_token`; auth runs through a callback at
ActivateSession and is not retained). Shipping a real cross-session transfer **without** the
user check would let any session hijack any subscription — so adding per-session identity is a
required part of this facet, not optional.

## Requirements

**FR-1 — Per-session user identity + real transfer primitive.**
- Store a compact user-identity fingerprint on `mu_session_t` at ActivateSession: an identity
  kind (anonymous / username / x509) plus a bounded discriminator (username bytes, or a cert
  thumbprint) so two sessions can be compared for "same user". Anonymous == anonymous.
- Add `mu_subscription_find_any(subs, subscription_id)` (by id across all sessions) and
  `mu_subscription_transfer(subs, sub, new_session_id)` — reassign the owning `session_id` and
  re-target the publish state (clear the old owner's parked Publish requests for the sub so
  queued publishes don't strand; the publish queue is keyed by `session_id`).

**FR-2 — Real TransferSubscriptions handler** (`transfer_handler.c`), per §5.14.7, for each
`subscriptionId`:
- not found → `Bad_SubscriptionIdInvalid`;
- owned by the calling session (¬SessionChanged) → `Bad_NothingToDo`;
- owned by a different user (¬ClientValidated) → `Bad_UserAccessDenied`;
- new session at its subscription cap → `Bad_TooManySubscriptions`;
- else transfer: `mu_subscription_transfer`, issue `Good_SubscriptionTransferred`
  StatusChangeNotification to the **old** session, return `availableSequenceNumbers` from the
  retransmit slot, and honor `sendInitialValues` (mark the sub to re-send initial values on the
  next Publish). Empty `subscriptionIds` → service-level `Bad_NothingToDo`.

**FR-3 — Serve `RedundancySupport` (3709)** = `None (0)` (enum encoded Int32) under
`Server.ServerRedundancy` (2296); wire the already-declared `s_str_RedundancySupport`.

**FR-4 — Tests + docs + size.** Cross-session transfer (create on session A, transfer to
session B, assert Good + the sub now owned by B + publish delivery on B), `Bad_NothingToDo`
(same session), `Bad_UserAccessDenied` (different user), non-zero `availableSequenceNumbers`
when a retransmit slot exists, `RedundancySupport` reads None. Update
`test_transfer_subscriptions` (which currently encodes the no-op). Conformance doc + status +
claim rows (full-only). Measure `.text`/RAM (session identity grows RAM slightly); `full` under
the 128 KiB stopper.

## Verification (test-first)

- The load-bearing test: subscription created on session A is found and transferred to session
  B (different session), returns `Good`, `mu_subscription_find_any` now resolves it to B, and a
  Publish on B delivers its notifications. This fails against the current stub (which can't find
  a cross-session subscription).
- Same-user enforcement: transfer attempt by a different-user session → `Bad_UserAccessDenied`.
- Full matrix green; nano/micro/embedded/standard unaffected (facet is `full`-only /
  `-DMUC_OPCUA_REDUNDANCY`).

## Out of scope

- Server-to-server redundancy (Cold/Warm/Hot/Transparent) — RedundancySupport is `None`; this
  is *client* redundancy only.
- Durable Subscriptions (§6.8, `SetSubscriptionDurable`) — a separate facet.
- The Auditing `AuditSessionEventType` audit event for transfer (§6.5.10) — optional, deferred
  (Auditing is itself a separate optional facet).

## On approval

Speckit plan/tasks, execute test-first, PR against `main` with a merge commit. B7 completes
Project B (all seven Part-7 facets grounded and formalized).
