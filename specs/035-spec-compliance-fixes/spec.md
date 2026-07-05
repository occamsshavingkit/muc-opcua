# Feature Specification: Spec Compliance Fixes

**Feature Branch**: `035-spec-compliance-fixes`  
**Created**: 2026-07-05  
**Source**: `docs/audit/spec-compliance-2026-07-05.md` (23 INCORRECT findings)
**Scope**: Fix all implemented-but-non-conformant behaviors. NOT IMPLEMENTED items are out of scope.

## User Stories

### US1 — SecureChannel & Session fixes (P1) — 7 issues
I1, I2, I3, I4, I6, I14, I21 — ClientNonce validation (×2), ApplicationUri check, CloseSecureChannel handler, requestType validation, ServerProtocolVersion, ServerUri/EndpointUrl validation.

### US2 — Subscription & MonitoredItem fixes (P1) — 8 issues
I5, I8, I9, I10, I11, I12, I13, I22 — SetPublishingMode state table, sampling interval=-1 (×2), timestampsToReturn, queue size, queueSize/discardOldest apply, filter type changes, post-close rejection.

### US3 — Browse, Read, Write fixes (P2) — 6 issues
I7, I15, I16, I17, I18, I19 — ViewDescription handling, Read timestamps, TranslateBrowsePaths namespace + empty targetName validation, Write attributeId type, Write subtype matching.

### US4 — Encoding & Security fixes (P2) — 2 issues
I20, I23 — Channel ID randomness, Variant array read.

## Requirements

- **FR-001**: ClientNonce length MUST be validated in OpenSecureChannel and CreateSession (32-128 bytes, Bad_NonceInvalid).
- **FR-002**: CreateSession MUST validate ApplicationUri against ClientCertificate (Bad_CertificateUriInvalid).
- **FR-003**: CloseSecureChannel MUST call mu_secure_channel_close() and encode a CloseSecureChannelResponse.
- **FR-004**: requestType in OpenSecureChannel MUST be validated (Bad_RequestTypeInvalid if not ISSUE/RENEW).
- **FR-005**: ServerProtocolVersion in OPN response MUST report actual supported version, not 0.
- **FR-006**: CreateSession MUST validate ServerUri and EndpointUrl against configured values (Bad_ServerUriInvalid).
- **FR-007**: SetPublishingMode MUST reset lifetime counter and set MoreNotifications=FALSE per state table row 19.
- **FR-008**: Sampling interval=-1 MUST use the Subscription publishing interval, not 50ms.
- **FR-009**: CreateMonitoredItems MUST populate timestampsToReturn in DataValues.
- **FR-010**: CreateMonitoredItems response MUST use actual revised queue_size, not hardcoded 1.
- **FR-011**: ModifyMonitoredItems MUST apply decoded queueSize and discardOldest to the item.
- **FR-012**: ModifyMonitoredItems MUST apply filter type changes.
- **FR-013**: CloseSession MUST reject subsequent requests with Bad_SessionClosed.
- **FR-014**: Browse MUST filter references by ViewDescription when a View is requested.
- **FR-015**: Read MUST populate SourceTimestamp/ServerTimestamp in DataValues per timestampsToReturn.
- **FR-016**: TranslateBrowsePaths MUST match targetName including namespace_index.
- **FR-017**: TranslateBrowsePaths MUST reject empty targetName on last path element (Bad_BrowseNameInvalid).
- **FR-018**: Write attributeId MUST use mu_binary_read_uint32 (not int32).
- **FR-019**: Write type check MUST accept subtypes of the Attribute's DataType.
- **FR-020**: Channel ID MUST use crytographically random values from entropy adapter.
- **FR-021**: Variant reader MUST support arrays (parse Int32 length + elements).
- **FR-022**: All 108 existing tests MUST pass with zero regressions.

## Success Criteria

- **SC-001**: All 23 INCORRECT findings resolved.
- **SC-002**: 108/108 tests pass.
- **SC-003**: No new .data, .bss, or heap.
