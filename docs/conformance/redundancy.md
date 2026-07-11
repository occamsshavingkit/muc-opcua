# Conformance: Client Redundancy Facet (spec 066)

Client Redundancy (OPC-10000-4 §6.6.3) lets a redundant set of Clients keep a Server's
Subscriptions alive across a failover: *"Client Redundancy is supported in OPC UA by the
TransferSubscriptions Service and by exposing Client information in the Server diagnostic
information. TransferSubscriptions requires the same user on all redundant Clients to
succeed."* Optional facet — `MUC_OPCUA_REDUNDANCY`, `full`-only by default (spec 067).

## TransferSubscriptions (OPC-10000-4 §5.14.7)

Moves a Subscription (and its MonitoredItems) from the Session that owns it to the Session
that calls the service. Per-`subscriptionId` behaviour (state machine §5.14.1):

| Condition | Result |
|---|---|
| subscription id unknown | `Bad_SubscriptionIdInvalid` |
| already owned by the calling Session (`SessionChanged()==FALSE`) | `Bad_NothingToDo` |
| owned by a **different user** (`ClientValidated()==FALSE`) | `Bad_UserAccessDenied` |
| different Session, **same user** (row 23) | `Good` — `SetSession()`, notify the OLD Session (`Good_SubscriptionTransferred` StatusChangeNotification), report `availableSequenceNumbers` |
| empty `subscriptionIds` | service-level `Bad_NothingToDo` |

- **`SetSession()`** re-homes the Subscription (`mu_subscription_transfer`) and re-targets the
  old owner's parked Publish queue so queued publishes don't strand.
- **`availableSequenceNumbers`** are the sequence numbers still available for Republish, read
  from the subscription's retransmit slot.
- **`sendInitialValues`**: when TRUE, the data MonitoredItems are marked to re-send current
  values on the next Publish.

**Same-user (`ClientValidated()`) enforcement** is a security requirement — without it any
Session could hijack another's Subscription. Each Session records a user-identity fingerprint
at ActivateSession (identity kind — anonymous / username / x509 — plus a bounded
discriminator: the username, or a certificate prefix); `mu_session_same_user` compares them.

## RedundancySupport (OPC-10000-5 §12.5)

`Server.ServerRedundancy` (i=**2296**) exposes `RedundancySupport` (i=**3709**), the
`RedundancySupport` enum (DataType i=**851**). This server supports *client* redundancy via
TransferSubscriptions without being itself redundant, so it advertises **`None (0)`**. Served
from the runtime-bound base address space (read-addressable by NodeId).

## Evidence

- `test_transfer_subscriptions`:
  - `test_transfer_cross_session_succeeds` — a Subscription created on Session A is
    transferred to Session B (same user): returns `Good` and is thereafter owned by B.
  - `test_transfer_different_user_denied` — a different-user Session gets
    `Bad_UserAccessDenied` and ownership is unchanged.
  - `test_transfer_subscriptions_own_returns_nothing_to_do` — transferring your own
    Subscription → `Bad_NothingToDo`.
  - `test_transfer_subscriptions_unknown_id_returns_bad_id`, `_zero_count`,
    `_negative_count_*`, `_16_item_cap_overflow`, `_mixed_ids_response_encoding`.
  - `test_redundancy_support_reads_none` — `RedundancySupport` (3709) reads `None (0)`.

## Out of scope

- Server-to-server redundancy (Cold/Warm/Hot/Transparent) — `RedundancySupport` is `None`.
- Durable Subscriptions (§6.8) and the transfer `AuditSessionEventType` audit event
  (§6.5.10, needs the separate Auditing facet).
