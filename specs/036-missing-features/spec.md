# Feature Specification: Implement Missing OPC UA Features

**Feature Branch**: `036-missing-features`  
**Source**: `docs/audit/spec-compliance-2026-07-05.md` (NOT IMPLEMENTED §3)
**Scope**: 15 practical missing features. Audit events, PKI infrastructure, poll-model limitations excluded.

## User Stories

### US1 — Subscription Lifecycle (P1) — N2, N4, N5, N6, N7, N9, N10
Session timeout monitoring, subscription lifetime expiration, timeoutHint, Bad_NoSubscription, StatusChangeNotification, DeleteClientPublReqQueue, immediate sample on SetMonitoringMode enable.

### US2 — Read Enhancements (P1) — N12, N13, N14
index_range support, maxAge handling, additional readable attributes.

### US3 — Write & Browse (P2) — N15, N16, N19
Write timestamps, additional writable attributes, Browse resultMask.

### US4 — Encoding (P2) — N21, N22, N23
GUID NodeId, Opaque NodeId, multi-chunk messages.

## Requirements

- **FR-001**: Session timeout monitoring shall auto-close idle sessions per configured timeout.
- **FR-002**: Subscription lifetime counter shall auto-delete expired subscriptions.
- **FR-003**: Publish shall check timeoutHint on parked requests.
- **FR-004**: Publish shall return Bad_NoSubscription when session has no subscriptions.
- **FR-005**: StatusChangeNotification shall be issued on subscription expiry.
- **FR-006**: DeleteClientPublReqQueue shall run on subscription param changes.
- **FR-007**: SetMonitoringMode enable shall trigger immediate sample.
- **FR-008**: Read shall support index_range for array attributes.
- **FR-009**: Read shall honor maxAge (re-read from source if 0).
- **FR-010**: Read shall support additional attributes (DESCRIPTION, WRITEMASK, VALUERANK, DATATYPE, ACCESSLEVEL, HISTORIZING).
- **FR-011**: Write shall preserve SourceTimestamp/ServerTimestamp from DataValue.
- **FR-012**: Write shall support NodeClass attribute changes for Variables.
- **FR-013**: Browse shall apply resultMask to ReferenceDescription fields.
- **FR-014**: NodeId encoder shall support GUID (0x04) and Opaque (0x05) formats.
- **FR-015**: MessageChunk shall support multi-chunk ('C' continuation) messages.
- **FR-016**: All 108 tests shall pass with zero regressions.

## Success Criteria

- **SC-001**: All 15 NOT IMPLEMENTED findings resolved.
- **SC-002**: 108/108 tests pass.

## Edge Cases

- **Lifetime expiry with queued data**: When a subscription expires, any queued Publish requests must be drained with appropriate StatusChangeNotification.
- **timeoutHint race**: Parked Publish request may be serviced between timeout check and processing — use atomic check.
- **index_range on scalar values**: Return Bad_IndexRangeNoData.
- **maxAge with no adapter**: If time adapter not configured, treat all maxAge as 0 (re-read).
- **GUID NodeId in dispatch table**: Existing dispatch table uses NUMERIC NodeIds — GUID/Opaque are encoder-only, not for dispatch.
- **Multi-chunk with security**: Chunk reassembly must handle encrypted chunks before decryption.
