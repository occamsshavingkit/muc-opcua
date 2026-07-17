<!-- markdownlint-disable MD013 -->

# CU Reconciliation Log (spec 073 long tail)

Per-facet grounding of the fine-grained OPC UA Server conformance units against
the muc-opcua implementation, reconciled via `satisfied_by` links to our coarse
feature aliases (or left honestly unimplemented). Each claim requires **code +
a backing test**; behavioural mismatches and untested/partial paths are **not**
claimed. Numbers refer to `docs/conformance/completion.md`.

## 2026-07-15 — Standard DataChange Subscription 2022 Server Facet (opc_id 1324)

Aliases used: `opc_cu_subscription_basic` (`MUC_OPCUA_CU_SUBSCRIPTION_BASIC`),
`opc_cu_subscription_standard` (`MUC_OPCUA_CU_SUBSCRIPTION_STANDARD`).

Facet: 16 required + 1 optional. Reconciled **11/16 required** (was 1/16).
Overall reconciled required 21 → 31.

### Claimed (satisfied_by)

| CU | Name | Alias | Evidence |
| --- | --- | --- | --- |
| 2928 | Monitored Items Deadband Filter | basic | `subscription_monitor.c` abs-deadband compare; `test_subscriptions::test_monitored_item_absolute_deadband`, `test_subscription_deadband` |
| 2940 | GetMonitoredItems Method | standard | `standard/dispatch_method.c` (method 11492); `test_method_call::test_get_monitored_items_returns_server_and_client_handles` |
| 2963 | Monitor Basic | basic | `basic/monitored_items.c` + `subscription_monitor.c`; `test_subscriptions::test_create/modify/delete_monitored_items`, `test_set_monitoring_mode` |
| 3146 | Monitor Triggering | standard | `standard/triggering.c` + link storage; `test_subscriptions::test_set_triggering`, capacity/error tests |
| 3532 | Monitor Queueing | standard | `basic/monitored_items.c` clamp + `subscription_monitor.c` overflow bit; `test_subscriptions::test_monitored_item_queue_overflow`, capacity tests |
| 3534 | Subscription Multiple | basic | `subscription.h` `subscriptions[]` `_Static_assert(>=5)`; `test_subscriptions_capacity`, `test_subscription_session_isolation` |
| 3535 | Subscription Retransmission Queue | basic | `basic/retransmit.c` + `handle_republish`; `test_subscriptions::test_republish_and_acknowledge`. **Caveat**: single-slot store (most-recent message), profile-targeting minimal capacity — may not meet CTT multi-message republish depth. |
| 3544 | ResendData Method | standard | `standard/dispatch_method.c` (method 12873) + `mu_subscription_request_resend_data`; `test_method_call::test_resend_data_reissues_current_values_on_next_publish` |
| 3913 | Subscription Publish Basic | basic | `basic/publish_due.c` + `tick.c`; `test_subscriptions::test_publish_delivers_data_change`, `test_publish_keep_alive`, `test_subscription_publish` |
| 5207 | Monitor Items 2 | basic | `basic/monitored_items.c` alloc; `subscription.h` `monitored_items[]` `_Static_assert(>=500)`; `test_subscriptions_capacity` |
| 3727 | Subscription Basic | basic | (already reconciled before this increment) |

### NOT claimed (grounded reasons — honest under-claim)

| CU | Name | Reason |
| --- | --- | --- |
| 3143 | PublishRequest Queue Overflow | Behavioural mismatch: the CU requires discarding the **oldest** queued PublishRequest with the error; our code returns `Bad_TooManyPublishRequests` on the **incoming** request (`tick.c`). Different observable behaviour → not conformant. |
| 3196 | Base Info Fixed SamplingInterval (opt) | The `SamplingIntervalDiagnosticsArray` diagnostic node does not exist in the address space (only a disabled stub). |
| 3911 | ServerCapabilities Subscriptions | Only `MaxMonitoredItemsPerCall` is exposed; `MaxSubscriptions`, `MaxMonitoredItems`, `MaxSubscriptionsPerSession`, `MaxMonitoredItemsPerSubscription`, `AggregateFunctions` nodes are absent. |
| 3922 | Base Info SemanticChange Bit | The SemanticsChanged status bit (0x4000) is never set (only a disabled stub exists; the overflow InfoBit is a different bit). |
| 4055 | ServerCapabilities MaxMonitoredItemsQueueSize | No `MaxMonitoredItemsQueueSize` node in the address space (disabled stub only). |
| 5208 | Monitor Value Change V2 | Value-change monitoring works, but the CU's required IndexRange element selection on a monitored item is decoded-and-skipped, never applied to slice array values, and untested. |

### Follow-ups surfaced

- 3143: implement oldest-request-discard semantics (OPC-10000-4 §5.13.5) to claim it.
- 3535: grow the retransmission store to a multi-message queue for CTT-level republish.
- 3911/4055/3922/3196: add the missing ServerCapabilities / diagnostics / status-bit surface.
- 5208: apply monitored-item IndexRange slicing + test.

## 2026-07-15 — Auditing 2022 Server Facet (opc_id 1328)

30 CUs (18 required + 12 optional). This facet reuses the subscription CUs
already reconciled under 1324 (2963, 3534, 3535, 3727, 3913, 5207). New work:
the auditing / event / base-info CUs. Reconciled **5 more** (facet 11/18
required; overall reconciled required 31 → 36).

### Claimed (satisfied_by) — added as granular manifest entries

| CU | Name | Alias | Evidence |
| --- | --- | --- | --- |
| 2318 | Monitor QueueSize_ServerMax | subscription_standard | `subscription_monitor.c` clamps requested QueueSize to compiled max; `test_subscriptions_capacity` |
| 2515 | Address Space Events 2 | events | `notification.c` `mu_server_trigger_event` queues + delivers events; `test_event_notifications::test_alarm_event_generation_and_publishing` |
| 2536 | Base Info ContentFilter | event_filter_where | `event_filter.c` `mu_where_clause_eval` full operator set; `test_event_filter_where` |
| 3150 | Monitor Events | events | `notification.c` MonitoredItem on EventNotifier attr delivers EventFieldLists E2E; `test_event_notifications` |
| 4030 | Monitor Complex Event Filter | event_filter_where | `filter_reader.c` SELECT + `event_filter.c` WHERE, unsupported-operator rejection; `test_event_notifications` (E2E) |

### NOT claimed — grounded reasons

- **Auditing CUs (2422, 3968, 5213, 3228, 3224, 3226, 3230)**: the audit subsystem
  (`auditing/audit_events.c` `mu_raise_audit_event`) is a **tested dispatch stub with
  zero service integration** — no session/secure-channel/write/method/history/node-
  management handler ever raises an audit event (only 4 audit types defined, none
  emitted). Infra + isolated unit tests exist (PARTIAL), but the server does not
  actually emit AuditEvents, so none of the granular auditing CUs are claimed.
- **3194 Base Info Events Capabilities**: the EventNotifier attribute is never
  exposed — `read_attribute.c` has no `MU_ATTRIBUTEID_EVENTNOTIFIER` case and no node
  sets `.event_notifier`; a client cannot discover the Server as an event source.
- **3206 EventQueueOverflow EventType**: overflow silently discards the oldest event;
  no `EventQueueOverflowEventType` is inserted (behavioural mismatch).
- **3546 LocalTime Events**: the LocalTime event field is name-resolvable but always
  emitted Null (behavioural mismatch).
- **3199 System Status**: ServerStatus/State nodes exist and are read, but no
  system-status *event* generation; CU intent (node vs event) ambiguous — deferred.
- **Absent**: 2747, 2822, 2978, 3224, 3226, 3230, 4427 (N/A client), 5578 — no code.

### Surfaced concern (follow-up)

`opc_cu_auditing` (`MUC_OPCUA_CU_AUDITING`) is currently **claimed**, but the audit
subsystem has no service integration (no service call raises an audit event). Its
claim appears **over-stated** and should be reviewed — either wire audit-event
emission into the service handlers, or downgrade the alias. Out of scope for this
reconciliation increment; flagged here so it is not lost.

## 2026-07-15 — Node Management 2022 Server Facet (opc_id 1329)

54 CUs (15 required + 39 optional). Reconciled the 4 core NodeManagement
**service** CUs (facet 0/15 → 5/15 required incl. 2536 from 1328; overall
reconciled required 36 → 40). Added as granular manifest entries under
`service_nodemanagement` (`MUC_OPCUA_CU_NODEMANAGEMENT`, full only):

| CU | Name | Evidence |
| --- | --- | --- |
| 2380 | Node Management Add Node | `dispatch_node_mgmt.c` handle_add_nodes → mu_add_nodes_process; `test_node_management` (decode/encode, duplicate-NodeId) |
| 2394 | Node Management Delete Node | handle_delete_nodes → mu_delete_nodes_process; `test_node_management` |
| 2939 | Node Management Add Ref | handle_add_references → mu_add_references_process; `test_node_management` |
| 3153 | Node Management Delete Ref | handle_delete_references → mu_delete_references_process; `test_node_management` (incl. Bad_NotFound) |

### NOT claimed / deferred

- **2489 Node Management Capabilities**: no NodeManagement ServerCapabilities node
  in the address space.
- **~44 "Base Info X DataType" companion CUs** (2423/2481/2482/2500/2513/2514/…):
  these require exposing specific companion-spec DataType/ReferenceType nodes
  (RationalNumber, EUInformation, Audio, Spatial, ISA-95 relations, etc.) in the
  address space. As a minimal profile-targeting server we do not expose most of
  these; deferred as a batch — each needs per-node address-space grounding and most
  are genuinely absent. Core-type CUs (3185/3188/3189 base type folders/ServerType)
  are also deferred pending per-node grounding.

## 2026-07-16 — Auditing made real (spec 074), reconciles 4 CUs

Spec 074 (server-emitted, client-observable AuditEvents) turned the audit
subsystem from a host-callback stub into genuine OPC UA auditing (emit + observe
via the Server EventNotifier). That makes previously-unclaimable CUs honest:

| CU | Name | Alias | Now backed by |
| --- | --- | --- | --- |
| 3194 | Base Info Events Capabilities | opc_cu_events | Server EventNotifier readable; test_read_service::test_read_service_eventnotifier |
| 2422 | Auditing Secure Communication | opc_cu_auditing | AuditOpenSecureChannelEvent emitted+observable; test_event_notifications |
| 3968 | Auditing Services | opc_cu_auditing | Create/Activate/Write AuditEvents emitted+observable; test_event_notifications |
| 3228 | Auditing Write | opc_cu_auditing | AuditWriteUpdateEvent emitted+observable; test_event_notifications |

`opc_cu_auditing` re-annotated + test_event_notifications added as backing (the
emit->observe proof), resolving the over-claim flagged in the 1328 increment.
Overall reconciled required 40 -> 43. Still deferred: 5213 (no connection-close
audit type), 3224/3226/3230 (no NodeManagement/History/Method audit emit sites);
documented 074 follow-ups: failure-path (Status=false) auditing, OldValue capture,
SessionId/SecureChannelId string population.

## 2026-07-16 — Publish-queue overflow discards the oldest request (CU 3143)

Closes the behavioural mismatch flagged in the 1324 increment. OPC-10000-4
**§5.14.5.1** (verified against the reference, not §5.13.5 as the earlier follow-up
mis-cited): "When a Server receives a new Publish request that exceeds its limit it
shall **de-queue the oldest** Publish request and return a response with the result
set to Bad_TooManyPublishRequests." We previously rejected the **incoming** request
(`Bad_TooManyPublishRequests` on the new one) — a different observable behaviour.

Fix: on overflow, `handle_publish` (`subscription_crud.c`) now calls the new
`publish_request_evict_oldest` (`publish_due.c`), which de-queues the session's
oldest parked request (`publish_request_dequeue`, already FIFO-by-`enqueued_ms`) and
answers **that** request with a `Bad_TooManyPublishRequests` ServiceFault via the same
`mu_write_service_fault` + `mu_server_emit_message` path the timeout-eviction already
uses; the incoming request then parks in the freed slot. If the session has no parked
request to evict (queue full of another session), we fall back to returning the error
on the incoming request. The low-level `mu_publish_request_enqueue` primitive is
unchanged (still returns the error when full) — the discard policy lives at the
service layer.

| CU | Name | Alias | Now backed by |
| --- | --- | --- | --- |
| 3143 | Subscription PublishRequest Queue Overflow | opc_cu_subscription_basic | `publish_request_evict_oldest` (publish_due.c) + handle_publish wiring; test_subscriptions_capacity::test_publish_queue_overflow_evicts_oldest_and_parks_newest |

Facet 1324 (Standard DataChange Subscription) required 11/16 -> 12/16. Overall
reconciled required 44 -> 45 (one unique CU). The same shared CU also advances every
facet that lists 3143 — 1328 Auditing 14/18 -> 15/18, the A&C alarm facets, and the
Embedded/Standard 2022 profiles — but overall counts it once. Remaining 1324
required: 3911/4055 (ServerCapabilities nodes), 3922 (SemanticChange status bit),
5208 (monitored-item IndexRange slicing).

## 2026-07-16 — DataChange Subscription facet completed: 16/16 required (CUs 5208, 3911, 4055, 3922)

The remaining four required CUs of facet 1324, each with real code + a backing test.
All are gated on `MUC_OPCUA_CU_SUBSCRIPTION_BASIC` and aliased to
`opc_cu_subscription_basic` (off for nano, on for micro/embedded/standard/full).

| CU | Name | Now backed by |
| --- | --- | --- |
| 5208 | Monitor Value Change V2 | IndexRange parsed at MonitoredItem create (`subscription_helpers.c`), malformed → Bad_IndexRangeInvalid, applied to array samples via `apply_numeric_index_range` in `read_monitored_item_value`; `test_monitored_index_range` |
| 3911 | Base Info ServerCapabilities Subscriptions | `AggregateFunctions`(2997), `MaxSubscriptions`(24096), `MaxMonitoredItems`(24097), `MaxSubscriptionsPerSession`(24098), `MaxMonitoredItemsPerSubscription`(24104) added to both `base_nodes.c` tables; `test_operation_limits::test_subscription_capability_nodes_resolve` |
| 4055 | Base Info MaxMonitoredItemsQueueSize | `MaxMonitoredItemsQueueSize`(31916) node; same resolve test |
| 3922 | Base Info SemanticChange Bit | `mu_server_signal_semantic_change` latches the item; next DataChange Notification sets StatusCode bit 14 (0x4000) then the one-shot latch clears; `test_subscriptions::test_publish_semantics_changed_bit` |

Implementation notes:
- **5208**: the read-path `apply_index_range` helpers (`variant_elem_size`,
  `parse_numeric_range`) were previously gated on `MUC_OPCUA_CU_MULTI_CHUNK` (off on
  standard); the gate was widened to `|| MUC_OPCUA_CU_SUBSCRIPTION_BASIC` and a numeric
  core `apply_numeric_index_range` was extracted so read and sampling share one slicer.
  Per-item cost: two `opcua_int32_t` (parsed start/end, `-1` = no range).
- **3911/4055**: the base address space is binary-searched with **no linear fallback**,
  so it must stay strictly ascending by NodeId. The existing
  `test_base_address_space_is_sorted` guard (runs on every profile) confirms the new
  nodes preserve the ordering; `AggregateFunctions`(2997) needs two guarded copies in
  the type-system table (DataAccess-on vs -off) so exactly one is emitted in sorted
  position. nano excludes all of these via the compile gate.
- **3922**: the SemanticsChanged trigger is integrator-driven (the library owns no
  runtime EU/EURange changes in its static address space); providing the ABI +
  emitting the bit + clearing it one-shot is the honest, spec-conformant mechanism.

Facet 1324 required 12/16 -> **16/16** (complete). Overall reconciled required
45 -> 49 (four unique CUs). The shared subscription/base-info CUs also lift the
Auditing (1328), A&C alarm, and Embedded/Standard 2022 facet tallies that list them.

## 2026-07-16 — Auditing 074 follow-ups: OldValue, failure-path, SessionId/SecureChannelId

Deepens the already-claimed AuditEvents (2422 SecureChannel, 3968 Services, 3228
Write) so they are genuinely conformant, resolving the three follow-ups flagged when
spec 074 first made auditing real. No new CU numbers — this raises the fidelity of
existing claims. All full-profile-only (auditing is full-only).

- **OldValue capture** (attribute_handler.c): the pre-write value is read BEFORE the
  write and carried in AuditWriteUpdateEvent.OldValue (was always Null).
- **Failure-path auditing** (Status=false): rejected writes now emit an
  AuditWriteUpdateEvent with Status=false (was success-only); ActivateSession emits on
  failed activation (bad user identity); CreateSession emits on rejection
  (capacity/validation). An auditor now sees failed/rejected operations, not just
  successes — the security-relevant half.
- **SecureChannelId** (osc_handler.c): AuditOpenSecureChannelEvent carries the numeric
  channel id formatted as a String (was Null).
- **SessionId** (activate_session.c): AuditActivateSessionEvent carries the SessionId
  NodeId (was unset; CreateSession already set it).

Backing test: `tests/integration/test_write_service.c` gains an audit callback that
captures every raised event while the harness drives OPN + CreateSession +
ActivateSession + a successful write (1001, 10→42) + rejected writes; it asserts the
successful write's OldValue=10/NewValue=42, a rejected write with Status=false, a
non-empty SecureChannelId on the channel event, and a populated SessionId on the
activate event. Still deferred (new emit sites, not this increment): 3224/3226/3230
(NodeManagement/History/Method audits) and 5213 (connection-close).

## 2026-07-16 — Cheap-win reconciliation: Micro required-complete (30/30)

Claimed the required CUs that are genuinely satisfied but were unclaimed, after
rigorous per-CU verification against the honest-evidence bar. NET: overall required
49 -> 52; **Micro 28/30 -> 30/30 (required-complete)**, Embedded 36 -> 39/48,
Standard 36 -> 39/52, Nano 18/18.

Claimed:
- **3536** User Name/Password 2 -> `satisfied_by: opc_cu_user_auth` (claimed alias;
  MUC_OPCUA_CU_USER_AUTH on for micro/embedded/standard/full). Encrypted
  username/password decrypt+verify in activate_session.c; test_user_auth_encrypted/
  plaintext/secure_e2e.
- **3808** Documentation - Core Capacities, **3080** Default ApplicationInstance
  Certificate -> new `implementation_state: "documented"`. These are Documentation
  CUs satisfied by shipped docs, not code. Added the `documented` state to the
  tooling (model.py allows it, completion.py counts it, generate.py renders it as
  "(DOCUMENTED)"), exempt from the backing_tests/kconfig rules that code claims
  carry — see the model.py comment. 3808 is backed by docs/integration-guide.md
  §2.2.1 (new capacities table) + the runtime ServerCapabilities/OperationLimits
  nodes; 3080 by §6 (self-signed-at-boot + get_own_certificate provisioning).

Deliberately NOT claimed (verification caught over-claims flagged by an initial
optimistic pass — honest under-claim per the 073 ethos):
- **3188** Base Types, **3189** ServerType, **5801** Type Information — require the
  COMPLETE type system (all Encoding Objects, ModellingRuleType, every
  InstanceDeclaration, all EventTypes); we expose a subset. The type-system facet
  opc_facet_1219 stays `deferred` for the same reason.
- **3185** Core Types Folders — the folders ARE exposed+tested (test_type_system),
  but claiming needs a Kconfig gate distinct from the (deferred) full-type-system
  facet whose symbol it would otherwise duplicate. Left deferred pending that split.
- **2823** Invalid user token — requires anti-brute-force mitigation (repeated-attempt
  lockout) we don't implement; rejection alone is insufficient.
- **3125** User X509 — token signature is verified (proof-of-possession) but not the
  full user-certificate trust/chain validation the CU requires.

Genuine remaining gaps (real feature work): 3641/2483/4426 (specialized DataType
nodes), 2231 (Part-12 push cert mgmt), 2271/3170 (outbound RegisterServer/2), 2190
(Session Cancel).

## 2026-07-16 — Roadmap A1: specialized DataType nodes (CU 4426, 2483)

First step of the Embedded/Standard completion roadmap. Exposes specialized DataType
nodes in the base address-space type-system table, gated by a new selectable CU
symbol `MUC_OPCUA_CU_BASE_INFO_DATATYPES` (embedded/standard/full; implied by the
type-system facet). Emb/Std required 39 -> 41 (on top of the merged cheap-win pass).

| CU | Now backed by |
| --- | --- |
| 4426 Base Info Decimal DataType | `Number`(i=26) + `Decimal`(i=50) nodes added to base_nodes.c with HasSubtype closure BaseDataType→Number→Decimal; test_type_system::test_specialized_datatype_nodes_and_supertypes |
| 2483 Base Info Date DataTypes | `DurationString`(12879)/`TimeString`(12880)/`DateString`(12881) as subtypes of String(12); same test |

Both `satisfied_by` the new claimed alias `opc_cu_base_info_datatypes`. Notes:
- Sorted-table discipline (no linear fallback): Number(26) sorts after BaseDataType(24)
  before References(31); Decimal(50) sorts between HasComponent(47) and BaseObjectType(58)
  — a first pass wrongly put 50 before 31, caught by test_base_address_space_is_sorted.
- The new CU symbol had to be propagated to test TUs via
  `target_compile_definitions(... PUBLIC)` in src/CMakeLists.txt (autoconf force-include
  reaches only the library, not tests).
- **3641** (Argument DataType) was deferred to A2: its Encoding Objects require
  DataTypeEncodingType(i=76), which is part of the full type-system work (3188).

## 2026-07-17 — Roadmap A2 (first slice): Argument DataType + encoding infra (CU 3641)

First slice of the type-system completion. Exposes the Argument(i=296) DataType
(subtype of Structure) + its Default XML(297)/Default Binary(298) Encoding Objects,
plus the shared DataTypeEncodingType(76) ObjectType and HasEncoding(38) ReferenceType,
in base_nodes.c — gated by a new symbol MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE (alias
opc_cu_base_info_argument_type; embedded/standard/full). Emb/Std required 41 -> 42.
Backed by test_type_system::test_argument_datatype_and_encodings; sorted-table
invariant enforced. The larger 3188 base-DataType set + 3185 folders follow; 3189
ServerType and 5801 completeness (the ~35 KB flash item — scoped vs full is a
maintainer decision) come after.

## 2026-07-17 — Roadmap A2 (second slice): Base Types + Core Types Folders (CU 3188, 3185)

Completes the base OPC UA type system in `base_nodes.c`, gated by a new selectable CU
symbol `MUC_OPCUA_CU_BASE_INFO_BASE_TYPES` (alias `opc_cu_base_info_base_types`;
embedded/standard/full; `depends on` the type-system facet **and** the specialized-DataTypes
CU). Emb/Std required 44 -> 46.

| CU | Now backed by |
| --- | --- |
| 3188 Base Info Base Types | The remaining built-in/abstract DataTypes (Guid14, ByteString15, XmlElement16, ExpandedNodeId18, DataValue23, DiagnosticInfo25, Integer27, UInteger28, Enumeration29, Duration290, NumericRange291, UtcTime294, EnumValueType7594, Union12756), HasModellingRule(37), ModellingRuleType(77) + its ModellingRule Objects (Optional80/Mandatory78/ExposesItsArray83/OptionalPlaceholder11508/MandatoryPlaceholder11510), and the EnumValueType Encoding Objects (DefaultXml7616/DefaultBinary8251) exposed in base_nodes.c with full HasSubtype/HasEncoding closure; test_type_system::test_base_types_and_modelling_rules |
| 3185 Base Info Core Types Folders | The core type folders Types(86)/ObjectTypes(88)/VariableTypes(89)/DataTypes(90)/ReferenceTypes(91) (already present) asserted as CU-3185 entry points; same test |

Both `satisfied_by` the new claimed alias `opc_cu_base_info_base_types`. Notes:
- **Honest hierarchy (GPT-5 pre-code review):** claiming 3188 while leaving Int32/Float/etc.
  directly under BaseDataType would advertise a wrong type tree, so the numeric primitives are
  **re-parented under Integer(27)/UInteger(28)/Number(26)** per OPC-10000-3 whenever BASE_TYPES
  is on (`s_base_data_type_refs` drops 2..11 under `#if !BASE_TYPES`; new
  `s_integer_subtype_refs`/`s_uinteger_subtype_refs` + Number->Integer/UInteger/Float/Double).
  Existing `test_type_hierarchies_have_subtype_references` assertions were made conditional
  (24->6/7 becomes 27->6 / 28->7 under BASE_TYPES, with negative checks locking the re-parenting).
- **Structure(22) gate hardening (review DESIGN DECISION 1):** Structure(22), its browse string,
  its subtype-ref array, and the BaseDataType->Structure edge were gated on the historical
  LocalTime/EngineeringUnits/Currency trio only; slice-1's Argument(296) worked by profile
  coincidence. Replaced with a single `MU_HAVE_STRUCTURE_TYPE` macro that also includes
  ARGUMENT_TYPE and BASE_TYPES, decoupling those claims from unrelated CUs. Likewise
  HasEncoding(38)/DataTypeEncodingType(76) and s_str_Default_Binary widened to
  `ARGUMENT_TYPE || BASE_TYPES`.
- **DataAccess-branch duplication:** the 2997..8912 tail is emitted twice (once inside
  `#if DATA_ACCESS`, once in the `!DATA_ACCESS` branch) to preserve the binary-searched sort
  order. EnumValueType(7594)/its encodings therefore appear in both branches (mirroring the
  existing TimeZone(8912) handling); `test_base_address_space_is_sorted` caught the first,
  DA-only, placement (embedded builds DATA_ACCESS off).
- `s_str_Mandatory`/`s_str_Optional` were moved out of the `#if DATA_ACCESS` string block so the
  78/80 ModellingRule Objects (now gated `DATA_ACCESS || BASE_TYPES`, typed by ModellingRuleType
  77 when BASE_TYPES is on) resolve their BrowseNames.
- New CU symbol propagated to test TUs via `target_compile_definitions(... PUBLIC)` in
  src/CMakeLists.txt (mirrors DATATYPES/ARGUMENT_TYPE; the top-level KCONFIG_FEATURES list is
  not required — only LOCALTIME appears there).
- Deferred earlier, now DONE: the strict primitive re-parenting the A2 handoff had punted to the
  5801 block was pulled forward here because it is a prerequisite for an honest 3188 claim.
- Verified: all 5 profiles green (nano 101 / micro 124 / embedded 125 / standard 125 / full 137),
  manifest validate OK, 32-bit ARM `-ffreestanding -Werror` compile of base_nodes.c clean.

### Pre-existing observation (out of scope for 080b)
GPT-5's diff review flagged that `MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS` (and, by the same
pattern, CURRENCY/LOCALTIME) emit their structured-DataType nodes (EUInformation i=887,
CurrencyUnitType, TimeZoneDataType) in dedicated blocks that are NOT in global sorted position:
on `main`, the EUInformation(887) node already sits physically after ServerCapabilities(2268), so
enabling ENGINEERING_UNITS with the type system yields an unsorted `s_base_nodes[]` (2268 -> 887).
This is a pre-existing latent issue in those optional CUs — no named profile enables them, so
`test_base_address_space_is_sorted` never exercises it. Spec 080b does not introduce or worsen any
NAMED-profile ordering (all 5 remain sorted); fixing the ENG/CURRENCY/LOCALTIME node placement is a
separate concern tracked for a future slice.

## 2026-07-17 — Roadmap A2 (third slice): ServerType type tree (CU 3189)

Exposes the `ServerType`(2004) type tree in `base_nodes.c`, gated by a new selectable CU
symbol `MUC_OPCUA_CU_BASE_INFO_SERVERTYPE` (alias none; embedded/standard/full;
`depends on` the type-system facet `MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER` **and**
`MUC_OPCUA_CU_BASE_INFO_BASE_TYPES`, spec 080b's second slice). Emb/Std required 46 -> 47.

**61 new nodes**, by category:
- **14 ObjectTypes**: `ServerCapabilitiesType`(2013), `ServerDiagnosticsType`(2020),
  `SessionsDiagnosticsSummaryType`(2026), `SessionDiagnosticsObjectType`(2029),
  `VendorServerInfoType`(2033), `ServerRedundancyType`(2034) +
  `TransparentRedundancyType`(2036) / `NonTransparentRedundancyType`(2039) /
  `NonTransparentNetworkRedundancyType`(11945), `OperationLimitsType`(11564),
  `FileType`(11575), `AddressSpaceFileType`(11595), `NamespaceMetadataType`(11616),
  `NamespacesType`(11645).
- **12 VariableTypes**: `ServerVendorCapabilityType`(2137), `ServerStatusType`(2138),
  `ServerDiagnosticsSummaryType`(2150), the Sampling/Subscription/Session/SessionSecurity
  diagnostics (Array)Types (2164/2165/2171/2172/2196/2197/2243/2244), `BuildInfoType`(3051).
- **13 DataTypes**: 11 structured `Structure`-subtypes — `BuildInfo`(338),
  `RedundantServerDataType`(853), `SamplingIntervalDiagnosticsDataType`(856),
  `ServerDiagnosticsSummaryDataType`(859), `ServerStatusDataType`(862),
  `SessionDiagnosticsDataType`(865), `SessionSecurityDiagnosticsDataType`(868),
  `ServiceCounterDataType`(871), `SubscriptionDiagnosticsDataType`(874),
  `EndpointUrlListDataType`(11943), `NetworkGroupDataType`(11944) — plus 2 enums
  `RedundancySupport`(851), `ServerState`(852).
- **22 Default XML/Binary Encoding Objects** for the 11 structured DataTypes above.

All 61 nodes carry a full `HasSubtype` supertype closure back to their abstract roots and
`HasEncoding` references for the structured DataTypes, matching the existing 3188/3185
sorted-table + closure conventions.

**Scope: type-nodes-only.** This slice exposes the *type tree* — the ObjectType/
VariableType/DataType nodes and their relationships — not the ~550 Mandatory child
Variables (InstanceDeclarations) each type prescribes (e.g. `ServerType.ServerStatus`,
`ServerType.ServerCapabilities.MaxBrowseContinuationPoints`, …). Per the roadmap's
InstanceDeclaration-completeness policy, those are **deferred to CU 5801**, where they
will nest inside this same `MUC_OPCUA_CU_BASE_INFO_SERVERTYPE` gate rather than a new one.
Claiming 3189 now (type tree only) and 5801 later (instance completeness) mirrors how
3188/3185 claimed the base type system before instance-level facets landed.

**BuildInfoType(3051) dual-branch placement:** its NodeId falls inside the pre-existing
`DATA_ACCESS` dual-copy region of `s_base_nodes[]` (the 2997..8912 tail that is emitted
once under `#if DATA_ACCESS` and once under `#if !DATA_ACCESS` to preserve global sort
order — see the 080b second-slice entry above for why that duplication exists). 3051
therefore had to be added to **both** branches; embedded builds with `DATA_ACCESS` off
exercise the second copy, so a single-branch add would have broken
`test_base_address_space_is_sorted` on embedded specifically.

**Verified:** all 5 profiles green (nano 101 / micro 124 / embedded 125 / standard 125 /
full 137 — `make test-profiles`), manifest validate OK, 32-bit ARM `-ffreestanding -Werror` compile of
`base_nodes.c` clean. `.text` cost (ARM Cortex-M0+ `-Os`, `scripts/measure_size.sh all`,
vs `main` @ 3d097b8): nano/micro unaffected (no type system); embedded/standard/full each
+9,280 B (identical delta — the 61 nodes are profile-independent once the type system is
on). Archive `.data`/`.bss` remain 0 B on all 5 profiles (constitution rule intact).

| CU | Now backed by |
| --- | --- |
| 3189 Base Info ServerType | The `ServerType`(2004) type tree — 14 ObjectTypes, 12 VariableTypes, 13 DataTypes (11 structured + 2 enums), 22 Default XML/Binary Encoding Objects — with full HasSubtype/HasEncoding closure in `base_nodes.c`; type-nodes-only, InstanceDeclarations deferred to CU 5801; backed by `test_type_system::test_servertype_type_tree_and_encodings` |

## 2026-07-17 — Roadmap A2 (fourth slice): ServerType InstanceDeclarations (CU 5801 core, first slice)

Adds the core Mandatory InstanceDeclarations for the `ServerType`(2004) tree that the
previous (third) slice's type-nodes-only scope deferred. Gated by a new selectable CU
symbol `MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION` (OPC item `opc_cu_5801`, alias none;
embedded/standard/full; `depends on` the type-system facet
`MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER` **and** `MUC_OPCUA_CU_BASE_INFO_SERVERTYPE`,
the 3189 gate this slice nests inside). `implementation_state` is `deferred` — the CU is
selectable and buildable but **not counted** toward the profile's required/claimed CU
tally, and `opc_cu_5801` is **not claimed** by this slice (see below for why).

**65 new InstanceDeclaration nodes**, by owning type:
- **`ServerStatusType`(2138) — 6**: own Mandatory children (State/CurrentTime/StartTime
  already existed pre-slice from the ServerStatus runtime work; this slice adds the
  remaining declared children — SecondsTillShutdown(2752)/ShutdownReason(2753) plus
  BuildInfo(2141) and its HasComponent closure) modeled as HasComponent/HasProperty +
  HasTypeDefinition + HasModellingRule, matching OPC-10000-5 §6.3.2's InstanceDeclaration
  tables.
- **`BuildInfoType`(3051) — 6**: ProductUri/ManufacturerName/ProductName/SoftwareVersion/
  BuildNumber/BuildDate (3052-3057), each a Mandatory Property.
- **`ServerDiagnosticsSummaryType`(2150) — 12**: the full Mandatory Variable set (Server-
  ViewCount/CurrentSessionCount/CumulatedSessionCount/SecurityRejectedSessionCount/
  RejectedSessionCount/SessionTimeoutCount/SessionAbortCount/PublishingIntervalCount/
  CurrentSubscriptionCount/CumulatedSubscriptionCount/SecurityRejectedRequestsCount/
  RejectedRequestsCount).
- **`ServerCapabilitiesType`(2013) — 24**: the Mandatory capability Variables/Objects
  (ServerProfileArray/LocaleIdArray/MinSupportedSampleRate/MaxBrowseContinuationPoints/
  MaxQueryContinuationPoints/MaxHistoryContinuationPoints/SoftwareCertificates/
  MaxArrayLength/MaxStringLength/MaxByteStringLength, ModellingRules Folder,
  AggregateFunctions Folder, OperationLimits Object + its own Mandatory children, and
  `<VendorCapability>`). `RoleSet`(16295) is **dropped**: its TypeDefinition
  `RoleSetType`(15607) doesn't exist yet in this address space, so instantiating RoleSet
  would dangle a HasTypeDefinition reference — deferred to whichever future slice adds
  RoleSetType. `<VendorCapability>` uses `OptionalPlaceholder`(11508) (the existing CU
  3188 ModellingRule Object) rather than a new node.
- **`ServerType`(2004) direct children — 17** (incl. 4 Methods): ServerArray/
  NamespaceArray/ServerStatus/ServiceLevel/Auditing/ServerCapabilities/
  ServerDiagnostics/VendorServerInfo/ServerRedundancy/NamespaceMetadata/
  Namespaces (deferred where their TypeDefinitions aren't yet exposed — see below) plus
  the 4 Methods `GetMonitoredItems`/`ResendData`/`SetSubscriptionDurable`/
  `RequestServerStateChange`, declared with HasComponent + HasModellingRule but their
  In/OutputArguments Property nodes omitted (Argument-list fidelity is out of scope for
  this core slice).

All 65 nodes are modeled structurally: HasComponent/HasProperty (as appropriate) from
their owning type, HasTypeDefinition to the correct VariableType/ObjectType, and
HasModellingRule to Mandatory(78) (or OptionalPlaceholder(11508) for `<VendorCapability>`).

**DATA_ACCESS dual-branch placement:** `BuildInfoType` children 3052-3057, `ServerStatusType`
2752/2753, and 6 `ServerCapabilitiesType`/`ServerType` property nodes
(MaxBrowseContinuationPoints(2732)/MaxQueryContinuationPoints(2733)/
MaxHistoryContinuationPoints(2734)/Auditing(2742)/AggregateFunctions(2754)/
SoftwareCertificates(3049)) fall inside the pre-existing `DATA_ACCESS` dual-copy region of
`s_base_nodes[]` (see the
3189-slice entry above for why that region is duplicated once under `#if DATA_ACCESS` and
once under `#if !DATA_ACCESS`). Each of those NodeIds was added to **both** branches so the
table stays globally sorted whether `DATA_ACCESS` is on or off; embedded builds with
`DATA_ACCESS` off exercise the second copy, so a single-branch add would have broken
`test_base_address_space_is_sorted` on embedded specifically.

**Design note — ServerType.EstimatedReturnTime/LocalTime/the 4 Methods declared
unconditionally:** these are emitted under `MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`
without also gating on the sibling instance-facet CUs that would normally govern whether
the *instance* behavior (e.g. an actual LocalTime runtime value, or the Methods being
callable) is present. Per CU 5801's own Kconfig help text, 5801 is about
type-*completeness* in the AddressSpace — declaring that an InstanceDeclaration exists —
not about whether the corresponding runtime feature is wired up. Gating the declaration
nodes on those sibling CUs would make the InstanceDeclaration set vary with unrelated
feature flags, which is exactly the kind of incompleteness 5801 exists to catch.

**Why 5801 is NOT yet claimed (two named prerequisites):**
1. **Scope**: this is the *core* slice only. The much larger set of
   Session/Subscription/SamplingInterval diagnostics-type InstanceDeclarations (the
   `SessionDiagnosticsObjectType`/`SessionsDiagnosticsSummaryType`/
   `SubscriptionDiagnosticsType`/`SamplingIntervalDiagnosticsArrayType` families under
   `ServerDiagnosticsType`(2020), roughly 350 nodes) are deferred to later slices.
2. **Pre-existing server-wide gap**: DataType/ValueRank attribute fidelity is not
   per-node accurate anywhere in the server today. `read_attribute.c` returns the node's
   `type_definition` for the `DataType` Attribute and `-1` for `ValueRank` on **every**
   Variable, regardless of what DataType/ValueRank that Variable's type actually
   declares. CU 5801's own text requires "the DataType of the Variable shall be provided
   in the AddressSpace" — which is a weaker requirement about node *presence*, but an
   honest claim also implies a Server that reports its own DataTypes correctly when
   queried; this pre-existing gap needs fixing before 5801 can be claimed even for the
   subset of the tree this slice covers.

**Verified:** all 5 profiles green (nano 101/102, micro 124/125, embedded 125/126,
standard 125/126, full 137/138 — each with exactly one failure,
`test_no_stale_project_name`, caused by untracked planning-doc files under
`docs/superpowers/plans/` containing absolute `micro-opcua` paths, not a code
regression), manifest validate OK, 32-bit ARM `-mcpu=cortex-m0plus -mthumb
-ffreestanding -Werror` compile of `base_nodes.c` clean. `.text` cost (ARM Cortex-M0+
`-Os`, `scripts/measure_size.sh all`, vs `main` @ 1ef418b): nano/micro unaffected (CU
gated off, byte-identical); embedded/standard/full each **+11,111 B** archive `.text`
(identical delta — the 65 nodes are profile-independent once the type-information CU is
on). Archive `.data`/`.bss` remain 0 B on all 5 profiles (constitution rule intact; pure
`const` flash addition, no mutable static state).

| CU | Now backed by |
| --- | --- |
| 5801 Base Info Type Information (core, not yet claimed) | 65 core InstanceDeclarations under the `ServerType`(2004) tree — `ServerStatusType`(6), `BuildInfoType`(6), `ServerDiagnosticsSummaryType`(12), `ServerCapabilitiesType`(24, `RoleSet` dropped pending `RoleSetType`), `ServerType`-direct(17 incl. 4 Methods) — in `base_nodes.c`, gated `MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION` (state `deferred`); backed by `test_type_system::test_servertype_instance_declarations`; the ~350-node Session/Subscription diagnostics InstanceDeclarations and the server-wide DataType/ValueRank attribute-fidelity fix remain outstanding before an honest claim |

## 2026-07-17 — DataType/ValueRank attribute fidelity (CU 5801 prerequisite)

Closes prerequisite #2 named in the slice above ("Pre-existing server-wide gap:
DataType/ValueRank attribute fidelity is not per-node accurate anywhere in the
server today").

**Read path (`mu_node_t` + `read_attribute.c`):** added `mu_node_t.data_type`
(`opcua_uint32_t`, ns0 numeric DataType NodeId, e.g. `12`=String, `294`=UtcTime,
`862`=ServerStatusDataType; `0` = unset) and `mu_node_t.value_rank`
(`opcua_sbyte_t`, only consulted once `data_type != 0`). `sizeof(mu_node_t)`
is **unchanged (136 B)** — both fields land in existing struct padding, verified
by compiling a `sizeof` probe against the pre- and post-change header. `DATATYPE`/
`VALUERANK` reads now return the populated per-node value when `data_type != 0`,
else fall back to the legacy behavior (`type_definition` NodeId / `-1`) exactly as
before.

**The MULTI_CHUNK bugfix:** `DATATYPE`/`VALUERANK` are mandatory attributes of
every Variable and VariableType node (OPC-10000-3 §5.6.2) — but the handling for
both lived inside `read_multichunk_attribute()`, a helper only compiled in under
`MUC_OPCUA_CU_MULTI_CHUNK`. On nano/micro/embedded (where that CU is off), reading
either attribute returned `Bad_AttributeIdInvalid` for every Variable in the
address space, including base nodes like `ServerStatus`/`ServerArray`. Both cases
were moved into the main `read_attribute()` switch, unconditional on any CU —
these attributes are now answered correctly on all five profiles for the first
time, with no other test regressing.

**Populated 53 Variable-class CU-5801 InstanceDeclarations** under the
`ServerType`(2004) tree in `base_nodes.c` with their true DataType/ValueRank,
grounded against the official OPC Foundation `NodeSet2.xml` (not memory/assumed
values, per project convention) — including correcting `StartTime`(2139),
`CurrentTime`(2140), and `BuildDate`(3057) to **UtcTime(294)**, a `DateTime`
subtype, rather than the previously-assumed plain `DateTime`(13) (`NodeSet2.xml`
lists these as `UtcTime`; `test_type_system.c`'s original Task-1 placeholder
assertion had the wrong DataType *and* cited the wrong spec section for it,
corrected in the same slice). Structured/Object-class and diagnostics-type
InstanceDeclarations are out of scope for this fix.

**This closes ONE of the two named prerequisites for claiming CU 5801** — the
DataType/ValueRank fidelity gap. The remaining prerequisite (the ~350-node
Session/Subscription/SamplingInterval diagnostics-type InstanceDeclarations under
`ServerDiagnosticsType`(2020)) is still outstanding. **5801 remains `deferred`** —
the manifest state and completion counts (47/51 required, 47/55 overall) are
unchanged by this slice; only the read-path accuracy underneath the already-added
node set improved.

**Verified:** all 5 profiles green — nano 102/102, micro 125/125, embedded
126/126, standard 126/126, full 138/138 (no regressions; DATATYPE/VALUERANK now
answered instead of `Bad_AttributeIdInvalid` on nano/micro/embedded, confirmed
by the existing attribute-read tests still passing plus the new
`assert_datatype_and_valuerank` cases, compiled out on nano/micro where CU 5801
itself is off). 32-bit ARM Cortex-M0+ freestanding compile
(`-mcpu=cortex-m0plus -mthumb -Wall -Wextra -Werror -Wpedantic -Wshadow
-Wconversion -Wcast-align -Wunused -Wformat=2`, embedded profile's exact `-D`
set) of both `base_nodes.c` and `read_attribute.c` clean — no `-Wconversion`
fallout from the new `sbyte`/`uint32` fields. `clang-format --dry-run --Werror`
clean on all 4 changed files. `scripts/measure_size.sh all` vs `main`
(`d76119d`): archive `.bss` **0 B on all 5 profiles**, `.text` delta small and
non-zero as expected from the read-path logic gaining unconditional
DATATYPE/VALUERANK handling (see `docs/size/feature-size-ledger.md` for the exact
per-profile numbers) — no struct growth, so the cost is code, not data.
